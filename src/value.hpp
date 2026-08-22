
#pragma once


// includes

#include "common.hpp"


// values

#define CaroNull          ((Value){ TYPE_NULL,   { .number  = 0            }})
#define CaroBool(value)   ((Value){ TYPE_BOOL,   { .boolean = value        }})
#define CaroNumber(value) ((Value){ TYPE_NUMBER, { .number  = value        }})
#define CaroObj(object)   ((Value){ TYPE_OBJ,    { .obj     = (Obj*)object }})

struct Obj;    // forward declaration

enum ValueType{
    TYPE_NULL,
    TYPE_BOOL,
    TYPE_NUMBER,
    TYPE_OBJ,
};

struct Value{
    ValueType type;
    union{
        bool boolean;     // 1 byte
        double number;    // 8 bytes
        Obj* obj;         // size_t = 8 bytes
    } as;
};


// functions

#include "object.hpp"

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;
    switch(a.type) {
        case TYPE_BOOL:   return a.as.boolean == b.as.boolean;
        case TYPE_NULL:   return true;
        case TYPE_NUMBER: return a.as.number == b.as.number;
        case TYPE_OBJ:    return asString(a)->str == asString(b)->str;
        default:          return false;    // unreachable
    }
}

bool isFalsey(Value value) {
    return value.type == TYPE_NULL || (value.type == TYPE_BOOL && !value.as.boolean);
}

void printValue(Value value) {
    switch(value.type) {
        case TYPE_BOOL:
            cout << (value.as.boolean? "true": "false");
            break;
        case TYPE_NULL:
            cout << "null";
            break;
        case TYPE_NUMBER:
            cout << value.as.number;
            break;
        case TYPE_OBJ: {
            switch(value.as.obj->type) {
                case OBJ_STRING:
                    cout << asString(value)->str;
                    break;
            }
            break;
        }
    }
}