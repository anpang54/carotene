
#pragma once


// INCLUDES

#include <string_view>

#include "common.hpp"
#include "scanner.hpp"
#include "chunk.hpp"


// SETUP


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


// structs

class Compiler;    // forward declaration

typedef void (Compiler::*ParseFn)(bool canAssign);

struct ParseRule{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
};

struct Local{
    Token name;
    int depth;
};


// COMPILER

class Compiler{

    public:


        // variables

        Chunk* compilingChunk;
        Scanner scanner;
        Token current;
        Token previous;

        bool hadError = false;
        bool panicMode = false;    // suppresses other errors

        vector<Local> locals;
        int localCount;
        int scopeDepth;


        // compile

        bool compile(string source, Chunk* chunk) {
            
            this->compilingChunk = chunk;
            this->scanner = Scanner(source);
            this->localCount = 0;
            this->scopeDepth = 0;
            
            this->advance();
            while(!match(TOKEN_EOF)) {
                this->declaration();
            }
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
        

        // advance/consume

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

        void emitConstant(Value value) {
            emitBytes(OP_CONSTANT, makeConstant(value));
        }
        void emitNumber(double value) {
            emitConstant(CaroNumber(TYPE_DOUBLE, value));
                // todo: emit something other than a double
        }

        int emitJump(uint8_t instruction) {
            emitByte(instruction);
            emitByte(0xff);
            emitByte(0xff);
            return this->compilingChunk->code.size() - 2;
        }
        void emitLoop(int loopStart) {

            emitByte(OP_LOOP);

            int offset = this->compilingChunk->code.size() - loopStart + 2;

            emitByte((offset >> 8) & 0xff);
            emitByte(offset & 0xff);
            
        }

        void emitReturn() {
            emitByte(OP_RETURN);
        }


        // each part

        void makeGrouping(bool canAssign) {
            expression();
            consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
        }

        uint8_t makeConstant(Value value) {
            int constant = this->compilingChunk->addConstant(value);
            if(constant > UINT8_MAX) {
                error("Too many constants in one chunk.");
                return 0;
            }
            return (uint8_t)constant;
        }

        void makeUnary(bool canAssign) {
            TokenType operatorType = this->previous.type;
            parsePrecedence(PREC_UNARY);    // compile the operand
            switch (operatorType) {    // emit the operator instruction
                case TOKEN_MINUS:
                    emitByte(OP_NEGATE);
                    break;
                case TOKEN_BANG:
                    emitByte(OP_NOT);
                    break;
                case TOKEN_TYPEOF:
                    emitByte(OP_TYPEOF);
                    break;
                default: return;    // unreachable
            }
        }
        void makeBinary(bool canAssign) {

            TokenType operatorType = this->previous.type;
            ParseRule* rule = getRule(operatorType);

            if(operatorType == TOKEN_CARET) {
                parsePrecedence((Precedence)(rule->precedence));
            } else {
                parsePrecedence((Precedence)(rule->precedence + 1));
            }

            switch(operatorType) {

                case TOKEN_PLUS:          emitByte(OP_ADD);           break;
                case TOKEN_MINUS:         emitByte(OP_SUBTRACT);      break;
                case TOKEN_STAR:          emitByte(OP_MULTIPLY);      break;
                case TOKEN_SLASH:         emitByte(OP_DIVIDE);        break;
                case TOKEN_PERCENT:       emitByte(OP_MODULO);        break;
                case TOKEN_CARET:         emitByte(OP_EXPONENTIATE);  break;

                case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL);         break;
                case TOKEN_BANG_EQUAL:    emitByte(OP_NOT_EQUAL);     break;
                case TOKEN_LESS:          emitByte(OP_LESS);          break;
                case TOKEN_LESS_EQUAL:    emitByte(OP_LESS_EQUAL);    break;
                case TOKEN_GREATER:       emitByte(OP_GREATER);       break;
                case TOKEN_GREATER_EQUAL: emitByte(OP_GREATER_EQUAL); break;
                case TOKEN_SPACESHIP:     emitByte(OP_SPACESHIP);     break;
                
                default: return;    // unreachable

            }

        }
        
