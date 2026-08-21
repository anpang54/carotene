
#pragma once


// includes

#include <string>
#include <vector>
#include <iostream>

#include <cstdbool>
#include <cstddef>
#include <cstdint>


// convenience

using std::cin, std::cout, std::cerr, std::string, std::vector, std::format;

typedef unsigned int uint;


// switches

#define DEBUG_TRACE_EXECUTION true


// helpers

bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool isDigit(char c) {
    return c >= '0' && c <= '9';
}