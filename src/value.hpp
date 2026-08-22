
#pragma once


// includes

#include "common.hpp"


// values

#define CaroNull          ((Value){ TYPE_NULL,   { .number  = 0     }})
#define CaroBool(value)   ((Value){ TYPE_BOOL,   { .boolean = value }})
#define CaroNumber(value) ((Value){ TYPE_NUMBER, { .number  = value }})

enum ValueType{
    TYPE_NULL,
    TYPE_BOOL,
    TYPE_NUMBER,
};

struct Value{
    ValueType type;
    union{
        bool boolean;     // 1 byte
        double number;    // 8 bytes
    } as;
};


// functions

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
    }
}