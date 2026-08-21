
#pragma once


// includes

#include "common.hpp"
#include "compiler.hpp"
#include "scanner.hpp"


// compile

void compile(string source) {

    Scanner scanner(source);

    int line = -1;
    for (;;) {
        Token token = scanner.scanToken();
        if(token.line != line) {
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type, token.length, token.start.c_str()); 
        if (token.type == TOKEN_EOF) break;
    }

}