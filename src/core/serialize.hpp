
#pragma once


// INCLUDES

#include <cstring>

#include "common.hpp"
#include "chunk.hpp"
#include "value.hpp"
#include "object.hpp"


// HEADER

constexpr char MAGIC_NUMBER[] = "\x7f" "reti";
                             // 7f 72 65 74 69 00
                             // 7f because ELF is cool
                             // also if you open a .reti file in a text editor it's gonna know something's up
const uint16_t BYTECODE_FORMAT = 0x0002;


// SERIALIZE


// writer

struct Writer{

    vector<uint8_t> buffer;

    void writeUint8 (uint8_t v)  { this->buffer.push_back(v); }
    void writeUint16(uint16_t v) { writeUint8(v & 0xff);        writeUint8(v >> 8); }
    void writeUint32(uint32_t v) { writeUint16(v & 0xffff);     writeUint16(v >> 16); }
    void writeUint64(uint64_t v) { writeUint32(v & 0xffffffff); writeUint32(v >> 32); }
    void writeFloat (float v)    { uint32_t bits; memcpy(&bits, &v, 4); writeUint32(bits); }
    void writeDouble(double v)   { uint64_t bits; memcpy(&bits, &v, 8); writeUint64(bits); }

    void writeString(const string& str) {
        writeUint32(str.size());
        this->buffer.insert(this->buffer.end(), str.begin(), str.end());
    }

};


// serialize

void serializeFunction(Writer& w, ObjFunction* function);

void serializeValue(Writer& w, Value& value) {

    // write type
    w.writeUint8(value.type);

    // write actual content
    switch(value.type) {

        case TYPE_NULL: case TYPE_SMTH: break;

        case TYPE_BOOL:   w.writeUint8 (value.as.Abool);   break;
        case TYPE_BYTE:   w.writeUint8 (value.as.Abyte);   break;
        case TYPE_UINT:   w.writeUint32(value.as.Auint);   break;
        case TYPE_INT:    w.writeUint32(value.as.Aint);    break;
        case TYPE_ULONG:  w.writeUint64(value.as.Aulong);  break;
        case TYPE_LONG:   w.writeUint64(value.as.Along);   break;
        case TYPE_FLOAT:  w.writeFloat (value.as.Afloat);  break;
        case TYPE_DOUBLE: w.writeDouble(value.as.Adouble); break;

        case TYPE_OBJ: {
            Obj* obj = value.as.obj;
            w.writeUint8(obj->type);
            switch(obj->type) {
                case OBJ_STRING:
                    w.writeUint8(static_cast<ObjString*>(obj)->fString);
                    w.writeString(static_cast<ObjString*>(obj)->str);
                    break;
                case OBJ_FUNCTION:
                    serializeFunction(w, static_cast<ObjFunction*>(obj));
                    break;
                default:
                    break;
                    // unreachable
            }
            break;
        }

    }
}

void serializeFunction(Writer& w, ObjFunction* function) {

    // name and arity
    w.writeString(function->name);
    w.writeUint8(function->arity);

    // code
    Chunk& chunk = function->chunk;
    w.writeUint32(chunk.code.size());
    w.buffer.insert(w.buffer.end(), chunk.code.begin(), chunk.code.end());

    // lines
    for(size_t i = 0, j; i < chunk.lines.size(); i = j) {
        for(j = i; j < chunk.lines.size() && chunk.lines[j] == chunk.lines[i]; ++j);
        w.writeUint32(chunk.lines[i]);
        w.writeUint32(j - i);
    }

    // constants
    w.writeUint16(chunk.constants.size());
    for(Value& constant: chunk.constants) {
        serializeValue(w, constant);
    }

}

vector<uint8_t> serializeApp(ObjFunction* function, const string& path) {

    Writer w;

    // 0 - 5: magic number
    w.buffer.insert(w.buffer.end(), MAGIC_NUMBER, MAGIC_NUMBER + sizeof(MAGIC_NUMBER));

    // 6 - 7: bytecode version
    w.writeUint16(BYTECODE_FORMAT);

    serializeFunction(w, function);
    
    return w.buffer;

}


