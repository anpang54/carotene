
#pragma once

/*

    There are 3 separate backends:
      - Windows:                      WinHTTP, linked normally
      - macOS, Linux, FreeBSD, Haiku: libcurl, opened at runtime with dlopen(), because there's no sysroot
      - Web:                          XMLHttpRequest (HTTP) or the browser's WebSocket (WebSocket)
    
    All 3 are blocking/synchronous for consistency. I will likely add a callback system in the future.

    This file is mostly vibecoded, though I have reviewed everything and made changes on some parts.

*/


// INCLUDES

#include <algorithm>
#include <chrono>
#include <deque>

#ifdef _WIN32

    // WinHTTP
    #define WIN32_LEAN_AND_MEAN           // skip a bunch of useless windows.h stuff
    #define NOMINMAX                      // don't define min and max
    #define TokenType WindowsTokenType    // to not clash with our TokenType
    #include <windows.h>
    #include <winhttp.h>
    #undef TokenType

    // For the WebSocket receiving thread
    #include <thread>
    #include <mutex>
    #include <condition_variable>

#elif !defined(__EMSCRIPTEN__)

    // libcurl
    #include <dlfcn.h>
    #include <poll.h>

#endif

#include "../../core/format.hpp"

namespace chrono = std::chrono;
using std::wstring;


// HTTP

namespace CaroHttp{


    // SETTINGS

    #define HTTP_USER_AGENT "Carotene/" VERSION

    const double defaultTimeout = 30;    // seconds
    const long   maxRedirects   = 20;

    long toMs(double seconds) {
        return (long)(std::clamp(seconds, 0.0, 1e6) * 1000);
    }


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

    string checkScheme(string& url, const string& plain, const string& secure, const char* name, bool addMissing) {

        size_t schemePosition = url.find("://");

        if(schemePosition == string::npos) {
            if(addMissing) url = secure + "://" + url;
            return "";
        }

        string protocol = lower(url.substr(0, schemePosition));
        if(protocol != plain && protocol != secure) {
            return format("\"{:s}\" isn't {:s} URL.", url, name);
        }

        return "";

    }

    string normalizeUrl(string& url) {
        #ifdef __EMSCRIPTEN__
            bool addMissing = false;    // relative URLs are fine in the browser
        #else
            bool addMissing = true;
        #endif
        return checkScheme(url, "http", "https", "an HTTP or HTTPS", addMissing);
    }


    // ERRORS

    string withReason(const string& message, const string& reason) {
        return reason.empty()? message + ".": format("{:s}: {:s}.", message, reason);
    }

    string requestError(const string& method, const string& url, const string& reason = "") {
        return withReason(format("Couldn't make a {:s} request to \"{:s}\"", method, url), reason);
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

        wstring toWide(const string& str) {
            if(str.empty()) return L"";
            int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
            if(size <= 0) return L"";
            wstring wide(size, L'\0');
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

            wstring raw(size / sizeof(wchar_t), L'\0');
            if(!WinHttpQueryHeaders(
                request, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &size, WINHTTP_NO_HEADER_INDEX
            )) return "";

            return fromWide(raw.data(), (int)(size / sizeof(wchar_t)));

        }

        // WinHTTP connects to the host and asks for the path as two separate steps, so the
        // URL has to be taken apart first.
        struct WinUrl{
            wstring host;
            wstring path;
            INTERNET_PORT port = 0;
            bool secure = false;
        };

        bool crackUrl(const string& url, WinUrl& result) {

            wstring wideUrl = toWide(url);
            URL_COMPONENTS parts = {};
            parts.dwStructSize      = sizeof(parts);
            parts.dwSchemeLength    = (DWORD)-1;
            parts.dwHostNameLength  = (DWORD)-1;
            parts.dwUrlPathLength   = (DWORD)-1;
            parts.dwExtraInfoLength = (DWORD)-1;
            if(!WinHttpCrackUrl(wideUrl.c_str(), (DWORD)wideUrl.size(), 0, &parts)) return false;

            result.host.assign(parts.lpszHostName, parts.dwHostNameLength);
            result.path.assign(parts.lpszUrlPath,  parts.dwUrlPathLength);
            result.path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);    // the ?query part
            if(result.path.empty()) result.path = L"/";
            result.port   = parts.nPort;
            result.secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
            return true;

        }

