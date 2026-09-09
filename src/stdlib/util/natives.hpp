
#pragma once


// PARAMETER CHECKING

struct PType{
    ValueType valueType;
    ObjType   objType;
    bool      isObjType;
    PType(ValueType type): valueType(type),     objType(),     isObjType(false) {}
    PType(ObjType   type): valueType(TYPE_OBJ), objType(type), isObjType(true ) {}
};
    // {}                    = any type
    // {TYPE_INT, TYPE_LONG} = int or long
    // {TYPE_OBJECT}         = any object
    // {OBJ_STRING}          = string

struct P{
    vector<PType> allowedTypes;
    bool required;
};

bool matchesType(const PType& allowed, const Value& value) {
    if(value.type != allowed.valueType) return false;
    if(!allowed.isObjType) return true;
    return value.as.obj->type == allowed.objType;
}
string typeofPType(const PType& type) {
    return type.isObjType? typeofObjType(type.objType): typeofType(type.valueType);
}

string checkParameters(const vector<P>& parameters, const vector<Value>& args) {

    // check for too many parameters
    if(args.size() > parameters.size()) {
        return format(
            "Too many parameters. The function accepts {:d}, but {:d} were given.",
            parameters.size(), args.size()
        );
    }

    for(uint i = 0; i < parameters.size(); ++i) {

        const P& parameter = parameters[i];

        // check for missing parameters
        if(i >= args.size()) {
            if(parameter.required) {
                return format(
                    "Parameter {:d} is required, but wasn't given.",
                    i + 1
                );
            }
            continue;
        }

        // check parameter type
        if(!parameter.allowedTypes.empty() && !std::ranges::any_of(parameter.allowedTypes,
            [&](const PType& allowed) { return matchesType(allowed, args[i]); }
        )) {

            string acceptedTypes = "";
            for(auto it = parameter.allowedTypes.begin(); it != parameter.allowedTypes.end(); ++it) {
                const auto& type = *it;
                acceptedTypes += typeofPType(type);
                if(std::next(it) == parameter.allowedTypes.end()) {    // last type
                    // do nothing
                }  else if(std::next(it, 2) == parameter.allowedTypes.end()) {    // 2nd to last type
                    acceptedTypes += parameter.allowedTypes.size() == 2 ? " or " : ", or ";
                } else {    // prior types
                    acceptedTypes += ", ";
                }
            }

            return format(
                "Parameter {:d} is the wrong type. It should be {:s}, but {:s} was given.",
                i + 1, acceptedTypes, typeofValue(args[i])
            );

        }

    }

    return {};

}


// UTILITY MACROS


// definitions

#define nFunc(cppName, module, caroName, ...)\
    DefineNativeFunction nFunc_##cppName (module, string(module).empty()? string(caroName): string(module) + "." + caroName, [](VM* vm, vector<Value> args) -> Value __VA_ARGS__)
    // every native function has the same C++ function signature soo
#define nConst(cppName, module, caroName, ...)\
    DefineNativeConstant nConst_##cppName(module, string(module).empty()? string(caroName): string(module) + "." + caroName, []() -> Value __VA_ARGS__)


// parameters

#define params(...)\
    do{\
        string checkResult = checkParameters(__VA_ARGS__, args);\
        if(!checkResult.empty()) {\
            vm->runtimeError("%s", checkResult.c_str());\
            return CaroNull;\
        }\
    } while(false)
    // can't be named p() cuz else it'll eat up functions that start with p
    // variadic so that the braced parameter list can be passed in as one argument

#define ANY_NUMERIC {TYPE_BYTE, TYPE_UINT, TYPE_INT, TYPE_ULONG, TYPE_LONG, TYPE_FLOAT, TYPE_DOUBLE}

#define STR(index)  printValue(args[index])


// reducing boilerplate

#define nArrayStat(moduleRaw, moduleString, name, numType, caroType, allowed, allowedName, ...)\
    nFunc(moduleRaw##name, moduleString, #name, {\
        params({\
            {{OBJ_ARRAY}, true}\
        });\
        \
        vector<Value>& data = asArray(args[0])->data;\
        \
        if(data.empty()) {\
            vm->runtimeError("That array is empty.");\
            return CaroNull;\
        }\
        \
        vector<numType> values;\
        values.reserve(data.size());\
        for(const Value& number: data) {\
            if(!allowed(number.type)) {\
                vm->runtimeError("Every item should be " allowedName ", but there's %s.", typeofValue(number).c_str());\
                return CaroNull;\
            }\
            values.push_back(asNumberTo<numType>(number));\
        }\
        \
        auto stat = [](vector<numType>& values) -> numType __VA_ARGS__;\
        return caroType(stat(values));\
        \
    })

#define nArrayStatAny(name, ...)\
    nArrayStat(     , ""    , name, double,  CaroDouble, isNumeric, "numeric",    __VA_ARGS__)
#define nArrayStatInt(name, ...)\
    nArrayStat(math_, "math", name, int64_t, CaroLong,   isInt,     "an integer", __VA_ARGS__)