        void parseNumber(bool canAssign) {
            emitNumber(std::stod(this->previous.start));
        }
        void parseLiteral(bool canAssign) {
            switch(this->previous.type) {
                case TOKEN_NULL:  emitByte(OP_NULL);  break;
                case TOKEN_SMTH:  emitByte(OP_SMTH);  break;
                case TOKEN_TRUE:  emitByte(OP_TRUE);  break;
                case TOKEN_FALSE: emitByte(OP_FALSE); break;
                default: return;    // unreachable
            }
        }
        void parseString(bool canAssign) {
            emitConstant(
                CaroObj(copyString(this->previous.start.substr(1, this->previous.length - 2)))
            );
        }


        // precedence

        void parsePrecedence(Precedence precedence) {

            advance();
            ParseFn prefixRule = getRule(this->previous.type)->prefix;
            if(prefixRule == NULL) {
                error("Expect expression.");
                return;
            }

            bool canAssign = precedence <= PREC_ASSIGNMENT;
            (this->*prefixRule)(canAssign);

            while(precedence <= getRule(this->current.type)->precedence) {
                advance();
                ParseFn infixRule = getRule(this->previous.type)->infix;
                (this->*infixRule)(canAssign);
            }

            if(canAssign && match(TOKEN_EQUAL)) {
                error("Invalid assignment target.");
            }

        }

        
        // parse expression

        ParseRule* getRule(TokenType type);    // defined below rules[]

        void expression() {
            parsePrecedence(PREC_ASSIGNMENT);
        }


        // statements

        bool check(TokenType type) {
            return this->current.type == type;
        }
        bool match(TokenType type) {
            if(!check(type)) return false;
            advance();
            return true;
        }

        void statement() {
            if(match(TOKEN_PRINT)) {
                printStatement();
            } else if(match(TOKEN_FOR)) {
                forStatement();
            } else if(match(TOKEN_REPEAT)) {
                repeatStatement();
            } else if(match(TOKEN_IF)) {
                ifStatement();
            } else if(match(TOKEN_WHILE)) {
                whileStatement();
            } else if(match(TOKEN_FOREVER)) {
                foreverStatement();
            } else if(match(TOKEN_LEFT_BRACE)) {
                beginScope();
                block();
                endScope();
            } else {
                expressionStatement();
            }
        }

        void printStatement() {
            expression();
            consume(TOKEN_SEMICOLON, "Expect ';' after value.");
            emitByte(OP_PRINT);
        }

        void forStatement() {

            beginScope();

            consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
            if (match(TOKEN_SEMICOLON)) {
                // no initializer
            } else if(match(TOKEN_DOLLAR)) {    // variable
                varDeclaration();
            } else {
                expressionStatement();
            }

            int loopStart = this->compilingChunk->code.size();

            int exitJump = -1;
            if(!match(TOKEN_SEMICOLON)) {

                expression();
                consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

                // jump out of the loop if the condition is false
                exitJump = emitJump(OP_JUMP_IF_FALSE);
                emitByte(OP_POP);    // condition

            }

            if(!match(TOKEN_RIGHT_PAREN)) {

                int bodyJump = emitJump(OP_JUMP);
                int incrementStart = this->compilingChunk->code.size();
                expression();
                emitByte(OP_POP);
                consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

                emitLoop(loopStart);
                loopStart = incrementStart;
                patchJump(bodyJump);

            }

            statement();
            emitLoop(loopStart);

            if(exitJump != -1) {
                patchJump(exitJump);
                emitByte(OP_POP); // Condition.
            }

            endScope();

        }

