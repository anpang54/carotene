
#pragma once


// includes

#include <array>
#include <unordered_map>
#include <utility>
#include <algorithm>

#include <cstdarg>
#include <ctime>

#include "common.hpp"
#include "chunk.hpp"
#include "object.hpp"
#include "compiler.hpp"

using std::array, std::pair, std::unordered_map;


// setup

enum InterpretResult{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

#define FRAMES_MAX  64    // there is a limit here to stop infinite recursion
#define FRAME_SLOTS 256
#define STACK_MAX   (FRAMES_MAX * FRAME_SLOTS)

struct CallFrame{
    ObjFunction* function;
    uint8_t* ip;
    Value* slots;
};
    // represents a single ongoing function call


// vm

class VM{

    public:

    
        vector<CallFrame> frames;

        array<Value, STACK_MAX> stack;    // an array is faster than a vector
        Value* stackTop = stack.data();
        
        unordered_map<string, Value> globals;
    
        CallFrame* frame = nullptr;
        bool hadError = false;

        string appName, appDesc, appVersion;
        

        // native functions

        void defineNative(string name, NativeFn function) {
            this->globals[name] = CaroObj(newNative(function));
        }


        // interpret

        InterpretResult interpret(string source) {

            Compiler compiler;
            ObjFunction* function = compiler.compile(source);
            if(function == NULL) return INTERPRET_COMPILE_ERROR;

            this->frames.reserve(FRAMES_MAX);

            // add script arguments
            this->globals["_args"] = CaroDouble(moreArguments.size());
                                  // todo: change to uint
            for(uint i = 0; i < moreArguments.size(); ++i) {
                this->globals["_" + to_string(i + 1)] = CaroObj(copyString(moreArguments[i]));
                // yes, indexes are supposed to start at 0
                // but in C argv[0] is the name of the file and argv[1] is the first argument, so we're gonna match that
            }
            
            // add native functions
            for(const pair<string, NativeFn>& native: natives) {
                defineNative(native.first, native.second);
            }

            push(CaroObj(function));
            if(!call(function, 0)) return INTERPRET_RUNTIME_ERROR;

            return run();

        }


        // helpers

