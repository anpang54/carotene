
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

            InterpretResult result = interpretBytecode(function);

            // runtime error in repl
            if(this->replMode && result == INTERPRET_RUNTIME_ERROR) {
                this->replLocals.resize(savedReplLocals);
                this->stackTop = this->stack.data() + savedReplLocals + 1;
            }

            return result;

        }

        InterpretResult interpretBytecode(ObjFunction* function) {

            this->frames.reserve(FRAMES_MAX);

            // reuse the persistent top-level frame
            bool reuseFrame = this->replMode && this->stackTop != this->stack.data();
            if(reuseFrame) {
                this->stack[0] = CaroObj(function);
            } else {
                push(CaroObj(function));
            }

            // add script arguments
            this->globals["_args"] = CaroUint(moreArguments.size());
            for(uint i = 0; i < moreArguments.size(); ++i) {
                this->globals["_" + to_string(i + 1)] = CaroObj(copyString(moreArguments[i]));
                // yes, indexes are supposed to start at 0
                // but in C argv[0] is the name of the file and argv[1] is the first argument, so we're gonna match that
            }
            
            // add natives
            for(const pair<string, NativeFn>& native: nativeFunctions) {
                defineNative(native.first, native.second);
            }
            for(const pair<string, NativeConst>& native: nativeConstants) {
                this->globals[native.first] = native.second();
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

            return run();

        }

        Value eval(const string& source) {

            Compiler compiler;
            compiler.inEval = true;

            ObjFunction* function = compiler.compile(source);
            if(function == NULL) {
                runtimeError("Couldn't compile.");
                return CaroNull;
            }

            size_t depth = this->frames.size();
            push(CaroObj(function));
            if(!call(function, 0)) return CaroNull;

            Value result = CaroNull;
            if(run(depth, &result) != INTERPRET_OK) return CaroNull;
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

            if(isInt(peek(0).type) && isInt(peek(1).type)) {
                top() = CaroNumber(peek(0).type, -asNumberTo<uint64_t>(top()));
            } else {
                top() = CaroNumber(peek(0).type, -asNumberTo<double>(top()));
            }

            return INTERPRET_OK;

        }

        template<typename T>
        InterpretResult numberBinaryOperationAs(OpCode op) {

            // get a and b
            T b = asNumberTo<T>(pop());
            T a = asNumberTo<T>(pop());

            // check division by zero
            if((op == OP_DIVIDE || op == OP_MODULO) && b == 0) {
                runtimeError("Division by zero.");
                return INTERPRET_RUNTIME_ERROR;
            }

            // wrap back into a value depending on the type
            auto num = [](auto v) {
                     if constexpr(std::is_same_v<T, uint8_t>) return CaroByte  (v);
                else if constexpr(std::is_same_v<T, int32_t>) return CaroInt   (v);
                else if constexpr(std::is_same_v<T, int64_t>) return CaroLong  (v);
                else if constexpr(std::is_same_v<T, float>)   return CaroFloat (v);
                else if constexpr(std::is_same_v<T, double>)  return CaroDouble(v);
            };

            // do the operation
            Value result;
            switch(op) {

                case OP_ADD:           result = num(a + b);                         break;
                case OP_SUBTRACT:      result = num(a - b);                         break;
                case OP_MULTIPLY:      result = num(a * b);                         break;
                case OP_DIVIDE:        result = num(a / b);                         break;
                case OP_MODULO:
                    // the only operation that's different C++ depending on the type
                    // has to be a constexpr cuz a % b won't compile otherwise
                    if constexpr(std::is_floating_point_v<T>) result = num(std::fmod(a, b));
                    else                                      result = num(a % b);
                    break;
                case OP_EXPONENTIATE:  result = num(std::pow(a, b));                break;

                case OP_LESS:          result = CaroBool(a < b);                    break;
                case OP_LESS_EQUAL:    result = CaroBool(a <= b);                   break;
                case OP_GREATER:       result = CaroBool(a > b);                    break;
                case OP_GREATER_EQUAL: result = CaroBool(a >= b);                   break;
                case OP_SPACESHIP:     result = CaroInt (a < b? -1: (a > b? 1: 0)); break;

                default: break;

            }
            push(result);

            return INTERPRET_OK;

        }

        InterpretResult numberBinaryOperation(OpCode op) {

            // check that both operands are some sort of numeric type
            if(!isNumeric(peek(0).type) || !isNumeric(peek(1).type)) {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }

            // do int arithmetic if both types are ints, if one or both are float then float arithmetic
            if((isInt(peek(0).type) && isInt(peek(1).type)) && op != OP_DIVIDE) {
                if(sizeofType(peek(0).type) == 1 && sizeofType(peek(1).type) == 1) {
                    return numberBinaryOperationAs<uint8_t>(op);
                }
                if(sizeofType(peek(0).type) <= 4 && sizeofType(peek(1).type) <= 4) {
                    return numberBinaryOperationAs<int32_t>(op);
                }
                return numberBinaryOperationAs<int64_t>(op);
            } else {
                if(sizeofType(peek(0).type) <= 4 && sizeofType(peek(1).type) <= 4) {
                    return numberBinaryOperationAs<float>(op);
                }
                return numberBinaryOperationAs<double>(op);
            }

        }

        InterpretResult stringBinaryOperation(OpCode op) {

            // get a and b
            string strA, strB;
            int multiplier;

            bool fString = false;
            auto popString = [&]() {
                ObjString* str = asString(pop());
                fString = fString || str->fString;
                return str->str;
            };

            if(op == OP_MULTIPLY) {

                if(isNumeric(peek(0).type)) {    // number is on the right
                    multiplier = asNumberTo<int>(pop());
                    strA = popString();
                } else if(isNumeric(peek(1).type)) {    // number is on the left
                    strA = popString();
                    multiplier = asNumberTo<int>(pop());
                } else {
                    return INTERPRET_RUNTIME_ERROR;
                }

            } else if(op == OP_DIVIDE || op == OP_MODULO) {

                if(isNumeric(peek(0).type) && isString(peek(1))) {
                    multiplier = asNumberTo<int>(pop());
                    strA = popString();
                } else {
                    return INTERPRET_RUNTIME_ERROR;
                }

            } else {

                strB = popString();
                strA = popString();

            }

            // do the operation
            switch(op) {

                case OP_ADD: {    // concatenates a and b
                    push(CaroObj(copyString(strA + strB, fString)));
                    break;
                }
                case OP_SUBTRACT: {    // removes all occurrences of b in a
                    string result = strA;
                    replace(result, strB, "");
                    push(CaroObj(copyString(result, fString)));
                    break;
                }

                case OP_MULTIPLY: {    // duplicates a b times
                    if(multiplier < 0) {
                        runtimeError("Strings can only be duplicated a positive amount of times.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    string result;
                    result.reserve(strA.length() * multiplier);
                    for(int i = 0; i < multiplier; ++i) {
                        result += strA;
                    }
                    push(CaroObj(copyString(result, fString)));
                    break;
                }

                case OP_DIVIDE: {    // divides a into b parts, with the remainder excluded
                    if(multiplier <= 0) {
                        runtimeError("Strings can only be divided a positive amount of times.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    int eachPartLength = strA.length() / multiplier;
                    vector<Value> result;
                    result.reserve(multiplier);
                    for(int i = 0; i < multiplier; ++i) {
                        result.push_back(CaroObj(copyString(strA.substr(i * eachPartLength, eachPartLength), fString)));
                    }
                    push(CaroObj(copyArray(result)));
                    break;
                }

                case OP_MODULO: {    // gets that remainder
                    if(multiplier <= 0) {
                        runtimeError("Strings can only be divided a positive amount of times.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    int eachPartLength = strA.length() / multiplier;
                    push(CaroObj(copyString(strA.substr(eachPartLength * multiplier), fString)));
                    break;
                }

                default: break;
                
            }

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

        InterpretResult run(size_t exitDepth = 0, Value* result = nullptr) {
            // runs until the call frames unwind back to exitDepth
            // the top level uses 0, while eval() uses the depth it was called from so that it returns to the native

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
                        cout << "[ " << printValue(*slot) << " ]";
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
                    case OP_MULTIPLY: case OP_DIVIDE: case OP_MODULO: {
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
                    case OP_EXPONENTIATE: { numberBinary(OP_EXPONENTIATE); }

                    case OP_NEGATE:       { unary();     }

                    case OP_NOT:
                        top() = CaroBool(isFalsy(top()));
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

                    case OP_INTERPOLATE: {

                        uint8_t pieceCount = READ_BYTE();
                        string result;
                        for(Value* piece = this->stackTop - pieceCount; piece < this->stackTop; ++piece) {
                            if(isString(*piece) && asString(*piece)->fString) {
                                result += asString(*piece)->str;
                            } else {
                                for(char c: printValue(*piece)) {
                                    if(c == '[' || c == ']') result.push_back('\\');
                                    result.push_back(c);
                                }
                            }
                        }
                        this->stackTop -= pieceCount;
                        push(CaroObj(copyString(result, true)));

                        break;
                    }

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

                        Value returned = pop();
                        Value* returnSlots = frame->slots;
                        this->frames.pop_back();

                        if(this->frames.size() == exitDepth) {
                            // repl: leave the script function and top-level locals on the stack for the next line
                            if(!(this->replMode && exitDepth == 0)) this->stackTop = returnSlots;
                            if(result != nullptr) *result = returned;
                            return INTERPRET_OK;
                        }

                        this->stackTop = returnSlots;
                        push(returned);

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
                        if(isFalsy(peek(0))) ip += offset;
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

