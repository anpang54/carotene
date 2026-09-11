
#pragma once
#ifdef __EMSCRIPTEN__


// INCLUDES

#include "../util/natives.hpp"


// NODE OBJECT

/*
    the JS side has Module.caroNodes, a Map with number keys (ID) and Element values
    the C++ side just stores the ID
*/

// create Module.caroNodes if it doesn't exist yet
void initNodeMap() {
    EM_ASM({
        if(!Module.caroNodes) {
            Module.caroNodes    = new Map();
            Module.caroNextNode = 0;
        }
    });
}

// find the dom.Node class, or nullptr if it doesn't exist
ObjClass* findNodeClass(VM* vm) {
    if(vm == nullptr) return nullptr;
    auto found = vm->globals.find("dom.Node");
    if(found == vm->globals.end() || !isClass(found->second)) return nullptr;
    return asClass(found->second);
}

// data for each node
struct NodeData: NativeData{

    int id = -1;

    bool readProperty(const string& name, Value& result) override {

        if(name == "children") {

            int count = EM_ASM_INT({
                return Module.caroNodes.get($0).children?.length ?? 0;
            }, id);

            GCPause pause;
            vector<Value> children;
            children.reserve(count);

            ObjClass* nodeClass = findNodeClass(currentVM);
            if(nodeClass != nullptr) {

                for(int i = 0; i < count; i++) {

                    int childId = EM_ASM_INT({
                        const id = Module.caroNextNode++;
                        Module.caroNodes.set(id, Module.caroNodes.get($0).children[$1]);
                        return id;
                    }, id, i);

                    ObjInstance* instance = newInstance(nodeClass);
                    auto data = std::make_unique<NodeData>();
                    data->id = childId;
                    instance->native = std::move(data);
                    children.push_back(CaroObj(instance));

                }
            }

            result = CaroObj(copyArray(std::move(children)));
            return true;

        } else if(name == "classes") {

            int count = EM_ASM_INT({
                return Module.caroNodes.get($0).classList?.length ?? 0;
            }, id);

            GCPause pause;
            vector<Value> classes;
            classes.reserve(count);

            for(int i = 0; i < count; i++) {
                char* className = (char*)EM_ASM_PTR({
                    return stringToNewUTF8(Module.caroNodes.get($0).classList[$1]);
                }, id, i);
                classes.push_back(CaroObj(copyString(className)));
                free(className);
            }

            result = CaroObj(copyArray(std::move(classes)));
            return true;

        } else if(name == "css") {

            int count = EM_ASM_INT({
                return Module.caroNodes.get($0).style?.length ?? 0;
            }, id);

            GCPause pause;
            unordered_map<Value, Value> css;
            css.reserve(count);
            
            for(int i = 0; i < count; i++) {

                char* cssProperty = (char*)EM_ASM_PTR({
                    return stringToNewUTF8(Module.caroNodes.get($0).style[$1]);
                }, id, i);
                char* cssValue = (char*)EM_ASM_PTR({
                    const style    = Module.caroNodes.get($0).style;
                    const priority = style.getPropertyPriority(style[$1]);
                    return stringToNewUTF8(style.getPropertyValue(style[$1]) + (priority ? " !" + priority : ""));
                }, id, i);

                css.emplace(CaroObj(copyString(cssProperty)), CaroObj(copyString(cssValue)));
                free(cssProperty);
                free(cssValue);

            }
            
            result = CaroObj(copyDict(std::move(css)));
            return true;

        } else if(name == "html") {

            char* text = (char*)EM_ASM_PTR({
                return stringToNewUTF8(Module.caroNodes.get($0).innerHTML ?? "");
            }, id);
            result = CaroObj(copyString(text));
            free(text);
            return true;

        } else if(name == "id") {

            char* elementId = (char*)EM_ASM_PTR({
                return stringToNewUTF8(Module.caroNodes.get($0).id ?? "");
            }, id);
            result = CaroObj(copyString(elementId));
            free(elementId);
            return true;

        } else if(name == "text") {

            char* text = (char*)EM_ASM_PTR({
                return stringToNewUTF8(Module.caroNodes.get($0).innerText ?? "");
            }, id);
            result = CaroObj(copyString(text));
            free(text);
            return true;

        }

        return false;
    }

