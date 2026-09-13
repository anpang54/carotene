
#pragma once


// INCLUDES

#include "value.hpp"


// OPCODES

enum OpCode: uint8_t{

    /*
        each opcode has a specified value for bytecode compatibility
        0x00 is intentionally left unused because nulls are common in corrupted data
        the last value is 0xff so that the jump table a switch on this enum turns into takes up every byte
    */

    // app info
    OP_NAME                      = 0x01,
    OP_DESC                      = 0x02,
    OP_VERSION                   = 0x03,

    // values
    OP_CONSTANT                  = 0x10,
    OP_NULL                      = 0x11,
    OP_SMTH                      = 0x12,
    OP_TRUE                      = 0x13,
    OP_FALSE                     = 0x14,
    OP_INTERPOLATE               = 0x15,

    // arithmetic
    OP_ADD                       = 0x20,
    OP_SUBTRACT                  = 0x21,
    OP_MULTIPLY                  = 0x22,
    OP_DIVIDE                    = 0x23,
    OP_NEGATE                    = 0x24,
    OP_MODULO                    = 0x25,
    OP_EXPONENTIATE              = 0x26,
    
    // logic/bitwise
    OP_NOT                       = 0x30,

    // comparison
    OP_EQUAL                     = 0x41,
    OP_NOT_EQUAL                 = 0x42,
    OP_LESS                      = 0x43,
    OP_LESS_EQUAL                = 0x44,
    OP_GREATER                   = 0x45,
    OP_GREATER_EQUAL             = 0x46,
    OP_SPACESHIP                 = 0x47,

    // globals
    OP_DEFINE_GLOBAL             = 0x50,
    OP_DEFINE_CONSTANT           = 0x51,
    OP_GET_GLOBAL                = 0x52,
    OP_SET_GLOBAL                = 0x53,
    OP_INCREMENT_GLOBAL          = 0x54,
    OP_DECREMENT_GLOBAL          = 0x55,

    // locals (globals + 0x08)
    OP_GET_LOCAL                 = 0x5A,
    OP_SET_LOCAL                 = 0x5B,
    OP_INCREMENT_LOCAL           = 0x5C,
    OP_DECREMENT_LOCAL           = 0x5D,

    // collections
    OP_MAKE_ARRAY                = 0x60,
    OP_MAKE_DICT                 = 0x61,
    OP_MAKE_SET                  = 0x62,
    OP_GET_INDEX                 = 0x63,
    OP_SET_INDEX                 = 0x64,
    OP_DUPLICATE_INDEX           = 0x65,

    // functions
    OP_CALL                      = 0x70,
    OP_RETURN                    = 0x71,

    // specific functions
    OP_TYPEOF                    = 0x78,
    OP_SIZEOF                    = 0x79,

    // classes
    OP_CLASS                     = 0x80,
    OP_GET_PROPERTY              = 0x81,
    OP_SET_PROPERTY              = 0x82,
    OP_GET_MEMBER                = 0x83,
    OP_SET_MEMBER                = 0x84,
    OP_METHOD                    = 0x85,
    OP_INVOKE                    = 0x86,

    // jump (comparison + 0x50)
    OP_JUMP                      = 0x90,
    OP_JUMP_IF_NOT_EQUAL         = 0x91,
    OP_JUMP_IF_EQUAL             = 0x92,
    OP_JUMP_IF_NOT_LESS          = 0x93,
    OP_JUMP_IF_NOT_LESS_EQUAL    = 0x94,
    OP_JUMP_IF_NOT_GREATER       = 0x95,
    OP_JUMP_IF_NOT_GREATER_EQUAL = 0x96,
    OP_JUMP_IF_FALSE             = 0x98,

    // loops
    OP_LOOP                      = 0xA0,
    OP_FOR_LOOP                  = 0xA1,

    // stack
    OP_POP                       = 0xFD,
    OP_COPY                      = 0xFE,
    OP_DUPLICATE                 = 0xFF,

};


// CHUNKS

class Chunk{

    public:
        

        // variables

        vector<uint8_t> code;
        vector<uint>    lines;
        vector<Value>  constants;
            // crafting interpreters uses custom dynamic arrays because it's C but here we just use vectors

        vector<Value*> globalCache;

        // adding stuff

        void write(int opcode, unsigned int line) {
            this->code.push_back(opcode);
            this->lines.push_back(line);
        }

        int addConstant(Value value) {
            this->constants.push_back(value);
            this->globalCache.push_back(nullptr);
            return this->constants.size() - 1;    // return new constant's index
        }


        // disassembly

