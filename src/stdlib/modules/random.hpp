

#pragma once


// INCLUDES

#include <random>

#include "../util/natives.hpp"


// HELPERS

#define RANDOM_CHECK(low, high)\
    if((low) > (high)) {\
        vm->runtimeError("The lowest number can't be greater than the highest one.");\
        return CaroNull;\
    }


// FUNCTIONS

nFunc(random_int, "random", "int", {
    params({
        {ANY_NUMERIC, true},
        {ANY_NUMERIC, true}
    });
    int32_t low  = (int32_t)asNumberTo<double>(args[0]);
    int32_t high = (int32_t)asNumberTo<double>(args[1]);
    RANDOM_CHECK(low, high);
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<int32_t> distribution(low, high);
    return CaroInt(distribution(generator));
});
nFunc(random_long, "random", "long", {
    params({
        {ANY_NUMERIC, true},
        {ANY_NUMERIC, true}
    });
    int64_t low  = (int64_t)asNumberTo<double>(args[0]);
    int64_t high = (int64_t)asNumberTo<double>(args[1]);
    RANDOM_CHECK(low, high);
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<int64_t> distribution(low, high);
    return CaroLong(distribution(generator));
});

nFunc(random_float, "random", "float", {
    params({
        {ANY_NUMERIC, true},
        {ANY_NUMERIC, true}
    });
    float low  = (float)asNumberTo<double>(args[0]);
    float high = (float)asNumberTo<double>(args[1]);
    RANDOM_CHECK(low, high);
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_real_distribution<float> distribution(low, high);
    return CaroFloat(distribution(generator));
});
nFunc(random_double, "random", "double", {
    params({
        {ANY_NUMERIC, true},
        {ANY_NUMERIC, true}
    });
    double low  = asNumberTo<double>(args[0]);
    double high = asNumberTo<double>(args[1]);
    RANDOM_CHECK(low, high);
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_real_distribution<double> distribution(low, high);
    return CaroDouble(distribution(generator));
});
