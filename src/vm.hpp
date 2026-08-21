
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

        Chunk*         chunk;
        uint8_t*       ip;
        vector<double> stack;    // it's easier to iterate over a vector

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

                // debug trace execution
                if(DEBUG_TRACE_EXECUTION) {
                    
                    cout << "          ";
                    for(const double& slot: stack) {
                        cout << "[ ";
                        printValue(slot);
                        cout << " ]";
                    }
                    cout << '\n';

                    this->chunk->disassembleInstruction((int)(this->ip - this->chunk->code.data()));

                }

                // run the instruction
                uint8_t instruction;
                switch(instruction = readByte()) {

                    case OP_CONSTANT: {
                        double constant = this->chunk->constants[readByte()];
                        this->stack.push_back(constant);
                        break;
                    }

                    case OP_RETURN: {
                        printValue(this->stack.back());
                        this->stack.pop_back();    // apparently pop_back() doesn't return the value
                        cout << '\n';
                        return INTERPRET_OK;
                    }

                }

            }

        }


};