        void repeatStatement() {
            
            beginScope();

            // consume and store the number of repetitions
            consume(TOKEN_LEFT_PAREN, "Expect '(' after 'repeat'.");
            expression();
            consume(TOKEN_RIGHT_PAREN, "Expect ')' after the number of repetitions.");
            uint8_t countSlot = makeHiddenLocal('c');

            // consume and declare the counter variable
            if(match(TOKEN_COLON)) {
                consume(TOKEN_DOLLAR, "Expect '$' before the variable name.");
                consume(TOKEN_IDENTIFIER, "Expect variable name.");
                declareVariable();
                markInitialized();
            } else {
                makeHiddenLocal('r');
            }
            uint8_t counterSlot = countSlot + 1;
            emitNumber(0);

            // check counter < count
            int loopStart = this->compilingChunk->code.size();
            emitBytes(OP_GET_LOCAL, counterSlot);
            emitBytes(OP_GET_LOCAL, countSlot);
            emitByte(OP_LESS);
            int exitJump = emitJump(OP_JUMP_IF_FALSE);
            emitByte(OP_POP);

            // stuff inside the block
            statement();

            // increment counter
            emitBytes(OP_GET_LOCAL, counterSlot);
            emitNumber(1);
            emitByte(OP_ADD);
            emitBytes(OP_SET_LOCAL, counterSlot);
            emitByte(OP_POP);

            // loop
            emitLoop(loopStart);
            patchJump(exitJump);
            emitByte(OP_POP);

            endScope();
            
        }

        void ifStatement() {

            consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
            expression();
            consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition."); 

            int thenJump = emitJump(OP_JUMP_IF_FALSE);
            emitByte(OP_POP);
            statement();

            int elseJump = emitJump(OP_JUMP);
            patchJump(thenJump);
            emitByte(OP_POP);

            if(match(TOKEN_ELSE)) statement();
            patchJump(elseJump);
        
        }
        void patchJump(int offset) {
            
            int jump = this->compilingChunk->code.size() - offset - 2;
                // -2 to adjust for the bytecode for the jump offset itself

            this->compilingChunk->code[offset] = (jump >> 8) & 0xff;
            this->compilingChunk->code[offset + 1] = jump & 0xff;
        
        }

        void whileStatement() {

            int loopStart = this->compilingChunk->code.size();

            consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
            expression();
            consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

            int exitJump = emitJump(OP_JUMP_IF_FALSE);
            emitByte(OP_POP);
            statement();
            emitLoop(loopStart);

            patchJump(exitJump);
            emitByte(OP_POP);
        
        }

        void foreverStatement() {
            // derived from repeatStatement, not whileStatement, cuz it needs the counter variable thingy

            beginScope();

            // consume and declare the counter variable
            if(match(TOKEN_COLON)) {
                consume(TOKEN_DOLLAR, "Expect '$' before the variable name.");
                consume(TOKEN_IDENTIFIER, "Expect variable name.");
                declareVariable();
                markInitialized();
            } else {
                makeHiddenLocal('r');
            }
            uint8_t counterSlot = this->localCount - 1;
            emitNumber(0);

            int loopStart = this->compilingChunk->code.size();
            
            // stuff inside the block
            statement();

            // increment counter
            emitBytes(OP_GET_LOCAL, counterSlot);
            emitNumber(1);
            emitByte(OP_ADD);
            emitBytes(OP_SET_LOCAL, counterSlot);
            emitByte(OP_POP);

            // loop
            emitLoop(loopStart);

            endScope();

        }

        void expressionStatement() {
            expression();
            consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
            emitByte(OP_POP);
        }


        // variables and declarations

        void declaration() {

            if(match(TOKEN_DOLLAR)) {
                varDeclaration();
            } else {
                statement();
            }

            if(this->panicMode) synchronize();

        }
        void varDeclaration() {

            uint8_t global = parseVariable("Expect variable name.");

            if(match(TOKEN_EQUAL)) {
                expression();
            } else {
                emitByte(OP_NULL);
            }
            consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

            defineVariable(global);
            
        }

