

#pragma once


// INCLUDES

#define XXH_INLINE_ALL
namespace xxHash{
    #include "../../../include/xxHash/xxhash.h"
}

namespace SHA{
	#include "../../../include/SHA/SHA256.cpp"
	#include "../../../include/SHA/SHA384.cpp"
	#include "../../../include/SHA/SHA512.cpp"
}
	// wrapped to prevent typedef conflicts with haiku
	
#include "../util/natives.hpp"


// FUNCTIONS

nFunc(hash_xxhash, "hash", "xxhash", {
    params({
        {{OBJ_STRING}, true}
    });
    const string& str = asString(args[0])->str;
    uint64_t hashed = xxHash::XXH3_64bits(str.data(), str.size());
    return CaroObj(copyString(format("{:016x}", hashed)));
});

nFunc(hash_sha256, "hash", "sha256", {
    params({
        {{OBJ_STRING}, true}
    });
    SHA::SHA256 hasher;
    return CaroObj(copyString(hasher.hash(asString(args[0])->str)));
});
nFunc(hash_sha384, "hash", "sha384", {
    params({
        {{OBJ_STRING}, true}
    });
    SHA::SHA384 hasher;
    return CaroObj(copyString(hasher.hash(asString(args[0])->str)));
});
nFunc(hash_sha512, "hash", "sha512", {
    params({
        {{OBJ_STRING}, true}
    });
    SHA::SHA512 hasher;
    return CaroObj(copyString(hasher.hash(asString(args[0])->str)));
});