        uint8_t readByte() {
            return *frame->ip++;
        }
        uint16_t readShort() {
            return (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]));
        }

        void push(Value value) {
            *this->stackTop++ = value;
        }
        Value pop() {
            return *--this->stackTop;
        }
        Value& top() {
            return this->stackTop[-1];
        }
        Value peek(int distance) {
            return this->stackTop[-1 - distance];
        }
        
        void runtimeError(const char* format, ...) {

            va_list args;
            va_start(args, format);
            vfprintf(stderr, format, args);
            va_end(args);
            cerr << '\n';

            for(int i = (int)this->frames.size() - 1; i >= 0; --i) {
                CallFrame* callFrame = &this->frames[i];
                ObjFunction* function = callFrame->function;
                size_t instruction = callFrame->ip - function->chunk.code.data() - 1;
                cerr << "[line " << function->chunk.lines[instruction] << "] in ";
                if(function->name.empty()) {
                    cerr << "script";
                } else {
                    cerr << function->name << "()";
                }
                cerr << '\n';
            }

            resetStack();
            this->hadError = true;

        }

        void resetStack() {
            this->stackTop = this->stack.data();    // clear
            this->frames.clear();
            this->frame = nullptr;
        }


        // operators

        InterpretResult unaryOperation() {
            if(!isNumeric(peek(0).type)) {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            top() = CaroNumber(peek(0).type, -asNumberTo<double>(top()));
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
            int b = asNumberTo<int>(pop());
            int a = asNumberTo<int>(pop());
            
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
                case OP_MODULO:        result = CaroNumber(type, ((int)a % (int)b));              break;
                case OP_EXPONENTIATE:  result = CaroNumber(type, std::pow(a, b));                 break;

                case OP_LESS:          result = CaroBool  (a < b);                                break;
                case OP_LESS_EQUAL:    result = CaroBool  (a <= b);                               break;
                case OP_GREATER:       result = CaroBool  (a > b);                                break;
                case OP_GREATER_EQUAL: result = CaroBool  (a >= b);                               break;
                case OP_SPACESHIP:     result = CaroInt   (a < b? -1.0: (a > b? 1.0: 0.0));       break;
                                                        // todo: remove decimals when adding ints

                default: break;

            }
            push(result);

            return INTERPRET_OK;

        }

        InterpretResult stringBinaryOperation(OpCode op) {

            // get a and b
            string strA, strB;
            int multiplier;
            if(op == OP_MULTIPLY) {

                if(isNumeric(peek(0).type)) {    // number is on the right
                    multiplier = (int)asNumberTo<double>(pop());
                    strA = asString(pop())->str;
                } else if(isNumeric(peek(1).type)) {    // number is on the left
                    strA = asString(pop())->str;
                    multiplier = (int)asNumberTo<double>(pop());
                } else {
                    return INTERPRET_RUNTIME_ERROR;
                }

            } else {

                strB = asString(pop()) -> str;
                strA = asString(pop()) -> str;

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
            
            push(CaroObj(copyString(result)));

            return INTERPRET_OK;

        }


        // calling functions

        bool call(ObjFunction* function, int argCount) {

            // check number of arguments
            if(argCount != function->arity) {
                runtimeError("Expected %d arguments but got %d.", function->arity, argCount);
                return false;
            }

            // check stack
            if(
                this->frames.size() == FRAMES_MAX
                || this->stackTop - this->stack.data() + FRAME_SLOTS > STACK_MAX
            ) {
                runtimeError("Stack overflow.");
                return false;
            }
                // user probably wrote an infinitely recursing function
            CallFrame* newFrame = &this->frames.emplace_back();
            newFrame->function = function;
            newFrame->ip = function->chunk.code.data();
            newFrame->slots = this->stackTop - argCount - 1;

            return true;

        }

        bool callValue(Value callee, int argCount) {
            if(callee.type == TYPE_OBJ) {
                switch(callee.as.obj->type) {
                    case OBJ_FUNCTION: {
                        return call(asFunction(callee), argCount);
                    }
                    case OBJ_NATIVE: {
                        NativeFn native = asNative(callee)->function;
                        this->hadError = false;
                        Value result = native(this, vector<Value>(this->stackTop - argCount, this->stackTop));
                        if(this->hadError) return false;
                        this->stackTop -= argCount + 1;
                        push(result);
                        return true;
                    }
                    default:
                        break;    // non-callable object type
                }
            }
            runtimeError("Can only call functions and classes.");
            return false;
        }


        // run

        InterpretResult run() {

            frame = &this->frames.back();

            for (;;){

                // debug trace execution
                if(DEBUG_TRACE_EXECUTION) {
                    
                    cout << "          ";
                    for(Value* slot = this->stack.data(); slot < this->stackTop; ++slot) {
                        cout << "[ ";
                        printValue(*slot);
                        cout << " ]";
                    }
                    cout << '\n';

                    frame->function->chunk.disassembleInstruction((int)(frame->ip - frame->function->chunk.code.data()));

                }

                // run the instruction
                OpCode instruction;
                switch(instruction = (OpCode)readByte()) {

                    case OP_NAME: {
                        appName = asString(frame->function->chunk.constants[readByte()])->str;
                        break;
                    }
                    case OP_DESC: {
                        appDesc = asString(frame->function->chunk.constants[readByte()])->str;
                        break;
                    }
                    case OP_VERSION: {
                        appVersion = asString(frame->function->chunk.constants[readByte()])->str;
                        break;
                    }

                    case OP_CONSTANT: {
                        Value constant = frame->function->chunk.constants[readByte()];
                        push(constant);
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
                        top() = CaroBool(isFalsey(top()));
                        break;

                    case OP_EQUAL: {
                        Value b = pop();
                        Value a = pop();
                        push(CaroBool(valuesEqual(a, b)));
                        break;
                    }
                    case OP_NOT_EQUAL: {
                        Value b = pop();
                        Value a = pop();
                        push(CaroBool(!valuesEqual(a, b)));
                        break;
                    }
                    case OP_LESS:          { numberBinary(OP_LESS);          break; }
                    case OP_LESS_EQUAL:    { numberBinary(OP_LESS_EQUAL);    break; }
                    case OP_GREATER:       { numberBinary(OP_GREATER);       break; }
                    case OP_GREATER_EQUAL: { numberBinary(OP_GREATER_EQUAL); break; }
                    case OP_SPACESHIP:     { numberBinary(OP_SPACESHIP);     break; }

                    case OP_NULL:  push(CaroNull);        break;
                    case OP_SMTH:  push(CaroSmth);        break;
                    case OP_TRUE:  push(CaroBool(true));  break;
                    case OP_FALSE: push(CaroBool(false)); break;

                    case OP_DEFINE_GLOBAL: {
                        ObjString* name = asString(frame->function->chunk.constants[readByte()]);
                        this->globals[name->str] = peek(0);
                        --this->stackTop;
                        break;
                    }
                    case OP_GET_GLOBAL: {
                        ObjString* name = asString(frame->function->chunk.constants[readByte()]);
                        auto found = this->globals.find(name->str);
                        if(found == this->globals.end()) {
                            runtimeError("Undefined variable '%s'.", name->str.c_str());
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        push(found->second);
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
                        push(frame->slots[slot]);
                        break;
                    }
                    case OP_SET_LOCAL: {
                        uint8_t slot = readByte();
                        frame->slots[slot] = peek(0);
                        break;
                    }

                    case OP_CALL: {
                        int argCount = readByte();
                        if(!callValue(peek(argCount), argCount)) {
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        frame = &this->frames.back();
                        break;
                    }
                    case OP_RETURN: {

                        Value result = pop();
                        Value* returnSlots = frame->slots;
                        this->frames.pop_back();
                        
                        if(this->frames.empty()) {
                            --this->stackTop;
                            return INTERPRET_OK;
                        }

                        this->stackTop = returnSlots;
                        push(result);

                        frame = &this->frames.back();

                        break;

                    }

                    case OP_TYPEOF: {
                        top() = CaroObj(copyString(typeof(top())));
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
                        --this->stackTop;
                        break;
                    }

                }

            }

        }


};


#include "../stdlib/natives.hpp"