// DESERIALIZE


// reader

struct Reader{

    vector<uint8_t> data;
    size_t position = 0;

    uint8_t  readUint8()  { return this->data[this->position++]; }
    uint16_t readUint16() { uint16_t a = readUint8();  return a | ((uint16_t)readUint8()  << 8);  }
    uint32_t readUint32() { uint32_t a = readUint16(); return a | ((uint32_t)readUint16() << 16); }
    uint64_t readUint64() { uint64_t a = readUint32(); return a | ((uint64_t)readUint32() << 32); }
    float    readFloat()  { uint32_t bits = readUint32(); float  v; memcpy(&v, &bits, 4); return v; }
    double   readDouble() { uint64_t bits = readUint64(); double v; memcpy(&v, &bits, 8); return v; }

    string readString() {
        uint32_t length = readUint32();
        string str(this->data.begin() + this->position, this->data.begin() + this->position + length);
        this->position += length;
        return str;
    }

};



// deserialize

// todo: add some more guards on malformed bytecode
// but for now let's just assume it's all valid bytecode

ObjFunction* deserializeFunction(Reader& r);

Value deserializeValue(Reader& r) {

    uint8_t type = r.readUint8();

    switch(type) {

        case TYPE_NULL:   return CaroNull;
        case TYPE_SMTH:   return CaroSmth;
        case TYPE_BOOL:   return CaroBool  (         r.readUint8());

        case TYPE_BYTE:   return CaroByte  (         r.readUint8() );
        case TYPE_UINT:   return CaroUint  (         r.readUint32());
        case TYPE_INT:    return CaroInt   ((int32_t)r.readUint32());
        case TYPE_ULONG:  return CaroUlong (         r.readUint64());
        case TYPE_LONG:   return CaroLong  ((int64_t)r.readUint64());
        case TYPE_FLOAT:  return CaroFloat (         r.readFloat()) ;
        case TYPE_DOUBLE: return CaroDouble(         r.readDouble());

        case TYPE_OBJ: {
            uint8_t objType = r.readUint8();
            switch(objType) {
                case OBJ_STRING: {
                    bool fString = r.readUint8();
                    return CaroObj(copyString(r.readString(), fString));
                }
                case OBJ_FUNCTION: return CaroObj(deserializeFunction(r));
                default: break;    // unreachable
            }
        }

    }

    return CaroNull;

}

ObjFunction* deserializeFunction(Reader& r) {

    ObjFunction* function = newFunction();

    // name and arity
    function->name  = r.readString();
    function->arity = r.readUint8();

    // code
    uint32_t codeSize = r.readUint32();
    function->chunk.code.assign(r.data.begin() + r.position, r.data.begin() + r.position + codeSize);
    r.position += codeSize;

    // lines
    auto& lines = function->chunk.lines;
    while(lines.size() < codeSize) {
        uint32_t line  = r.readUint32();
        uint32_t count = r.readUint32();
        lines.insert(lines.end(), count, line);
    }

    // constants
    uint16_t constants = r.readUint16();
    for(uint16_t i = 0; i < constants; ++i) {
        function->chunk.constants.push_back(deserializeValue(r));
    }

    return function;

}

ObjFunction* deserializeApp(vector<uint8_t> buffer) {

    Reader r;
    r.data = buffer;

    // header
    if(r.data.size() < 8 || memcmp(r.data.data(), MAGIC_NUMBER, 6) != 0) {
        cliError("This isn't a .reti file.");
        exit(1);
    }

    r.position = 6;
    uint16_t version = r.readUint16();
    if(version != BYTECODE_FORMAT) {
        cliError("This .reti file was compiled in a very different version, rendering it incompatible with this version of Carotene.");
        exit(1);
    }

    GCPause pause;
    ObjFunction* function = deserializeFunction(r);

    if(r.position != r.data.size()) {
        cliError("There's trailing data after the bytecode.");    
        exit(1);
    }

    return function;

}
