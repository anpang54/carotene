
#pragma once


// INCLUDES

#include "../../core/value.hpp"


// PARSER

struct CaroJsonParser{


    const string& text;
    size_t i = 0;
    bool errored = false;


    CaroJsonParser(const string& text): text(text) {}


    // helpers

    bool match(const string& word) {
        if(text.compare(i, word.length(), word) != 0) return false;
        i += word.length();
        return true;
    }

    void skipWhitespace() {
        while(std::isspace(static_cast<unsigned char>(text[i]))) { ++i; }
    }

    Value throwError() {
        errored = true;
        return CaroNull;
    }
    

    // strings

    bool parseString(string& out) {

        if(i >= text.length() || text[i] != '"') return false;
        ++i;

        while(true) {

            if(i >= text.length()) return false;
            char c = text[i++];
            if(c == '"') return true;
            if(c != '\\') { out += c; continue; }

            if(i >= text.length()) return false;
            switch(text[i++]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default: return false;
            }

        }

    }


    // numbers

    Value parseNumber() {

        // check if int
        size_t start = i;
        bool whole = true;
        if(i < text.length() && text[i] == '-') ++i;
        while(i < text.length()) {
            char c = text[i];
            if(c == '.' || c == 'e' || c == 'E') whole = false;
            else if(!(c >= '0' && c <= '9') && c != '+' && c != '-') break;
            ++i;
        }

        string number = text.substr(start, i - start);
        size_t length = 0;

        // int
        if(whole) {
            try{
                int64_t integer = std::stoll(number, &length);
                if(length == number.length()) {
                    return integer >= INT32_MIN && integer <= INT32_MAX? CaroInt((int32_t)integer): CaroLong(integer);
                }
            } catch(...) {}    // too big for a long, so store it as a double instead
        }

        // float
        try{
            double real = std::stod(number, &length);
            if(length == number.length()) return CaroDouble(real);
        } catch(...) {}

        return throwError();

    }


    // collections

    Value parseArray() {

        ++i;    // [
        skipWhitespace();

        vector<Value> data;

        if(i < text.length() && text[i] == ']') {
            ++i;
        } else {
            
            while(true) {

                // item
                data.push_back(parseValue());
                if(errored) return CaroNull;

                skipWhitespace();
                if(i >= text.length()) return throwError();
                if(text[i] == ',') { ++i; continue; }
                if(text[i] == ']') { ++i; break;    }
                return throwError();

            }
        
        }

        return CaroObj(copyArray(std::move(data)));

    }

    Value parseDict() {

        ++i;    // {
        skipWhitespace();

        unordered_map<Value, Value> data;

        if(i < text.length() && text[i] == '}') {
            ++i;
        } else {
            
            while(true) {

                // key
                skipWhitespace();
                string key = "";
                if(!parseString(key)) return throwError();

                // :
                skipWhitespace();
                if(i >= text.length() || text[i] != ':') return throwError();
                ++i;

                // value
                Value value = parseValue();
                if(errored) return CaroNull;
                data[CaroObj(copyString(std::move(key)))] = value;

                skipWhitespace();
                if(i >= text.length()) return throwError();
                if(text[i] == ',') { ++i; continue; }
                if(text[i] == '}') { ++i; break;    }
                return throwError();

            }

        }

        return CaroObj(copyDict(std::move(data)));

    }


    // parse

    Value parseValue() {

        skipWhitespace();
        if(i >= text.length()) return throwError();

        switch(text[i]) {

            case 'n': return match("null" )? CaroNull:        throwError();
            case 't': return match("true" )? CaroBool(true ): throwError();
            case 'f': return match("false")? CaroBool(false): throwError();

            case '"': {
                string str = "";
                if(!parseString(str)) return throwError();
                return CaroObj(copyString(std::move(str)));
            }

            case '{': return parseDict();
            case '[': return parseArray();

            default:  return parseNumber();

        }

    }


};

