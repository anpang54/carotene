
#pragma once


// includes

#include <set>
#include <utility>

#include "common.hpp"
#include "chunk.hpp"

using std::set, std::pair;


// objects

class VM;    // forward? declaration
VM* currentVM = nullptr;

vector<Obj*> objects;
void maybeCollect();    // defined in vm.hpp
size_t nextGC = 256;
    // at first, the garbage collector activates when there are 256 objects, but it later dynamically changes the threshold
bool gcPaused = false;

struct GCPause{
    GCPause()  { gcPaused = true; }
    ~GCPause() { gcPaused = false; }
};

enum ObjType{
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_DICT,
    OBJ_FUNCTION,
    OBJ_NATIVE,
};

struct Obj{
    ObjType type;
    bool marked = false;
};


// strings

struct ObjString: Obj{
    string str;
};
    // just a wrapper around an std::string

bool isString(Value value) {
    return value.type == TYPE_OBJ && value.as.obj->type == OBJ_STRING;
}
ObjString* asString(Value value) {
    return static_cast<ObjString*>(value.as.obj);
}

ObjString* copyString(string str) {
    maybeCollect();
    ObjString* object = new ObjString({OBJ_STRING}, std::move(str));
    objects.push_back(object);
    return object;
}


// arrays

struct ObjArray: Obj{
    vector<Value> data;
};

bool isArray(Value value) {
    return value.type == TYPE_OBJ && value.as.obj->type == OBJ_ARRAY;
}
ObjArray* asArray(Value value) {
    return static_cast<ObjArray*>(value.as.obj);
}

ObjArray* copyArray(vector<Value> data) {
    maybeCollect();
    ObjArray* object = new ObjArray({OBJ_ARRAY}, std::move(data));
    objects.push_back(object);
    return object;
}


// dicts

// these are unordered maps just because they're faster than ordered maps
// if ordering becomes important in the future, a switch to std::map can be considered

struct ObjDict: Obj{
    unordered_map<Value, Value> data;
};

bool isDict(Value value) {
    return value.type == TYPE_OBJ && value.as.obj->type == OBJ_DICT;
}
ObjDict* asDict(Value value) {
    return static_cast<ObjDict*>(value.as.obj);
}

ObjDict* copyDict(unordered_map<Value, Value> data) {
    maybeCollect();
    ObjDict* object = new ObjDict({OBJ_DICT}, std::move(data));
    objects.push_back(object);
    return object;
}


// functions

struct ObjFunction: Obj{
    int arity;
    Chunk chunk;
    string name;
};

bool isFunction(Value value) {
    return value.type == TYPE_OBJ && value.as.obj->type == OBJ_FUNCTION;
}
ObjFunction* asFunction(Value value) {
    return static_cast<ObjFunction*>(value.as.obj);
}

ObjFunction* newFunction() {
    maybeCollect();
    ObjFunction* function = new ObjFunction({OBJ_FUNCTION}, 0, Chunk(), "");
    objects.push_back(function);
    return function;
}


// native functions

typedef Value (*NativeFn)(VM* vm, vector<Value> args);

struct ObjNative: Obj{
    NativeFn function;
};

vector<pair<string, NativeFn>> natives;
set<string> modules;
set<string> nativeNames;

struct DefineNative{
    DefineNative(string module, string name, NativeFn function) {
        if(!module.empty()) modules.insert(std::move(module));
        nativeNames.insert(name);
        natives.push_back({std::move(name), function});
    }
};

bool isNative(Value value) {
    return value.type == TYPE_OBJ && value.as.obj->type == OBJ_NATIVE;
}
ObjNative* asNative(Value value) {
    return static_cast<ObjNative*>(value.as.obj);
}

ObjNative* newNative(NativeFn function) {
    maybeCollect();
    ObjNative* native = new ObjNative({OBJ_NATIVE}, function);
    objects.push_back(native);
    return native;
}


// free

void freeObject(Obj* object) {
    switch(object->type) {
        case OBJ_STRING:   delete static_cast<ObjString*>  (object); break;
        case OBJ_ARRAY:    delete static_cast<ObjArray*>   (object); break;
        case OBJ_DICT:     delete static_cast<ObjDict*>    (object); break;
        case OBJ_FUNCTION: delete static_cast<ObjFunction*>(object); break;
        case OBJ_NATIVE:   delete static_cast<ObjNative*>  (object); break;
    }
}
void freeObjects() {
    for(Obj* object: objects) {
        freeObject(object);
    }
    objects.clear();
}


// object functions that were separated from value functions

