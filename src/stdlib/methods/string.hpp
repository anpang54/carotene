
#pragma once


// INCLUDES

#include <cctype>
#include <climits>
#include <cmath>

#include "../util/natives.hpp"


// MACROS

#define SELF    asString(self)   ->str
#define ARGS(n) asString(args[n])->str
#define FSELF   asString(self)   ->fString


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
    return CaroObj(copyString(std::move(result), FSELF));

});

nBuiltin(OBJ_STRING, lower, {
    params({});
    return CaroObj(copyString(lower(SELF), FSELF));
});

nBuiltin(OBJ_STRING, upper, {
    params({});
    return CaroObj(copyString(upper(SELF), FSELF));
});

nBuiltin(OBJ_STRING, capitalize, {
    params({});
    string result = SELF;
    if(!result.empty()) {
        result[0] = std::toupper(static_cast<unsigned char>(result[0]));
    }
    return CaroObj(copyString(std::move(result), FSELF));
});

nBuiltin(OBJ_STRING, swap_case, {
    params({});
    string result = SELF;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) -> char {
             if(std::isupper(c)) return std::tolower(c);
        else if(std::islower(c)) return std::toupper(c);
        return c;
    });
    return CaroObj(copyString(std::move(result), FSELF));
});

nBuiltin(OBJ_STRING, trim, {
    params({});
    return CaroObj(copyString(trim(SELF), FSELF));
});

nBuiltin(OBJ_STRING, trim_left, {
    params({});
    return CaroObj(copyString(leftTrim(SELF), FSELF));
});

nBuiltin(OBJ_STRING, trim_right, {
    params({});
    return CaroObj(copyString(rightTrim(SELF), FSELF));
});


// turning the string into another type entirely

nBuiltin(OBJ_STRING, split, {
    params({
        {{OBJ_STRING}, false}
    });
    string_view delimiter = args.size() >= 1? string_view(ARGS(0)): string_view(" ");
    GCPause pause;
    vector<Value> values;
    for(string& piece: split(SELF, delimiter)) {
        values.push_back(CaroObj(copyString(std::move(piece), FSELF)));
    }
    return CaroObj(copyArray(std::move(values)));
});


#undef SELF
#undef ARGS
#undef FSELF
