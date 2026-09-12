
#pragma once

/*
    these are the square tags that add formatting like bold and color,
    that are stored literally and only applied when used in a function like print()
    NOT string interpolation
*/


// INCLUDES

#include "common.hpp"


// ESCAPE CODES/CSS

const unordered_map<string, pair<string, string>> codes = {

    {"b",       { "\033[1m" , "font-weight: bold;"                                         }},
    {"/b",      { "\033[22m", "font-weight: normal"                                        }},
    {"f",       { "\033[2m" , "opacity: 0.5;"                                              }},
    {"/f",      { "\033[22m", "font-weight: normal"                                        }},

    {"i",       { "\033[3m" , "font-style: italic;"                                        }},
    {"/i",      { "\033[23m", "font-style: normal;"                                        }},

    {"u",       { "\033[4m" , "text-decoration: underline;"                                }},
    {"/u",      { "\033[24m", "text-decoration: none;"                                     }},
    {"uu",      { "\033[21m", "text-decoration: underline; text-decoration-style: double;" }},
    {"/uu",     { "\033[24m", "text-decoration: none;"                                     }},
    {"o",       { "\033[53m", "text-decoration: overline;"                                 }},
    {"/o",      { "\033[55m", "text-decoration: none;"                                     }},

    {"s",       { "\033[9m" , "text-decoration: line-through;"                             }},
    {"/s",      { "\033[29m", "text-decoration: none;"                                     }},

    {"hide",    { "\033[8m" , "color: transparent;"                                        }},
    {"/hide",   { "\033[28m", "color: #fff;"                                             }},

    {"invert",  { "\033[7m" , "background-color: #fff; color: #000;"                   }},    // filter: invert(1) doesn't work
    {"/invert", { "\033[27m", "background-color: transparent; color: #fff;"              }},
    
    {"/",       { ""        , "font-size: 1em;"                                            }},    // remove font size
    {"/#",      { "\033[39m", "color: #fff;"                                             }},    // remove color
    {"",        { "\033[0m" , ""                                                           }},    // remove all formatting

};


// HELPERS

// like [1.25]
bool isFontSizeTag(const string& tag) {
    for(const char& c: tag) {
        if(!(isDigit(c) || c == '.')) return false;
    }
    return true;
}

// like [#123abc]
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
            default:
                if((unsigned char)c < 0x20) {
                    const char* digits = "0123456789abcdef";
                    result += "\\x";
                    result.push_back(digits[((unsigned char)c >> 4) & 0xf]);
                    result.push_back(digits[ (unsigned char)c       & 0xf]);
                } else {
                    result.push_back(c);
                }
        }
    }
    return result;
}


// FORMAT

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


// OUTPUT

string consoleLogJS(const string& text, const vector<string>& cssRules) {
    string js = "console.log(\"" + escapeJS(text) + "\"";
    for(const string& rule: cssRules) {
        js += ", \"" + escapeJS(rule) + "\"";
    }
    return js + ")";
}

void printFormatted(const string& text, bool newline = true) {
    auto [formatted, cssRules] = formatString(text);
    #ifdef __EMSCRIPTEN__
        if(!cssRules.empty()) {
            runJS(consoleLogJS(formatted, cssRules));
            return;
        }
    #endif
    cout << formatted;
    if(newline) cout << '\n';
}
