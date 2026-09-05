
#pragma once


// includes

#include <type_traits>

#include <cmath>

#include "common.hpp"


// forward declarations for object functions

struct Obj;

string printObject   (Obj* object);
string typeofObject  (Obj* object);
bool   isTruthyObject(Obj* object);
bool   objectsEqual  (Obj* a, Obj* b);
size_t sizeofObject  (Obj* object);
size_t hashObject    (Obj* object);



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

    // vectors
    TYPE_VEC2I,
    TYPE_VEC2U,
    TYPE_VEC2F,
    TYPE_VEC3I,
    TYPE_VEC3U,
    TYPE_VEC3F,
    
    // object
    TYPE_OBJ,

};


// value struct

struct Value{

    // 0 - 3: type
    ValueType type;
    
    // 4 - 7: z value in vec3
    union{
        int32_t  Zint;
        uint32_t Zuint;
        float    Zfloat;
    } z;

    // 8 - 15: actual value
    union{
        bool     Abool;
        uint8_t  Abyte;
        int32_t  Aint;
        uint32_t Auint;
        int64_t  Along;
        uint64_t Aulong;
        float    Afloat;
        double   Adouble;
        struct{ int32_t  Xint;   int32_t  Yint;   } XYint;
        struct{ uint32_t Xuint;  uint32_t Yuint;  } XYuint;
        struct{ float    Xfloat; float    Yfloat; } XYfloat;
        Obj* obj;
    } as;

};
    // crafting interpreters section 30.3 uses NaN boxing to condense everything into a single double
    // however, we can't do that here cuz we can't afford throwing 64-bit ints and 3 value vectors into the heap


// functions to make values

inline constexpr Value CaroNull                             { TYPE_NULL,   {}, {              } };
inline constexpr Value CaroSmth                             { TYPE_SMTH,   {}, {              } };
       constexpr Value CaroBool  (    bool v) { return Value{ TYPE_BOOL,   {}, { .Abool   = v } }; }

       constexpr Value CaroByte  ( uint8_t v) { return Value{ TYPE_BYTE,   {}, { .Abyte   = v } }; }
       constexpr Value CaroInt   ( int32_t v) { return Value{ TYPE_INT,    {}, { .Aint    = v } }; }
       constexpr Value CaroUint  (uint32_t v) { return Value{ TYPE_UINT,   {}, { .Auint   = v } }; }
       constexpr Value CaroLong  ( int64_t v) { return Value{ TYPE_LONG,   {}, { .Along   = v } }; }
       constexpr Value CaroUlong (uint64_t v) { return Value{ TYPE_ULONG,  {}, { .Aulong  = v } }; }
       constexpr Value CaroFloat (   float v) { return Value{ TYPE_FLOAT,  {}, { .Afloat  = v } }; }
       constexpr Value CaroDouble(  double v) { return Value{ TYPE_DOUBLE, {}, { .Adouble = v } }; }

       constexpr Value CaroVec2i(int32_t  x, int32_t  y            ) { return Value{ TYPE_VEC2I, {             }, { .XYint   = { x, y } } }; }
       constexpr Value CaroVec2u(uint32_t x, uint32_t y            ) { return Value{ TYPE_VEC2U, {             }, { .XYuint  = { x, y } } }; }
       constexpr Value CaroVec2f(float    x, float    y            ) { return Value{ TYPE_VEC2F, {             }, { .XYfloat = { x, y } } }; }
       constexpr Value CaroVec3i(int32_t  x, int32_t  y, int32_t  z) { return Value{ TYPE_VEC3I, { .Zint   = z }, { .XYint   = { x, y } } }; }
       constexpr Value CaroVec3u(uint32_t x, uint32_t y, uint32_t z) { return Value{ TYPE_VEC3U, { .Zuint  = z }, { .XYuint  = { x, y } } }; }
       constexpr Value CaroVec3f(float    x, float    y, float    z) { return Value{ TYPE_VEC3F, { .Zfloat = z }, { .XYfloat = { x, y } } }; }

       constexpr Value CaroObj   (    Obj* v) { return Value{ TYPE_OBJ,    {}, { .obj     = v } }; }