        int simpleInstruction(string name, int offset) {
            cout << name << '\n';
            return offset + 1;
        }
        int constantInstruction(string name, int offset) {
            uint8_t constant = this->code[offset + 1];
            cout << format("{:<16} {:4d} '", name, constant) << printValue(this->constants[constant]) << "'\n";
            return offset + 2;
        }
        int byteInstruction(string name, int offset) {
            uint8_t slot = this->code[offset + 1];
            cout << format("{:<16} {:4d}\n", name, slot);
            return offset + 2; 
        }
        int memberInstruction(string name, int offset) {
            uint8_t component = this->code[offset + 1];
            uint8_t constant = this->code[offset + 2];
            cout << format("{:<16} {:4d} {:4d} '", name, component, constant) << printValue(this->constants[constant]) << "'\n";
            return offset + 3;
        }
        int invokeInstruction(string name, int offset) {
            uint8_t constant = this->code[offset + 1];
            uint8_t argCount = this->code[offset + 2];
            cout << format("{:<16} ({} args) {:4d} '", name, argCount, constant) << printValue(this->constants[constant]) << "'\n";
            return offset + 3;
        }
        int setMemberInstruction(string name, int offset) {
            uint8_t component = this->code[offset + 1];
            uint8_t constant = this->code[offset + 2];
            uint8_t setOp = this->code[offset + 3];
            uint8_t arg = this->code[offset + 4];
            cout << format("{:<16} {:4d} {:4d} '", name, component, constant) << printValue(this->constants[constant]);
            cout << format("' -> {:d} {:4d}\n", setOp, arg);
            return offset + 5;
        }
        int incrementInstruction(string name, bool global, int offset) {
            uint8_t variable = this->code[offset + 1];
            uint8_t step = this->code[offset + 2];
            cout << format("{:<16} {:4d} '", name, variable);
            if(global) {
                cout << printValue(this->constants[variable]) << "' + '";
            }
            cout << printValue(this->constants[step]) << "'\n";
            return offset + 3;
        }
        int jumpInstruction(string name, int sign, int offset) {
            uint16_t jump = (uint16_t)(this->code[offset + 1] << 8);
            jump |= this->code[offset + 2];
            cout << format("{:<16} {:4d} -> {:d}\n", name, offset, offset + 3 + (sign * jump));
            return offset + 3;
        }
        int forLoopInstruction(string name, int offset) {
            uint8_t counter = this->code[offset + 1];
            uint8_t limit = this->code[offset + 2];
            uint8_t step = this->code[offset + 3];
            uint16_t jump = (uint16_t)(this->code[offset + 4] << 8);
            jump |= this->code[offset + 5];
            cout << format("{:<16} {:4d} {:4d} {:4d} -> {:d}\n", name, counter, limit, step, offset + 6 - jump);
            return offset + 6;
        }

