
#pragma once


// includes

#include "common.hpp"


// values

vector<Obj*> objects;    // vector of pointers to objects for tracking allocation

enum ObjType{
    OBJ_STRING,
};

struct Obj{
    ObjType type;
};

struct ObjString: Obj{
    string str;
};
    // just a wrapper around an std::string


// functions

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

void freeObject(Obj* object) {
    switch(object->type) {
        case OBJ_STRING:
        delete static_cast<ObjString*>(object);
        break;
    }
}
void freeObjects() {
    for(Obj* object: objects) {
        freeObject(object);
    }
    objects.clear();
}
