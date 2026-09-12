
#pragma once


// INCLUDES

#include <array>
#include <algorithm>

#include <cstdarg>
#include <ctime>

#include "common.hpp"
#include "chunk.hpp"
#include "object.hpp"
#include "compiler.hpp"

using std::array;


// SETUP

enum InterpretResult{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

struct CallFrame{
    ObjFunction* function;
    uint8_t* ip;
    Value* slots;
};
    // represents a single ongoing function call


// VM

class VM{

    public:

    
        // variables

        vector<CallFrame> frames;

        array<Value, STACK_MAX + STACK_GUARD> stack;    // an array is faster than a vector
        Value* stackTop = stack.data();
        Value* const stackLimit = stack.data() + STACK_MAX;
        
        unordered_map<string, Value> globals;
        vector<Local> replLocals;
        unordered_map<ObjType, ObjClass*> builtinClasses;    // hidden classes for builtin types
        
        CallFrame* frame = nullptr;
        bool replMode = false;
        bool hadError = false;
        bool stackOverflowed = false;

        string appName, appDesc, appVersion;
        

        // constructor

        VM() {
            currentVM = this;
        }


        // interpret

        void defineNative(string name, NativeFn function) {
            this->globals[name] = CaroObj(newNative(function));
        }

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
            for(const NativeClass& native: nativeClasses) {
                ObjClass* klass = newClass(native.name);
                this->globals[native.global] = CaroObj(klass);
                for(const pair<string, NativeFn>& method: native.methods) {
                    klass->methods[method.first] = CaroObj(newNative(method.second));
                }
            }
            this->builtinClasses.clear();
            for(const BuiltinMethod& method: builtinMethods) {
                ObjClass*& klass = this->builtinClasses[method.type];
                if(klass == nullptr) klass = newClass(typeofObjType(method.type));
                klass->methods[method.name] = CaroObj(newNative(method.method));
            }
            for(const pair<string, NativeFn>& native: nativeFunctions) {
                defineNative(native.first, native.second);
            }
            for(const pair<string, NativeConst>& native: nativeConstants) {
                this->globals[native.first] = native.second(this);
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
            if(this->stackTop >= this->stackLimit) this->stackOverflowed = true;
            *this->stackTop++ = value;
        }
        Value pop() {
            return *--this->stackTop;
        }
        Value& top() {
            return this->stackTop[-1];
        }
        const Value& peek(int distance) {
            return this->stackTop[-1 - distance];
        }

        void resetStack() {
            this->stackTop = this->stack.data();    // clear
            this->frames.clear();
            this->frame = nullptr;
            this->stackOverflowed = false;
        }
        

        // runtime error

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


        // operators

