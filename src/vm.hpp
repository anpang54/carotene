
#pragma once


// includes

#include <cstdarg>

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

    
        Chunk*        chunk;
        uint8_t*      ip;
        vector<Value> stack;    // it's easier to iterate over a vector


        // interpret

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


        // helpers

        uint8_t readByte() {
            return *this->ip++;
        }
        Value peek(int distance) {
            return this->stack[this->stack.size() - 1 - distance];
        }

        bool valuesEqual(Value a, Value b) {
            if (a.type != b.type) return false;
            switch(a.type) {
                case TYPE_BOOL:   return a.as.boolean == b.as.boolean;
                case TYPE_NULL:   return true;
                case TYPE_NUMBER: return a.as.number == b.as.number;
                default:          return false;    // unreachable
            }
        }

        bool isFalsey(Value value) {
            return value.type == TYPE_NULL || (value.type == TYPE_BOOL && !value.as.boolean);
        }

        void runtimeError(const char* format, ...) {

            va_list args;
            va_start(args, format);
            vfprintf(stderr, format, args);
            va_end(args);
            cerr << '\n';

            size_t instruction = this->ip - this->chunk->code.data() - 1;
            int line = this->chunk->lines[instruction];
            cerr << "[line " << line << "] in script\n";
            this->stack.clear();

        }


        // operators

        InterpretResult unaryOperation() {
            if(peek(0).type != TYPE_NUMBER) {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            this->stack.back() = CaroNumber(-this->stack.back().as.number);
            return INTERPRET_OK;
        }

        InterpretResult binaryOperation(OpCode op) {

            // check type
            if(peek(0).type != TYPE_NUMBER || peek(1).type != TYPE_NUMBER) {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }

            // get a and b
            double b = this->stack.back().as.number;
            this->stack.pop_back();
            double a = this->stack.back().as.number;
            this->stack.pop_back();
            
            // do the operation
            Value result;
            switch(op) {

                case OP_ADD:           result = CaroNumber(a + b);                     break;
                case OP_SUBTRACT:      result = CaroNumber(a - b);                     break;
                case OP_MULTIPLY:      result = CaroNumber(a * b);                     break;
                case OP_DIVIDE:        result = CaroNumber(a / b);                     break;
                case OP_MODULO:        result = CaroNumber((double)((int)a % (int)b)); break;
                case OP_EXPONENTIATE:  result = CaroNumber(std::pow(a, b));            break;

                case OP_LESS:          result = CaroBool(a < b);                       break;
                case OP_LESS_EQUAL:    result = CaroBool(a <= b);                      break;
                case OP_GREATER:       result = CaroBool(a > b);                       break;
                case OP_GREATER_EQUAL: result = CaroBool(a >= b);                      break;
            
                default: break;

            }
            this->stack.push_back(result);

            return INTERPRET_OK;

        }


        // run

        InterpretResult run() {

            for (;;){

                // debug trace execution
                if(DEBUG_TRACE_EXECUTION) {
                    
                    cout << "          ";
                    for(const Value& slot: stack) {
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
                        Value constant = this->chunk->constants[readByte()];
                        this->stack.push_back(constant);
                        break;
                    }

                    #define unary()    if(unaryOperation()    == INTERPRET_OK) { break; } else { return INTERPRET_RUNTIME_ERROR; }
                    #define binary(op) if(binaryOperation(op) == INTERPRET_OK) { break; } else { return INTERPRET_RUNTIME_ERROR; }
                    
                    case OP_ADD:          { binary(OP_ADD);          }
                    case OP_SUBTRACT:     { binary(OP_SUBTRACT);     }
                    case OP_MULTIPLY:     { binary(OP_MULTIPLY);     }
                    case OP_DIVIDE:       { binary(OP_DIVIDE);       }
                    case OP_MODULO:       { binary(OP_MODULO);       }
                    case OP_EXPONENTIATE: { binary(OP_EXPONENTIATE); }

                    case OP_NEGATE:       { unary();     }

                    case OP_NOT:
                        this->stack.back() = CaroBool(isFalsey(this->stack.back()));
                        break;

                    case OP_EQUAL: {
                        Value b = this->stack.back();
                        this->stack.pop_back();
                        Value a = this->stack.back();
                        this->stack.pop_back();
                        this->stack.push_back(CaroBool(valuesEqual(a, b)));
                        break;
                    }
                    case OP_NOT_EQUAL: {
                        Value b = this->stack.back();
                        this->stack.pop_back();
                        Value a = this->stack.back();
                        this->stack.pop_back();
                        this->stack.push_back(CaroBool(!valuesEqual(a, b)));
                        break;
                    }
                    case OP_LESS:          { binary(OP_LESS);          }
                    case OP_LESS_EQUAL:    { binary(OP_LESS_EQUAL);    }
                    case OP_GREATER:       { binary(OP_GREATER);       }
                    case OP_GREATER_EQUAL: { binary(OP_GREATER_EQUAL); }
                    
                    case OP_NULL:  this->stack.push_back(CaroNull);        break;
                    case OP_TRUE:  this->stack.push_back(CaroBool(true));  break;
                    case OP_FALSE: this->stack.push_back(CaroBool(false)); break;

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