        int disassembleInstruction(int offset) {

            cout << format("{:04} ", offset);

            if(offset > 0 && this->lines[offset] == this->lines[offset - 1]) {
                cout << "   | ";
            } else {
                cout << format("{:04} ", this->lines[offset]);
            }

            uint8_t instruction = this->code[offset];

            switch(instruction) {

                case OP_NAME:
                    return constantInstruction("OP_NAME", offset);
                case OP_DESC:
                    return constantInstruction("OP_DESC", offset);
                case OP_VERSION:
                    return constantInstruction("OP_VERSION", offset);

                case OP_CONSTANT:
                    return constantInstruction("OP_CONSTANT", offset);

                case OP_ADD:
                    return simpleInstruction("OP_ADD", offset);
                case OP_SUBTRACT:
                    return simpleInstruction("OP_SUBTRACT", offset);
                case OP_MULTIPLY:
                    return simpleInstruction("OP_MULTIPLY", offset);
                case OP_DIVIDE:
                    return simpleInstruction("OP_DIVIDE", offset);
                case OP_MODULO:
                    return simpleInstruction("OP_MODULO", offset);
                case OP_EXPONENTIATE:
                    return simpleInstruction("OP_EXPONENTIATE", offset);

                case OP_NEGATE:
                    return simpleInstruction("OP_NEGATE", offset);

                case OP_NOT:
                    return simpleInstruction("OP_NOT", offset);

                case OP_EQUAL:
                    return simpleInstruction("OP_EQUAL", offset);
                case OP_NOT_EQUAL:
                    return simpleInstruction("OP_NOT_EQUAL", offset);
                case OP_LESS:
                    return simpleInstruction("OP_LESS", offset);
                case OP_LESS_EQUAL:
                    return simpleInstruction("OP_LESS_EQUAL", offset);
                case OP_GREATER:
                    return simpleInstruction("OP_GREATER", offset);
                case OP_GREATER_EQUAL:
                    return simpleInstruction("OP_GREATER_EQUAL", offset);
                case OP_SPACESHIP:
                    return simpleInstruction("OP_SPACEASHIP", offset);
                    
                case OP_NULL:
                    return simpleInstruction("OP_NULL", offset);
                case OP_TRUE:
                    return simpleInstruction("OP_TRUE", offset);
                case OP_FALSE:
                    return simpleInstruction("OP_FALSE", offset);
                case OP_INTERPOLATE:
                    return byteInstruction("OP_INTERPOLATE", offset);
                    
                case OP_DEFINE_GLOBAL:
                    return constantInstruction("OP_DEFINE_GLOBAL", offset);
                case OP_DEFINE_CONSTANT:
                    return constantInstruction("OP_DEFINE_CONSTANT", offset);
                
                case OP_GET_GLOBAL:
                    return constantInstruction("OP_GET_GLOBAL", offset);
                case OP_SET_GLOBAL:
                    return constantInstruction("OP_SET_GLOBAL", offset);
                case OP_INCREMENT_GLOBAL:
                    return incrementInstruction("OP_INCREMENT_GLOBAL", true, offset);
                case OP_DECREMENT_GLOBAL:
                    return incrementInstruction("OP_DECREMENT_GLOBAL", true, offset);

                case OP_GET_LOCAL:
                    return byteInstruction("OP_GET_LOCAL", offset);
                case OP_SET_LOCAL:
                    return byteInstruction("OP_SET_LOCAL", offset);
                case OP_INCREMENT_LOCAL:
                    return incrementInstruction("OP_INCREMENT_LOCAL", false, offset);
                case OP_DECREMENT_LOCAL:
                    return incrementInstruction("OP_DECREMENT_LOCAL", false, offset);
    
                case OP_MAKE_ARRAY:
                    return byteInstruction("OP_MAKE_ARRAY", offset);
                case OP_MAKE_DICT:
                    return byteInstruction("OP_MAKE_DICT", offset);
                case OP_MAKE_SET:
                    return byteInstruction("OP_MAKE_SET", offset);
                case OP_GET_INDEX:
                    return simpleInstruction("OP_GET_INDEX", offset);
                case OP_SET_INDEX:
                    return simpleInstruction("OP_SET_INDEX", offset);
                case OP_DUPLICATE_INDEX:
                    return simpleInstruction("OP_DUPLICATE_INDEX", offset);

                case OP_TYPEOF:
                    return simpleInstruction("OP_TYPEOF", offset);
                case OP_SIZEOF:
                    return simpleInstruction("OP_SIZEOF", offset);

                case OP_CALL:
                    return byteInstruction("OP_CALL", offset);
                case OP_RETURN:
                    return simpleInstruction("OP_RETURN", offset);

                case OP_CLASS:
                    return constantInstruction("OP_CLASS", offset);
                case OP_GET_PROPERTY:
                    return constantInstruction("OP_GET_PROPERTY", offset);
                case OP_SET_PROPERTY:
                    return constantInstruction("OP_SET_PROPERTY", offset);
                case OP_GET_MEMBER:
                    return memberInstruction("OP_GET_MEMBER", offset);
                case OP_SET_MEMBER:
                    return setMemberInstruction("OP_SET_MEMBER", offset);
                case OP_METHOD:
                    return constantInstruction("OP_METHOD", offset);
                case OP_INVOKE:
                    return invokeInstruction("OP_INVOKE", offset);

                case OP_JUMP:
                    return jumpInstruction("OP_JUMP", 1, offset);
                case OP_JUMP_IF_EQUAL:
                    return jumpInstruction("OP_JUMP_IF_EQUAL", 1, offset);
                case OP_JUMP_IF_FALSE:
                    return jumpInstruction("OP_JUMP_IF_FALSE", 1, offset);
                case OP_JUMP_IF_NOT_LESS:
                    return jumpInstruction("OP_JUMP_IF_NOT_LESS", 1, offset);
                case OP_JUMP_IF_NOT_LESS_EQUAL:
                    return jumpInstruction("OP_JUMP_IF_NOT_LESS_EQUAL", 1, offset);
                case OP_JUMP_IF_NOT_GREATER:
                    return jumpInstruction("OP_JUMP_IF_NOT_GREATER", 1, offset);
                case OP_JUMP_IF_NOT_GREATER_EQUAL:
                    return jumpInstruction("OP_JUMP_IF_NOT_GREATER_EQUAL", 1, offset);
                case OP_JUMP_IF_NOT_EQUAL:
                    return jumpInstruction("OP_JUMP_IF_NOT_EQUAL", 1, offset);

                case OP_LOOP:
                    return jumpInstruction("OP_LOOP", -1, offset);
                case OP_FOR_LOOP:
                    return forLoopInstruction("OP_FOR_LOOP", offset);

                case OP_POP:
                    return simpleInstruction("OP_POP", offset);
                case OP_COPY:
                    return simpleInstruction("OP_COPY", offset);
                case OP_DUPLICATE:
                    return simpleInstruction("OP_DUPLICATE", offset);

                default:
                    cout << "Unknown opcode " << instruction << '\n';
                    return offset + 1;

            }

        }

        void disassemble(string name) {
            cout << "== " << name << " ==\n";
            for(int offset = 0; offset < this->code.size();) {
                offset = disassembleInstruction(offset);
            }
        }


};
