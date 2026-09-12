
#pragma once


// INCLUDES

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <format>
#include <utility>

#include <cstddef>
#include <cstdint>
#include <climits>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using std::cin, std::cout, std::cerr,
      std::string, std::string_view, std::format, std::to_string,
      std::pair, std::vector, std::unordered_map, std::unordered_set,
      std::hash,
      std::int8_t, std::int16_t, std::int32_t, std::int64_t, std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t;

typedef unsigned int uint;


// MACROS


// version

#define VERSION      "0.2.0"
#define VERSION_DATE "8 Sep 2026"


// limits

#define FRAMES_MAX  64
#define FRAME_SLOTS 256
#define STACK_MAX   (FRAMES_MAX * FRAME_SLOTS)
#define STACK_GUARD 1024

#define MAX_STRING_LENGTH ((uint64_t)1 << 30)
    // 2^30 bytes = 1 GiB, the longest string that "abc" * n is allowed to build


// switches

#define DEBUG_PRINT_CODE      false
#define DEBUG_TRACE_EXECUTION false
#define DEBUG_STRESS_GC       false


// HELPERS

vector<string> moreArguments;


// general

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

string runJS(string code) {
    #ifdef __EMSCRIPTEN__
        const char* output = emscripten_run_script_string(code.c_str());
        return output;
    #else
        cliError("Carotene isn't running on web.");
        return "";
    #endif
}
    // wrapper around emscripten_run_script_string() because that's a very long name and it only works with C strings


// string manipulation

const char* whitespace = " \t\r\n";
    // line endings count as whitespace, so these also strip the CRLF off a line

string trim(string_view str) {
    size_t start = str.find_first_not_of(whitespace);
    if(start == string::npos) return "";
    return string(str.substr(start, str.find_last_not_of(whitespace) - start + 1));
}
string leftTrim(string_view str) {
    size_t start = str.find_first_not_of(whitespace);
    if(start == string::npos) return "";
    return string(str.substr(start));
}
string rightTrim(string_view str) {
    size_t end = str.find_last_not_of(whitespace);
    if(end == string::npos) return "";
    return string(str.substr(0, end + 1));
}

string lower(string_view str) {
    string result(str);
    for(char& c: result) {
        if(c >= 'A' && c <= 'Z') c += 'a' - 'A';
    }
    return result;
}
string upper(string_view str) {
    string result(str);
    for(char& c: result) {
        if(c >= 'a' && c <= 'z') c -= 'a' - 'A';
    }
    return result;
}

int replace(string& str, const string& from, const string& to, int maxReplacements = INT_MAX) {

    if(from.empty()) return 0;

    int replaced = 0;
    size_t start_pos = 0;

    while(replaced < maxReplacements && (start_pos = str.find(from, start_pos)) != string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
        ++replaced;
    }

    return replaced;

}
    // derived from https://stackoverflow.com/a/3418285

vector<string> split(string_view str, string_view delimiter) {

    vector<string> tokens;

    // empty delimiter, split into chars
    if(delimiter.empty()) {
        tokens.reserve(str.size());
        for(char c: str) tokens.push_back(string(1, c));
        return tokens;
    }

    size_t start = 0;
    size_t position;

    while((position = str.find(delimiter, start)) != string::npos) {
        tokens.push_back(string(str.substr(start, position - start)));
        start = position + delimiter.length();
    }
    tokens.push_back(string(str.substr(start)));

    return tokens;

}
    // derived from https://stackoverflow.com/a/46931770
