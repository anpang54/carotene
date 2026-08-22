
#pragma once


// includes

#include "value.hpp"


// opcodes

enum OpCode{

    // values
    OP_CONSTANT,
    OP_NULL,
    OP_SMTH,
    OP_TRUE,
    OP_FALSE,

    // arithmetic
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,
    OP_MODULO,
    OP_EXPONENTIATE,

    // logic
    OP_NOT,
    
    // comparison
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,

    // variables
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,

    // functions
    OP_PRINT,

    // misc
    OP_POP,
    OP_RETURN,

};


// chunks

class Chunk{

    public:
        

        // variables

        vector<uint8_t> code;
        vector<uint>    lines;
        vector<Value>  constants;
            // crafting interpreters uses custom dynamic arrays because it's C but here we just use vectors


        // adding stuff

        void write(int opcode, unsigned int line) {
            this->code.push_back(opcode);
            this->lines.push_back(line);
        }

        int addConstant(Value value) {
            this->constants.push_back(value);
            return this->constants.size() - 1;    // return new constant's index
        }


        // disassembly

        int simpleInstruction(const char* name, int offset) {
            cout << name << '\n';
            return offset + 1;
        }
        int constantInstruction(const char* name, int offset) {
            uint8_t constant = this->code[offset + 1];
            cout << format("{:<16} {:4d}", name, constant) << " '";
            printValue(this->constants[constant]);
            cout << "'\n";
            return offset + 2;
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

                case OP_NULL:
                    return simpleInstruction("OP_NULL", offset);
                case OP_TRUE:
                    return simpleInstruction("OP_TRUE", offset);
                case OP_FALSE:
                    return simpleInstruction("OP_FALSE", offset);

                case OP_DEFINE_GLOBAL:
                    return constantInstruction("OP_DEFINE_GLOBAL", offset);
                case OP_GET_GLOBAL:
                    return constantInstruction("OP_GET_GLOBAL", offset);
                case OP_SET_GLOBAL:
                    return constantInstruction("OP_SET_GLOBAL", offset);

                case OP_PRINT:
                    return simpleInstruction("OP_PRINT", offset);

                case OP_POP:
                    return simpleInstruction("OP_POP", offset);
                case OP_RETURN:
                    return simpleInstruction("OP_RETURN", offset);

                default:
                    cout << "Unknown opcode " << instruction << '\n';
                    return offset + 1;

            }

        }

        void disassemble(const char* name) {
            cout << "== " << name << " ==\n";
            for(int offset = 0; offset < this->code.size();) {
                offset = disassembleInstruction(offset);
            }
        }


};