// number handling functions
// including 6 very boilerplate switch cases

template<typename T>
Value CaroNumber(ValueType type, T v) {
    switch(type) {
        case TYPE_BYTE  : return Value{ type, {}, { .Abyte   =  (uint8_t)v } };
        case TYPE_INT   : return Value{ type, {}, { .Aint    =  (int32_t)v } };
        case TYPE_UINT  : return Value{ type, {}, { .Auint   = (uint32_t)v } };
        case TYPE_LONG  : return Value{ type, {}, { .Along   =  (int64_t)v } };
        case TYPE_ULONG : return Value{ type, {}, { .Aulong  = (uint64_t)v } };
        case TYPE_FLOAT : return Value{ type, {}, { .Afloat  =    (float)v } };
        case TYPE_DOUBLE: return Value{ type, {}, { .Adouble =   (double)v } };
        default: return CaroNull;    // unreachable
    }
}

template<typename T>
T asNumberTo(const Value& v) {
    switch(v.type) {
        case TYPE_BYTE  : return (T)v.as.Abyte  ;
        case TYPE_INT   : return (T)v.as.Aint   ;
        case TYPE_UINT  : return (T)v.as.Auint  ;
        case TYPE_LONG  : return (T)v.as.Along  ;
        case TYPE_ULONG : return (T)v.as.Aulong ;
        case TYPE_FLOAT : return (T)v.as.Afloat ;
        case TYPE_DOUBLE: return (T)v.as.Adouble;
        default: return (T)0;    // unreachable
    }
}
string asNumberToString(const Value& v) {
    switch(v.type) {
        case TYPE_BYTE  : return to_string(v.as.Abyte  );
        case TYPE_INT   : return to_string(v.as.Aint   );
        case TYPE_UINT  : return to_string(v.as.Auint  );
        case TYPE_LONG  : return to_string(v.as.Along  );
        case TYPE_ULONG : return to_string(v.as.Aulong );
        case TYPE_FLOAT : return to_string(v.as.Afloat );
        case TYPE_DOUBLE: return to_string(v.as.Adouble);
        default: return "0";    // unreachable
    }
}

