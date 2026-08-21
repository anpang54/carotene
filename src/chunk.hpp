
#pragma once


// includes

#include "value.hpp"


// opcodes

enum OpCode{
    OP_CONSTANT,
    OP_RETURN,
};


// chunks

class Chunk{

    public:
        

        // variables

        vector<uint8_t> code;
        vector<uint>    lines;
        vector<double>  constants;
            // crafting interpreters uses custom dynamic arrays because it's C but here we just use vectors


        // adding stuff

        void write(int opcode, unsigned int line) {
            this->code.push_back(opcode);
            this->lines.push_back(line);
        }

        int addConstant(double value) {
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
            cout << format("{:<16} {:4d}", name, offset) << " '";
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