        // nullptr on failure
        HINTERNET openSession(double timeout) {
            HINTERNET session = WinHttpOpen(
                L"" HTTP_USER_AGENT,
                WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0
            );
            if(!session) return nullptr;
            int timeoutMs = (int)toMs(timeout);
            WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
            return session;
        }

        void addRequestHeaders(HINTERNET request, const vector<pair<string, string>>& headers) {
            for(const pair<string, string>& header: headers) {
                wstring line = toWide(header.first + ": " + header.second);
                WinHttpAddRequestHeaders(
                    request, line.c_str(), (DWORD)line.size(),
                    WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE
                );
            }
        }

        bool winStatus(HINTERNET request, DWORD& status) {
            DWORD statusSize = sizeof(status);
            return WinHttpQueryHeaders(
                request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX
            );
        }

        string perform(const string& method, const string& url, const vector<pair<string, string>>& requestHeaders, const string& body, double timeout, Response& response) {

            WinUrl parts;
            if(!crackUrl(url, parts)) return format("\"{:s}\" isn't a valid URL.", url);

            WinHandle session;
            session.handle = openSession(timeout);
            if(!session.handle) return winError("start a connection");

            // Ask for compressed responses and have WinHTTP undo the compression. It's fine if
            // this isn't supported, the response just arrives uncompressed.
            DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
            WinHttpSetOption(session, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));

            WinHandle connection;
            connection.handle = WinHttpConnect(session, parts.host.c_str(), parts.port, 0);
            if(!connection.handle) return winError("connect");

            wstring wideMethod = toWide(method);

            WinHandle request;
            request.handle = WinHttpOpenRequest(
                connection, wideMethod.c_str(), parts.path.c_str(),
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                parts.secure? WINHTTP_FLAG_SECURE: 0
            );
            if(!request.handle) return winError("create the request");

            addRequestHeaders(request, requestHeaders);

            if(!WinHttpSendRequest(
                request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                body.empty()? WINHTTP_NO_REQUEST_DATA: (LPVOID)body.data(),
                (DWORD)body.size(), (DWORD)body.size(), 0
            )) return winError("send the request");

            if(!WinHttpReceiveResponse(request, nullptr)) return winError("get a response");

            DWORD status = 0;
            if(!winStatus(request, status)) return winError("read the status code");
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
            CURL_WRITEDATA         = 10001,
            CURL_URL               = 10002,
            CURL_POSTFIELDS        = 10015,
            CURL_USERAGENT         = 10018,
            CURL_HTTPHEADER        = 10023,
            CURL_HEADERDATA        = 10029,
            CURL_CUSTOMREQUEST     = 10036,
            CURL_ACCEPT_ENCODING   = 10102,
            CURL_WRITEFUNCTION     = 20011,
            CURL_HEADERFUNCTION    = 20079,

