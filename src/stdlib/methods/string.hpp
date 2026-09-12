
#pragma once


// INCLUDES

#include <cctype>
#include <climits>
#include <cmath>

#include "../util/natives.hpp"


// MACROS

#define SELF    asString(self)   ->str
#define ARGS(n) asString(args[n])->str


// METHODS


// getting info

nBuiltin(OBJ_STRING, length, {
    params({});
    return CaroUlong(SELF.size());
});

nBuiltin(OBJ_STRING, find, {
    params({
        {{OBJ_STRING}, true}
    });
    size_t position = SELF.find(ARGS(0));
    if(position == string::npos) return CaroNull;
    return CaroUlong(position);
});

nBuiltin(OBJ_STRING, count, {
    params({
        {{OBJ_STRING}, true}
    });
    const string& str = SELF;
    const string& piece = ARGS(0);
    size_t step = piece.empty()? 1: piece.size();
    size_t positions = 0;
    size_t position = str.find(piece);
    while(position != string::npos) {
        ++positions;
        position = str.find(piece, position + step);
    }
    return CaroUlong(positions);
});

nBuiltin(OBJ_STRING, find_all, {
    params({
        {{OBJ_STRING}, true}
    });
    const string& str = SELF;
    const string& piece = ARGS(0);
    size_t step = piece.empty()? 1: piece.size();
    vector<Value> positions;
    size_t position = str.find(piece);
    while(position != string::npos) {
        positions.push_back(CaroUlong(position));
        position = str.find(piece, position + step);
    }
    return CaroObj(copyArray(std::move(positions)));
});

nBuiltin(OBJ_STRING, starts_with, {
    params({
        {{OBJ_STRING}, true}
    });
    return CaroBool(SELF.starts_with(ARGS(0)));
});
nBuiltin(OBJ_STRING, ends_with, {
    params({
        {{OBJ_STRING}, true}
    });
    return CaroBool(SELF.ends_with(ARGS(0)));
});

nBuiltin(OBJ_STRING, contains, {
    params({
        {{OBJ_STRING}, true}
    });
    return CaroBool(SELF.contains(ARGS(0)));
});


// transformations

nBuiltin(OBJ_STRING, sub, {
    params({
        {ANY_NUMERIC, true },
        {ANY_NUMERIC, false}
    });

    const string& str = SELF;

    // check starting position
    double requestedStart = asNumberTo<double>(args[0]);
    if(requestedStart < 0 || requestedStart > (double)str.size() || std::isnan(requestedStart)) {
        vm->runtimeError("Invalid substring start.");
        return CaroNull;
    }
    size_t start = static_cast<size_t>(requestedStart);

    // check substring length
    size_t length = str.size() - start;
    if(args.size() >= 2) {
        double requestedLength = asNumberTo<double>(args[1]);
        if(requestedLength < 0 || std::isnan(requestedLength)) {
            vm->runtimeError("Invalid substring length.");
            return CaroNull;
        }
        if(requestedLength < (double)length) length = static_cast<size_t>(requestedLength);
    }

    return CaroObj(copyString(str.substr(start, length)));

});

nBuiltin(OBJ_STRING, replace, {
    params({
        {{OBJ_STRING}, true },
        {{OBJ_STRING}, true },
        {ANY_NUMERIC,  false}
    });

    int count = INT_MAX;
    if(args.size() >= 3) {
        double requested = asNumberTo<double>(args[2]);
        if(std::isnan(requested) || requested < 0) {
            vm->runtimeError("The replacement count can't be negative.");
            return CaroNull;
        }
        count = requested > INT_MAX? INT_MAX: static_cast<int>(requested);
    }

    string result = SELF;
    replace(result, ARGS(0), ARGS(1), count);
    return CaroObj(copyString(std::move(result)));

});

nBuiltin(OBJ_STRING, lower, {
    params({});
    return CaroObj(copyString(lower(SELF)));
});
nBuiltin(OBJ_STRING, upper, {
    params({});
    return CaroObj(copyString(upper(SELF)));
});

nBuiltin(OBJ_STRING, capitalize, {
    params({});
    string result = SELF;
    if(!result.empty()) {
        result[0] = std::toupper(static_cast<unsigned char>(result[0]));
    }
    return CaroObj(copyString(std::move(result)));
});

nBuiltin(OBJ_STRING, swap_case, {
    params({});
    string result = SELF;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) -> char {
             if(std::isupper(c)) return std::tolower(c);
        else if(std::islower(c)) return std::toupper(c);
        return c;
    });
    return CaroObj(copyString(std::move(result)));
});

nBuiltin(OBJ_STRING, trim, {
    params({});
    return CaroObj(copyString(trim(SELF)));
});
nBuiltin(OBJ_STRING, trim_left, {
    params({});
    return CaroObj(copyString(leftTrim(SELF)));
});
nBuiltin(OBJ_STRING, trim_right, {
    params({});
    return CaroObj(copyString(rightTrim(SELF)));
});

#define nStringPad(caroName, joined)\
    nBuiltin(OBJ_STRING, caroName, {\
        params({\
            {{ANY_NUMERIC}, true },\
            {{OBJ_STRING},  false}\
        });\
        \
        /* check requested length */\
        double requested = asNumberTo<double>(args[0]);\
        if(requested < 0 || requested > UINT32_MAX || std::isnan(requested)) {\
            vm->runtimeError("That target length is invalid.");\
            return CaroNull;\
        }\
        size_t targetLength = static_cast<size_t>(requested);\
        \
        /* check padding char */\
        char paddingChar = ' ';\
        if(args.size() >= 2) {\
            const string& given = ARGS(1);\
            if(given.size() != 1) {\
                vm->runtimeError("The padding should be a single character, but \"%s\" was given.", given.c_str());\
                return CaroNull;\
            }\
            paddingChar = given[0];\
        }\
        \
        /* return string if it's already long enough */\
        const string& str = SELF;\
        if(str.size() >= targetLength) return CaroObj(copyString(str));\
        \
        /* pad */\
        string padding(targetLength - str.size(), paddingChar);\
        return CaroObj(copyString(joined));\
        \
    })

nStringPad(pad_left,  padding + str);
nStringPad(pad_right, str + padding);


// turning the string into another type entirely

nBuiltin(OBJ_STRING, split, {
    params({
        {{OBJ_STRING}, false}
    });
    string_view delimiter = args.size() >= 1? string_view(ARGS(0)): string_view(" ");
    GCPause pause;
    vector<Value> values;
    for(string& piece: split(SELF, delimiter)) {
        values.push_back(CaroObj(copyString(std::move(piece))));
    }
    return CaroObj(copyArray(std::move(values)));
});


#undef SELF
#undef ARGS
