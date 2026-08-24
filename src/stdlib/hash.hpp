

#pragma once


// includes

#define XXH_INLINE_ALL
#include "../../include/xxHash/xxhash.h"

#include "../../include/SHA/SHA256.cpp"
#include "../../include/SHA/SHA384.cpp"
#include "../../include/SHA/SHA512.cpp"

#include "natives.hpp"


// functions

NATIVE(hash_xxhash, "hash", "xxhash", {
    params({
        {{TYPE_OBJ}, true}
    });
    const string& str = asString(args[0])->str;
    uint64_t hashed = XXH3_64bits(str.data(), str.size());
    return CaroObj(copyString(format("{:016x}", hashed)));
});

NATIVE(hash_sha256, "hash", "sha256", {
    params({
        {{TYPE_OBJ}, true}
    });
    SHA256 hasher;
    return CaroObj(copyString(hasher.hash(asString(args[0])->str)));
});
NATIVE(hash_sha384, "hash", "sha384", {
    params({
        {{TYPE_OBJ}, true}
    });
    SHA384 hasher;
    return CaroObj(copyString(hasher.hash(asString(args[0])->str)));
});
NATIVE(hash_sha512, "hash", "sha512", {
    params({
        {{TYPE_OBJ}, true}
    });
    SHA512 hasher;
    return CaroObj(copyString(hasher.hash(asString(args[0])->str)));
});