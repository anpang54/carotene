

#pragma once


// includes

#include <random>

#include "natives.hpp"


// functions

NATIVE(random_int, "random", "int", {
    params({
        {{ANY_NUMERIC}, true},
        {{ANY_NUMERIC}, true}
    });
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<int32_t> distribution((int32_t)asNumberToDouble(args[0]), (int32_t)asNumberToDouble(args[1]));
    return CaroInt(distribution(generator));
});
NATIVE(random_long, "random", "long", {
    params({
        {{ANY_NUMERIC}, true},
        {{ANY_NUMERIC}, true}
    });
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<int64_t> distribution((int64_t)asNumberToDouble(args[0]), (int64_t)asNumberToDouble(args[1]));
    return CaroLong(distribution(generator));
});

NATIVE(random_float, "random", "float", {
    params({
        {{ANY_NUMERIC}, true},
        {{ANY_NUMERIC}, true}
    });
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_real_distribution<float> distribution((float)asNumberToDouble(args[0]), (float)asNumberToDouble(args[1]));
    return CaroFloat(distribution(generator));
});
NATIVE(random_double, "random", "double", {
    params({
        {{ANY_NUMERIC}, true},
        {{ANY_NUMERIC}, true}
    });
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_real_distribution<double> distribution(asNumberToDouble(args[0]), asNumberToDouble(args[1]));
    return CaroDouble(distribution(generator));
});