        uint8_t parseVariable(string errorMessage) {
            consume(TOKEN_IDENTIFIER, errorMessage);
            declareVariable();
            if(this->scopeDepth > 0) return 0;
            return identifierConstant(&this->previous);
        }
        uint8_t identifierConstant(Token* name) {
            return makeConstant(CaroObj(copyString(name->start)));
        }

        void declareVariable() {

            if(this->scopeDepth == 0) return;
            Token* name = &this->previous;

            for(int i = this->localCount - 1; i >= 0; --i) {
                Local* local = &this->locals[i];
                if(local->depth != -1 && local->depth < this->scopeDepth) {
                    break; 
                }
                if(name->start == local->name.start) {
                    error("Already a variable with this name in this scope.");
                }
            }

            addLocal(*name);

        }

        void defineVariable(uint8_t global) {
            if(this->scopeDepth > 0) {
                markInitialized();
                return;
            }
            emitBytes(OP_DEFINE_GLOBAL, global);
        }
        void markInitialized() {
            this->locals[this->localCount - 1].depth = this->scopeDepth;
        }

        void addLocal(Token name) {
            // clox has a limit on the number of locals here but we don't because locals is a vector instead of an array
            this->locals.push_back({name, -1});
            ++this->localCount;
        }

        uint8_t makeHiddenLocal(char name) {
            addLocal({TOKEN_IDENTIFIER, string({' ', name}), 2, this->previous.line});
                // names start with a space so there can't be an actual local with the same name
            markInitialized();
            return this->localCount - 1;
        }


        // synchronize

        void synchronize() {

            this->panicMode = false;

            while(this->current.type != TOKEN_EOF) {
                if(this->previous.type == TOKEN_SEMICOLON) return;
                switch(this->current.type) {

                    case TOKEN_CLASS:
                    case TOKEN_FUNC:
                    case TOKEN_DOLLAR:
                    case TOKEN_FOR:
                    case TOKEN_REPEAT:
                    case TOKEN_IF:
                    case TOKEN_WHILE:
                    case TOKEN_PRINT:
                    case TOKEN_TYPEOF:
                    case TOKEN_RETURN:
                        return;

                    default:    // do nothing.
                }
                advance();
            }

        }


        // blocks

