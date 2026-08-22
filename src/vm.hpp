
#pragma once


// includes

#include <cstdarg>
#include <unordered_map>

#include "common.hpp"
#include "chunk.hpp"
#include "compiler.hpp"

using std::unordered_map;


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

        unordered_map<string, Value> globals;
    

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

        InterpretResult numberBinaryOperation(OpCode op) {

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

        InterpretResult stringBinaryOperation(OpCode op) {

            // get a and b
            string strA, strB;
            int multiplier;
            if(op == OP_MULTIPLY) {

                if(peek(0).type == TYPE_NUMBER) {    // number is on the right
                    multiplier = (int)this->stack.back().as.number;
                    this->stack.pop_back();
                    strA = asString(this->stack.back())->str;
                    this->stack.pop_back();
                } else if(peek(1).type == TYPE_NUMBER) {    // number is on the left
                    strA = asString(this->stack.back())->str;
                    this->stack.pop_back();
                    multiplier = (int)this->stack.back().as.number;
                    this->stack.pop_back();
                } else {
                    return INTERPRET_RUNTIME_ERROR;
                }

            } else {

                strB = asString(this->stack.back()) -> str;
                this->stack.pop_back();
                strA = asString(this->stack.back()) -> str;
                this->stack.pop_back();

            }

            // do the operation
            string result;
            switch(op) {

                case OP_ADD: {    // concatenates a and b
                    result = strA + strB;
                    break;
                }
                case OP_SUBTRACT: {    // removes all occurrences of b in a
                    result = strA;
                    replace(result, strB, "");
                    break;
                }

                case OP_MULTIPLY: {    // duplicates a b times
                    if(multiplier < 0) {
                        runtimeError("Strings can only be duplicated a positive amount of times.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    result.reserve(strA.length() * multiplier);
                    for(int i = 0; i < multiplier; ++i) {
                        result += strA;
                    }
                    break;
                }

                default: break;
                    
            }
            
            this->stack.push_back(CaroObj(copyString(result)));

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
                OpCode instruction;
                switch(instruction = (OpCode)readByte()) {

                    case OP_CONSTANT: {
                        Value constant = this->chunk->constants[readByte()];
                        this->stack.push_back(constant);
                        break;
                    }

                    #define unary()          if(unaryOperation()          == INTERPRET_OK) { break; } else { return INTERPRET_RUNTIME_ERROR; }
                    #define numberBinary(op) if(numberBinaryOperation(op) == INTERPRET_OK) { break; } else { return INTERPRET_RUNTIME_ERROR; }
                    #define stringBinary(op) if(stringBinaryOperation(op) == INTERPRET_OK) { break; } else { return INTERPRET_RUNTIME_ERROR; }
                    
                    case OP_ADD: case OP_SUBTRACT: {
                        if(isString(peek(0)) && isString(peek(1))) {
                            stringBinary(instruction);
                            break;
                        } else if(peek(0).type == TYPE_NUMBER && peek(1).type == TYPE_NUMBER) {
                            numberBinary(instruction);
                            break;
                        } else {
                            runtimeError("Operands must be numbers or strings.");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                    case OP_MULTIPLY: {
                        if(
                            (peek(0).type == TYPE_NUMBER && isString(peek(1))) ||
                            (peek(1).type == TYPE_NUMBER && isString(peek(0)))
                        ) {
                            stringBinary(instruction);
                            break;
                        } else if(peek(0).type == TYPE_NUMBER && peek(1).type == TYPE_NUMBER) {
                            numberBinary(instruction);
                            break;
                        } else {
                            runtimeError("Operands must be numbers or strings.");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                    case OP_DIVIDE:       { numberBinary(OP_DIVIDE);       }
                    case OP_MODULO:       { numberBinary(OP_MODULO);       }
                    case OP_EXPONENTIATE: { numberBinary(OP_EXPONENTIATE); }

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
                    case OP_LESS:          { numberBinary(OP_LESS);          break; }
                    case OP_LESS_EQUAL:    { numberBinary(OP_LESS_EQUAL);    break; }
                    case OP_GREATER:       { numberBinary(OP_GREATER);       break; }
                    case OP_GREATER_EQUAL: { numberBinary(OP_GREATER_EQUAL); break; }
                    
                    case OP_NULL:  this->stack.push_back(CaroNull);        break;
                    case OP_TRUE:  this->stack.push_back(CaroBool(true));  break;
                    case OP_FALSE: this->stack.push_back(CaroBool(false)); break;

                    case OP_DEFINE_GLOBAL: {
                        ObjString* name = asString(this->chunk->constants[readByte()]);
                        this->globals[name->str] = peek(0);
                        this->stack.pop_back();
                        break;
                    }
                    case OP_GET_GLOBAL: {
                        ObjString* name = asString(this->chunk->constants[readByte()]);
                        auto found = this->globals.find(name->str);
                        if(found == this->globals.end()) {
                            runtimeError("Undefined variable '%s'.", name->str.c_str());
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        this->stack.push_back(found->second);
                        break;
                    }
                    case OP_SET_GLOBAL: {
                        ObjString* name = asString(this->chunk->constants[readByte()]);
                        auto found = this->globals.find(name->str);
                        if(found == this->globals.end()) {
                            runtimeError("Undefined variable '%s'.", name->str.c_str());
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        found->second = peek(0);
                        break;
                    }

                    case OP_PRINT: {
                        printValue(this->stack.back());
                        this->stack.pop_back();
                        cout << '\n';
                        break;
                    }

                    case OP_POP: {
                        this->stack.pop_back();
                        break;
                    }
                    case OP_RETURN: {
                        return INTERPRET_OK;
                    }

                }

            }

        }


};