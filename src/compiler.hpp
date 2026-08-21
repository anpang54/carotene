
#pragma once


// includes

#include "common.hpp"
#include "scanner.hpp"
#include "chunk.hpp"


// precedence

enum Precedence{
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // |
    PREC_AND,         // &
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * / %
    PREC_POWER,       // ^
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
};


// parse rule type

class Compiler;    // forward declaration

typedef void (Compiler::*ParseFn)();
struct ParseRule{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
};


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

        void emitConstant(double value) {
            emitBytes(OP_CONSTANT, makeConstant(value));
        }
        void emitReturn() {
            emitByte(OP_RETURN);
        }


        // each part

        void makeGrouping() {
            expression();
            consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
        }

        uint8_t makeConstant(double value) {
            int constant = this->compilingChunk->addConstant(value);
            if(constant > UINT8_MAX) {
                error("Too many constants in one chunk.");
                return 0;
            }
            return (uint8_t)constant;
        }

        void makeBinary() {

            TokenType operatorType = this->previous.type;
            ParseRule* rule = getRule(operatorType);

            if(operatorType == TOKEN_CARET) {
                parsePrecedence((Precedence)(rule->precedence));
            } else {
                parsePrecedence((Precedence)(rule->precedence + 1));
            }

            switch(operatorType) {
                case TOKEN_PLUS:    emitByte(OP_ADD);          break;
                case TOKEN_MINUS:   emitByte(OP_SUBTRACT);     break;
                case TOKEN_STAR:    emitByte(OP_MULTIPLY);     break;
                case TOKEN_SLASH:   emitByte(OP_DIVIDE);       break;
                case TOKEN_PERCENT: emitByte(OP_MODULO);       break;
                case TOKEN_CARET:   emitByte(OP_EXPONENTIATE); break;
                default: return;    // unreachable
            }

        }
        void makeUnary() {
            TokenType operatorType = this->previous.type;
            parsePrecedence(PREC_UNARY);    // compile the operand
            switch (operatorType) {    // emit the operator instruction
                case TOKEN_MINUS:
                    emitByte(OP_NEGATE);
                    break;
                default: return;    // unreachable
            }
        }

        void parseNumber() {
            double value = std::stod(this->previous.start);
            emitConstant(value);
        }


        // precedence

        void parsePrecedence(Precedence precedence) {

            advance();
            ParseFn prefixRule = getRule(this->previous.type)->prefix;
            if(prefixRule == NULL) {
                error("Expect expression.");
                return;
            }
            (this->*prefixRule)();

            while(precedence <= getRule(this->current.type)->precedence) {
                advance();
                ParseFn infixRule = getRule(this->previous.type)->infix;
                (this->*infixRule)();
            }

        }

        
        // parse expression

        ParseRule* getRule(TokenType type);    // defined below rules[]

        void expression() {
            parsePrecedence(PREC_ASSIGNMENT);
        }


        // end

        void endCompiler() {

            emitReturn();

            if(DEBUG_PRINT_CODE) {
                if(!this->hadError) {
                    this->compilingChunk->disassemble("code");
                }
            }

        }


};


// parse rules

inline ParseRule rules[] = {

//   token                   prefix                    infix                  precedence

    [TOKEN_LEFT_PAREN]    = { &Compiler::makeGrouping, NULL,                  PREC_NONE   },
    [TOKEN_RIGHT_PAREN]   = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_LEFT_BRACE]    = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_RIGHT_BRACE]   = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_DOT]           = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_COMMA]         = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_SEMICOLON]     = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_PLUS]          = { NULL,                    &Compiler::makeBinary, PREC_TERM   },
    [TOKEN_MINUS]         = { &Compiler::makeUnary,    &Compiler::makeBinary, PREC_TERM   },
    [TOKEN_STAR]          = { NULL,                    &Compiler::makeBinary, PREC_FACTOR },
    [TOKEN_SLASH]         = { NULL,                    &Compiler::makeBinary, PREC_FACTOR },
    [TOKEN_PERCENT]       = { NULL,                    &Compiler::makeBinary, PREC_FACTOR },
    [TOKEN_CARET]         = { NULL,                    &Compiler::makeBinary, PREC_POWER  },

    [TOKEN_BANG]          = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_BANG_EQUAL]    = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_EQUAL]         = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_EQUAL_EQUAL]   = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_LESS]          = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_LESS_EQUAL]    = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_GREATER]       = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_GREATER_EQUAL] = { NULL,                    NULL,                  PREC_NONE   },

    // literals
    [TOKEN_IDENTIFIER]    = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_STRING]        = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_NUMBER]        = { &Compiler::parseNumber,  NULL,                  PREC_NONE   },

    // keywords
    [TOKEN_VAR]           = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_FUNC]          = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_RETURN]        = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_CLASS]         = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_THIS]          = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_SUPER]         = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_IF]            = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_ELSE]          = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_FOR]           = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_WHILE]         = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_TRUE]          = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_FALSE]         = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_NULL]          = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_PRINT]         = { NULL,                    NULL,                  PREC_NONE   },

    // misc
    [TOKEN_ERROR]         = { NULL,                    NULL,                  PREC_NONE   },
    [TOKEN_EOF]           = { NULL,                    NULL,                  PREC_NONE   },

};

inline ParseRule* Compiler::getRule(TokenType type) {
    return &rules[type];
}