

#pragma once


// INCLUDES

#include "../util/natives.hpp"
#include "../util/http.hpp"
#include "../util/json_parser.hpp"


// HELPERS

string wikiEncode(const string& text, bool isTitle = false) {
    string encoded;
    for(char c: text) {
        if(c == ' ' && isTitle) encoded += '_';
        else if(std::isalnum((unsigned char)c) || string("-._~").find(c) != string::npos) encoded += c;
        else encoded += format("%{:02X}", (unsigned char)c);
    }
    return encoded;
}

bool wikiRequest(VM* vm, const string& method, const string& url, const string& body, CaroHttp::Response& response) {
    vector<pair<string, string>> headers;
    if(!body.empty()) headers.push_back({"Content-Type", "application/json"});
    string error = CaroHttp::request(method, url, headers, body, CaroHttp::defaultTimeout, response);
    if(error.empty()) return true;
    vm->runtimeError("%s", error.c_str());
    return false;
}

Value wikiField(VM* vm, const string& body, const string& key, bool (*isRightType)(Value)) {
    CaroJsonParser parser(body);
    Value parsed = parser.parseValue();
    if(!parser.errored && isDict(parsed)) {
        auto found = asDict(parsed)->data.find(CaroObj(copyString(key)));
        if(found != asDict(parsed)->data.end() && isRightType(found->second)) return found->second;
    }
    vm->runtimeError("The response contained invalid JSON.");
    return CaroNull;
}


// WIKI OBJECT

struct WikiData: NativeData{
    string url;    // with script path, eg. https://en.wikipedia.org/w
};

nClass(mediawiki_Wiki, "mediawiki", "Wiki");

nMethod(mediawiki_Wiki, init, {
    params({
        {{OBJ_STRING}, true}
    });
    if(alreadyInitialized(vm, self)) return CaroNull;
    auto data = std::make_unique<WikiData>();
    data->url = asString(args[0])->str;
    asInstance(self)->native = std::move(data);
    return CaroNull;
});


// REST API


// getting

nMethod(mediawiki_Wiki, get, {
    params({
        {{OBJ_STRING}, true },
        {{TYPE_BOOL},  false}
    });

    WikiData* data = nativeData<WikiData>(vm, self);
    if(data == nullptr) return CaroNull;

    bool getHtml = args.size() >= 2 && isTruthy(args[1]);

    // make the request
    CaroHttp::Response response;
    if(!wikiRequest(
        vm,
        "GET",
        data->url + "/rest.php/v1/page/" + wikiEncode(asString(args[0])->str, true) + (getHtml? "/html": ""),
        "",
        response
    )) return CaroNull;

    // return
    if(getHtml) return CaroObj(copyString(response.body));
    GCPause pause;
    return wikiField(vm, response.body, "source", isString);

});

nMethod(mediawiki_Wiki, search, {
    params({
        {{OBJ_STRING}, true },
        {ANY_NUMERIC,  false}
    });

    WikiData* data = nativeData<WikiData>(vm, self);
    if(data == nullptr) return CaroNull;

    // check limit
    int64_t limit = 50;
    if(args.size() >= 2) {
        limit = asNumberTo<int64_t>(args[1]);
        if(limit < 1 || limit > 100) {
            vm->runtimeError("The limit should be between 1 and 100.");
            return CaroNull;
        }
    }

    CaroHttp::Response response;
    if(!wikiRequest(
        vm,
        "GET",
        data->url + "/rest.php/v1/search/page?q=" + wikiEncode(asString(args[0])->str, false) + "&limit=" + std::to_string(limit),
        "",
        response
    )) return CaroNull;

    GCPause pause;
    return wikiField(vm, response.body, "pages", isArray);

});

nMethod(mediawiki_Wiki, parse, {
    params({
        {{OBJ_STRING}, true },
        {{OBJ_STRING}, false}
    });

    WikiData* data = nativeData<WikiData>(vm, self);
    if(data == nullptr) return CaroNull;

    string title = args.size() >= 2? asString(args[1])->str: "Main Page";

    CaroHttp::Response response;
    if(!wikiRequest(
        vm,
        "POST",
        data->url + "/rest.php/v1/transform/wikitext/to/html/" + wikiEncode(title, true),
        "{\"wikitext\": " + jsonStringifyString(asString(args[0])->str) + "}",
        response
    )) return CaroNull;

    return CaroObj(copyString(response.body));

});

nMethod(mediawiki_Wiki, unparse, {
    params({
        {{OBJ_STRING}, true },
        {{OBJ_STRING}, false}
    });

    WikiData* data = nativeData<WikiData>(vm, self);
    if(data == nullptr) return CaroNull;

    string title = args.size() >= 2? asString(args[1])->str: "Main Page";

    CaroHttp::Response response;
    if(!wikiRequest(
        vm,
        "POST",
        data->url + "/rest.php/v1/transform/html/to/wikitext/" + wikiEncode(title, true),
        "{\"html\": " + jsonStringifyString(asString(args[0])->str) + "}",
        response
    )) return CaroNull;

    return CaroObj(copyString(response.body));

});
