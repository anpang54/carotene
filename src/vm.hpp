
#pragma once


// includes

#include "common.hpp"
#include "chunk.hpp"
#include "compiler.hpp"


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

        InterpretResult interpret(string source) {

            Chunk chunkToInterpret;

            // compile error
            Compiler compiler;
            if(!compiler.compile(source, &chunkToInterpret)) {
                return INTERPRET_COMPILE_ERROR;
            }

            this->chunk = &chunkToInterpret;
            this->ip = this->chunk->code.data();

            InterpretResult result = run();
            return result;

        }

        uint8_t readByte() {
            return *this->ip++;
        }

        void binaryOperation(char operation) {

            // get a and b
            double b = this->stack.back();
            this->stack.pop_back();
            double a = this->stack.back();
            this->stack.pop_back();
            
            // do the operation
            double result;
            switch(operation) {
                case '+': result = a + b;           break;
                case '-': result = a - b;           break;
                case '*': result = a * b;           break;
                case '/': result = a / b;           break;
                case '%': result = (int)a % (int)b; break;
                case '^': result = std::pow(a, b);  break;
            }
            this->stack.push_back(result);

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

                    case OP_ADD:          { binaryOperation('+'); break; }
                    case OP_SUBTRACT:     { binaryOperation('-'); break; }
                    case OP_MULTIPLY:     { binaryOperation('*'); break; }
                    case OP_DIVIDE:       { binaryOperation('/'); break; }
                    case OP_MODULO:       { binaryOperation('%'); break; }
                    case OP_EXPONENTIATE: { binaryOperation('^'); break; }

                    case OP_NEGATE: {
                        this->stack.back() = -this->stack.back();
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