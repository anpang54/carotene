

#pragma once

/*
    this module is named "fs" (short for filesystem) instead of "file" for a few reasons
     - "file" is a very common variable name
     - a folder, which this module also handles, is not a file, unless you're talking about specifically unix/unix-like OSes
     - the underlying C++ library for most of these functions is std::filesystem
*/


// includes

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#include "natives.hpp"

namespace filesystem = std::filesystem;
using std::ios, std::ifstream, std::ofstream, std::stringstream, std::error_code;


// helpers

#define PATH(index) filesystem::path(STR(index))

#define FILE_CHECK(errorCode, action, path)\
    do{\
        if(errorCode) {\
            vm->runtimeError("Couldn't %s \"%s\": %s.", action, string(path).c_str(), errorCode.message().c_str());\
            return CaroNull;\
        }\
    } while(false)


// read/write

nFunc(fs_read, "fs", "read", {
    params({
        {{TYPE_OBJ}, true}
    });

    string path = STR(0);

    if(!filesystem::is_regular_file(path)) {
        vm->runtimeError("\"%s\" isn't a file.", path.c_str());
        return CaroNull;
    }

    ifstream file(path, ios::in | ios::binary);
    if(!file) {
        vm->runtimeError("Couldn't open file \"%s\" for reading.", path.c_str());
        return CaroNull;
    }
    stringstream buffer;
    buffer << file.rdbuf();
    return CaroObj(copyString(buffer.str()));

});

nFunc(fs_write, "fs", "write", {
    params({
        {{TYPE_OBJ}, true},
        {{TYPE_OBJ}, true}
    });

    string path = STR(0);
    ofstream file(path, ios::out | ios::trunc | ios::binary);
    if(!file) {
        vm->runtimeError("Couldn't open file \"%s\" for writing.", path.c_str());
        return CaroNull;
    }
    
    file << STR(1);
    file.close();
    if(file.fail()) {
        vm->runtimeError("Couldn't write to file \"%s\".", path.c_str());
        return CaroNull;
    }
    return CaroNull;

});

nFunc(fs_append, "fs", "append", {
    params({
        {{TYPE_OBJ}, true},
        {{TYPE_OBJ}, true}
    });

    string path = STR(0);
    ofstream file(path, ios::out | ios::app | ios::binary);
    if(!file) {
        vm->runtimeError("Couldn't open file \"%s\" for appending.", path.c_str());
        return CaroNull;
    }

    file << STR(1);
    file.close();
    if(file.fail()) {
        vm->runtimeError("Couldn't append to file \"%s\".", path.c_str());
        return CaroNull;
    }
    return CaroNull;

});


// other operations

nFunc(fs_create_folder, "fs", "create_folder", {
    params({
        {{TYPE_OBJ}, true}
    });

    error_code errorCode;
    bool created = filesystem::create_directory(PATH(0), errorCode);
    FILE_CHECK(errorCode, "create folder", STR(0));
    return CaroBool(created);

});

nFunc(fs_symlink, "fs", "symlink", {
    params({
        {{TYPE_OBJ}, true},
        {{TYPE_OBJ}, true}
    });

    #ifdef _WIN32
        vm->runtimeError("Windows symlinks are annoying to work with so no.");
        return CaroNull;
    #else
        error_code errorCode;
        filesystem::create_symlink(PATH(1), PATH(0), errorCode);
            // carotene arguments: link,   target
            //      C++ arguments: target, link
            // therefore, the parameter order is reversed when passing to create_symlink()
        FILE_CHECK(errorCode, "create symlink", STR(0));
        return CaroNull;
    #endif

});

nFunc(fs_copy, "fs", "copy", {
    params({
        {{TYPE_OBJ }, true },
        {{TYPE_OBJ }, true },
        {{TYPE_BOOL}, false}
    });

    filesystem::copy_options options = args.size() >= 3 && isFalsy(args[2])? filesystem::copy_options::none: filesystem::copy_options::recursive;
    error_code errorCode;
    filesystem::copy(PATH(0), PATH(1), options, errorCode);
    FILE_CHECK(errorCode, "copy", STR(0));
    return CaroNull;

});

nFunc(fs_rename, "fs", "rename", {
    params({
        {{TYPE_OBJ}, true},
        {{TYPE_OBJ}, true}
    });

    error_code errorCode;
    filesystem::rename(PATH(0), PATH(1), errorCode);
    FILE_CHECK(errorCode, "rename", STR(0));
    return CaroNull;

});

nFunc(fs_delete, "fs", "delete", {
    params({
        {{TYPE_OBJ }, true },
        {{TYPE_BOOL}, false}
    });

    error_code errorCode;
    if(args.size() >= 2 && isFalsy(args[1])) {
        // false = only delete 1 file, stop if there are more
        bool deleted = filesystem::remove(PATH(0), errorCode);
        FILE_CHECK(errorCode, "delete", STR(0));
        return CaroBool(deleted);
    } else {
        // true = recursively delete
        uintmax_t deleted = filesystem::remove_all(PATH(0), errorCode);
        FILE_CHECK(errorCode, "delete", STR(0));
        return CaroBool(deleted > 0);
    }

});


// get file info

nFunc(fs_exists, "fs", "exists", {
    params({
        {{TYPE_OBJ}, true}
    });

    error_code errorCode;
    bool exists = filesystem::exists(PATH(0), errorCode);
    FILE_CHECK(errorCode, "check the existence of", STR(0));
    return CaroBool(exists);

});

nFunc(fs_size, "fs", "size", {
    params({
        {{TYPE_OBJ}, true}
    });

    error_code errorCode;
    uint64_t size = filesystem::file_size(PATH(0), errorCode);
    FILE_CHECK(errorCode, "get the size of", STR(0));
    return CaroUlong(size);

});

nFunc(fs_type, "fs", "type", {
    params({
        {{TYPE_OBJ}, true}
    });

    filesystem::path path = PATH(0);
    error_code errorCode;

    // firstly, check if it's a symlink and whether it exists or not
    filesystem::file_status status = filesystem::symlink_status(path, errorCode);
    FILE_CHECK(errorCode, "get the type of", STR(0));
    if(status.type() == filesystem::file_type::not_found) {
        vm->runtimeError("Couldn't get the type of \"%s\" because it doesn't exist.", STR(0).c_str());
        return CaroNull;
    }
    if(filesystem::is_symlink(status)) {
        return CaroObj(copyString("symlink"));
    }

    // other types
    status = filesystem::status(path, errorCode);
    FILE_CHECK(errorCode, "get the type of", STR(0));
         if(filesystem::is_regular_file(status)) return CaroObj(copyString("file"  ));
    else if(filesystem::is_directory   (status)) return CaroObj(copyString("folder"));
    else                                         return CaroObj(copyString("other" ));

});

nFunc(fs_target, "fs", "target", {
    params({
        {{TYPE_OBJ}, true}
    });

    error_code errorCode;
    filesystem::path target = filesystem::read_symlink(PATH(0), errorCode);
    FILE_CHECK(errorCode, "get the target of symlink", STR(0));
    return CaroObj(copyString(target.string()));
    
});