template<typename F>
Value mapNumber(const Value& v, F f) {
    switch(v.type) {
        case TYPE_BYTE  : return CaroByte  (f(v.as.Abyte  ));
        case TYPE_INT   : return CaroInt   (f(v.as.Aint   ));
        case TYPE_UINT  : return CaroUint  (f(v.as.Auint  ));
        case TYPE_LONG  : return CaroLong  (f(v.as.Along  ));
        case TYPE_ULONG : return CaroUlong (f(v.as.Aulong ));
        case TYPE_FLOAT : return CaroFloat (f(v.as.Afloat ));
        case TYPE_DOUBLE: return CaroDouble(f(v.as.Adouble));
        default: return CaroNull;    // unreachable
    }
}
template<typename F>
Value mapNumbers(const Value& a, const Value& b, F f) {
    switch(a.type) {
        case TYPE_BYTE  : return CaroByte  (f(a.as.Abyte  , b.as.Abyte  ));
        case TYPE_INT   : return CaroInt   (f(a.as.Aint   , b.as.Aint   ));
        case TYPE_UINT  : return CaroUint  (f(a.as.Auint  , b.as.Auint  ));
        case TYPE_LONG  : return CaroLong  (f(a.as.Along  , b.as.Along  ));
        case TYPE_ULONG : return CaroUlong (f(a.as.Aulong , b.as.Aulong ));
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


// vectors

template<typename T>
Value CaroVector(ValueType type, T x, T y, T z) {
    switch(type) {
        case TYPE_VEC2I: return CaroVec2i( (int32_t)x,  (int32_t)y             );
        case TYPE_VEC2U: return CaroVec2u((uint32_t)x, (uint32_t)y             );
        case TYPE_VEC2F: return CaroVec2f(   (float)x,    (float)y             );
        case TYPE_VEC3I: return CaroVec3i( (int32_t)x,  (int32_t)y,  (int32_t)z);
        case TYPE_VEC3U: return CaroVec3u((uint32_t)x, (uint32_t)y, (uint32_t)z);
        case TYPE_VEC3F: return CaroVec3f(   (float)x,    (float)y,    (float)z);
        default: return CaroNull;    // unreachable
    }
}

ValueType componentType(ValueType type) {
    switch(type) {
        case TYPE_VEC2I: case TYPE_VEC3I: return TYPE_INT;
        case TYPE_VEC2U: case TYPE_VEC3U: return TYPE_UINT;
        case TYPE_VEC2F: case TYPE_VEC3F: return TYPE_FLOAT;
        default: return TYPE_NULL;    // unreachable
    }
}
ValueType vectorType(ValueType component, uint8_t size) {
    switch(component) {
        case TYPE_INT:   return size == 2? TYPE_VEC2I: TYPE_VEC3I;
        case TYPE_UINT:  return size == 2? TYPE_VEC2U: TYPE_VEC3U;
        case TYPE_FLOAT: return size == 2? TYPE_VEC2F: TYPE_VEC3F;
        default: return TYPE_NULL;    // unreachable
    }
}

Value getComponent(const Value& v, int component) {
    switch(v.type) {
        case TYPE_VEC2I: case TYPE_VEC3I:
            return CaroInt  (component == 0? v.as.XYint  .Xint  : (component == 1? v.as.XYint  .Yint  : v.z.Zint  ));
        case TYPE_VEC2U: case TYPE_VEC3U:
            return CaroUint (component == 0? v.as.XYuint .Xuint : (component == 1? v.as.XYuint .Yuint : v.z.Zuint ));
        case TYPE_VEC2F: case TYPE_VEC3F:
            return CaroFloat(component == 0? v.as.XYfloat.Xfloat: (component == 1? v.as.XYfloat.Yfloat: v.z.Zfloat));
        default: return CaroNull;    // unreachable    
    }
}
void setComponent(Value& v, int component, const Value& to) {
    switch(v.type) {
        case TYPE_VEC2I: case TYPE_VEC3I: {
            int32_t&  slot = component == 0? v.as.XYint  .Xint  : (component == 1? v.as.XYint  .Yint  : v.z.Zint  );
            slot = to.as.Aint;
            break;
        }
        case TYPE_VEC2U: case TYPE_VEC3U: {
            uint32_t& slot = component == 0? v.as.XYuint .Xuint : (component == 1? v.as.XYuint .Yuint : v.z.Zuint );
            slot = to.as.Auint;
            break;
        }
        case TYPE_VEC2F: case TYPE_VEC3F: {
            float&    slot = component == 0? v.as.XYfloat.Xfloat: (component == 1? v.as.XYfloat.Yfloat: v.z.Zfloat);
            slot = to.as.Afloat;
            break;
        }
        default: break;    // unreachable
    }
}


// functions

bool isNumeric (ValueType type) { return type >= TYPE_BYTE  && type <= TYPE_DOUBLE; }
bool isInt     (ValueType type) { return type >= TYPE_BYTE  && type <= TYPE_LONG;   }
bool isFloat   (ValueType type) { return type == TYPE_FLOAT || type == TYPE_DOUBLE; }
bool isUnsigned(ValueType type) { return type == TYPE_BYTE  || type == TYPE_UINT || type == TYPE_ULONG; }

bool isVector (ValueType type) { return type >= TYPE_VEC2I && type <= TYPE_VEC3F;  }
bool isVec2   (ValueType type) { return type >= TYPE_VEC2I && type <= TYPE_VEC2F;  }
bool isVec3   (ValueType type) { return type >= TYPE_VEC3I && type <= TYPE_VEC3F;  }

int componentCount(ValueType type) { return isVec2(type)? 2: 3; }

bool isTruthy(Value value) {
    if(isNumeric(value.type)) {
        if(isFloat(value.type) && std::isnan(asNumberTo<double>(value))) {
            return false;
        }
        return asNumberTo<double>(value) != 0;
    }
    if(isVector(value.type)) {
        switch(value.type) {
            case TYPE_VEC2I: case TYPE_VEC3I: return value.as.XYint  .Xint        || value.as.XYint  .Yint        || value.z.Zint;
            case TYPE_VEC2U: case TYPE_VEC3U: return value.as.XYuint .Xuint       || value.as.XYuint .Yuint       || value.z.Zuint;
            case TYPE_VEC2F: case TYPE_VEC3F: return value.as.XYfloat.Xfloat != 0 || value.as.XYfloat.Yfloat != 0 || value.z.Zfloat != 0;
            default: return false;    // unreachable
        }
    }
    switch(value.type) {
        case TYPE_NULL: return false;
        case TYPE_SMTH: return true;
        case TYPE_BOOL: return value.as.Abool;
        case TYPE_OBJ:  return isTruthyObject(value.as.obj);
        default: return false;

    }
}
bool isFalsy(Value value) {
    return !isTruthy(value);
}

bool numbersEqual(const Value& a, const Value& b) {

    // an int and a float
    if(isFloat(a.type) != isFloat(b.type)) {

        const Value& number = isFloat(a.type)? b: a;
        double real = asNumberTo<double>(isFloat(a.type)? a: b);

        if(std::isnan(real) || std::isinf(real) || real != std::floor(real)) return false;

        if(isUnsigned(number.type)) {
            if(real < 0.0 || real >= 18446744073709551616.0) return false;
            return (uint64_t)real == asNumberTo<uint64_t>(number);
        }
        if(real < -9223372036854775808.0 || real >= 9223372036854775808.0) return false;
        return (int64_t)real == asNumberTo<int64_t>(number);

    }

    // 2 floats
    if(isFloat(a.type)) return asNumberTo<double>(a) == asNumberTo<double>(b);

    // a signed int and an unsigned int
    if(isUnsigned(a.type) != isUnsigned(b.type)) {
        int64_t signedValue = asNumberTo<int64_t>(isUnsigned(a.type)? b: a);
        if(signedValue < 0) return false;
        return (uint64_t)signedValue == asNumberTo<uint64_t>(isUnsigned(a.type)? a: b);
    }

    // 2 ints of the same signedness
    if(isUnsigned(a.type)) return asNumberTo<uint64_t>(a) == asNumberTo<uint64_t>(b);
    return asNumberTo<int64_t>(a) == asNumberTo<int64_t>(b);

}

bool valuesEqual(Value a, Value b) {

    // same type, vectors
    if(isVector(a.type) && isVector(b.type)) {
        if(componentCount(a.type) != componentCount(b.type)) return false;    // comparing a vec2 and a vec3
        for(int i = 0; i < componentCount(a.type); ++i) {    // same size, but check each element
            if(!valuesEqual(getComponent(a, i), getComponent(b, i))) return false;
        }
        return true;
    }

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
        return numbersEqual(a, b);
    }

    return false;

}

string printValue(Value value) {
    switch(value.type) {

        case TYPE_BOOL: return value.as.Abool? "true": "false";
        case TYPE_NULL: return "null";
        case TYPE_SMTH: return "smth";
        case TYPE_OBJ:  return printObject(value.as.obj);

        default:
            if(isNumeric(value.type)) {
                return asNumberToString(value);
            } else if(isVector(value.type)) {
                switch(value.type) {
                    case TYPE_VEC2I: return "(" + to_string(value.as.XYint  .Xint  ) + ", " + to_string(value.as.XYint  .Yint  ) + ")";
                    case TYPE_VEC2U: return "(" + to_string(value.as.XYuint .Xuint ) + ", " + to_string(value.as.XYuint .Yuint ) + ")";
                    case TYPE_VEC2F: return "(" + to_string(value.as.XYfloat.Xfloat) + ", " + to_string(value.as.XYfloat.Yfloat) + ")";
                    case TYPE_VEC3I: return "(" + to_string(value.as.XYint  .Xint  ) + ", " + to_string(value.as.XYint  .Yint  ) + ", " + to_string(value.z.Zint  ) + ")";
                    case TYPE_VEC3U: return "(" + to_string(value.as.XYuint .Xuint ) + ", " + to_string(value.as.XYuint .Yuint ) + ", " + to_string(value.z.Zuint ) + ")";
                    case TYPE_VEC3F: return "(" + to_string(value.as.XYfloat.Xfloat) + ", " + to_string(value.as.XYfloat.Yfloat) + ", " + to_string(value.z.Zfloat) + ")";
                    default: return "vector";    // unreachable
                }
            } else {
                return "unknown";    // should be unreachable
            }

    }
}

string typeofType(ValueType type) {
    switch(type) {

        case TYPE_BOOL:   return "bool";
        case TYPE_NULL:   return "null";
        case TYPE_SMTH:   return "smth";

        case TYPE_BYTE:   return "byte";
        case TYPE_INT:    return "int";
        case TYPE_UINT:   return "uint";
        case TYPE_LONG:   return "long";
        case TYPE_ULONG:  return "ulong";
        case TYPE_FLOAT:  return "float";
        case TYPE_DOUBLE: return "double";

        case TYPE_VEC2I:  return "vec2i";
        case TYPE_VEC2U:  return "vec2u";
        case TYPE_VEC2F:  return "vec2f";
        case TYPE_VEC3I:  return "vec3i";
        case TYPE_VEC3U:  return "vec3u";
        case TYPE_VEC3F:  return "vec3f";
        
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

        case TYPE_NULL: case TYPE_SMTH:
            return 0;
        case TYPE_BOOL: case TYPE_BYTE:
            return 1;    // bool is technically 1 bit but is stored as 1 byte
        case TYPE_INT: case TYPE_UINT: case TYPE_FLOAT:
            return 4;
        case TYPE_LONG: case TYPE_ULONG: case TYPE_DOUBLE: case TYPE_VEC2I: case TYPE_VEC2U: case TYPE_VEC2F:
            return 8;
        case TYPE_VEC3I: case TYPE_VEC3U: case TYPE_VEC3F:
            return 12;

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

size_t hashNumber(const Value& value) {
    if(isFloat(value.type)) {
        double real = asNumberTo<double>(value);
        if(!std::isnan(real) && !std::isinf(real) && real == std::floor(real)) {
            if(real >= 0.0 && real <  18446744073709551616.0) return hash<uint64_t>{}(          (uint64_t)real);
            if(real <  0.0 && real >= -9223372036854775808.0) return hash<uint64_t>{}((uint64_t)(int64_t) real);
        }
        return hash<double>{}(real);
    }
    if(isUnsigned(value.type)) return hash<uint64_t>{}(          asNumberTo<uint64_t>(value));
    return                            hash<uint64_t>{}((uint64_t)asNumberTo< int64_t>(value));
}

size_t hashValue(const Value& value) {

    size_t h;

    switch(value.type) {

        case TYPE_NULL:   h = 69;                   break;
        case TYPE_SMTH:   h = 420;                  break;
        case TYPE_BOOL:   h = value.as.Abool? 6: 7; break;

        case TYPE_BYTE: case TYPE_UINT: case TYPE_INT: case TYPE_ULONG: case TYPE_LONG:
        case TYPE_FLOAT: case TYPE_DOUBLE: {
            h = hashNumber(value); break;
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
struct std::hash<Value>{
    size_t operator()(const Value& value) const {
        return hashValue(value);
    }
};