            // numbers
            CURL_NOBODY            = 44,
            CURL_POST              = 47,
            CURL_FOLLOWLOCATION    = 52,
            CURL_POSTFIELDSIZE     = 60,
            CURL_MAXREDIRS         = 68,
            CURL_HTTPGET           = 80,
            CURL_NOSIGNAL          = 99,
            CURL_CONNECT_ONLY      = 141,
            CURL_TIMEOUT_MS        = 155,
            CURL_CONNECTTIMEOUT_MS = 156,

        };

        enum CurlInfo{
            CURL_RESPONSE_CODE = 0x200002,
            CURL_ACTIVESOCKET  = 0x500000 + 44,
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
            int (*setopt_ptr    )(void*, int, const void*);
            int (*setopt_long   )(void*, int, long);
            int (*getinfo_long  )(void*, int, long*);
            int (*getinfo_socket)(void*, int, int*);

            // WebSockets need curl 7.86+ built with them enabled, so these are optional.
            int (*ws_send)(void*, const void*, size_t, size_t*, int64_t, unsigned int) = nullptr;
            int (*ws_recv)(void*, void*, size_t, size_t*, const void**)                = nullptr;

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
                    load(getinfo_long,   "curl_easy_getinfo"  ) &&
                    load(getinfo_socket, "curl_easy_getinfo"  );

                if(!complete) {
                    dlclose(library);
                    library = nullptr;
                    return;
                }

                if(!load(ws_send, "curl_ws_send") || !load(ws_recv, "curl_ws_recv")) {
                    ws_send = nullptr;
                    ws_recv = nullptr;
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

            bool add(const string& line) {
                void* appended = curl().slist_append(list, line.c_str());
                if(appended) list = appended;
                return appended != nullptr;
            }
            bool add(const vector<pair<string, string>>& headers) {
                for(const pair<string, string>& header: headers) {
                    if(!add(header.first + ": " + header.second)) return false;
                }
                return true;
            }

        };

        void setBasicOptions(void* handle, const string& url, double timeout) {
            curl().setopt_ptr (handle, CURL_URL,               url.c_str());
            curl().setopt_ptr (handle, CURL_USERAGENT,         HTTP_USER_AGENT);
            curl().setopt_long(handle, CURL_NOSIGNAL,          1);
            curl().setopt_long(handle, CURL_TIMEOUT_MS,        toMs(timeout));
            curl().setopt_long(handle, CURL_CONNECTTIMEOUT_MS, toMs(timeout));
        }

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
            setBasicOptions(request.handle, url, timeout);
            curl().setopt_ptr (request.handle, CURL_ACCEPT_ENCODING, "");    // any compression
            curl().setopt_long(request.handle, CURL_FOLLOWLOCATION,  1);
            curl().setopt_long(request.handle, CURL_MAXREDIRS,       maxRedirects);
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
            if(
                !headers.add(requestHeaders) || !headers.add("Expect:") ||
                (!hasHeader(requestHeaders, "content-type") && !headers.add("Content-Type:"))
            ) return "Couldn't set the request headers.";

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


// WEB HELPERS

#ifdef __EMSCRIPTEN__

    // 0 = connecting, 1 = open, 2 = closed
    EM_JS(int, caroWsCreate, (const char* url, int length), {
        if(!Module.caroSockets) {
            Module.caroSockets = {};
            Module.caroSocketCount = 0;
        }
        let socket;
        try{
            socket = new WebSocket(new TextDecoder().decode(HEAPU8.slice(url, url + length)));
        } catch(error){
            return 0;
        }
        socket.binaryType = "arraybuffer";
        let entry = {socket: socket, queue: [], state: 0, code: 0, reason: new Uint8Array(0), wake: null};
        let wake = () => { if(entry.wake) entry.wake(); };
        socket.onopen = () => {
            entry.state = 1;
            wake();
        };
        socket.onmessage = (event) => {
            let binary = typeof event.data != "string";
            entry.queue.push({binary: binary, bytes: binary? new Uint8Array(event.data): new TextEncoder().encode(event.data)});
            wake();
        };
        socket.onclose = (event) => {
            entry.state  = 2;
            entry.code   = event.code;
            entry.reason = new TextEncoder().encode(event.reason);
            wake();
        };
        let id = ++Module.caroSocketCount;
        Module.caroSockets[id] = entry;
        return id;
    });

    EM_ASYNC_JS(void, caroWsWait, (int id, int ms), {
        let entry = Module.caroSockets[id];
        await new Promise(resolve => {
            entry.wake = resolve;
            if(ms >= 0) setTimeout(resolve, ms);
        });
        entry.wake = null;
    });

    EM_JS(int, caroWsState, (int id), {
        return Module.caroSockets[id].state;
    });
    
    EM_JS(char*, caroWsTakeNext, (int id, int* length, int* binary), {
        let next = Module.caroSockets[id].queue.shift();
        if(!next) return 0;
        let buffer = _malloc(next.bytes.length + 1);
        HEAPU8.set(next.bytes, buffer);
        HEAP32[length >> 2] = next.bytes.length;
        HEAP32[binary >> 2] = next.binary;
        return buffer;
    });

    EM_JS(char*, caroWsTakeClose, (int id, int* code, int* length), {
        let entry = Module.caroSockets[id];
        let buffer = _malloc(entry.reason.length + 1);
        HEAPU8.set(entry.reason, buffer);
        HEAP32[code   >> 2] = entry.code;
        HEAP32[length >> 2] = entry.reason.length;
        return buffer;
    });

    EM_JS(int, caroWsSend, (int id, const char* data, int length, int binary), {
        let entry = Module.caroSockets[id];
        if(entry.state != 1) return 0;
        let bytes = HEAPU8.slice(data, data + length);
        try{
            entry.socket.send(binary? bytes: new TextDecoder().decode(bytes));
        } catch(error){
            return 0;
        }
        return 1;
    });

    EM_JS(void, caroWsClose, (int id, int code, const char* reason, int length), {
        let socket = Module.caroSockets[id].socket;
        try{
            if(code == 0) socket.close();
            else          socket.close(code, new TextDecoder().decode(HEAPU8.slice(reason, reason + length)));
        } catch(error){
            socket.close();
        }
    });

    EM_JS(void, caroWsDelete, (int id), {
        let socket = Module.caroSockets[id].socket;
        socket.onopen = socket.onmessage = socket.onclose = null;
        delete Module.caroSockets[id];
    });

#endif


// WEBSOCKET

namespace CaroWebSocket{


    // SETTINGS

    using CaroHttp::defaultTimeout;
    const double closeTimeout = 5;


    // MESSAGES

    struct Message{
        bool binary = false;
        string data;
    };

    struct MessageBuilder{

        Message message;
        bool    started = false;

        // false if the message got too big
        bool add(const char* data, size_t length, bool binary) {
            if(!started) message.binary = binary;
            started = true;
            message.data.append(data, length);
            return message.data.size() <= MAX_STRING_LENGTH;
        }

        Message take() {
            started = false;
            return std::exchange(message, {});
        }

    };

    enum Event{
        EVENT_NONE,    // timed out
        EVENT_MESSAGE,
        EVENT_CLOSED,
    };

    const uint16_t CLOSE_NO_STATUS = 1005;
    const uint16_t CLOSE_ABNORMAL  = 1006;
    const uint16_t CLOSE_TOO_BIG   = 1009;


    // TIME

    using Clock = chrono::steady_clock;

    Clock::time_point deadlineAfter(double seconds) {
        if(seconds < 0) return Clock::time_point::max();
        seconds = std::min(seconds, 1e8);    // no overflow
        return Clock::now() + chrono::duration_cast<Clock::duration>(chrono::duration<double>(seconds));
    }

    int msLeft(Clock::time_point deadline) {
        if(deadline == Clock::time_point::max()) return -1;
        int64_t left = chrono::duration_cast<chrono::milliseconds>(deadline - Clock::now()).count();
        return (int)std::clamp<int64_t>(left, 0, INT_MAX);
    }


    // URL CHECKING

    string normalizeUrl(string& url) {
        return CaroHttp::checkScheme(url, "ws", "wss", "a WS or WSS", true);
    }


    // ERRORS

    string connectError(const string& url, const string& reason = "") {
        return CaroHttp::withReason(format("Couldn't connect to \"{:s}\"", url), reason);
    }

    const char* closedError = "The connection is closed.";


    // CLOSE FRAMES

    string closePayload(uint16_t code, const string& reason) {
        string payload;
        payload.push_back((char)(code >> 8));
        payload.push_back((char)(code & 0xff));
        payload += reason.substr(0, 123);
        return payload;
    }

    void parseClose(string_view payload, uint16_t& code, string& reason) {
        if(payload.size() < 2) {
            code   = CLOSE_NO_STATUS;
            reason = "";
            return;
        }
        code   = (uint16_t)(((unsigned char)payload[0] << 8) | (unsigned char)payload[1]);
        reason = string(payload.substr(2));
    }


    // CONNECTIONS

    class Connection{

        public:

            bool     isOpen    = false;
            uint16_t closeCode = 0;
            string   closeReason;

            Connection() = default;
            Connection(const Connection&) = delete;
            Connection& operator=(const Connection&) = delete;
            ~Connection() { close(1001, "", false); }

            string open(string url, const vector<pair<string, string>>& headers = {}, double timeout = defaultTimeout);
            string send(const string& data, bool binary = false);

            Event receive(double timeout, Message& message);

            void close(uint16_t code = 1000, const string& reason = "", bool wait = true);

        private:

            string backendOpen(const string& url, const vector<pair<string, string>>& headers, double timeout);
            string backendSend(const string& data, bool binary);
            Event  backendReceive(double timeout, Message& message);
            void   backendClose(uint16_t code, const string& reason, bool wait);

            void release();

            void finish(uint16_t code, string reason) {
                release();
                isOpen      = false;
                closeCode   = code;
                closeReason = std::move(reason);
            }

            #ifdef __EMSCRIPTEN__

                int id = 0;

            #elif defined(_WIN32)

                HINTERNET session    = nullptr;
                HINTERNET connection = nullptr;
                HINTERNET socket     = nullptr;

                std::thread             receiver;
                std::mutex              lock;
                std::condition_variable changed;
                std::deque<Message>     queue;
                bool                    threadClosed = false;
                uint16_t                threadCode   = 0;
                string                  threadReason;

                std::mutex sendLock;
                bool       shutdownSent = false;

                void receiveLoop();

            #else

                void*          handle  = nullptr;
                int            fd      = -1;
                MessageBuilder pending;
                string         closeFrame;
                bool           closing = false;

                bool   waitFor(short events, Clock::time_point deadline);
                string sendFrame(string_view data, unsigned int flags);

            #endif

    };

    string Connection::open(string url, const vector<pair<string, string>>& headers, double timeout) {

        if(isOpen) return "This connection is already open.";

        string error = normalizeUrl(url);
        if(error.empty()) error = backendOpen(url, headers, timeout);
        if(!error.empty()) return error;

        isOpen = true;
        closeCode = 0;
        closeReason.clear();

        return "";

    }

    string Connection::send(const string& data, bool binary) {
        return isOpen? backendSend(data, binary): closedError;
    }
    Event Connection::receive(double timeout, Message& message) {
        return isOpen? backendReceive(timeout, message): EVENT_CLOSED;
    }

    void Connection::close(uint16_t code, const string& reason, bool wait) {
        if(!isOpen) return;
        backendClose(code, reason, wait);
        finish(code, reason.substr(0, 123));
    }


    // BACKENDS

    #ifdef __EMSCRIPTEN__


        // web

        string Connection::backendOpen(const string& url, const vector<pair<string, string>>& headers, double timeout) {

            if(!headers.empty()) return "Browsers don't allow custom headers on WebSockets.";

            id = caroWsCreate(url.data(), (int)url.size());
            if(id == 0) return format("\"{:s}\" isn't a valid URL.", url);

            Clock::time_point deadline = deadlineAfter(timeout);
            while(caroWsState(id) == 0) {
                int left = msLeft(deadline);
                if(left == 0) {
                    caroWsClose(id, 0, nullptr, 0);
                    release();
                    return connectError(url, "timed out");
                }
                caroWsWait(id, left);
            }

            if(caroWsState(id) != 1) {
                release();
                return connectError(url);
            }

            return "";

        }

        string Connection::backendSend(const string& data, bool binary) {
            if(data.size() > INT_MAX) return "That message is too big.";
            if(!caroWsSend(id, data.data(), (int)data.size(), binary)) return "Couldn't send the message.";
            return "";
        }

        Event Connection::backendReceive(double timeout, Message& message) {

            Clock::time_point deadline = deadlineAfter(timeout);
            while(true) {

                int length, binary;
                if(char* next = caroWsTakeNext(id, &length, &binary)) {
                    message.binary = binary;
                    message.data.assign(next, length);
                    free(next);
                    return EVENT_MESSAGE;
                }

                if(caroWsState(id) == 2) {
                    int code;
                    char* reason = caroWsTakeClose(id, &code, &length);
                    finish((uint16_t)code, string(reason, length));
                    free(reason);
                    return EVENT_CLOSED;
                }

                int left = msLeft(deadline);
                if(left == 0) return EVENT_NONE;
                caroWsWait(id, left);

            }

        }

        void Connection::backendClose(uint16_t code, const string& reason, bool wait) {

            caroWsClose(id, code, reason.data(), (int)reason.size());

            if(wait) {
                Clock::time_point deadline = deadlineAfter(closeTimeout);
                int left;
                while(caroWsState(id) != 2 && (left = msLeft(deadline)) > 0) caroWsWait(id, left);
            }

        }

        void Connection::release() {
            if(id) caroWsDelete(id);
            id = 0;
        }


    #elif defined(_WIN32)


        // WinHTTP

        string Connection::backendOpen(const string& url, const vector<pair<string, string>>& headers, double timeout) {

            // WinHTTP only knows http:// and https://, then gets told to upgrade.
            bool secure = lower(url.substr(0, 3)) == "wss";
            CaroHttp::WinUrl parts;
            if(!CaroHttp::crackUrl((secure? "https": "http") + url.substr(url.find("://")), parts)) {
                return format("\"{:s}\" isn't a valid URL.", url);
            }

            auto fail = [&](const char* action) {
                string error = CaroHttp::winError(action);
                release();
                return error;
            };

            session = CaroHttp::openSession(timeout);
            if(!session) return fail("start a connection");

            connection = WinHttpConnect(session, parts.host.c_str(), parts.port, 0);
            if(!connection) return fail("connect");

            CaroHttp::WinHandle request;
            request.handle = WinHttpOpenRequest(
                connection, L"GET", parts.path.c_str(),
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                secure? WINHTTP_FLAG_SECURE: 0
            );
            if(!request.handle) return fail("create the request");

            if(!WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) {
                return fail("ask for a WebSocket");
            }

            CaroHttp::addRequestHeaders(request, headers);

            if(!WinHttpSendRequest(
                request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0
            )) return fail("send the request");

            if(!WinHttpReceiveResponse(request, nullptr)) return fail("get a response");

            DWORD status = 0;
            CaroHttp::winStatus(request, status);
            if(status != 101) {
                release();
                return connectError(url, format("the server answered with status {:d}", (uint32_t)status));
            }

            int timeoutMs = (int)CaroHttp::toMs(timeout);
            WinHttpSetTimeouts(request, timeoutMs, timeoutMs, timeoutMs, 0);

            socket = WinHttpWebSocketCompleteUpgrade(request, 0);
            if(!socket) return fail("finish the WebSocket handshake");

            DWORD forever = 0;
            WinHttpSetOption(socket, WINHTTP_OPTION_RECEIVE_TIMEOUT, &forever, sizeof(forever));

            receiver = std::thread(&Connection::receiveLoop, this);
            return "";

        }

        void Connection::receiveLoop() {

            vector<char> buffer(65536);
            MessageBuilder partial;

            auto end = [&](uint16_t code, string reason) {
                std::lock_guard guard(lock);
                threadClosed = true;
                threadCode   = code;
                threadReason = std::move(reason);
                changed.notify_all();
            };

            auto answer = [&](uint16_t code) {
                std::lock_guard guard(sendLock);
                if(shutdownSent) return;
                WinHttpWebSocketShutdown(socket, code, nullptr, 0);
                shutdownSent = true;
            };

            while(true) {

                DWORD read = 0;
                WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
                DWORD error = WinHttpWebSocketReceive(socket, buffer.data(), (DWORD)buffer.size(), &read, &type);
                if(error != NO_ERROR) {
                    return end(CLOSE_ABNORMAL, format("Windows error {:d}", (uint32_t)error));
                }

                if(type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
                    USHORT status = 0;
                    char reason[123];
                    DWORD reasonLength = 0;
                    if(WinHttpWebSocketQueryCloseStatus(socket, &status, reason, sizeof(reason), &reasonLength) != NO_ERROR) {
                        status = CLOSE_NO_STATUS;
                        reasonLength = 0;
                    }
                    answer(status == CLOSE_NO_STATUS? 1000: status);
                    return end(status, string(reason, reasonLength));
                }

                bool binary = type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE
                           || type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE;
                if(!partial.add(buffer.data(), read, binary)) {
                    answer(CLOSE_TOO_BIG);
                    return end(CLOSE_TOO_BIG, "The message was too big.");
                }

                if(type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
                    std::lock_guard guard(lock);
                    queue.push_back(partial.take());
                    changed.notify_all();
                }

            }

        }

        string Connection::backendSend(const string& data, bool binary) {

            if(data.size() > UINT32_MAX) return "That message is too big.";

            std::lock_guard guard(sendLock);
            if(shutdownSent) return closedError;
            DWORD error = WinHttpWebSocketSend(
                socket,
                binary? WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE: WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                (PVOID)data.data(), (DWORD)data.size()
            );
            if(error != NO_ERROR) return format("Couldn't send the message: Windows error {:d}.", (uint32_t)error);
            return "";

        }

        Event Connection::backendReceive(double timeout, Message& message) {

            std::unique_lock guard(lock);
            auto ready = [&]() { return !queue.empty() || threadClosed; };
            if(timeout < 0) {
                changed.wait(guard, ready);
            } else if(!changed.wait_until(guard, deadlineAfter(timeout), ready)) {
                return EVENT_NONE;
            }

            if(!queue.empty()) {
                message = std::move(queue.front());
                queue.pop_front();
                return EVENT_MESSAGE;
            }

            uint16_t code = threadCode;
            string reason = std::move(threadReason);
            guard.unlock();
            finish(code, std::move(reason));
            return EVENT_CLOSED;

        }

        void Connection::backendClose(uint16_t code, const string& reason, bool wait) {

            {
                std::lock_guard guard(sendLock);
                if(!shutdownSent) {
                    string trimmed = reason.substr(0, 123);
                    WinHttpWebSocketShutdown(socket, code, trimmed.empty()? nullptr: trimmed.data(), (DWORD)trimmed.size());
                    shutdownSent = true;
                }
            }

            if(wait) {
                std::unique_lock guard(lock);
                changed.wait_until(guard, deadlineAfter(closeTimeout), [&]() { return threadClosed; });
            }

        }

        void Connection::release() {

            if(socket) WinHttpCloseHandle(socket);
            if(receiver.joinable()) receiver.join();
            if(connection) WinHttpCloseHandle(connection);
            if(session)    WinHttpCloseHandle(session);
            socket = connection = session = nullptr;

            queue.clear();
            threadClosed = false;
            threadCode   = 0;
            threadReason.clear();
            shutdownSent = false;

        }


    #else


        // libcurl

        enum CurlWsNumber{

            // results
            CURL_UNSUPPORTED_PROTOCOL = 1,
            CURL_AGAIN                = 81,

            // frame flags
            CURLWS_TEXT   = 1 << 0,
            CURLWS_BINARY = 1 << 1,
            CURLWS_CONT   = 1 << 2,
            CURLWS_CLOSE  = 1 << 3,
            CURLWS_PING   = 1 << 4,
            CURLWS_PONG   = 1 << 6,

        };

        struct CurlFrame{
            int     age;
            int     flags;
            int64_t offset;
            int64_t bytesleft;
            size_t  len;
        };

        string Connection::backendOpen(const string& url, const vector<pair<string, string>>& headers, double timeout) {

            const CaroHttp::CurlLibrary& curl = CaroHttp::curl();
            if(!curl.library) return "Couldn't find libcurl.";
            if(!curl.ws_send) return "The installed libcurl doesn't have WebSocket support.";

            handle = curl.easy_init();
            if(!handle) return "Couldn't connect.";

            CaroHttp::setBasicOptions(handle, url, timeout);
            curl.setopt_long(handle, CaroHttp::CURL_CONNECT_ONLY, 2);    // 2 = stop after the WebSocket handshake

            CaroHttp::CurlHeaders list;
            if(!list.add(headers)) {
                release();
                return "Couldn't set the request headers.";
            }
            if(list.list) curl.setopt_ptr(handle, CaroHttp::CURL_HTTPHEADER, list.list);

            int result = curl.easy_perform(handle);
            if(result != 0) {
                long status = 0;
                curl.getinfo_long(handle, CaroHttp::CURL_RESPONSE_CODE, &status);
                release();
                if(result == CURL_UNSUPPORTED_PROTOCOL) return "The installed libcurl doesn't have WebSocket support.";
                if(status != 0 && status != 101) {
                    return connectError(url, format("the server answered with status {:d}", status));
                }
                return connectError(url, curl.easy_strerror(result));
            }

            curl.setopt_long(handle, CaroHttp::CURL_TIMEOUT_MS, 0);
            curl.setopt_ptr (handle, CaroHttp::CURL_HTTPHEADER, nullptr);

            curl.getinfo_socket(handle, CaroHttp::CURL_ACTIVESOCKET, &fd);
            return "";

        }

        bool Connection::waitFor(short events, Clock::time_point deadline) {
            pollfd target = {fd, events, 0};
            return poll(&target, 1, msLeft(deadline)) != 0;
        }

        string Connection::sendFrame(string_view data, unsigned int flags) {

            Clock::time_point deadline = deadlineAfter(defaultTimeout);
            size_t offset = 0;

            do{
                size_t sent = 0;
                int result = CaroHttp::curl().ws_send(handle, data.data() + offset, data.size() - offset, &sent, 0, flags);
                offset += sent;
                if(result == CURL_AGAIN) {
                    if(!waitFor(POLLOUT, deadline)) return "timed out";
                } else if(result != 0) {
                    return CaroHttp::curl().easy_strerror(result);
                }
            } while(offset < data.size());

            return "";

        }

        string Connection::backendSend(const string& data, bool binary) {

            string error = sendFrame(data, binary? CURLWS_BINARY: CURLWS_TEXT);
            if(!error.empty()) {
                finish(CLOSE_ABNORMAL, error);
                return format("Couldn't send the message: {:s}.", error);
            }
            return "";

        }

        Event Connection::backendReceive(double timeout, Message& message) {

            Clock::time_point deadline = deadlineAfter(timeout);
            char buffer[65536];

            while(true) {

                size_t received = 0;
                const CurlFrame* frame = nullptr;
                int result = CaroHttp::curl().ws_recv(handle, buffer, sizeof(buffer), &received, (const void**)&frame);

                if(result == CURL_AGAIN || (result == 0 && !frame)) {
                    if(!waitFor(POLLIN, deadline)) return EVENT_NONE;
                    continue;
                }
                if(result != 0) {
                    finish(CLOSE_ABNORMAL, CaroHttp::curl().easy_strerror(result));
                    return EVENT_CLOSED;
                }

                if(frame->flags & CURLWS_CLOSE) {
                    closeFrame.append(buffer, received);
                    if(frame->bytesleft > 0) continue;
                    uint16_t code;
                    string reason;
                    parseClose(closeFrame, code, reason);
                    if(!closing) sendFrame(closePayload(code == CLOSE_NO_STATUS? 1000: code, ""), CURLWS_CLOSE);
                    finish(code, std::move(reason));
                    return EVENT_CLOSED;
                }

                if(frame->flags & (CURLWS_PING | CURLWS_PONG)) continue;

                if(!pending.add(buffer, received, frame->flags & CURLWS_BINARY)) {
                    sendFrame(closePayload(CLOSE_TOO_BIG, ""), CURLWS_CLOSE);
                    finish(CLOSE_TOO_BIG, "The message was too big.");
                    return EVENT_CLOSED;
                }

                if(frame->bytesleft == 0 && !(frame->flags & CURLWS_CONT)) {
                    message = pending.take();
                    return EVENT_MESSAGE;
                }

            }

        }

        void Connection::backendClose(uint16_t code, const string& reason, bool wait) {

            closing = true;
            if(sendFrame(closePayload(code, reason), CURLWS_CLOSE).empty() && wait) {

                Clock::time_point deadline = deadlineAfter(closeTimeout);
                Message ignored;
                while(isOpen) {
                    double left = msLeft(deadline) / 1000.0;
                    if(left <= 0 || receive(left, ignored) == EVENT_NONE) break;
                }

            }

        }

        void Connection::release() {
            if(handle) CaroHttp::curl().easy_cleanup(handle);
            handle  = nullptr;
            fd      = -1;
            pending = {};
            closeFrame.clear();
            closing = false;
        }


    #endif


}