void printObject(Obj* object) {
    switch(object->type) {

        case OBJ_STRING: {
            cout << static_cast<ObjString*>(object)->str;
            break;
        }

        case OBJ_ARRAY: {

            // print [...] if an array contains itself
            static set<Obj*> beingPrinted;
            if(!beingPrinted.insert(object).second) {
                cout << "[...]";
                break;
            }

            cout << '[';
            vector<Value>& array = static_cast<ObjArray*>(object)->data;
            for(auto it = array.begin(); it != array.end(); ++it) {
                const auto& type = *it;
                printValue(type);
                if(std::next(it) != array.end()) {
                    cout << ", ";
                }
            }
            cout << ']';

            beingPrinted.erase(object);

            break;
        }

        case OBJ_DICT: {

            // print {...} if a dict contains itself
            static set<Obj*> beingPrinted;
            if(!beingPrinted.insert(object).second) {
                cout << "{...}";
                break;
            }

            cout << '{';
            unordered_map<Value, Value>& dict = static_cast<ObjDict*>(object)->data;
            for(auto it = dict.begin(); it != dict.end(); ++it) {
                printValue(it->first);
                cout << ": ";
                printValue(it->second);
                if(std::next(it) != dict.end()) {
                    cout << ", ";
                }
            }
            cout << '}';

            beingPrinted.erase(object);

            break;

        }

        case OBJ_FUNCTION: {
            ObjFunction* function = static_cast<ObjFunction*>(object);
            if(function->name.empty()) {
                cout << "<script>";
            } else {
                cout << "<func " << function->name << '>';
            }
            break;
        }

        case OBJ_NATIVE: {
            cout << "<native func>";
            break;
        }

    }
}

string typeofObject(Obj* object) {
    switch(object->type) {
        case OBJ_STRING:   return "str";
        case OBJ_ARRAY:    return "array";
        case OBJ_DICT:     return "dict";
        case OBJ_FUNCTION: return "func";
        case OBJ_NATIVE:   return "native";
    }
    return "unknown";    // should be unreachable
}

bool objectsEqual(Obj* a, Obj* b) {
    if(a == b) return true;
    if(a->type != b->type) return false;
    switch(a->type) {

        case OBJ_STRING: {
            return static_cast<ObjString*>(a)->str == static_cast<ObjString*>(b)->str;
        }

        case OBJ_ARRAY: {

            vector<Value>& dataA = static_cast<ObjArray*>(a)->data;
            vector<Value>& dataB = static_cast<ObjArray*>(b)->data;

            if(dataA.size() != dataB.size()) return false;

            static set<pair<Obj*, Obj*>> beingCompared;
            if(!beingCompared.insert({a, b}).second) return true;
            bool equal = true;
            for(size_t i = 0; i < dataA.size(); ++i) {
                if(!valuesEqual(dataA[i], dataB[i])) {
                    equal = false;
                    break;
                }
            }

            beingCompared.erase({a, b});

            return equal;

        }

        case OBJ_DICT: {

            unordered_map<Value, Value>& dataA = static_cast<ObjDict*>(a)->data;
            unordered_map<Value, Value>& dataB = static_cast<ObjDict*>(b)->data;

            if(dataA.size() != dataB.size()) return false;

            static set<pair<Obj*, Obj*>> beingCompared;
            if(!beingCompared.insert({a, b}).second) return true;
            bool equal = true;
            for(auto& [key, value]: dataA) {
                auto it = dataB.find(key);
                if(it == dataB.end() || !valuesEqual(value, it->second)) {
                    equal = false;
                    break;
                }
            }

            beingCompared.erase({a, b});

            return equal;

        }

        case OBJ_FUNCTION: return a == b;
        case OBJ_NATIVE:   return a == b;    // ?

    }
    return false;    // should be unreachable
}

size_t sizeofObject(Obj* object) {
    switch(object->type) {
        case OBJ_STRING:   return static_cast<ObjString*>  (object)->str.size();
        case OBJ_ARRAY:    return static_cast<ObjArray*>   (object)->data.size();
        case OBJ_DICT:     return static_cast<ObjDict*>    (object)->data.size();
        case OBJ_FUNCTION: return static_cast<ObjFunction*>(object)->chunk.code.size();
        case OBJ_NATIVE:   return 0;
    }
    return 0;    // should be unreachable
}


// GC marking

// the 2 functions are dependent on each other so here's a markValue forward declaration
void markValue(Value value);

void markObject(Obj* object) {
    
    // mark if not already marked
    if(object == nullptr || object->marked) return;
    object->marked = true;

    // mark more values
    switch(object->type) {
        case OBJ_ARRAY: {
            for(Value& item: static_cast<ObjArray*>(object)->data) {
                markValue(item);
            }
            break;
        }
        case OBJ_DICT: {
            for(auto& [key, value]: static_cast<ObjDict*>(object)->data) {
                markValue(key);
                markValue(value);
            }
            break;
        }
        case OBJ_FUNCTION: {
            for(Value& constant: static_cast<ObjFunction*>(object)->chunk.constants) {
                markValue(constant);
            }
        }
        default: break;
    }

}
void markValue(Value value) {
    if(value.type == TYPE_OBJ) markObject(value.as.obj);
}


// hash

bool isValidKey(Value value) {
    return !(isArray(value) || isDict(value));
}

size_t hashObject(Obj* object) {
    switch(object->type) {
        case OBJ_STRING: {
            return hash<string>{}(static_cast<ObjString*>(object)->str);
        }
        case OBJ_FUNCTION: case OBJ_NATIVE: {
            return hash<Obj*>{}(object);
        }
        default: return 2763;
    }
}