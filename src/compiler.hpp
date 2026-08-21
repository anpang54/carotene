
#pragma once


// includes

#include "common.hpp"
#include "scanner.hpp"


// parser

class Parser{

    public:


        Token current;
        Token previous;

        bool hadError = false;
        bool panicMode = false;    // suppresses other errors

        Scanner* scanner;


        // errors

        void errorAt(Token* token, string message) {

            if(this->panicMode) return;
            this->panicMode = true;

            cerr << "[line " << token->line << "] Error";

            if(token->type == TOKEN_EOF) {
                cerr << " at end";
            } else if(token->type == TOKEN_ERROR) {
                // nothing
            } else {
                cerr << " at '" << token->start << '\'';
            }

            cerr << ": " << message << '\n';

            this->hadError = true;

        }

        void error(string message) {
            errorAt(&this->previous, message);
        }
        void errorAtCurrent(string message) {
            errorAt(&this->current, message);
        }
        

        // advance

        void advance() {
            this->previous = this->current;
            for(;;) {
                this->current = this->scanner->scanToken();
                if(this->current.type != TOKEN_ERROR) break;
                errorAtCurrent(this->current.start);
            }
        }


        // consume

        void consume(TokenType type, string message) {
            if(this->current.type == type) {
                advance();
                return;
            }
            errorAtCurrent(message);
        }

};


// compile

bool compile(string source, Chunk* chunk) {

    Scanner scanner(source);

    Parser parser;
    parser.advance();    // "primes the pump"
    parser.expression();    // parse 1 expression
    parser.consume(TOKEN_EOF, "Expect end of expression.");    // end expression
    return !parser.hadError;

}