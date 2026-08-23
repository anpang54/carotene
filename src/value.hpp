
#pragma once


// includes

#include "common.hpp"


// forward declarations

struct Obj;

void   printObject (Obj* object);
string typeofObject(Obj* object);
bool   objectsEqual(Obj* a, Obj* b);



// types

enum ValueType{

    // general
    TYPE_NULL,
    TYPE_SMTH,
    TYPE_BOOL,

    // numeric
    TYPE_BYTE,
    TYPE_UINT,
    TYPE_INT,
    TYPE_ULONG,
    TYPE_LONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,

    // object
    TYPE_OBJ,

};


// value struct

struct Value{    // total 16 bytes
    ValueType type;     // 4 bytes
                        // 4 bytes of padding
    union{              // 8 bytes max
        bool     Abool;   // 1
        uint8_t  Abyte;   // 1
        uint32_t Auint;   // 4
        int32_t  Aint;    // 4
        uint64_t Aulong;  // 8
        int64_t  Along;   // 8
        float    Afloat;  // 4
        double   Adouble; // 8
        Obj* obj;         // 8
    } as;
};
    // crafting interpreters section 30.3 uses NaN boxing to condense everything into a single double
    // however, we can't do that here cuz we have 64 bit ints


// functions to make values

inline constexpr Value CaroNull                             { TYPE_NULL,   {                  } };
inline constexpr Value CaroSmth                             { TYPE_SMTH,   {                  } };
       constexpr Value CaroBool  (    bool v) { return Value{ TYPE_BOOL,   { .Abool   = v } }; }

       constexpr Value CaroByte  ( uint8_t v) { return Value{ TYPE_BYTE,   { .Abyte   = v } }; }
       constexpr Value CaroUint  (uint32_t v) { return Value{ TYPE_UINT,   { .Auint   = v } }; }
       constexpr Value CaroInt   ( int32_t v) { return Value{ TYPE_INT,    { .Aint    = v } }; }
       constexpr Value CaroUlong (uint64_t v) { return Value{ TYPE_ULONG,  { .Aulong  = v } }; }
       constexpr Value CaroLong  ( int64_t v) { return Value{ TYPE_LONG,   { .Along   = v } }; }
       constexpr Value CaroFloat (   float v) { return Value{ TYPE_FLOAT,  { .Afloat  = v } }; }
       constexpr Value CaroDouble(  double v) { return Value{ TYPE_DOUBLE, { .Adouble = v } }; }

       constexpr Value CaroObj   (    Obj* v) { return Value{ TYPE_OBJ,    { .obj     = v } }; }

template<typename T>
Value CaroNumber(ValueType type, T v) {
    switch(type) {
        case TYPE_BYTE  : return Value{ type, { .Abyte   =  (uint8_t)v } };
        case TYPE_UINT  : return Value{ type, { .Auint   = (uint32_t)v } };
        case TYPE_INT   : return Value{ type, { .Aint    =  (int32_t)v } };
        case TYPE_ULONG : return Value{ type, { .Aulong  = (uint64_t)v } };
        case TYPE_LONG  : return Value{ type, { .Along   =  (int64_t)v } };
        case TYPE_FLOAT : return Value{ type, { .Afloat  =    (float)v } };
        case TYPE_DOUBLE: return Value{ type, { .Adouble =   (double)v } };
        default: return CaroNull;    // unreachable
    }
}
double asNumberToDouble(Value& v) {
    switch(v.type) {
        case TYPE_BYTE  : return (double)v.as.Abyte  ;
        case TYPE_UINT  : return (double)v.as.Auint  ;
        case TYPE_INT   : return (double)v.as.Aint   ;
        case TYPE_ULONG : return (double)v.as.Aulong ;
        case TYPE_LONG  : return (double)v.as.Along  ;
        case TYPE_FLOAT : return (double)v.as.Afloat ;
        case TYPE_DOUBLE: return (double)v.as.Adouble;
        default: return (double)0;    // unreachable
    }
}
string asNumberToString(Value& v) {
    switch(v.type) {
        case TYPE_BYTE  : return to_string(v.as.Abyte  );
        case TYPE_UINT  : return to_string(v.as.Auint  );
        case TYPE_INT   : return to_string(v.as.Aint   );
        case TYPE_ULONG : return to_string(v.as.Aulong );
        case TYPE_LONG  : return to_string(v.as.Along  );
        case TYPE_FLOAT : return to_string(v.as.Afloat );
        case TYPE_DOUBLE: return to_string(v.as.Adouble);
        default: return "0";    // unreachable
    }
}


// functions

bool isNumeric(ValueType type) {
    return type == TYPE_BYTE
        || type == TYPE_UINT  || type == TYPE_INT    || type == TYPE_ULONG || type == TYPE_LONG
        || type == TYPE_FLOAT || type == TYPE_DOUBLE;
}
bool isFalsey(Value value) {
    return value.type == TYPE_NULL || (value.type == TYPE_BOOL && !value.as.Abool);
}

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;
    switch(a.type) {

        case TYPE_BOOL:   return a.as.Abool == b.as.Abool;
        case TYPE_NULL:   return true;
        case TYPE_SMTH:   return true;
        case TYPE_OBJ:    return objectsEqual(a.as.obj, b.as.obj);

        default:
            switch(a.type) {
                case TYPE_BYTE:   return a.as.Abyte   == b.as.Abyte;
                case TYPE_UINT:   return a.as.Auint   == b.as.Auint;
                case TYPE_INT:    return a.as.Aint    == b.as.Aint;
                case TYPE_ULONG:  return a.as.Aulong  == b.as.Aulong;
                case TYPE_LONG:   return a.as.Along   == b.as.Along;
                case TYPE_FLOAT:  return a.as.Afloat  == b.as.Afloat;
                case TYPE_DOUBLE: return a.as.Adouble == b.as.Adouble;
                default:          return false;    // unreachable
            }

    }
}

void printValue(Value value) {
    switch(value.type) {

        case TYPE_BOOL:
            cout << (value.as.Abool? "true": "false");
            break;
        case TYPE_NULL:
            cout << "null";
            break;
        case TYPE_SMTH:
            cout << "smth";
            break;
        case TYPE_OBJ:
            printObject(value.as.obj);
            break;

        default:
            if(isNumeric(value.type)) {
                cout << asNumberToString(value);
            } else {
                cout << "unknown";    // should be unreachable
            }

    }
}

string typeof(Value value) {
    switch(value.type) {

        case TYPE_BOOL:   return "bool";
        case TYPE_NULL:   return "null";
        case TYPE_SMTH:   return "smth";

        case TYPE_BYTE:   return "byte";
        case TYPE_UINT:   return "uint";
        case TYPE_INT:    return "int";
        case TYPE_ULONG:  return "ulong";
        case TYPE_LONG:   return "long";
        case TYPE_FLOAT:  return "float";
        case TYPE_DOUBLE: return "double";
        
        case TYPE_OBJ:    return typeofObject(value.as.obj);

        default:        return "unknown";    // should be unreachable

    }
}