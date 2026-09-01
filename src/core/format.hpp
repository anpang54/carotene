
#pragma once


// includes

#include <utility>

#include "common.hpp"

using std::pair;


// ANSI escape codes and CSS

const unordered_map<string, pair<string, string>> codes = {
    {"b",       { "\033[1m" , "font-weight: bold;"                                         }},
    {"f",       { "\033[2m" , "opacity: 0.5;"                                              }},
    {"i",       { "\033[3m" , "font-style: italic;"                                        }},
    {"u",       { "\033[4m" , "text-decoration: underline;"                                }},
    {"inverse", { "\033[7m" , "background-color: #fff; color: #000;"                   }},    // filter: invert(1) doesn't work
    {"hide",    { "\033[8m" , "color: transparent;"                                        }},
    {"s",       { "\033[9m" , "text-decoration: line-through;"                             }},
    {"uu",      { "\033[21m", "text-decoration: underline; text-decoration-style: double;" }},
    {"o",       { "\033[53m", "text-decoration: overline;"                                 }},
    {"",        { "\033[0m" , ""                                                           }},
};


// hex color tags like [#123abc]

bool isFontSizeTag(const string& tag) {
    for(const char& c: tag) {
        if(!(isDigit(c) || c == '.')) return false;
    }
    return true;
}

bool isHexColorTag(const string& tag) {
    if(tag.length() != 7 || tag[0] != '#') return false;    // no # or wrong length
    for(int i = 1; i < 7; ++i) { // check each char
        if(!isDigitInBase('x', tag[i])) return false;
    }
    return true;
}


// escaping for embedding in a JS string literal

string escapeJS(const string& str) {
    string result = "";
    for(char c: str) {
        switch(c) {
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            case '\t': result += "\\t";  break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            default: result.push_back(c);
        }
    }
    return result;
}


// format

// only handles the actual formatting, not the interpolation
pair<string, vector<string>> formatString(string str) {

    string result = "";
    string tag;
    vector<string> cssRules = {};

    // loop over every char
    for(int i = 0; i < str.length(); ++i) {

        if(str[i] == '\\' && i + 1 < str.length() && (str[i + 1] == '[' || str[i + 1] == ']')) {

            // escaped bracket, output normally
            ++i;
            result.push_back(str[i]);

        } else if(str[i] == '[') {

            // tag

            // scan until ]
            tag = "";
            ++i;
            while(i < str.length() && str[i] != ']') {
                tag += str[i];
                ++i;
            }

            if(codes.contains(tag)) {

                // normal tag
                #ifdef __EMSCRIPTEN__
                    result += "%c";
                    if(tag == "") {
                        cssRules.push_back("");
                    } else {
                        cssRules.push_back((cssRules.empty() ? "" : cssRules.back()) + codes.at(tag).second);
                        // accumulate the rules like with the ANSI escape codes
                    }
                #else
                    result += codes.at(tag).first;
                #endif

            } else if(isFontSizeTag(tag)) {

                // font size tag
                #ifdef __EMSCRIPTEN__
                    result += "%c";
                    cssRules.push_back((cssRules.empty()? "": cssRules.back()) + "font-size: " + tag + "em;");
                #else
                    result += "";    // not available on terminals
                #endif

            } else if(isHexColorTag(tag)) {
                
                // hex color tag
                #ifdef __EMSCRIPTEN__
                    result += "%c";
                    cssRules.push_back((cssRules.empty()? "": cssRules.back()) + "color: " + tag + ";");
                #else
                    result += "\033[38;2;" + to_string(std::stoi(tag.substr(1, 2), nullptr, 16))
                            + ";"          + to_string(std::stoi(tag.substr(3, 2), nullptr, 16))
                            + ";"          + to_string(std::stoi(tag.substr(5, 2), nullptr, 16)) + "m";
                #endif

            }
                // if the tag is invalid, just don't output anything
                
        } else {

            // normal char
            result.push_back(str[i]);

        }

    }

    return {result, cssRules};

}