    bool writeProperty(const string& name, Value value, string& error) override {

        if(name == "children") {

            if(!matchesType(OBJ_ARRAY, value)) {
                error = format("The children should be {:s}, but {:s} was given.", typeofObjType(OBJ_ARRAY), typeofValue(value));
                return true;
            }

            // get the IDs
            vector<int> childIds;
            for(Value child: asArray(value)->data) {
                NodeData* childData = isInstance(child)? dynamic_cast<NodeData*>(asInstance(child)->native.get()): nullptr;
                if(childData == nullptr) {
                    error = format("Each child should be a node, but {:s} was given.", typeofValue(child));
                    return true;
                }
                childIds.push_back(childData->id);
            }

            // replace the children
            bool success = EM_ASM_INT({
                try{
                    const children = [];
                    for(let i = 0; i < $2; i++) {
                        children.push(Module.caroNodes.get(HEAP32[($1 >> 2) + i]));
                    }
                    Module.caroNodes.get($0).replaceChildren(...children);
                } catch(error) {
                    return 0;
                }
                return 1;
            }, id, childIds.data(), (int)childIds.size());
            if(!success) error = "The children couldn't be set.";
            return true;

        } else if(name == "classes") {

            if(!matchesType(OBJ_ARRAY, value)) {
                error = format("The classes should be {:s}, but {:s} was given.", typeofObjType(OBJ_ARRAY), typeofValue(value));
                return true;
            }

            string classes;
            for(Value className: asArray(value)->data) {
                if(!matchesType(OBJ_STRING, className)) {
                    error = format("Each class should be {:s}, but {:s} was given.", typeofObjType(OBJ_STRING), typeofValue(className));
                    return true;
                }
                classes += asString(className)->str + " ";
            }
            if(!classes.empty()) classes.pop_back();

            EM_ASM({
                Module.caroNodes.get($0).className = UTF8ToString($1);
            }, id, classes.c_str());
            return true;

        } else if(name == "css") {

            if(!matchesType(OBJ_DICT, value)) {
                error = format("The CSS should be {:s}, but {:s} was given.", typeofObjType(OBJ_DICT), typeofValue(value));
                return true;
            }

            string css;
            for(const auto& [cssProperty, cssValue]: asDict(value)->data) {
                css += printValue(cssProperty) + ": " + printValue(cssValue) + "; ";
            }
            if(!css.empty()) css.pop_back();

            bool success = EM_ASM_INT({
                try{
                    Module.caroNodes.get($0).style = UTF8ToString($1);
                } catch(error) {
                    return 0;
                }
                return 1;
            }, id, css.c_str());
            if(!success) error = "This CSS couldn't be edited.";
            return true;

        } else if(name == "html") {

            if(!matchesType(OBJ_STRING, value)) {
                error = format("The HTML should be {:s}, but {:s} was given.", typeofObjType(OBJ_STRING), typeofValue(value));
                return true;
            }
            EM_ASM({
                Module.caroNodes.get($0).innerHTML = UTF8ToString($1);
            }, id, asString(value)->str.c_str());
            return true;

        } else if(name == "id") {

            if(!matchesType(OBJ_STRING, value)) {
                error = format("The ID should be {:s}, but {:s} was given.", typeofObjType(OBJ_STRING), typeofValue(value));
                return true;
            }
            EM_ASM({
                Module.caroNodes.get($0).id = UTF8ToString($1);
            }, id, asString(value)->str.c_str());
            return true;

        } else if(name == "text") {

            if(!matchesType(OBJ_STRING, value)) {
                error = format("The text should be {:s}, but {:s} was given.", typeofObjType(OBJ_STRING), typeofValue(value));
                return true;
            }
            EM_ASM({
                Module.caroNodes.get($0).innerText = UTF8ToString($1);
            }, id, asString(value)->str.c_str());
            return true;
    
        }

        return false;
    }

    // destructor
    ~NodeData() override {
        EM_ASM({
            Module.caroNodes.delete($0);
        }, id);
    }

};

