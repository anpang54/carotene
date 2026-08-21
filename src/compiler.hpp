
#pragma once


// includes

#include "common.hpp"
#include "scanner.hpp"


// parser

class Compiler{

    public:


        Token current;
        Token previous;

        bool hadError = false;
        bool panicMode = false;    // suppresses other errors

        Chunk* compilingChunk;
        Scanner scanner;


        // constructor/compile

        bool compile(string source, Chunk* chunk) {
            
            this->scanner = Scanner(source);
            this->compilingChunk = chunk;
            
            this->advance();    // "primes the pump"
            this->expression();    // parse 1 expression
            this->consume(TOKEN_EOF, "Expect end of expression.");    // end expression
            this->endCompiler();

            return !hadError;

        }


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
                this->current = this->scanner.scanToken();
                if(this->current.type != TOKEN_ERROR) break;
                errorAtCurrent(this->current.start);
            }
        }

        void consume(TokenType type, string message) {
            if(this->current.type == type) {
                advance();
                return;
            }
            errorAtCurrent(message);
        }


        // emitting bytecode

        void emitByte(uint8_t byte) {
            compilingChunk->write(byte, this->previous.line);
        }
        void emitBytes(uint8_t byte1, uint8_t byte2) {
            emitByte(byte1);
            emitByte(byte2);
        }
        void emitReturn() {
            emitByte(OP_RETURN);
        }


        // end

        void endCompiler() {
            emitReturn();
        }
};

