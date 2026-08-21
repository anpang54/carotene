
#pragma once


// includes

#include "common.hpp"


// tokens

typedef enum {

    // 1 char
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_DOT, TOKEN_COMMA, TOKEN_SEMICOLON,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT, TOKEN_CARET,
    TOKEN_AMPERSAND, TOKEN_PIPE,

    // 1 or 2 chars
    TOKEN_BANG,    TOKEN_BANG_EQUAL,
    TOKEN_EQUAL,   TOKEN_EQUAL_EQUAL,
    TOKEN_LESS,    TOKEN_LESS_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,

    // literals
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

    // keywords
    TOKEN_VAR,
    TOKEN_FUNC, TOKEN_RETURN,
    TOKEN_CLASS, TOKEN_THIS, TOKEN_SUPER,
    TOKEN_IF, TOKEN_ELSE,
    TOKEN_FOR, TOKEN_WHILE,
    TOKEN_TRUE, TOKEN_FALSE,
    TOKEN_NULL,
    TOKEN_PRINT,

    // misc
    TOKEN_ERROR, TOKEN_EOF

} TokenType;

struct Token{
    TokenType type;
    string start;
    int length;
    int line;
};


// scanner

class Scanner{

    public:

        string source;
        int start;
        int current;
            // clox has start and current as const char*
        int line;

        Scanner(string source) {
            this->source = source;
            this->start = 0;
            this->current = 0;
            this->line = 1;
        }


        // functions to make a token

        Token makeToken(TokenType type) {
            Token token;
            token.type = type;
            token.start = this->source.substr(this->start, this->current - this->start);
            token.length = (int)(this->current - this->start);
            token.line = this->line;
            return token;
        }
        Token errorToken(string message) {
            Token token;
            token.type = TOKEN_ERROR;
            token.start = message;
            token.length = message.length();
            token.line = this->line;
            return token;
        }


        // helpers for scanning tokens

        bool isAtEnd() {
            return this->current >= (int)this->source.length();
        }

        char peek() {
            if(isAtEnd()) return '\0';
            return this->source[this->current];
        }
        char peekNext() {
            if(isAtEnd()) return '\0';
            return this->source[this->current + 1];
        }

        char advance() {
            this->current++;
            return this->source[this->current - 1];
        }
        bool match(char expected) {
            if(isAtEnd()) return false;
            if(this->source[this->current] != expected) return false;
            this->current++;
            return true;
        }


        // identifier identifier (lol)

        TokenType checkKeyword(int start, int length, string rest, TokenType type) {
            if(
                this->current - this->start == start + length &&
                this->source.compare(this->start + start, length, rest) == 0
            ) {
                return type;
            }
            return TOKEN_IDENTIFIER;
        }

        TokenType identifierType() {

            // trie
            switch(this->source[this->start]) {
             // case 'a':
             // case 'b':
                case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
             // case 'd':
                case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
                case 'f':
                    if(this->current - this->start > 1) {
                        switch(this->source[this->start + 1]) {
                            case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                            case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
                            case 'u': return checkKeyword(2, 2, "nc", TOKEN_FUNC);
                        }
                    }
                    break;
             // case 'g':
             // case 'h':
                case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
             // case 'j':
             // case 'k':
             // case 'l':
             // case 'm':
                case 'n': return checkKeyword(1, 3, "ull", TOKEN_NULL);
             // case 'o':
                case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
             // case 'q':
                case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
                case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
                case 't':
                    if(this->current - this->start > 1) {
                        switch(this->source[this->start + 1]) {
                            case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
                            case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                        }
                    }
                    break;
             // case 'u':
                case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
                case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
             // case 'x':
             // case 'y':
             // case 'z':
            }

            return TOKEN_IDENTIFIER;

        }


        // mini-scanners

        void skipWhitespace() {
            for(;;) {
                switch(peek()) {
                    case ' ': case '\r': case '\t':
                        advance();
                        break;
                    case '\n':
                        this->line++;
                        advance();
                        break;
                    default:
                        return;
                }
            }
        }

        Token scanNumber() {
            while(isDigit(peek())) advance();
            if(peek() == '.' && isDigit(peekNext())) {    // look for a decimal part
                advance();    // consume .
                while(isDigit(peek())) advance();
            }
            return makeToken(TOKEN_NUMBER);
        }
        Token scanString() {
            while(peek() != '"' && !isAtEnd()) {
                if(peek() == '\n') this->line++;
                advance();
            }
            if(isAtEnd()) return errorToken("Unterminated string.");
            advance();    // closing quote
            return makeToken(TOKEN_STRING);
        }
        Token scanIdentifier() {
            while(isAlpha(peek()) || isDigit(peek())) advance();
                // the first char can only be a letter or _, but the other chars also be digits
            return makeToken(identifierType());
        }


        // scan token

        Token scanToken() {

            skipWhitespace();

            this->start = this->current;

            // file ended
            if(isAtEnd()) {
                return makeToken(TOKEN_EOF);
            }

            char c = advance();
            
            if(isAlpha(c)) return scanIdentifier();
            if(isDigit(c)) return scanNumber();

            switch (c) {

                // 1 char
                case '(': return makeToken(TOKEN_LEFT_PAREN);
                case ')': return makeToken(TOKEN_RIGHT_PAREN);
                case '{': return makeToken(TOKEN_LEFT_BRACE);
                case '}': return makeToken(TOKEN_RIGHT_BRACE);
                case '.': return makeToken(TOKEN_DOT);
                case ',': return makeToken(TOKEN_COMMA);
                case ';': return makeToken(TOKEN_SEMICOLON);
                case '+': return makeToken(TOKEN_PLUS);
                case '-': return makeToken(TOKEN_MINUS);
                case '*': return makeToken(TOKEN_STAR);
                case '/': return makeToken(TOKEN_SLASH);
                case '%': return makeToken(TOKEN_PERCENT);
                case '^': return makeToken(TOKEN_CARET);
                case '&': return makeToken(TOKEN_AMPERSAND);
                case '|': return makeToken(TOKEN_PIPE);

                // 1 or 2 chars
                case '!': return makeToken(match('=')? TOKEN_BANG_EQUAL   : TOKEN_BANG);
                case '=': return makeToken(match('=')? TOKEN_EQUAL_EQUAL  : TOKEN_EQUAL);
                case '<': return makeToken(match('=')? TOKEN_LESS_EQUAL   : TOKEN_LESS);
                case '>': return makeToken(match('=')? TOKEN_GREATER_EQUAL: TOKEN_GREATER);

                // literals
                case '"': return scanString();

            }

            return errorToken("Unexpected character.");

        }

};