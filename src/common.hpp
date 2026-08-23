
#pragma once


// includes

#include <string>
#include <vector>
#include <iostream>

#include <cstdbool>
#include <cstddef>
#include <cstdint>


// convenience

using std::cin, std::cout, std::cerr,
      std::string, std::format, std::to_string,
      std::vector,
      std::int8_t, std::int16_t, std::int32_t, std::int64_t, std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t;

typedef unsigned int uint;


// switches

#define DEBUG_PRINT_CODE      true
#define DEBUG_TRACE_EXECUTION true


// helpers

bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool isDigit(char c) {
    return c >= '0' && c <= '9';
}


// string manipulation

int replace(string& str, const string& from, const string& to, int maxReplacements = 0) {

    if(from.empty()) return 0;

    int replaced = 0;
    size_t start_pos = 0;

    while((start_pos = str.find(from, start_pos)) != string::npos) {

        // replace
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
        ++replaced;

        // stop if reached count
        if(maxReplacements != 0 && replaced >= maxReplacements) {
            break;
        }

    }

    return replaced;

}
    // derived from https://stackoverflow.com/a/3418285


