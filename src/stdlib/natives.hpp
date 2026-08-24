
#pragma once


// check parameters

struct P{
    vector<ValueType> allowedTypes;    // empty list = any type is allowed
    bool required;
};

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
        if(!parameter.allowedTypes.empty() && !std::ranges::contains(parameter.allowedTypes, args[i].type)) {
            
            string acceptedTypes = "";
            for(auto it = parameter.allowedTypes.begin(); it != parameter.allowedTypes.end(); ++it) {
                const auto& type = *it;
                acceptedTypes += typeofType(type);
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
                i + 1, acceptedTypes, typeof(args[i])
            );

        }

    }

    return {};

}


// macros to help shorten stuff

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

#define NATIVE(cppName, module, caroName, ...)\
    DefineNative N_##cppName(module, string(module).empty()? string(caroName): string(module) + "." + caroName, [](VM* vm, vector<Value> args) -> Value __VA_ARGS__)
    // every native function has the same C++ function signature soo


// include all the libraries

#include "main.hpp"

#include "hash.hpp"
#include "math.hpp"
#include "random.hpp"
#include "time.hpp"

