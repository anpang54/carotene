
#pragma once

/*

    There are 3 separate backends:
      - Windows:                      WinHTTP, linked normally
      - macOS, Linux, FreeBSD, Haiku: libcurl, opened at runtime with dlopen(), because there's no sysroot
      - Web:                          XMLHttpRequest
    
    All 3 are blocking/synchronous for consistency. I will likely add a callback system in the future.

*/


// INCLUDES

#ifdef _WIN32

    // WinHTTP
    #define WIN32_LEAN_AND_MEAN           // skip a bunch of useless windows.h stuff
    #define NOMINMAX                      // don't define min and max
    #define TokenType WindowsTokenType    // to not clash with our TokenType
    #include <windows.h>
    #include <winhttp.h>
    #undef TokenType

#elif !defined(__EMSCRIPTEN__)

    // libcurl
    #include <dlfcn.h>

#endif

#include "../../core/format.hpp"


namespace CaroHttp{


    // SETTINGS

    #define HTTP_USER_AGENT "Carotene/" VERSION

    const double defaultTimeout = 30;    // seconds
    const long   maxRedirects   = 20;


    // RESPONSES

    struct Response{
        uint32_t status = 0;
        vector<pair<string, string>> headers;    // order matters
        string body;
    };


    // adding headers

    void addHeader(Response& response, string_view line) {

        // ignore status lines like HTTP/1.1 301 Moved Permanently
        if(line.starts_with("HTTP/")) {
            response.headers.clear();
            return;
        }

        // parse
        size_t colon = line.find(':');
        if(colon == string::npos) return;
        string name = lower(trim(line.substr(0, colon)));
        if(name.empty()) return;
        string value = trim(line.substr(colon + 1));

        // join headers that appear multiple times with commas
        for(pair<string, string>& header: response.headers) {
            if(header.first == name) {
                header.second += ", " + value;
                return;
            }
        }

        response.headers.push_back({std::move(name), std::move(value)});

    }

    bool hasHeader(const vector<pair<string, string>>& headers, const string& name) {
        for(const pair<string, string>& header: headers) {
            if(lower(header.first) == name) return true;
        }
        return false;
    }

    size_t addHeaderBlock(Response& response, string_view block, size_t position = 0) {

        while(position < block.size()) {

            size_t lineEnd = block.find('\n', position);
            string_view line = block.substr(position, lineEnd - position);
            position = lineEnd == string::npos? block.size(): lineEnd + 1;

            if(line.empty() || line == "\r") break; 
            addHeader(response, line);

        }

        return position;

    }


    // URL CHECKING

    string normalizeUrl(string& url) {

        size_t schemePosition = url.find("://");

        // add https:// on native platforms
        #ifndef __EMSCRIPTEN__
            if(schemePosition == string::npos) {
                url = "https://" + url;
                schemePosition = 5;
            }
        #endif

        // only allow http:// and https://
        if(schemePosition != string::npos) {
            string protocol = lower(url.substr(0, schemePosition));
            if(protocol != "http" && protocol != "https") {
                return format("\"{:s}\" isn't an HTTP or HTTPS URL.", url);
            }
        }

        return "";

    }


    // ERRORS

    string requestError(const string& method, const string& url, const string& reason = "") {
        return reason.empty()?
               format("Couldn't make a {:s} request to \"{:s}\".",       method, url):
               format("Couldn't make a {:s} request to \"{:s}\": {:s}.", method, url, reason);
    }


    // BACKENDS
    // vibecoded because I'm not gonna waste time dealing with WinHTTP and libcurl

    #ifdef __EMSCRIPTEN__


        // web

        string perform(const string& method, const string& url, const vector<pair<string, string>>& requestHeaders, const string& body, double /*timeout*/, Response& response) {

            // make js
            string js =
                "(() => {"
                    "try{"
                        "let request = new XMLHttpRequest();"
                        "request.open(\"" + method + "\", \"" + escapeJS(url) + "\", false);";    // false = synchronous
            for(const pair<string, string>& header: requestHeaders) {
                js += "request.setRequestHeader(\"" + escapeJS(header.first) + "\", \"" + escapeJS(header.second) + "\");";
            }
            js +=       "request.send(" + (body.empty()? string("null"): "\"" + escapeJS(body) + "\"") + ");"
                        "return request.status+\"\\r\\n\" + request.getAllResponseHeaders() + \"\\r\\n\" + request.responseText;"
                    "} catch(error){"
                        "return \"!\"+(error&&error.message? error.message: error);"
                    "}"
                "})()";

            // run
            string result = runJS(js);

            // check for errors
            if(result.empty())   return requestError(method, url);
            if(result[0] == '!') return requestError(method, url, result.substr(1));
            size_t statusEnd = result.find('\n');
            if(statusEnd == string::npos) return requestError(method, url);
            response.status = (uint32_t)strtoul(result.c_str(), nullptr, 10);
            if(response.status == 0) {
                return format(
                    "The browser blocked the request to \"{:s}\", probably because of CORS.",
                    url
                );
            }

            // body
            size_t bodyStart = addHeaderBlock(response, result, statusEnd + 1);
            response.body = result.substr(bodyStart);

            return "";

        }