        void block() {
            while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
                declaration();
            }
            consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
        }
        void beginScope() {
            this->scopeDepth++;
        }
        void endScope() {
            this->scopeDepth--;
            while(this->localCount > 0 && this->locals[this->localCount - 1].depth > this->scopeDepth) {
                emitByte(OP_POP);
                this->locals.pop_back();
                this->localCount--;
            }
        }


        // logical operators

        void makeAnd(bool canAssign) {

            int endJump = emitJump(OP_JUMP_IF_FALSE);

            emitByte(OP_POP);
            parsePrecedence(PREC_AND);

            patchJump(endJump);

        }

        void makeOr(bool canAssign) {

            int elseJump = emitJump(OP_JUMP_IF_FALSE);
            int endJump = emitJump(OP_JUMP);

            patchJump(elseJump);
            emitByte(OP_POP);

            parsePrecedence(PREC_OR);
            patchJump(endJump);
        
        }


        // variables

        void makeVariable(bool canAssign) {
            namedVariable(this->previous, canAssign);
        }
        void namedVariable(Token name, bool canAssign) {

            uint8_t getOp, setOp;
            int arg = resolveLocal(&name);
            if(arg != -1) {
                getOp = OP_GET_LOCAL;
                setOp = OP_SET_LOCAL;
            } else {
                arg = identifierConstant(&name);
                getOp = OP_GET_GLOBAL;
                setOp = OP_SET_GLOBAL;
            }

            if(canAssign && match(TOKEN_EQUAL)) {
                expression();
                emitBytes(setOp, (uint8_t)arg);
            } else {
                emitBytes(getOp, (uint8_t)arg);
            }

        }

        int resolveLocal(Token* name) {
            for(int i = this->localCount - 1; i >= 0; --i) {
                Local* local = &this->locals[i];
                if(name->start == local->name.start) {
                    if(local->depth == -1) {
                        error("Can't read local variable in its own initializer.");
                    }
                    return i;
                }
            }
            return -1;
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

    // 1 char
    [TOKEN_LEFT_PAREN]    = { &Compiler::makeGrouping, NULL,                  PREC_NONE       },
    [TOKEN_RIGHT_PAREN]   = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_LEFT_BRACE]    = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_RIGHT_BRACE]   = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_DOT]           = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_COMMA]         = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_COLON]         = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_SEMICOLON]     = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_PLUS]          = { NULL,                    &Compiler::makeBinary, PREC_TERM       },
    [TOKEN_MINUS]         = { &Compiler::makeUnary,    &Compiler::makeBinary, PREC_TERM       },
    [TOKEN_STAR]          = { NULL,                    &Compiler::makeBinary, PREC_FACTOR     },
    [TOKEN_SLASH]         = { NULL,                    &Compiler::makeBinary, PREC_FACTOR     },
    [TOKEN_PERCENT]       = { NULL,                    &Compiler::makeBinary, PREC_FACTOR     },
    [TOKEN_CARET]         = { NULL,                    &Compiler::makeBinary, PREC_POWER      },
    [TOKEN_AMPERSAND]     = { NULL,                    &Compiler::makeAnd,    PREC_AND        },
    [TOKEN_PIPE]          = { NULL,                    &Compiler::makeOr,     PREC_OR         },
    [TOKEN_DOLLAR]        = { NULL,                    NULL,                  PREC_NONE       },

    // 1 or 2 chars
    [TOKEN_BANG]          = { &Compiler::makeUnary,    NULL,                  PREC_NONE       },
    [TOKEN_BANG_EQUAL]    = { NULL,                    &Compiler::makeBinary, PREC_EQUALITY   },
    [TOKEN_EQUAL]         = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_EQUAL_EQUAL]   = { NULL,                    &Compiler::makeBinary, PREC_EQUALITY   },
    [TOKEN_LESS]          = { NULL,                    &Compiler::makeBinary, PREC_COMPARISON },
    [TOKEN_LESS_EQUAL]    = { NULL,                    &Compiler::makeBinary, PREC_COMPARISON },
    [TOKEN_GREATER]       = { NULL,                    &Compiler::makeBinary, PREC_COMPARISON },
    [TOKEN_GREATER_EQUAL] = { NULL,                    &Compiler::makeBinary, PREC_COMPARISON },
    [TOKEN_SPACESHIP]     = { NULL,                    &Compiler::makeBinary, PREC_COMPARISON },

    // literals
    [TOKEN_IDENTIFIER]    = { &Compiler::makeVariable, NULL,                  PREC_NONE       },
    [TOKEN_STRING]        = { &Compiler::parseString,  NULL,                  PREC_NONE       },
    [TOKEN_NUMBER]        = { &Compiler::parseNumber,  NULL,                  PREC_NONE       },

    // keywords
    [TOKEN_FUNC]          = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_RETURN]        = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_CLASS]         = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_THIS]          = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_SUPER]         = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_IF]            = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_ELSE]          = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_FOR]           = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_WHILE]         = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_REPEAT]        = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_FOREVER]       = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_TRUE]          = { &Compiler::parseLiteral, NULL,                  PREC_NONE       },
    [TOKEN_FALSE]         = { &Compiler::parseLiteral, NULL,                  PREC_NONE       },
    [TOKEN_NULL]          = { &Compiler::parseLiteral, NULL,                  PREC_NONE       },
    [TOKEN_SMTH]          = { &Compiler::parseLiteral, NULL,                  PREC_NONE       },
    [TOKEN_PRINT]         = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_TYPEOF]        = { &Compiler::makeUnary,    NULL,                  PREC_NONE       },

    // misc
    [TOKEN_ERROR]         = { NULL,                    NULL,                  PREC_NONE       },
    [TOKEN_EOF]           = { NULL,                    NULL,                  PREC_NONE       },

};

inline ParseRule* Compiler::getRule(TokenType type) {
    return &rules[type];
}