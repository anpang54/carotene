
#pragma once


// INCLUDES

#include "../util/natives.hpp"
#include "../../core/serialize.hpp"


// CONSTANTS

nConst(caro_version,          "caro", "version",          { return CaroObj(copyString(VERSION     )); });
nConst(caro_version_date,     "caro", "version_date",     { return CaroObj(copyString(VERSION_DATE)); });

nConst(caro_magic_number,     "caro", "magic_number",     {
    vector<Value> result;
    for(char byte: MAGIC_NUMBER) {
        result.push_back(CaroByte(byte));
    }
    return CaroObj(copyArray(result));
});
nConst(caro_bytecode_version, "caro", "bytecode_version", { return CaroUint(BYTECODE_FORMAT);         });

nConst(caro_platform, "caro", "platform", {

    string platform;

    #if defined(__EMSCRIPTEN__)
        // emscripten larps as unix, which yeah it doesn't affect any of the real OSes below but better safe than sorry
        platform = "web";
    #elif defined(_WIN32)
        platform = "windows";
    #elif defined(__APPLE__)
        platform = "macos";
    #elif defined(__linux__)
        platform = "linux";
    #elif defined(__FreeBSD__)
        platform = "freebsd";
    #elif defined(__HAIKU__)
        platform = "haiku";
    #else
        platform = "unknown";
    #endif
        // wow, zero harmony between how each platform defines its macro, how beautiful
        
    return CaroObj(copyString(platform));

});
