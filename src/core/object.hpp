
#pragma once


// includes

#include <set>
#include <utility>

#include "common.hpp"
#include "chunk.hpp"

using std::set, std::pair;


// objects

vector<Obj*> objects;    // vector of pointers to objects for tracking allocation

enum ObjType{
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
};

struct Obj{
    ObjType type;
};


// strings

struct ObjString: Obj{
    string str;
};
    // just a wrapper around an std::string

bool isString(Value value) {
    return value.type == TYPE_OBJ && value.as.obj->type == OBJ_STRING;
}
ObjString* asString(Value value) {
    return static_cast<ObjString*>(value.as.obj);
}

ObjString* copyString(string str) {
    ObjString* object = new ObjString({OBJ_STRING}, std::move(str));
    objects.push_back(object);
    return object;
}


// functions

struct ObjFunction: Obj{
    int arity;
    Chunk chunk;
    string name;
};

bool isFunction(Value value) {
    return value.type == TYPE_OBJ && value.as.obj->type == OBJ_FUNCTION;
}
ObjFunction* asFunction(Value value) {
    return static_cast<ObjFunction*>(value.as.obj);
}

ObjFunction* newFunction() {
    ObjFunction* function = new ObjFunction({OBJ_FUNCTION}, 0, Chunk(), "");
    objects.push_back(function);
    return function;
}


// native functions

class VM;    // forward? declaration

typedef Value (*NativeFn)(VM* vm, vector<Value> args);

struct ObjNative: Obj{
    NativeFn function;
};

vector<pair<string, NativeFn>> natives;
set<string> modules;
set<string> nativeNames;

struct DefineNative{
    DefineNative(string module, string name, NativeFn function) {
        if(!module.empty()) modules.insert(std::move(module));
        nativeNames.insert(name);
        natives.push_back({std::move(name), function});
    }
};

bool isNative(Value value) {
    return value.type == TYPE_OBJ && value.as.obj->type == OBJ_NATIVE;
}
ObjNative* asNative(Value value) {
    return static_cast<ObjNative*>(value.as.obj);
}

ObjNative* newNative(NativeFn function) {
    ObjNative* native = new ObjNative({OBJ_NATIVE}, function);
    objects.push_back(native);
    return native;
}


// free

void freeObject(Obj* object) {
    switch(object->type) {
        case OBJ_STRING:
            delete static_cast<ObjString*>(object);
            break;
        case OBJ_FUNCTION:
            delete static_cast<ObjFunction*>(object);
            break;
        case OBJ_NATIVE:
            delete static_cast<ObjNative*>(object);
            break;
    }
}
void freeObjects() {
    for(Obj* object: objects) {
        freeObject(object);
    }
    objects.clear();
}


// object functions that were separated from value functions

void printObject(Obj* object) {
    switch(object->type) {

        case OBJ_STRING: {
            cout << static_cast<ObjString*>(object)->str;
            break;
        }

        case OBJ_FUNCTION: {
            ObjFunction* function = static_cast<ObjFunction*>(object);
            if(function->name.empty()) {
                cout << "<script>";
            } else {
                cout << "<func " << function->name << '>';
            }
            break;
        }

        case OBJ_NATIVE: {
            cout << "<native func>";
            break;
        }

    }
}

string typeofObject(Obj* object) {
    switch(object->type) {
        case OBJ_STRING:   return "str";
        case OBJ_FUNCTION: return "func";
        case OBJ_NATIVE:   return "native";
    }
    return "unknown";    // should be unreachable
}

bool objectsEqual(Obj* a, Obj* b) {
    if(a->type != b->type) return false;
    switch(a->type) {
        case OBJ_STRING:   return static_cast<ObjString*>(a)->str == static_cast<ObjString*>(b)->str;
        case OBJ_FUNCTION: return a == b;
        case OBJ_NATIVE:   return a == b;    // ?
    }
    return false;    // should be unreachable
}


