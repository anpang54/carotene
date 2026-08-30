
#pragma once


// includes

#include <type_traits>

#include "common.hpp"


// forward declarations

struct Obj;

void   printObject (Obj* object);
string typeofObject(Obj* object);
bool   objectsEqual(Obj* a, Obj* b);
size_t sizeofObject(Obj* object);
size_t hashObject  (Obj* object);



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
        bool     Abool;
        uint8_t  Abyte;
        uint32_t Auint;
        int32_t  Aint;
        uint64_t Aulong;
        int64_t  Along;
        float    Afloat;
        double   Adouble;
        Obj* obj;
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


// number handling functions
// including 6 very boilerplate switch cases

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

template<typename T>
T asNumberTo(const Value& v) {
    switch(v.type) {
        case TYPE_BYTE  : return (T)v.as.Abyte  ;
        case TYPE_UINT  : return (T)v.as.Auint  ;
        case TYPE_INT   : return (T)v.as.Aint   ;
        case TYPE_ULONG : return (T)v.as.Aulong ;
        case TYPE_LONG  : return (T)v.as.Along  ;
        case TYPE_FLOAT : return (T)v.as.Afloat ;
        case TYPE_DOUBLE: return (T)v.as.Adouble;
        default: return (T)0;    // unreachable
    }
}
string asNumberToString(const Value& v) {
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

template<typename F>
Value mapNumber(const Value& v, F f) {
    switch(v.type) {
        case TYPE_BYTE  : return CaroByte  (f(v.as.Abyte  ));
        case TYPE_UINT  : return CaroUint  (f(v.as.Auint  ));
        case TYPE_INT   : return CaroInt   (f(v.as.Aint   ));
        case TYPE_ULONG : return CaroUlong (f(v.as.Aulong ));
        case TYPE_LONG  : return CaroLong  (f(v.as.Along  ));
        case TYPE_FLOAT : return CaroFloat (f(v.as.Afloat ));
        case TYPE_DOUBLE: return CaroDouble(f(v.as.Adouble));
        default: return CaroNull;    // unreachable
    }
}
template<typename F>
Value mapNumbers(const Value& a, const Value& b, F f) {
    switch(a.type) {
        case TYPE_BYTE  : return CaroByte  (f(a.as.Abyte  , b.as.Abyte  ));
        case TYPE_UINT  : return CaroUint  (f(a.as.Auint  , b.as.Auint  ));
        case TYPE_INT   : return CaroInt   (f(a.as.Aint   , b.as.Aint   ));
        case TYPE_ULONG : return CaroUlong (f(a.as.Aulong , b.as.Aulong ));
        case TYPE_LONG  : return CaroLong  (f(a.as.Along  , b.as.Along  ));
        case TYPE_FLOAT : return CaroFloat (f(a.as.Afloat , b.as.Afloat ));
        case TYPE_DOUBLE: return CaroDouble(f(a.as.Adouble, b.as.Adouble));
        default: return CaroNull;    // unreachable
    }
}
template<typename F>
Value mapFloat(Value& v, F f) {
    return mapNumber(v, [&](auto x) -> decltype(x) {
        if constexpr(std::is_floating_point_v<decltype(x)>) {
            return f(x);
        } else {
            return x;
        }
    });
}
template<typename F>
Value toFloat(Value& v, F f) {
    if(v.type == TYPE_FLOAT) {
        return CaroFloat(f(v.as.Afloat));
    } else {
        return CaroDouble(f(asNumberTo<double>(v)));
    }
}


// functions

bool isNumeric(ValueType type) {
    return type >= TYPE_BYTE && type <= TYPE_DOUBLE;
}
bool isInt(ValueType type) {
    return type >= TYPE_BYTE && type <= TYPE_LONG;
}
bool isFloat(ValueType type) {
    return type == TYPE_FLOAT || type == TYPE_DOUBLE;
}

bool isFalsey(Value value) {
    return value.type == TYPE_NULL || (value.type == TYPE_BOOL && !value.as.Abool);
}

bool valuesEqual(Value a, Value b) {

    // same type, neither is numeric
    if(a.type == b.type && !isNumeric(a.type)) {
        switch(a.type) {
            case TYPE_BOOL:   return a.as.Abool == b.as.Abool;
            case TYPE_NULL:   return true;
            case TYPE_SMTH:   return true;
            case TYPE_OBJ:    return objectsEqual(a.as.obj, b.as.obj);
            default:          return false;
        }
    }

    // different type, but numeric
    if(isNumeric(a.type) && isNumeric(b.type)) {
        if(isFloat(a.type) || isFloat(b.type)) {
            return asNumberTo<double>(a) == asNumberTo<double>(b);
        } else {
            return asNumberTo<int64_t>(a) == asNumberTo<int64_t>(b);
        }
    }

    return false;

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

string typeofType(ValueType type) {
    switch(type) {

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

        default:        return "unknown";    // should be unreachable

    }
}
string typeofValue(Value value) {
    if(value.type == TYPE_OBJ) {
        return typeofObject(value.as.obj);
    } else {
        return typeofType(value.type);
    }
}

size_t sizeofType(ValueType type) {

    // result is in bytes
    // doesn't include all the wrapper stuff, only the actual value

    switch(type) {

        case TYPE_BOOL:   return 1;    // technically 1 bit but it's stored as 1 byte
        case TYPE_NULL:   return 0;
        case TYPE_SMTH:   return 0;

        case TYPE_BYTE:   return 1;
        case TYPE_UINT:   return 4;
        case TYPE_INT:    return 4;
        case TYPE_ULONG:  return 8;
        case TYPE_LONG:   return 8;
        case TYPE_FLOAT:  return 4;
        case TYPE_DOUBLE: return 8;

        default:          return 0;    // should be unreachable

    }
}
size_t sizeofValue(Value& value) {
    if(value.type == TYPE_OBJ) {
        return sizeofObject(value.as.obj);
    } else {
        return sizeofType(value.type);
    }
}


// hash for use in dict keys

size_t hashValue(const Value& value) {

    size_t h;

    switch(value.type) {

        case TYPE_NULL:   h = 69;                   break;
        case TYPE_SMTH:   h = 420;                  break;
        case TYPE_BOOL:   h = value.as.Abool? 6: 7; break;

        case TYPE_BYTE: case TYPE_UINT: case TYPE_INT: case TYPE_ULONG: case TYPE_LONG: {
            h = hash<int64_t>{}(asNumberTo<int64_t>(value)); break;
        }
        case TYPE_FLOAT: case TYPE_DOUBLE: {
            double number = asNumberTo<double>(value);
            if(number == 0) number = 0.0;    // -0.0 = 0.0
            h = hash<double>{}(number);
            break;
        }

        case TYPE_OBJ:    h = hashObject(value.as.obj); break;

        default: h = 0; break;    // should be unreachable

    }

    return h;

}


// override operators

bool operator==(const Value& a, const Value& b) {
    return valuesEqual(a, b);
}

template<>
struct hash<Value>{
    size_t operator()(const Value& value) const {
        return hashValue(value);
    }
};