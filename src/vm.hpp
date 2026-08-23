
#pragma once


// includes

#include <cstdarg>
#include <unordered_map>

#include "common.hpp"
#include "chunk.hpp"
#include "object.hpp"
#include "compiler.hpp"

using std::unordered_map;


// setup

enum InterpretResult{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

struct CallFrame{
    ObjFunction* function;
    uint8_t* ip;
    size_t slots;
};
    // represents a single ongoing function call


// vm

class VM{

    public:

    
        vector<CallFrame> frames;
        vector<Value> stack;    // it's easier to iterate over a vector
        unordered_map<string, Value> globals;
    
        CallFrame* frame = nullptr;
        

        // interpret

        InterpretResult interpret(string source) {

            Compiler compiler;
            ObjFunction* function = compiler.compile(source);
            if(function == NULL) return INTERPRET_COMPILE_ERROR;

            this->stack.push_back(CaroObj(function));
            this->frames.push_back(CallFrame());
            frame = &this->frames.back();
            frame->function = function;
            frame->ip = function->chunk.code.data();
            frame->slots = this->stack.size() - 1;

            return run();

        }


        // helpers

        uint8_t readByte() {
            return *frame->ip++;
        }
        uint16_t readShort() {
            return (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]));
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

            CallFrame* frame = &this->frames.back();
            size_t instruction = frame->ip - frame->function->chunk.code.data() - 1;
            int line = frame->function->chunk.lines[instruction];

            cerr << "[line " << line << "] in script\n";
            this->stack.clear();

        }


        // operators

        InterpretResult unaryOperation() {
            if(!isNumeric(peek(0).type)) {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            this->stack.back() = CaroNumber(peek(0).type, -asNumberToDouble(this->stack.back()));
            return INTERPRET_OK;
        }

        InterpretResult numberBinaryOperation(OpCode op) {

            // check type
            if(!isNumeric(peek(0).type) || !isNumeric(peek(1).type)) {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }
            if(peek(0).type != peek(1).type) {
                runtimeError("Arithmetic operations between numbers of different types currently aren't supported yet.");
                    // todo: support
                return INTERPRET_RUNTIME_ERROR;
            }
            const ValueType type = peek(0).type;

            // get a and b
            double b = asNumberToDouble(this->stack.back());
            this->stack.pop_back();
            double a = asNumberToDouble(this->stack.back());
            this->stack.pop_back();
            
            // check divisiom by zero
            if((op == OP_DIVIDE || op == OP_MODULO) && b == 0) {
                runtimeError("Division by zero.");
                return INTERPRET_COMPILE_ERROR;
            }

            // do the operation
            Value result;
            switch(op) {

                case OP_ADD:           result = CaroNumber(type, a + b);                          break;
                case OP_SUBTRACT:      result = CaroNumber(type, a - b);                          break;
                case OP_MULTIPLY:      result = CaroNumber(type, a * b);                          break;
                case OP_DIVIDE:        result = CaroNumber(type, a / b);                          break;
                case OP_MODULO:        result = CaroNumber(type, (double)((int)a % (int)b));      break;
                case OP_EXPONENTIATE:  result = CaroNumber(type, std::pow(a, b));                 break;

                case OP_LESS:          result = CaroBool  (a < b);                                break;
                case OP_LESS_EQUAL:    result = CaroBool  (a <= b);                               break;
                case OP_GREATER:       result = CaroBool  (a > b);                                break;
                case OP_GREATER_EQUAL: result = CaroBool  (a >= b);                               break;
                case OP_SPACESHIP:     result = CaroInt   (a < b? -1.0: (a > b? 1.0: 0.0));       break;
                                                        // todo: remove decimals when adding ints

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

                if(isNumeric(peek(0).type)) {    // number is on the right
                    multiplier = (int)asNumberToDouble(this->stack.back());
                    this->stack.pop_back();
                    strA = asString(this->stack.back())->str;
                    this->stack.pop_back();
                } else if(isNumeric(peek(1).type)) {    // number is on the left
                    strA = asString(this->stack.back())->str;
                    this->stack.pop_back();
                    multiplier = (int)asNumberToDouble(this->stack.back());
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

            frame = &this->frames.back();

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

                    frame->function->chunk.disassembleInstruction((int)(frame->ip - frame->function->chunk.code.data()));

                }

                // run the instruction
                OpCode instruction;
                switch(instruction = (OpCode)readByte()) {

                    case OP_CONSTANT: {
                        Value constant = frame->function->chunk.constants[readByte()];
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
                        } else if(isNumeric(peek(0).type) && isNumeric(peek(1).type)) {
                            numberBinary(instruction);
                            break;
                        } else {
                            runtimeError("Operands must be numbers or strings.");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                    case OP_MULTIPLY: {
                        if(
                            (isNumeric(peek(0).type) && isString(peek(1))) ||
                            (isNumeric(peek(1).type) && isString(peek(0)))
                        ) {
                            stringBinary(instruction);
                            break;
                        } else if(isNumeric(peek(0).type) && isNumeric(peek(1).type)) {
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
                    case OP_SPACESHIP:     { numberBinary(OP_SPACESHIP);     break; }

                    case OP_NULL:  this->stack.push_back(CaroNull);        break;
                    case OP_SMTH:  this->stack.push_back(CaroSmth);        break;
                    case OP_TRUE:  this->stack.push_back(CaroBool(true));  break;
                    case OP_FALSE: this->stack.push_back(CaroBool(false)); break;

                    case OP_DEFINE_GLOBAL: {
                        ObjString* name = asString(frame->function->chunk.constants[readByte()]);
                        this->globals[name->str] = peek(0);
                        this->stack.pop_back();
                        break;
                    }
                    case OP_GET_GLOBAL: {
                        ObjString* name = asString(frame->function->chunk.constants[readByte()]);
                        auto found = this->globals.find(name->str);
                        if(found == this->globals.end()) {
                            runtimeError("Undefined variable '%s'.", name->str.c_str());
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        this->stack.push_back(found->second);
                        break;
                    }
                    case OP_SET_GLOBAL: {
                        ObjString* name = asString(frame->function->chunk.constants[readByte()]);
                        auto found = this->globals.find(name->str);
                        if(found == this->globals.end()) {
                            runtimeError("Undefined variable '%s'.", name->str.c_str());
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        found->second = peek(0);
                        break;
                    }

                    case OP_GET_LOCAL: {
                        uint8_t slot = readByte();
                        this->stack.push_back(this->stack[frame->slots + slot]);
                        break;
                    }
                    case OP_SET_LOCAL: {
                        uint8_t slot = readByte();
                        this->stack[frame->slots + slot] = peek(0);
                        break;
                    }

                    case OP_PRINT: {
                        printValue(this->stack.back());
                        this->stack.pop_back();
                        cout << '\n';
                        break;
                    }
                    case OP_TYPEOF: {
                        this->stack.back() = CaroObj(copyString(typeof(this->stack.back())));
                        break;
                    }

                    case OP_JUMP: {
                        uint16_t offset = readShort();
                        frame->ip += offset;
                        break;
                    }
                    case OP_JUMP_IF_FALSE: {
                        uint16_t offset = readShort();
                        if(isFalsey(peek(0))) frame->ip += offset;
                        break;
                    }
                    case OP_LOOP: {
                        uint16_t offset = readShort();
                        frame->ip -= offset;
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