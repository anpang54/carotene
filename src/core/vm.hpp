
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
        vector<Local> replLocals;
        
        CallFrame* frame = nullptr;
        bool replMode = false;
        bool hadError = false;

        string appName, appDesc, appVersion;
        

        // constructor

        VM() {
            currentVM = this;
        }


        // native functions

        void defineNative(string name, NativeFn function) {
            this->globals[name] = CaroObj(newNative(function));
        }


        // interpret

        InterpretResult interpret(string source) {

            size_t savedReplLocals = this->replLocals.size();

            Compiler compiler;
            ObjFunction* function = compiler.compile(source, this->replMode? &this->replLocals: nullptr);
            if(function == NULL) return INTERPRET_COMPILE_ERROR;

            this->frames.reserve(FRAMES_MAX);

            // reuse the persistent top-level frame
            bool reuseFrame = this->replMode && this->stackTop != this->stack.data();
            if(reuseFrame) {
                this->stack[0] = CaroObj(function);
            } else {
                push(CaroObj(function));
            }

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

            // actually reuse
            if(reuseFrame) {
                CallFrame* newFrame = &this->frames.emplace_back();
                newFrame->function = function;
                newFrame->ip = function->chunk.code.data();
                newFrame->slots = this->stack.data();
            } else if(!call(function, 0)) {
                return INTERPRET_RUNTIME_ERROR;
            }

            InterpretResult result = run();

            // runtime error in repl
            if(this->replMode && result == INTERPRET_RUNTIME_ERROR) {
                this->replLocals.resize(savedReplLocals);
                this->stackTop = this->stack.data() + savedReplLocals + 1;
            }

            return result;

        }


        // helpers

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

            cerr << "\033[38;5;210m";
            va_list args;
            va_start(args, format);
            vfprintf(stderr, format, args);
            va_end(args);
            cerr << "\033[0m\n";

            for(int i = (int)this->frames.size() - 1; i >= 0; --i) {
                CallFrame* callFrame = &this->frames[i];
                ObjFunction* function = callFrame->function;
                size_t instruction = callFrame->ip - function->chunk.code.data() - 1;
                cerr << "\033[38;5;203m[line " << function->chunk.lines[instruction] << "] in ";
                if(function->name.empty()) {
                    cerr << "script";
                } else {
                    cerr << function->name << "()";
                }
                cerr << "\n\033[0m";
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

            // cache for performance
            uint8_t* ip        = frame->ip;
            Value*   slots     = frame->slots;
            Value*   constants = frame->function->chunk.constants.data();

            // functions replaced with macros
            #define READ_BYTE()  (*ip++)
            #define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
            #define SYNC()       (frame->ip = ip)
            #define LOAD_FRAME() (frame = &this->frames.back(), ip = frame->ip, slots = frame->slots, constants = frame->function->chunk.constants.data())

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

                    frame->function->chunk.disassembleInstruction((int)(ip - frame->function->chunk.code.data()));

                }

                // run the instruction
                OpCode instruction;
                switch(instruction = (OpCode)READ_BYTE()) {

                    case OP_NAME: {
                        appName = asString(constants[READ_BYTE()])->str;
                        break;
                    }
                    case OP_DESC: {
                        appDesc = asString(constants[READ_BYTE()])->str;
                        break;
                    }
                    case OP_VERSION: {
                        appVersion = asString(constants[READ_BYTE()])->str;
                        break;
                    }

                    case OP_CONSTANT: {
                        Value constant = constants[READ_BYTE()];
                        push(constant);
                        break;
                    }

                    #define unary()          SYNC(); if(unaryOperation()          == INTERPRET_OK) { break; } else { return INTERPRET_RUNTIME_ERROR; }
                    #define numberBinary(op) SYNC(); if(numberBinaryOperation(op) == INTERPRET_OK) { break; } else { return INTERPRET_RUNTIME_ERROR; }
                    #define stringBinary(op) SYNC(); if(stringBinaryOperation(op) == INTERPRET_OK) { break; } else { return INTERPRET_RUNTIME_ERROR; }
                    
                    case OP_ADD: case OP_SUBTRACT: {
                        if(isString(peek(0)) && isString(peek(1))) {
                            stringBinary(instruction);
                            break;
                        } else if(isNumeric(peek(0).type) && isNumeric(peek(1).type)) {
                            numberBinary(instruction);
                            break;
                        } else {
                            SYNC();
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
                            SYNC();
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
                        ObjString* name = asString(constants[READ_BYTE()]);
                        this->globals[name->str] = peek(0);
                        --this->stackTop;
                        break;
                    }
                    case OP_DEFINE_CONSTANT: {
                        ObjString* name = asString(constants[READ_BYTE()]);
                        if(this->globals.contains(name->str)) {
                            SYNC();
                            runtimeError("You can't edit a constant.");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        this->globals[name->str] = peek(0);
                        break;
                    }

                    case OP_GET_GLOBAL: {
                        ObjString* name = asString(constants[READ_BYTE()]);
                        auto found = this->globals.find(name->str);
                        if(found == this->globals.end()) {
                            SYNC();
                            runtimeError("Undefined variable '%s'.", name->str.c_str());
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        push(found->second);
                        break;
                    }
                    case OP_SET_GLOBAL: {
                        ObjString* name = asString(constants[READ_BYTE()]);
                        this->globals[name->str] = peek(0);
                        break;
                    }

                    // fused i = i + 1
                    #define incrementError(variable) { \
                        SYNC(); \
                        if(isNumeric((variable).type)) { \
                            runtimeError("Arithmetic operations between numbers of different types currently aren't supported yet."); \
                        } else { \
                            runtimeError("Operands must be numbers or strings."); \
                        } \
                        return INTERPRET_RUNTIME_ERROR; \
                    }
                    #define incrementLocal(op) { \
                        uint8_t slot = READ_BYTE(); \
                        const Value& step = constants[READ_BYTE()]; \
                        if(slots[slot].type != TYPE_INT) incrementError(slots[slot]); \
                        slots[slot].as.Aint op step.as.Aint; \
                        break; \
                    }
                    #define incrementGlobal(op) { \
                        ObjString* name = asString(constants[READ_BYTE()]); \
                        const Value& step = constants[READ_BYTE()]; \
                        auto found = this->globals.find(name->str); \
                        if(found == this->globals.end()) { \
                            SYNC(); \
                            runtimeError("Undefined variable '%s'.", name->str.c_str()); \
                            return INTERPRET_RUNTIME_ERROR; \
                        } \
                        if(found->second.type != TYPE_INT) incrementError(found->second); \
                        found->second.as.Aint op step.as.Aint; \
                        break; \
                    }

                    case OP_INCREMENT_LOCAL:  incrementLocal(+=)
                    case OP_DECREMENT_LOCAL:  incrementLocal(-=)
                    case OP_INCREMENT_GLOBAL: incrementGlobal(+=)
                    case OP_DECREMENT_GLOBAL: incrementGlobal(-=)

                    case OP_GET_LOCAL: {
                        uint8_t slot = READ_BYTE();
                        push(slots[slot]);
                        break;
                    }
                    case OP_SET_LOCAL: {
                        uint8_t slot = READ_BYTE();
                        slots[slot] = peek(0);
                        break;
                    }

                    case OP_MAKE_ARRAY: {

                        uint8_t elementCount = READ_BYTE();
                        ObjArray* array = copyArray(vector<Value>(this->stackTop - elementCount, this->stackTop));
                        this->stackTop -= elementCount;
                        push(CaroObj(array));

                        break;
                    }
                    case OP_MAKE_DICT: {

                        uint8_t elementCount = READ_BYTE();
                        unordered_map<Value, Value> data;
                        data.reserve(elementCount);
                        Value* start = this->stackTop - 2 * elementCount;
                        for(int i = 0; i < elementCount; ++i) {
                            if(!isValidKey(start[2 * i])) {
                                SYNC();
                                runtimeError("Arrays and dicts currently can't be used as dict keys.");
                                    // todo:
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            data.insert_or_assign(start[2 * i], start[2 * i + 1]);
                        }
                        ObjDict* dict = copyDict(std::move(data));
                        this->stackTop -= 2 * elementCount;
                        push(CaroObj(dict));

                        break;
                    }

                    case OP_GET_INDEX: {

                        if(isString(peek(1))) {

                            if(!isNumeric(peek(0).type)) {
                                SYNC();
                                runtimeError("The right side must be a number.");
                                return INTERPRET_RUNTIME_ERROR;
                            }

                            int64_t index = asNumberTo<int64_t>(pop());
                            const string& str = asString(pop())->str;
                            if(index < 0 || index >= (int64_t)str.size()) {
                                SYNC();
                                runtimeError("String index out of bounds.");
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            push(CaroObj(copyString(string(1, str[index]))));

                        } else if(isArray(peek(1))) {
                            
                            if(!isNumeric(peek(0).type)) {
                                SYNC();
                                runtimeError("The right side must be a number.");
                                return INTERPRET_RUNTIME_ERROR;
                            }

                            int64_t index = asNumberTo<int64_t>(pop());
                            ObjArray* array = asArray(pop());
                            if(index < 0 || index >= (int64_t)array->data.size()) {
                                SYNC();
                                runtimeError("Array index out of bounds.");
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            push(array->data[index]);

                        } else if(isDict(peek(1))) {

                            if(!isValidKey(peek(0))) {
                                SYNC();
                                runtimeError("Arrays and dicts currently can't be used as dict keys.");
                                return INTERPRET_RUNTIME_ERROR;
                            }

                            Value key = pop();
                            ObjDict* dict = asDict(pop());
                            auto it = dict->data.find(key);
                            if(it == dict->data.end()) {
                                SYNC();
                                runtimeError("Key not found.");
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            push(it->second);

                        } else {
                            SYNC();
                            runtimeError("The left side must be an array or dict.");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        break;
                    }

                    case OP_SET_INDEX: {

                        if(isString(peek(2))) {

                            if(!isNumeric(peek(1).type)) {
                                SYNC();
                                runtimeError("The right side must be a number.");
                                return INTERPRET_RUNTIME_ERROR;
                            }

                            if(!isString(peek(0))) {
                                SYNC();
                                runtimeError("The value to assign must be a string.");
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            if(asString(peek(0))->str.size() != 1) {
                                SYNC();
                                runtimeError("The string to assign must be a single character.");
                                return INTERPRET_RUNTIME_ERROR;
                            }

                            Value value = pop();
                            int64_t index = asNumberTo<int64_t>(pop());
                            ObjString* str = asString(pop());
                            if(index < 0 || index >= (int64_t)str->str.size()) {
                                SYNC();
                                runtimeError("String index out of bounds.");
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            str->str[index] = asString(value)->str[0];
                            push(value);

                        } else if(isArray(peek(2))) {

                            if(!isNumeric(peek(1).type)) {
                                SYNC();
                                runtimeError("The right side must be a number.");
                                return INTERPRET_RUNTIME_ERROR;
                            }

                            Value value = pop();
                            int64_t index = asNumberTo<int64_t>(pop());
                            ObjArray* array = asArray(pop());
                            if(index < 0 || index >= (int64_t)array->data.size()) {
                                SYNC();
                                runtimeError("Array index out of bounds.");
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            array->data[index] = value;
                            push(value);

                        } else if(isDict(peek(2))) {

                            if(!isValidKey(peek(1))) {
                                SYNC();
                                runtimeError("Arrays and dicts currently can't be used as dict keys.");
                                return INTERPRET_RUNTIME_ERROR;
                            }

                            Value value = pop();
                            Value key = pop();
                            ObjDict* dict = asDict(pop());
                            dict->data.insert_or_assign(key, value);
                            push(value);

                        } else {
                            SYNC();
                            runtimeError("The left side must be an array or dict.");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        break;
                    }

                    case OP_CALL: {
                        int argCount = READ_BYTE();
                        SYNC();
                        if(!callValue(peek(argCount), argCount)) {
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        LOAD_FRAME();
                        break;
                    }
                    case OP_RETURN: {

                        Value result = pop();
                        Value* returnSlots = frame->slots;
                        this->frames.pop_back();
                        
                        if(this->frames.empty()) {
                            // repl: leave the script function and top-level locals on the stack for the next line
                            if(!this->replMode) --this->stackTop;
                            return INTERPRET_OK;
                        }

                        this->stackTop = returnSlots;
                        push(result);

                        LOAD_FRAME();

                        break;

                    }

                    case OP_TYPEOF: {
                        top() = CaroObj(copyString(typeofValue(top())));
                        break;
                    }
                    case OP_SIZEOF: {
                        top() = CaroUlong(sizeofValue(top()));
                        break;
                    }

                    case OP_JUMP: {
                        uint16_t offset = READ_SHORT();
                        ip += offset;
                        break;
                    }
                    case OP_JUMP_IF_FALSE: {
                        uint16_t offset = READ_SHORT();
                        if(isFalsey(peek(0))) ip += offset;
                        break;
                    }
                    case OP_LOOP: {
                        uint16_t offset = READ_SHORT();
                        ip -= offset;
                        break;
                    }

                    // fused comparison + jump
                    #define jumpUnless(op) { \
                        uint16_t offset = READ_SHORT(); \
                        if(!isNumeric(peek(0).type) || !isNumeric(peek(1).type)) { \
                            SYNC(); \
                            runtimeError("Operands must be numbers."); \
                            return INTERPRET_RUNTIME_ERROR; \
                        } \
                        if(peek(0).type != peek(1).type) { \
                            SYNC(); \
                            runtimeError("Arithmetic operations between numbers of different types currently aren't supported yet."); \
                            return INTERPRET_RUNTIME_ERROR; \
                        } \
                        int b = asNumberTo<int>(pop()); \
                        int a = asNumberTo<int>(pop()); \
                        if(!(a op b)) ip += offset; \
                        break; \
                    }

                    case OP_JUMP_IF_NOT_LESS:          jumpUnless(<)
                    case OP_JUMP_IF_NOT_LESS_EQUAL:    jumpUnless(<=)
                    case OP_JUMP_IF_NOT_GREATER:       jumpUnless(>)
                    case OP_JUMP_IF_NOT_GREATER_EQUAL: jumpUnless(>=)

                    case OP_JUMP_IF_NOT_EQUAL: {
                        uint16_t offset = READ_SHORT();
                        Value b = pop();
                        Value a = pop();
                        if(!valuesEqual(a, b)) ip += offset;
                        break;
                    }
                    case OP_JUMP_IF_EQUAL: {
                        uint16_t offset = READ_SHORT();
                        Value b = pop();
                        Value a = pop();
                        if(valuesEqual(a, b)) ip += offset;
                        break;
                    }

                    case OP_FOR_LOOP: {

                        Value& counter = slots[READ_BYTE()];
                        const Value& limit = slots[READ_BYTE()];
                        const Value& step = constants[READ_BYTE()];
                        uint16_t offset = READ_SHORT();

                        if(counter.type != TYPE_INT || limit.type != TYPE_INT) {
                            SYNC();
                            if(isNumeric(counter.type) && isNumeric(limit.type)) {
                                runtimeError("Arithmetic operations between numbers of different types currently aren't supported yet.");
                            } else {
                                runtimeError("Operands must be numbers or strings.");
                            }
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        counter.as.Aint += step.as.Aint;
                        if(counter.as.Aint < limit.as.Aint) ip -= offset;

                        break;

                    }

                    case OP_POP: {
                        --this->stackTop;
                        break;
                    }

                }

            }

        }


        // garbage collector

        void collectGarbage() {

            // don't do anything if the collector is paused
            if(gcPaused) return;

            // mark roots
            // root = any object that the VM can reach directly, so stack, globals, and call frames
            for(Value* slot = this->stack.data(); slot < this->stackTop; ++slot) {
                markValue(*slot);
            }
            for(auto& [name, value]: this->globals) {
                markValue(value);
            }
            for(CallFrame& callFrame: this->frames) {
                markObject(callFrame.function);
            }

            // sweep
            std::erase_if(objects, [](Obj* object) {
                if(!object->marked) {    // not marked, so goodbye
                    freeObject(object);
                    return true;
                }
                object->marked = false;    // reset for the next time we garbage collect
                return false;
            });

            nextGC = std::max<size_t>(objects.size() * 2, 256);

        }


};


// collect garbage if there are too many objects

void maybeCollect() {
    if((DEBUG_STRESS_GC || objects.size() >= nextGC) && currentVM != nullptr) {
        currentVM->collectGarbage();
    }
}


// load natives

#include "../stdlib/natives.hpp"

