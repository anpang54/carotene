

#pragma once


// INCLUDES

#include "../util/natives.hpp"
#include "../util/http.hpp"
#include "../util/json_parser.hpp"


// HELPERS

string wikiEncodeTitle(const string& title) {
    string encoded;
    for(char c: title) {
        if(c == ' ') encoded += '_';
        else if(std::isalnum((unsigned char)c) || string("-._~").find(c) != string::npos) encoded += c;
        else encoded += format("%{:02X}", (unsigned char)c);
    }
    return encoded;
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

nMethod(mediawiki_Wiki, get, {
    params({
        {{OBJ_STRING}, true },
        {{TYPE_BOOL},  false}
    });

    WikiData* data = nativeData<WikiData>(vm, self);
    if(data == nullptr) return CaroNull;

    bool getHtml = args.size() >= 2 && isTruthy(args[1]);

    // make the request
    string url = data->url + "/rest.php/v1/page/" + wikiEncodeTitle(asString(args[0])->str) + (getHtml? "/html": "");
    CaroHttp::Response response;
    string error = CaroHttp::request("GET", url, {}, "", CaroHttp::defaultTimeout, response);

    // error
    if(!error.empty()) {
        vm->runtimeError("%s", error.c_str());
        return CaroNull;
    }

    // return
    if(getHtml) {
        return CaroObj(copyString(response.body));
    } else {
        // parse json first
        GCPause pause;
        CaroJsonParser parser(response.body);
        Value parsed = parser.parseValue();
        if(parser.errored || !isDict(parsed)) {
            vm->runtimeError("The response contained invalid JSON.");
            return CaroNull;
        }
        auto found = asDict(parsed)->data.find(CaroObj(copyString("source")));
        if(found == asDict(parsed)->data.end() || !isString(found->second)) {
            vm->runtimeError("The response contained invalid JSON.");
            return CaroNull;
        }
        return found->second;
    }

});
