
#pragma once


// includes

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

#include <cstdbool>
#include <cstddef>
#include <cstdint>


// version

#define VERSION      "0.1.0c"
#define VERSION_DATE "29 Aug 2026"


// convenience

using std::cin, std::cout, std::cerr,
      std::string, std::format, std::to_string,
      std::vector, std::unordered_map, std::hash,
      std::int8_t, std::int16_t, std::int32_t, std::int64_t, std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t;

typedef unsigned int uint;


// switches

#define DEBUG_PRINT_CODE      false
#define DEBUG_TRACE_EXECUTION false
#define DEBUG_STRESS_GC       false


// helpers

vector<string> moreArguments;

bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool isDigit(char c) {
    return c >= '0' && c <= '9';
}
bool isDigitInBase(char base, char c = '\0') {
    switch(base) {
        case 'b': return c == '0' || c == '1';
        case 'o': return c >= '0' && c <= '7';
        case 'x': return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); 
        default:  return isDigit(c);
    }
}

void cliError(string message) {
    cerr << "\033[38;5;203m" << message << "\033[0m\n";
    exit(1);
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