        InterpretResult unaryOperation() {

            if(!isNumeric(peek(0).type)) {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }

            if(isInt(peek(0).type)) {    // negation only needs 1 operand
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
                     if constexpr(std::is_same_v<T, uint8_t>)  return CaroByte  (v);
                else if constexpr(std::is_same_v<T, int32_t>)  return CaroInt   (v);
                else if constexpr(std::is_same_v<T, uint32_t>) return CaroUint  (v);
                else if constexpr(std::is_same_v<T, int64_t>)  return CaroLong  (v);
                else if constexpr(std::is_same_v<T, uint64_t>) return CaroUlong (v);
                else if constexpr(std::is_same_v<T, float>)    return CaroFloat (v);
                else if constexpr(std::is_same_v<T, double>)   return CaroDouble(v);
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
                    else if constexpr(std::is_signed_v<T>)    result = num(b == -1? 0: a % b);
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

        static bool isComparison(OpCode op) {
            return op == OP_LESS    || op == OP_LESS_EQUAL
                || op == OP_GREATER || op == OP_GREATER_EQUAL
                || op == OP_SPACESHIP;
        }

        InterpretResult mixedSignComparison(OpCode op, bool aIsSigned) {

            if(asNumberTo<int64_t>(aIsSigned? peek(1): peek(0)) >= 0) {
                return numberBinaryOperationAs<uint64_t>(op);
            }

            pop();
            pop();

            bool aIsLess = aIsSigned;
            switch(op) {
                case OP_LESS:    case OP_LESS_EQUAL:    push(CaroBool( aIsLess));         break;
                case OP_GREATER: case OP_GREATER_EQUAL: push(CaroBool(!aIsLess));         break;
                case OP_SPACESHIP:                      push(CaroInt (aIsLess? -1: 1));   break;
                default: push(CaroNull); break;
            }

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

                ValueType a = peek(1).type, b = peek(0).type;

                // both unsigned, so use an unsigned type
                if(a == TYPE_BYTE && b == TYPE_BYTE) return numberBinaryOperationAs<uint8_t>(op);
                if(isUnsigned(a) && isUnsigned(b)) {
                    if(a == TYPE_ULONG || b == TYPE_ULONG) return numberBinaryOperationAs<uint64_t>(op);
                    return numberBinaryOperationAs<uint32_t>(op);
                }

                // one signed and one unsigned
                if(isComparison(op) && isUnsigned(a) != isUnsigned(b)) {
                    return mixedSignComparison(op, !isUnsigned(a));
                }

                if(sizeofType(a) <= 4 && sizeofType(b) <= 4) return numberBinaryOperationAs<int32_t>(op);
                return numberBinaryOperationAs<int64_t>(op);

            } else {
                if(sizeofType(peek(0).type) <= 4 && sizeofType(peek(1).type) <= 4) {
                    return numberBinaryOperationAs<float>(op);
                }
                return numberBinaryOperationAs<double>(op);
            }

        }

        template<typename T>
        InterpretResult vectorBinaryOperationAs(OpCode op, ValueType resultType) {

            // get a and b
            Value b = pop();
            Value a = pop();

            // do the operation on every component
            T result[3] = {};
            for(int i = 0; i < componentCount(resultType); ++i) {

                T x = asNumberTo<T>(getComponent(a, i));
                T y = asNumberTo<T>(getComponent(b, i));

                // check division by zero
                if((op == OP_DIVIDE || op == OP_MODULO) && y == 0) {
                    runtimeError("Division by zero.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                switch(op) {

                    case OP_ADD:          result[i] = x + y; break;
                    case OP_SUBTRACT:     result[i] = x - y; break;
                    case OP_MULTIPLY:     result[i] = x * y; break;
                    case OP_DIVIDE:       result[i] = x / y; break;
                    case OP_MODULO:
                        if constexpr(std::is_floating_point_v<T>) result[i] = std::fmod(x, y);
                        else                                      result[i] = x % y;
                        break;
                    case OP_EXPONENTIATE: result[i] = (T)std::pow(x, y); break;

                    default: break;

                }

            }
            push(CaroVector(resultType, result[0], result[1], result[2]));

            return INTERPRET_OK;

        }

        InterpretResult vectorBinaryOperation(OpCode op) {

            // check
            if(!isVector(peek(0).type) || !isVector(peek(1).type)) {
                runtimeError("Arithmetic operations between a vector and a non-vector currently aren't supported.");
                return INTERPRET_RUNTIME_ERROR;
            }
            if(componentCount(peek(0).type) != componentCount(peek(1).type)) {
                runtimeError("%s and %s don't have the same amount of components.", typeofType(peek(1).type).c_str(), typeofType(peek(0).type).c_str());
                return INTERPRET_RUNTIME_ERROR;
            }

            uint8_t size = componentCount(peek(0).type);
            ValueType componentA = componentType(peek(1).type);
            ValueType componentB = componentType(peek(0).type);

            if(componentA == TYPE_FLOAT || componentB == TYPE_FLOAT || op == OP_DIVIDE) {
                return vectorBinaryOperationAs<float>(op, vectorType(TYPE_FLOAT, size));
            }
            if(componentA == TYPE_UINT && componentB == TYPE_UINT) {
                return vectorBinaryOperationAs<uint32_t>(op, vectorType(TYPE_UINT, size));
            }
            return vectorBinaryOperationAs<int32_t>(op, vectorType(TYPE_INT, size));

        }

        InterpretResult stringBinaryOperation(OpCode op) {

            // get a and b
            string strA, strB;
            int64_t multiplier = 0;

            auto popString = [&]() {
                return asString(pop())->str;
            };

            auto popCount = [&]() -> int64_t {
                double raw = asNumberTo<double>(pop());
                if(std::isnan(raw)) return -1;
                if(raw >  1e18) return  (int64_t)1e18;
                if(raw < -1e18) return -(int64_t)1e18;
                return (int64_t)raw;
            };

            if(op == OP_MULTIPLY) {

                if(isNumeric(peek(0).type)) {    // number is on the right
                    multiplier = popCount();
                    strA = popString();
                } else if(isNumeric(peek(1).type)) {    // number is on the left
                    strA = popString();
                    multiplier = popCount();
                } else {
                    runtimeError("Operands must be numbers or strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }

            } else if(op == OP_DIVIDE || op == OP_MODULO) {

                if(isNumeric(peek(0).type) && isString(peek(1))) {
                    multiplier = popCount();
                    strA = popString();
                } else {
                    runtimeError("A string can only go on the left side of '/' and '%%'.");
                    return INTERPRET_RUNTIME_ERROR;
                }

            } else {

                strB = popString();
                strA = popString();

            }

            // do the operation
            switch(op) {

                case OP_ADD: {    // concatenates a and b
                    push(CaroObj(copyString(strA + strB)));
                    break;
                }
                case OP_SUBTRACT: {    // removes all occurrences of b in a
                    string result = strA;
                    replace(result, strB, "");
                    push(CaroObj(copyString(result)));
                    break;
                }

                case OP_MULTIPLY: {    // duplicates a b times
                    if(multiplier < 0) {
                        runtimeError("Strings can only be duplicated a positive amount of times.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    if(strA.empty()) {
                        push(CaroObj(copyString(strA)));
                        break;
                    }
                    if((uint64_t)multiplier > MAX_STRING_LENGTH / strA.length()) {
                        runtimeError("The resulting string would be too large.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    string result;
                    result.reserve(strA.length() * multiplier);
                    for(int64_t i = 0; i < multiplier; ++i) {
                        result += strA;
                    }
                    push(CaroObj(copyString(result)));
                    break;
                }

                case OP_DIVIDE: {    // divides a into b parts, with the remainder excluded
                    if(multiplier <= 0) {
                        runtimeError("Strings can only be divided a positive amount of times.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    if((uint64_t)multiplier > strA.length()) {
                        runtimeError("A string can't be divided into more parts than it has characters.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    GCPause pause;
                    int64_t eachPartLength = strA.length() / multiplier;
                    vector<Value> result;
                    result.reserve(multiplier);
                    for(int64_t i = 0; i < multiplier; ++i) {
                        result.push_back(CaroObj(copyString(strA.substr(i * eachPartLength, eachPartLength))));
                    }
                    push(CaroObj(copyArray(result)));
                    break;
                }

                case OP_MODULO: {    // gets that remainder
                    if(multiplier <= 0) {
                        runtimeError("Strings can only be divided a positive amount of times.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    if((uint64_t)multiplier > strA.length()) {
                        runtimeError("A string can't be divided into more parts than it has characters.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    int64_t eachPartLength = strA.length() / multiplier;
                    push(CaroObj(copyString(strA.substr(eachPartLength * multiplier))));
                    break;
                }

                default: break;
                
            }

            return INTERPRET_OK;

        }

        InterpretResult addOrSubtract(OpCode op) {
            if(isString(peek(0)) && isString(peek(1))) {
                return stringBinaryOperation(op);
            } else if(isVector(peek(0).type) || isVector(peek(1).type)) {
                return vectorBinaryOperation(op);
            } else if(isNumeric(peek(0).type) && isNumeric(peek(1).type)) {
                return numberBinaryOperation(op);
            }
            runtimeError("Operands must be numbers or strings.");
            return INTERPRET_RUNTIME_ERROR;
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

            for(Value* argument = this->stackTop - argCount; argument < this->stackTop; ++argument) {
                *argument = copyIfString(*argument);
            }

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

                    case OBJ_CLASS: {

                        ObjClass* klass = asClass(callee);
                        Value instance = CaroObj(newInstance(klass));
                        this->stackTop[-argCount - 1] = instance;

                        auto initializer = klass->methods.find("init");
                        if(initializer != klass->methods.end()) {
                            if(!callMethod(initializer->second.as.obj, argCount)) return false;
                            if(isNative(initializer->second)) top() = instance;
                            return true;
                        } else if(argCount != 0) {
                            runtimeError("Expected 0 arguments but got %d.", argCount);
                            return false;
                        }

                        return true;
                    }

                    case OBJ_BOUND_METHOD: {
                        ObjBoundMethod* boundMethod = asBoundMethod(callee);
                        this->stackTop[-argCount - 1] = boundMethod->receiver;
                        return callMethod(boundMethod->method, argCount);
                    }

                    case OBJ_NATIVE: {
                        return callNative(asNative(callee), argCount, false);
                    }

                    default: break;
                }
            }
            runtimeError("Can only call functions and classes.");
            return false;
        }

        bool callNative(ObjNative* native, int argCount, bool withReceiver) {
            Value* first = this->stackTop - argCount - (withReceiver? 1: 0);
            this->hadError = false;
            Value result = native->function(this, vector<Value>(first, this->stackTop));
            if(this->hadError) return false;
            this->stackTop -= argCount + 1;
            push(result);
            return true;
        }

        bool callMethod(Obj* method, int argCount) {
            if(method->type == OBJ_NATIVE) return callNative(static_cast<ObjNative*>(method), argCount, true);
            return call(static_cast<ObjFunction*>(method), argCount);
        }

        bool callFromNative(Value callee, const vector<Value>& args, Value* result) {

            size_t depth = this->frames.size();

            push(callee);
            for(const Value& arg: args) push(arg);
            if(!callValue(callee, (int)args.size())) return false;

            if(this->frames.size() > depth) {
                return run(depth, result) == INTERPRET_OK;
            }

            *result = pop();
            return true;

        }
        
        ObjClass* builtinClassFor(Value value) {
            if(value.type != TYPE_OBJ) return nullptr;
            auto found = this->builtinClasses.find(value.as.obj->type);
            return found == this->builtinClasses.end()? nullptr: found->second;
        }

        bool invoke(ObjString* name, int argCount) {

            Value receiver = peek(argCount);

            if(!isInstance(receiver)) {
                ObjClass* builtin = builtinClassFor(receiver);
                if(builtin != nullptr) return invokeFromClass(builtin, name, argCount);
                runtimeError("A %s doesn't have any methods.", typeofValue(receiver).c_str());
                return false;
            }

            ObjInstance* instance = asInstance(receiver);

            auto field = instance->fields.find(name->str);
            if(field != instance->fields.end()) {
                this->stackTop[-argCount - 1] = field->second;
                return callValue(field->second, argCount);
            }

            return invokeFromClass(instance->klass, name, argCount);

        }

        bool invokeFromClass(ObjClass* klass, ObjString* name, int argCount) {

            auto method = klass->methods.find(name->str);
            if(method == klass->methods.end()) {
                runtimeError("Undefined property '%s'.", name->str.c_str());
                return false;
            }

            return callMethod(method->second.as.obj, argCount);

        }


        // defining methods

        void defineMethod(const string& name) {
            Value method = peek(0);
            ObjClass* klass = asClass(peek(1));
            klass->methods[name] = method;
            pop();
        }

        bool bindMethod(ObjClass* klass, const string& name) {

            auto found = klass->methods.find(name);
            if(found == klass->methods.end()) {
                runtimeError("A %s doesn't have a property '%s'.", klass->name.c_str(), name.c_str());
                return false;
            }

            top() = CaroObj(newBoundMethod(peek(0), found->second.as.obj));
            return true;

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

            // overflow is checked at frame boundaries instead of before every instruction
            #define CHECK_OVERFLOW() \
                if(this->stackOverflowed) { \
                    SYNC(); \
                    runtimeError("Stack overflow."); \
                    return INTERPRET_RUNTIME_ERROR; \
                }

            for (;;){

                // debug trace execution
                #if DEBUG_TRACE_EXECUTION

                    cout << "          ";
                    for(Value* slot = this->stack.data(); slot < this->stackTop; ++slot) {
                        cout << "[ " << printValue(*slot) << " ]";
                    }
                    cout << '\n';

                    frame->function->chunk.disassembleInstruction((int)(ip - frame->function->chunk.code.data()));

                #endif

                // run the instruction
                OpCode instruction;
                switch(instruction = (OpCode)READ_BYTE()) {

                    case OP_NAME: {
                        appName = asString(constants[READ_BYTE()])->str;
                        this->globals["_name"] = CaroObj(copyString(appName));
                        break;
                    }
                    case OP_DESC: {
                        appDesc = asString(constants[READ_BYTE()])->str;
                        this->globals["_desc"] = CaroObj(copyString(appDesc));
                        break;
                    }
                    case OP_VERSION: {
                        appVersion = asString(constants[READ_BYTE()])->str;
                        this->globals["_version"] = CaroObj(copyString(appVersion));
                        break;
                    }

                    case OP_CONSTANT: {
                        Value constant = constants[READ_BYTE()];
                        push(constant);
                        break;
                    }

                    #define unary()          SYNC(); if(unaryOperation()          == INTERPRET_OK) { break; } else { return INTERPRET_RUNTIME_ERROR; }
                    #define numberBinary(op) SYNC(); if(numberBinaryOperation(op) == INTERPRET_OK) { break; } else { return INTERPRET_RUNTIME_ERROR; }
                    #define vectorBinary(op) SYNC(); if(vectorBinaryOperation(op) == INTERPRET_OK) { break; } else { return INTERPRET_RUNTIME_ERROR; }
                    #define stringBinary(op) SYNC(); if(stringBinaryOperation(op) == INTERPRET_OK) { break; } else { return INTERPRET_RUNTIME_ERROR; }

                    case OP_ADD: case OP_SUBTRACT: {
                        SYNC();
                        if(addOrSubtract(instruction) != INTERPRET_OK) return INTERPRET_RUNTIME_ERROR;
                        break;
                    }
                    case OP_MULTIPLY: case OP_DIVIDE: case OP_MODULO: {
                        if(
                            (isNumeric(peek(0).type) && isString(peek(1))) ||
                            (isNumeric(peek(1).type) && isString(peek(0)))
                        ) {
                            stringBinary(instruction);
                            break;
                        } else if(isVector(peek(0).type) || isVector(peek(1).type)) {
                            vectorBinary(instruction);
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
                    case OP_EXPONENTIATE: {
                        if(isVector(peek(0).type) || isVector(peek(1).type)) {
                            vectorBinary(OP_EXPONENTIATE);
                            break;
                        }
                        numberBinary(OP_EXPONENTIATE);
                    }

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
                            result += printValue(*piece);
                        }
                        this->stackTop -= pieceCount;
                        push(CaroObj(copyString(result)));

                        break;
                    }

                    case OP_DEFINE_GLOBAL: {
                        ObjString* name = asString(constants[READ_BYTE()]);
                        this->globals[name->str] = copyIfString(peek(0));
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
                        this->globals[name->str] = copyIfString(peek(0));
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
                        this->globals[name->str] = copyIfString(peek(0));
                        break;
                    }

                    // fused i = i + 1
                    #define incrementSlow(target, opcode) { \
                        SYNC(); \
                        push(target); \
                        push(step); \
                        if(addOrSubtract(opcode) != INTERPRET_OK) return INTERPRET_RUNTIME_ERROR; \
                        target = pop(); \
                    }
                    #define incrementLocal(op, opcode) { \
                        uint8_t slot = READ_BYTE(); \
                        const Value& step = constants[READ_BYTE()]; \
                        if(slots[slot].type == TYPE_INT && step.type == TYPE_INT) { \
                            slots[slot].as.Aint op step.as.Aint; \
                        } else incrementSlow(slots[slot], opcode) \
                        break; \
                    }
                    #define incrementGlobal(op, opcode) { \
                        ObjString* name = asString(constants[READ_BYTE()]); \
                        const Value& step = constants[READ_BYTE()]; \
                        auto found = this->globals.find(name->str); \
                        if(found == this->globals.end()) { \
                            SYNC(); \
                            runtimeError("Undefined variable '%s'.", name->str.c_str()); \
                            return INTERPRET_RUNTIME_ERROR; \
                        } \
                        if(found->second.type == TYPE_INT && step.type == TYPE_INT) { \
                            found->second.as.Aint op step.as.Aint; \
                        } else incrementSlow(found->second, opcode) \
                        break; \
                    }

                    case OP_INCREMENT_LOCAL:  incrementLocal (+=, OP_ADD)
                    case OP_DECREMENT_LOCAL:  incrementLocal (-=, OP_SUBTRACT)
                    case OP_INCREMENT_GLOBAL: incrementGlobal(+=, OP_ADD)
                    case OP_DECREMENT_GLOBAL: incrementGlobal(-=, OP_SUBTRACT)

                    case OP_GET_LOCAL: {
                        uint8_t slot = READ_BYTE();
                        push(slots[slot]);
                        break;
                    }
                    case OP_SET_LOCAL: {
                        uint8_t slot = READ_BYTE();
                        slots[slot] = copyIfString(peek(0));
                        break;
                    }

                    #define checkComponent(vec, component) { \
                        if((component) == 2 && isVec2((vec).type)) { \
                            SYNC(); \
                            runtimeError("A %s doesn't have a Z component.", typeofType((vec).type).c_str()); \
                            return INTERPRET_RUNTIME_ERROR; \
                        } \
                    }

                    #define getProperty(name) { \
                        if(isInstance(peek(0))) { \
                            ObjInstance* instance = asInstance(peek(0)); \
                            Value nativeValue; \
                            auto found = instance->fields.find((name)->str); \
                            if(instance->native && instance->native->readProperty((name)->str, nativeValue)) { \
                                top() = nativeValue; \
                            } else if(found != instance->fields.end()) { \
                                top() = found->second; \
                            } else { \
                                SYNC(); \
                                if(!bindMethod(instance->klass, (name)->str)) return INTERPRET_RUNTIME_ERROR; \
                            } \
                        } else { \
                            ObjClass* builtin = builtinClassFor(peek(0)); \
                            SYNC(); \
                            if(builtin == nullptr) { \
                                runtimeError("You can only get a property from an instance, not %s.", typeofValue(peek(0)).c_str()); \
                                return INTERPRET_RUNTIME_ERROR; \
                            } \
                            if(!bindMethod(builtin, (name)->str)) return INTERPRET_RUNTIME_ERROR; \
                        } \
                    }
                    #define setProperty(name) { \
                        if(!isInstance(peek(1))) { \
                            SYNC(); \
                            runtimeError("You can only set a property on an instance, not %s.", typeofValue(peek(1)).c_str()); \
                            return INTERPRET_RUNTIME_ERROR; \
                        } \
                        ObjInstance* instance = asInstance(peek(1)); \
                        string nativeError; \
                        if(instance->native && instance->native->writeProperty((name)->str, peek(0), nativeError)) { \
                            if(!nativeError.empty()) { \
                                SYNC(); \
                                runtimeError("%s", nativeError.c_str()); \
                                return INTERPRET_RUNTIME_ERROR; \
                            } \
                        } else { \
                            instance->fields[(name)->str] = copyIfString(peek(0)); \
                        } \
                        Value assigned = pop(); \
                        top() = assigned; \
                    }

                    case OP_GET_PROPERTY: {
                        ObjString* name = asString(constants[READ_BYTE()]);
                        getProperty(name);
                        break;
                    }

                    case OP_SET_PROPERTY: {

                        ObjString* name = asString(constants[READ_BYTE()]);

                        if(isVector(peek(1).type)) {
                            SYNC();
                            runtimeError("You can only assign to a component of a variable.");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        setProperty(name);

                        break;
                    }

                    case OP_GET_MEMBER: {

                        uint8_t component = READ_BYTE();
                        ObjString* name = asString(constants[READ_BYTE()]);

                        if(!isVector(peek(0).type)) {
                            getProperty(name);
                            break;
                        }

                        checkComponent(peek(0), component);
                        top() = getComponent(top(), component);

                        break;
                    }

                    case OP_SET_MEMBER: {

                        uint8_t component = READ_BYTE();
                        ObjString* name = asString(constants[READ_BYTE()]);
                        uint8_t setOp = READ_BYTE();
                        uint8_t arg = READ_BYTE();

                        if(!isVector(peek(1).type)) {
                            setProperty(name);
                            break;
                        }

                        checkComponent(peek(1), component);

                        ValueType wanted = componentType(peek(1).type);
                        if(peek(0).type != wanted) {
                            SYNC();
                            runtimeError("A %s has %ss, but %s was given.", typeofType(peek(1).type).c_str(), typeofType(wanted).c_str(), typeofValue(peek(0)).c_str());
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        Value assigned = pop();
                        setComponent(top(), component, assigned);

                        if(setOp == OP_SET_LOCAL) slots[arg] = top();
                        else                      this->globals[asString(constants[arg])->str] = top();

                        top() = assigned;

                        break;
                    }

                    case OP_METHOD: {
                        defineMethod(asString(constants[READ_BYTE()])->str);
                        break;
                    }

                    case OP_MAKE_ARRAY: {

                        uint8_t elementCount = READ_BYTE();
                        GCPause pause;
                        vector<Value> data(this->stackTop - elementCount, this->stackTop);
                        for(Value& element: data) element = copyIfString(element);
                        ObjArray* array = copyArray(std::move(data));
                        this->stackTop -= elementCount;
                        push(CaroObj(array));

                        break;
                    }

                    case OP_MAKE_DICT: {

                        uint8_t elementCount = READ_BYTE();
                        GCPause pause;
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
                            data.insert_or_assign(copyIfString(start[2 * i]), copyIfString(start[2 * i + 1]));
                        }
                        ObjDict* dict = copyDict(std::move(data));
                        this->stackTop -= 2 * elementCount;
                        push(CaroObj(dict));

                        break;
                    }

                    case OP_MAKE_SET: {

                        uint8_t elementCount = READ_BYTE();
                        GCPause pause;
                        unordered_set<Value> data;
                        data.reserve(elementCount);
                        Value* start = this->stackTop - elementCount;
                        for(int i = 0; i < elementCount; ++i) {
                            if(!isValidKey(start[i])) {
                                SYNC();
                                runtimeError("Arrays, dicts, and sets currently can't be used as set items.");
                                    // todo:
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            data.insert(copyIfString(start[i]));
                        }
                        ObjSet* set = copySet(std::move(data));
                        this->stackTop -= elementCount;
                        push(CaroObj(set));

                        break;
                    }

                    case OP_DUPLICATE_INDEX: {
                        push(peek(1));    // collection
                        push(peek(1));    // index
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
                                runtimeError("Arrays, dicts, and sets currently can't be used as dict keys.");
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
                            runtimeError("The left side must be an array, dict, or string.");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        break;
                    }

                    case OP_SET_INDEX: {

                        GCPause pause;

                        if(isString(peek(2))) {

                            if(!isNumeric(peek(1).type)) {
                                SYNC();
                                runtimeError("The right side must be a number.");
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            if(asString(peek(2))->immutable) {
                                SYNC();
                                runtimeError("You can't modify a string literal.");
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
                            array->data[index] = copyIfString(value);
                            push(value);

                        } else if(isDict(peek(2))) {

                            if(!isValidKey(peek(1))) {
                                SYNC();
                                runtimeError("Arrays, dicts, and sets currently can't be used as dict keys.");
                                return INTERPRET_RUNTIME_ERROR;
                            }

                            Value value = pop();
                            Value key = pop();
                            ObjDict* dict = asDict(pop());
                            dict->data.insert_or_assign(copyIfString(key), copyIfString(value));
                            push(value);

                        } else {
                            SYNC();
                            runtimeError("The left side must be an array, dict, or string.");
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        break;
                    }

                    case OP_CALL: {
                        int argCount = READ_BYTE();
                        SYNC();
                        CHECK_OVERFLOW();
                        if(!callValue(peek(argCount), argCount)) {
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        LOAD_FRAME();
                        break;
                    }
                    case OP_INVOKE: {
                        ObjString* method = asString(constants[READ_BYTE()]);
                        int argCount = READ_BYTE();
                        SYNC();
                        CHECK_OVERFLOW();
                        if(!invoke(method, argCount)) {
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        LOAD_FRAME();
                        break;
                    }
                    case OP_RETURN: {

                        CHECK_OVERFLOW();

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

                    case OP_CLASS: {
                        ObjString* name = asString(constants[READ_BYTE()]);
                        push(CaroObj(newClass(name->str)));
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
                    #define jumpUnless(comparison) { \
                        uint16_t offset = READ_SHORT(); \
                        SYNC(); \
                        if(numberBinaryOperation(comparison) != INTERPRET_OK) return INTERPRET_RUNTIME_ERROR; \
                        if(isFalsy(pop())) ip += offset; \
                        break; \
                    }

                    case OP_JUMP_IF_NOT_LESS:          jumpUnless(OP_LESS)
                    case OP_JUMP_IF_NOT_LESS_EQUAL:    jumpUnless(OP_LESS_EQUAL)
                    case OP_JUMP_IF_NOT_GREATER:       jumpUnless(OP_GREATER)
                    case OP_JUMP_IF_NOT_GREATER_EQUAL: jumpUnless(OP_GREATER_EQUAL)

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

                        // fast path: an int counter against an int limit
                        if(counter.type == TYPE_INT && limit.type == TYPE_INT && step.type == TYPE_INT) {
                            counter.as.Aint += step.as.Aint;
                            if(counter.as.Aint < limit.as.Aint) ip -= offset;
                            break;
                        }

                        SYNC();

                        push(counter);
                        push(step);
                        if(addOrSubtract(OP_ADD) != INTERPRET_OK) return INTERPRET_RUNTIME_ERROR;
                        counter = pop();

                        push(counter);
                        push(limit);
                        if(numberBinaryOperation(OP_LESS) != INTERPRET_OK) return INTERPRET_RUNTIME_ERROR;
                        if(isTruthy(pop())) ip -= offset;

                        break;

                    }

                    case OP_POP: {
                        --this->stackTop;
                        break;
                    }
                    case OP_COPY: {
                        top() = copyIfString(top());
                        break;
                    }
                    case OP_DUPLICATE: {
                        push(peek(0));
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
            // root = any object that the VM can reach directly, so stack, globals, call frames, and builtin type classes
            for(Value* slot = this->stack.data(); slot < this->stackTop; ++slot) {
                markValue(*slot);
            }
            for(auto& [name, value]: this->globals) {
                markValue(value);
            }
            for(CallFrame& callFrame: this->frames) {
                markObject(callFrame.function);
            }
            for(auto& [type, klass]: this->builtinClasses) {
                markObject(klass);
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


// COLLECT GARBAGE
// if there are too many objects

void maybeCollect() {
    if((DEBUG_STRESS_GC || objects.size() >= nextGC) && currentVM != nullptr) {
        currentVM->collectGarbage();
    }
}


// LOAD NATIVES

#include "../stdlib/modules/main.hpp"

#include "../stdlib/modules/caro.hpp"
#include "../stdlib/modules/dom.hpp"
#include "../stdlib/modules/fs.hpp"
#include "../stdlib/modules/gui.hpp"
#include "../stdlib/modules/hash.hpp"
#include "../stdlib/modules/http.hpp"
#include "../stdlib/modules/math.hpp"
#include "../stdlib/modules/random.hpp"
#include "../stdlib/modules/text.hpp"
#include "../stdlib/modules/time.hpp"

#include "../stdlib/methods/array.hpp"
#include "../stdlib/methods/string.hpp"