// class
nClass(dom_Node, "dom", "Node");

Value newNode(VM* vm, int id) {

    ObjClass* nodeClass = findNodeClass(vm);
    if(nodeClass == nullptr) {
        EM_ASM({
            Module.caroNodes.delete($0);
        }, id);
        vm->runtimeError("Couldn't find the dom.Node.");
        return CaroNull;
    }

    ObjInstance* instance = newInstance(nodeClass);
    auto data = std::make_unique<NodeData>();
    data->id = id;
    instance->native = std::move(data);
    return CaroObj(instance);

}

// constructor
nMethod(dom_Node, init, {
    params({
        {{OBJ_STRING}, true },
        {{OBJ_STRING}, false}
    });

    if(alreadyInitialized(vm, self)) return CaroNull;
    initNodeMap();

    // optional initial innerHTML
    const char* html = args.size() >= 2? asString(args[1])->str.c_str(): nullptr;

    // create the element and store it in JS
    int id = EM_ASM_INT({

        let element;
        try{
            element = document.createElement(UTF8ToString($0));
        } catch(error) {
            return -1;
        }
        if($1) element.innerHTML = UTF8ToString($1);

        const id = Module.caroNextNode++;
        Module.caroNodes.set(id, element);
        return id;

    }, asString(args[0])->str.c_str(), html);

    // error
    if(id == -1) {
        vm->runtimeError("\"%s\" isn't a valid tag.", asString(args[0])->str.c_str());
        return CaroNull;
    }

    // store it in C++
    auto data = std::make_unique<NodeData>();
    data->id = id;
    asInstance(self)->native = std::move(data);

    return CaroNull;
});


// GETTING ELEMENTS

nConst(dom_body, "dom", "body", {

    initNodeMap();

    int id = EM_ASM_INT({
        const id = Module.caroNextNode++;
        Module.caroNodes.set(id, document.body);
        return id;
    });

    return newNode(vm, id);

});

nFunc(dom_id, "dom", "id", {
    params({
        {{OBJ_STRING}, true}
    });

    initNodeMap();

    // get the element and store it
    int id = EM_ASM_INT({

        let element = document.getElementById(UTF8ToString($0));
        if(element === null) return -1;

        const id = Module.caroNextNode++;
        Module.caroNodes.set(id, element);
        return id;

    }, asString(args[0])->str.c_str());
    if(id < 0) return CaroNull;

    return newNode(vm, id);

});


// METHODS

nMethod(dom_Node, add, {
    params({
        {{OBJ_INSTANCE}, true}
    });

    NodeData* data = nativeData<NodeData>(vm, self);
    if(data == nullptr) return CaroNull;

    // check the child
    NodeData* child = dynamic_cast<NodeData*>(asInstance(args[0])->native.get());
    if(child == nullptr) {
        vm->runtimeError("%s is not a node.", typeofValue(args[0]).c_str());
        return CaroNull;
    }

    // append it
    bool success = EM_ASM_INT({
        try{
            Module.caroNodes.get($0).appendChild(Module.caroNodes.get($1));
        } catch(error) {
            return 0;
        }
        return 1;
    }, data->id, child->id);
    if(!success) vm->runtimeError("The node couldn't be added.");

    return CaroNull;
});

nMethod(dom_Node, replace, {
    params({
        {{OBJ_INSTANCE}, true}
    });

    NodeData* data = nativeData<NodeData>(vm, self);
    if(data == nullptr) return CaroNull;

    // check the replacement
    NodeData* replacement = dynamic_cast<NodeData*>(asInstance(args[0])->native.get());
    if(replacement == nullptr) {
        vm->runtimeError("%s is not a node.", typeofValue(args[0]).c_str());
        return CaroNull;
    }

    // replace
    bool success = EM_ASM_INT({
        try{
            const element = Module.caroNodes.get($0);
            if(!element.parentNode) return 0;
            element.replaceWith(Module.caroNodes.get($1));
        } catch(error) {
            return 0;
        }
        return 1;
    }, data->id, replacement->id);
    if(!success) vm->runtimeError("The node couldn't be replaced.");

    return CaroNull;
});


#endif