    #elif defined(_WIN32)


        // WinHTTP
        // uses UTF-16

        std::wstring toWide(const string& str) {
            if(str.empty()) return L"";
            int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
            if(size <= 0) return L"";
            std::wstring wide(size, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), wide.data(), size);
            return wide;
        }
        string fromWide(const wchar_t* str, int length) {
            if(length <= 0) return "";
            int size = WideCharToMultiByte(CP_UTF8, 0, str, length, nullptr, 0, nullptr, nullptr);
            if(size <= 0) return "";
            string narrow(size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, str, length, narrow.data(), size, nullptr, nullptr);
            return narrow;
        }

        struct WinHandle{
            HINTERNET handle = nullptr;
            ~WinHandle() { if(handle) WinHttpCloseHandle(handle); }
            operator HINTERNET() const { return handle; }
        };

        string winError(const char* action) {
            return format("Couldn't {:s}: Windows error {:d}.", action, (uint32_t)GetLastError());
        }

        string winRawHeaders(HINTERNET request) {

            DWORD size = 0;
            WinHttpQueryHeaders(
                request, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size, WINHTTP_NO_HEADER_INDEX
            );
            if(GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) return "";

            std::wstring raw(size / sizeof(wchar_t), L'\0');
            if(!WinHttpQueryHeaders(
                request, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &size, WINHTTP_NO_HEADER_INDEX
            )) return "";

            return fromWide(raw.data(), (int)(size / sizeof(wchar_t)));

        }

        string perform(const string& method, const string& url, const vector<pair<string, string>>& requestHeaders, const string& body, double timeout, Response& response) {

            // WinHTTP connects to the host and asks for the path as two separate steps, so the
            // URL has to be taken apart first.
            std::wstring wideUrl = toWide(url);
            URL_COMPONENTS parts = {};
            parts.dwStructSize      = sizeof(parts);    
            parts.dwSchemeLength    = (DWORD)-1;
            parts.dwHostNameLength  = (DWORD)-1;
            parts.dwUrlPathLength   = (DWORD)-1;
            parts.dwExtraInfoLength = (DWORD)-1;
            if(!WinHttpCrackUrl(wideUrl.c_str(), (DWORD)wideUrl.size(), 0, &parts)) {
                return format("\"{:s}\" isn't a valid URL.", url);
            }

            std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
            std::wstring path(parts.lpszUrlPath,  parts.dwUrlPathLength);
            path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);    // the ?query part
            if(path.empty()) path = L"/";

            WinHandle session;
            session.handle = WinHttpOpen(
                L"" HTTP_USER_AGENT, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0
            );
            if(!session.handle) return winError("start a connection");

            int timeoutMs = (int)(timeout * 1000);
            WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

            // Ask for compressed responses and have WinHTTP undo the compression. It's fine if
            // this isn't supported, the response just arrives uncompressed.
            DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
            WinHttpSetOption(session, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));

            WinHandle connection;
            connection.handle = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
            if(!connection.handle) return winError("connect");

            std::wstring wideMethod = toWide(method);

            WinHandle request;
            request.handle = WinHttpOpenRequest(
                connection, wideMethod.c_str(), path.c_str(),
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                parts.nScheme == INTERNET_SCHEME_HTTPS? WINHTTP_FLAG_SECURE: 0
            );
            if(!request.handle) return winError("create the request");

            for(const pair<string, string>& header: requestHeaders) {
                std::wstring line = toWide(header.first + ": " + header.second);
                WinHttpAddRequestHeaders(
                    request, line.c_str(), (DWORD)line.size(),
                    WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE
                );
            }

            if(!WinHttpSendRequest(
                request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                body.empty()? WINHTTP_NO_REQUEST_DATA: (LPVOID)body.data(),
                (DWORD)body.size(), (DWORD)body.size(), 0
            )) return winError("send the request");

            if(!WinHttpReceiveResponse(request, nullptr)) return winError("get a response");

            DWORD status = 0;
            DWORD statusSize = sizeof(status);
            if(!WinHttpQueryHeaders(
                request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX
            )) return winError("read the status code");
            response.status = (uint32_t)status;

            addHeaderBlock(response, winRawHeaders(request));

            while(true) {

                DWORD available = 0;
                if(!WinHttpQueryDataAvailable(request, &available)) return winError("read the response");
                if(available == 0) break;

                size_t offset = response.body.size();
                response.body.resize(offset + available);

                DWORD read = 0;
                if(!WinHttpReadData(request, response.body.data() + offset, available, &read)) {
                    return winError("read the response");
                }
                response.body.resize(offset + read);
                if(read == 0) break;

            }

            return "";

        }


    #else


        // libcurl

        enum CurlOption{

            // pointers
            CURL_WRITEDATA       = 10001,
            CURL_URL             = 10002,
            CURL_POSTFIELDS      = 10015,
            CURL_USERAGENT       = 10018,
            CURL_HTTPHEADER      = 10023,
            CURL_HEADERDATA      = 10029,
            CURL_CUSTOMREQUEST   = 10036,
            CURL_ACCEPT_ENCODING = 10102,
            CURL_WRITEFUNCTION   = 20011,
            CURL_HEADERFUNCTION  = 20079,

            // numbers
            CURL_NOBODY          = 44,
            CURL_POST            = 47,
            CURL_FOLLOWLOCATION  = 52,
            CURL_POSTFIELDSIZE   = 60,
            CURL_MAXREDIRS       = 68,
            CURL_CONNECTTIMEOUT  = 78,
            CURL_HTTPGET         = 80,
            CURL_NOSIGNAL        = 99,
            CURL_TIMEOUT_MS      = 155,

        };

        enum CurlInfo{
            CURL_RESPONSE_CODE = 0x200002,
        };

        struct CurlLibrary{

            void* library = nullptr;

            void*       (*easy_init     )(void);
            int         (*easy_perform  )(void*);
            void        (*easy_cleanup  )(void*);
            const char* (*easy_strerror )(int);
            void*       (*slist_append  )(void*, const char*);
            void        (*slist_free_all)(void*);

            // curl_easy_setopt and curl_easy_getinfo are variadic, so each gets a prototype per
            // argument type it's actually called with.
            int (*setopt_ptr  )(void*, int, const void*);
            int (*setopt_long )(void*, int, long);
            int (*getinfo_long)(void*, int, long*);

            template<typename T>
            bool load(T& function, const char* name) {
                function = (T)dlsym(library, name);
                return function != nullptr;
            }

            CurlLibrary() {

                const char* names[] = {
                    #ifdef __APPLE__
                        "libcurl.4.dylib", "/usr/lib/libcurl.4.dylib", "libcurl.dylib"
                    #else
                        "libcurl.so.4", "libcurl.so", "libcurl.so.3"
                    #endif
                };
                for(const char* name: names) {
                    library = dlopen(name, RTLD_LAZY | RTLD_LOCAL);
                    if(library) break;
                }
                if(!library) return;

                bool complete =
                    load(easy_init,      "curl_easy_init"     ) &&
                    load(easy_perform,   "curl_easy_perform"  ) &&
                    load(easy_cleanup,   "curl_easy_cleanup"  ) &&
                    load(easy_strerror,  "curl_easy_strerror" ) &&
                    load(slist_append,   "curl_slist_append"  ) &&
                    load(slist_free_all, "curl_slist_free_all") &&
                    load(setopt_ptr,     "curl_easy_setopt"   ) &&
                    load(setopt_long,    "curl_easy_setopt"   ) &&
                    load(getinfo_long,   "curl_easy_getinfo"  );

                if(!complete) {
                    dlclose(library);
                    library = nullptr;
                }

            }

        };

        // Loaded the first time a request is made, then reused. It's deliberately never closed,
        // since curl doesn't like being unloaded and loaded again.
        const CurlLibrary& curl() {
            static CurlLibrary library;
            return library;
        }

        // These free what they're holding however the function they're in returns.
        struct CurlHandle{
            void* handle = nullptr;
            ~CurlHandle() { if(handle) curl().easy_cleanup(handle); }
        };
        struct CurlHeaders{
            void* list = nullptr;
            ~CurlHeaders() { if(list) curl().slist_free_all(list); }
        };

        size_t writeBody(char* data, size_t size, size_t count, void* userData) {
            static_cast<Response*>(userData)->body.append(data, size * count);
            return size * count;
        }
        size_t writeHeader(char* data, size_t size, size_t count, void* userData) {
            addHeader(*static_cast<Response*>(userData), string_view(data, size * count));
            return size * count;
        }

        string perform(const string& method, const string& url, const vector<pair<string, string>>& requestHeaders, const string& body, double timeout, Response& response) {

            if(!curl().library) {
                return "Couldn't find libcurl, which the http module needs. Please install curl.";
            }

            // make the request
            CurlHandle request;
            request.handle = curl().easy_init();
            if(!request.handle) return "Couldn't start a request.";

            // set options
            curl().setopt_ptr (request.handle, CURL_URL,             url.c_str());
            curl().setopt_ptr (request.handle, CURL_USERAGENT,       HTTP_USER_AGENT);
            curl().setopt_ptr (request.handle, CURL_ACCEPT_ENCODING, "");    // any compression
            curl().setopt_long(request.handle, CURL_FOLLOWLOCATION,  1);
            curl().setopt_long(request.handle, CURL_MAXREDIRS,       maxRedirects);
            curl().setopt_long(request.handle, CURL_NOSIGNAL,        1);
            curl().setopt_long(request.handle, CURL_TIMEOUT_MS,      (long)(timeout * 1000));
            curl().setopt_long(request.handle, CURL_CONNECTTIMEOUT,  (long)timeout);
            curl().setopt_ptr (request.handle, CURL_WRITEFUNCTION,   (void*)writeBody);
            curl().setopt_ptr (request.handle, CURL_WRITEDATA,       &response);
            curl().setopt_ptr (request.handle, CURL_HEADERFUNCTION,  (void*)writeHeader);
            curl().setopt_ptr (request.handle, CURL_HEADERDATA,      &response);

            // Method and body.
            // A request that sends a body is shaped like a POST and one that doesn't like a GET.
            // CURL_POSTFIELDSIZE must be set before CURL_POSTFIELDS, otherwise curl measures the
            // body with strlen() and truncates it at the first null byte.
            bool sendsBody = !body.empty() || method == "POST" || method == "PUT" || method == "PATCH";
            if(method == "HEAD") {
                curl().setopt_long(request.handle, CURL_NOBODY,        1);
            } else if(sendsBody) {
                curl().setopt_long(request.handle, CURL_POST,          1);
                curl().setopt_long(request.handle, CURL_POSTFIELDSIZE, (long)body.size());
                curl().setopt_ptr (request.handle, CURL_POSTFIELDS,    body.c_str());
            } else {
                curl().setopt_long(request.handle, CURL_HTTPGET,       1);
            }
            if(method != "GET" && method != "HEAD") {
                curl().setopt_ptr(request.handle, CURL_CUSTOMREQUEST, method.c_str());
            }

            // headers
            CurlHeaders headers;
            for(const pair<string, string>& header: requestHeaders) {
                void* appended = curl().slist_append(headers.list, (header.first + ": " + header.second).c_str());
                if(!appended) return "Couldn't set the request headers.";
                headers.list = appended;
            }

            auto drop = [&](const char* header) {
                void* appended = curl().slist_append(headers.list, header);
                if(appended) headers.list = appended;
                return appended != nullptr;
            };
            if(!drop("Expect:")) return "Couldn't set the request headers.";
            if(!hasHeader(requestHeaders, "content-type") && !drop("Content-Type:")) {
                return "Couldn't set the request headers.";
            }

            if(headers.list) curl().setopt_ptr(request.handle, CURL_HTTPHEADER, headers.list);

            // run
            int result = curl().easy_perform(request.handle);
            if(result != 0) return requestError(method, url, curl().easy_strerror(result));

            // get status
            long status = 0;
            curl().getinfo_long(request.handle, CURL_RESPONSE_CODE, &status);
            response.status = (uint32_t)status;

            return "";

        }


    #endif


    // REQUESTING

    string request(
        const string& method,
        string url, vector<pair<string, string>> requestHeaders, const string& body,
        double timeout, Response& response
    ) {

        string error = normalizeUrl(url);
        if(!error.empty()) return error;

        if(!body.empty() && !hasHeader(requestHeaders, "content-type")) {
            requestHeaders.push_back({"Content-Type", "text/plain; charset=utf-8"});
        }

        return perform(method, url, requestHeaders, body, timeout, response);

    }


}
