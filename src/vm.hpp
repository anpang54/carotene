
#pragma once


// includes

#include "common.hpp"
#include "chunk.hpp"


// interpret results

enum InterpretResult{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};


// vm

class VM{

    public:

        Chunk* chunk;
        uint8_t* ip;

        InterpretResult interpret(Chunk* chunk) {
            this->chunk = chunk;
            this->ip = this->chunk->code.data();    // ip = instruction pointer
            return run();
        }

        uint8_t readByte() {
            return *this->ip++;
        }

        InterpretResult run() {

            for (;;){

                if(DEBUG_TRACE_EXECUTION) {
                    this->chunk->disassembleInstruction((int)(this->ip - this->chunk->code.data()));
                }

                uint8_t instruction;
                switch(instruction = readByte()) {

                    case OP_CONSTANT: {
                        double constant = this->chunk->constants[readByte()];
                        printValue(constant);
                        cout << '\n';
                        break;
                    }

                    case OP_RETURN: {
                        return INTERPRET_OK;
                    }

                }

            }

        }

};