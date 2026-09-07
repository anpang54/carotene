#pragma once


// INCLUDES

#include "../util/natives.hpp"
#include "../util/http.hpp"


// HELPERS

string httpReadHeaders(const Value& dict, vector<pair<string, string>>& headers) {
    // check each header's name

    for(const auto& [nameValue, valueValue]: asDict(dict)->data) {

        string name  = printValue(nameValue);
        string value = printValue(valueValue);

        if(name.empty()) return "A request header's name can't be empty.";
        for(char c: name) {
            bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                        || string("!#$%&'*+-.^_`|~").find(c) != string::npos;
            if(!allowed) return format("\"{:s}\" isn't a valid header name.", name);
        }
        for(char c: value) {
            if(((unsigned char)c < 0x20 && c != '\t') || (unsigned char)c == 0x7f) {
                return format("The value of header \"{:s}\" has an invalid character.", name);
            }
        }

        headers.push_back({std::move(name), std::move(value)});

    }

    return "";

}

Value httpRequest(VM* vm, const vector<Value>& args, const string& method, const string& body, uint first) {

    string error;

    // check headers
    vector<pair<string, string>> requestHeaders;
    if(args.size() > first) {
        error = httpReadHeaders(args[first], requestHeaders);
    }

    // check timeout
    double timeout = CaroHttp::defaultTimeout;
    if(error.empty() && args.size() > first + 1) {
        timeout = asNumberTo<double>(args[first + 1]);
        if(!(timeout > 0)) error = "The timeout should be more than 0 seconds.";
    }

    // the request itself
    CaroHttp::Response response;
    if(error.empty()) {
        error = CaroHttp::request(method, asString(args[0])->str, requestHeaders, body, timeout, response);
    }

    // error
    if(!error.empty()) {
        vm->runtimeError("%s", error.c_str());
        return CaroNull;
    }

    // build the result
    GCPause pause;    // temporarily pause the garbage collector
    unordered_map<Value, Value> headers;
    headers.reserve(response.headers.size());
    for(const pair<string, string>& header: response.headers) {
        headers.emplace(
            CaroObj(copyString(header.first )),
            CaroObj(copyString(header.second))
        );
    }
    unordered_map<Value, Value> result;
    result.emplace(CaroObj(copyString("status" )), CaroUint(response.status));
    result.emplace(CaroObj(copyString("body"   )), CaroObj(copyString(std::move(response.body))));
    result.emplace(CaroObj(copyString("headers")), CaroObj(copyDict(std::move(headers))));
    return CaroObj(copyDict(std::move(result)));

}


// FUNCTIONS

#define nHttpNoBody(cppName, caroName, method)\
    nFunc(cppName, "http", caroName, {\
        params({\
            {{OBJ_STRING}, true },    /* url     */\
            {{OBJ_DICT},   false},    /* headers */\
            {ANY_NUMERIC,  false}     /* timeout */\
        });\
        return httpRequest(vm, args, method, "", 1);\
    })

#define nHttpHasBody(cppName, caroName, method)\
    nFunc(cppName, "http", caroName, {\
        params({\
            {{OBJ_STRING}, true },    /* url     */\
            {{OBJ_STRING}, false},    /* body    */\
            {{OBJ_DICT},   false},    /* headers */\
            {ANY_NUMERIC,  false}     /* timeout */\
        });\
        const string noBody;\
        const string& body = args.size() >= 2? asString(args[1])->str: noBody;\
        return httpRequest(vm, args, method, body, 2);\
    })

nHttpNoBody (http_get,     "get",     "GET"    );
nHttpHasBody(http_post,    "post",    "POST"   );
nHttpHasBody(http_put,     "put",     "PUT"    );
nHttpHasBody(http_patch,   "patch",   "PATCH"  );
nHttpHasBody(http_delete,  "delete",  "DELETE" );
nHttpNoBody (http_head,    "head",    "HEAD"   );
nHttpNoBody (http_options, "options", "OPTIONS");

nConst(http_user_agent, "http", "user_agent", { return CaroObj(copyString(HTTP_USER_AGENT)); });
