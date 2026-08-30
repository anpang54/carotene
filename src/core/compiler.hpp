
#pragma once


// INCLUDES

#include <string_view>

#include "common.hpp"
#include "scanner.hpp"
#include "object.hpp"


// SETUP


// precedence

enum Precedence{
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_TERNARY,     // c? t: f
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

enum FunctionType{
    TYPE_FUNCTION,
    TYPE_SCRIPT
};

struct FunctionState{
    ObjFunction* function;
    FunctionType type;
    vector<Local> locals;
    int localCount;
    int scopeDepth;
};



// COMPILER

class Compiler{

    public:


        // variables

        vector<FunctionState> states;
        FunctionState& cur() { return this->states.back(); }

        Scanner scanner;
        Token current;
        Token previous;

        bool hadError = false;
        bool panicMode = false;    // suppresses other errors

        int lastCmpOffset = -1;    // code offset of the last comparison opcode

        inline static set<string> usedModules;

        Chunk* currentChunk() {
            return &cur().function->chunk;
        }
            // I guess I do need currentChunk()


        // init/compile

        void initCompiler(FunctionType type) {
            
            FunctionState& state = this->states.emplace_back();
            state.function = newFunction();
            state.type = type;
            state.scopeDepth = 0;

            this->lastCmpOffset = -1;    // now emitting into a different chunk

            if(type != TYPE_SCRIPT) state.function->name = this->previous.start;

            Local& local = state.locals.emplace_back();
            state.localCount = 1;
            local.depth = 0;
            local.name.start = "";
            local.name.length = 0;


        }
        ObjFunction* compile(string source, vector<Local>* replLocals = nullptr) {

            GCPause pause;

            this->scanner = Scanner(source);

            initCompiler(TYPE_SCRIPT);
            beginScope();    // make the top level have its own scope

            // put the previous line's locals into this compiler so that the repl is continuous
            if(replLocals != nullptr) {
                for(const Local& local: *replLocals) {
                    cur().locals.push_back(local);
                    ++cur().localCount;
                }
            }

            this->advance();
            while(!match(TOKEN_EOF)) {
                this->declaration();
            }

            // save this compiler's locals for the next one
            if(replLocals != nullptr && !this->hadError) {
                *replLocals = vector<Local>(cur().locals.begin() + 1, cur().locals.end());
            }

            ObjFunction* function = this->endCompiler();

            return this->hadError? NULL: function;

        }


        // errors

        void errorAt(Token* token, string message) {

            if(this->panicMode) return;
            this->panicMode = true;

            cerr << "\033[38;5;203m[line " << token->line << "] Error";

            if(token->type == TOKEN_EOF) {
                cerr << " at end";
            } else if(token->type == TOKEN_ERROR) {
                // nothing
            } else {
                cerr << " at '" << token->start << '\'';
            }

            cerr << ": " << message << "\n\033[0m";

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
            currentChunk()->write(byte, this->previous.line);
        }
        void emitBytes(uint8_t byte1, uint8_t byte2) {
            emitByte(byte1);
            emitByte(byte2);
        }

        void emitConstant(Value value) {
            emitBytes(OP_CONSTANT, makeConstant(value));
        }
        void emitNumber(double value) {
            emitConstant(CaroNumber(TYPE_INT, value));
                // todo: emit a specific number type depending on the literal
        }

        int emitJump(uint8_t instruction) {
            emitByte(instruction);
            emitByte(0xff);
            emitByte(0xff);
            return currentChunk()->code.size() - 2;
        }
        void emitLoop(int loopStart) {

            this->lastCmpOffset = -1;

            emitByte(OP_LOOP);

            int offset = currentChunk()->code.size() - loopStart + 2;

            emitByte((offset >> 8) & 0xff);
            emitByte(offset & 0xff);

        }

        struct ConditionJump{
            int offset;
            bool fused;
        };
        ConditionJump emitConditionJump() {
            // fuse a trailing comparison into a single compare-and-jump instruction

            if(this->lastCmpOffset == (int)currentChunk()->code.size() - 1) {

                uint8_t fusedOp;
                switch(currentChunk()->code.back()) {
                    case OP_LESS:          fusedOp = OP_JUMP_IF_NOT_LESS;          break;
                    case OP_LESS_EQUAL:    fusedOp = OP_JUMP_IF_NOT_LESS_EQUAL;    break;
                    case OP_GREATER:       fusedOp = OP_JUMP_IF_NOT_GREATER;       break;
                    case OP_GREATER_EQUAL: fusedOp = OP_JUMP_IF_NOT_GREATER_EQUAL; break;
                    case OP_EQUAL:         fusedOp = OP_JUMP_IF_NOT_EQUAL;         break;
                    case OP_NOT_EQUAL:     fusedOp = OP_JUMP_IF_EQUAL;             break;
                    default: return {emitJump(OP_JUMP_IF_FALSE), false};    // unreachable
                }

                currentChunk()->code.pop_back();
                currentChunk()->lines.pop_back();
                return {emitJump(fusedOp), true};

            }

            return {emitJump(OP_JUMP_IF_FALSE), false};

        }

        bool tryFuseIncrement(int startOffset) {
            // fuse i = i + 1 into a single increment/decrement instruction

            Chunk* chunk = currentChunk();
            if((int)chunk->code.size() - startOffset != 8) return false;
            const uint8_t* seq = chunk->code.data() + startOffset;

            // match [GET var][CONSTANT step][ADD or SUBTRACT][SET var][POP]
            bool local  = seq[0] == OP_GET_LOCAL  && seq[5] == OP_SET_LOCAL;
            bool global = seq[0] == OP_GET_GLOBAL && seq[5] == OP_SET_GLOBAL;
            if(!local && !global) return false;
            if(seq[2] != OP_CONSTANT || (seq[4] != OP_ADD && seq[4] != OP_SUBTRACT) || seq[7] != OP_POP) return false;

            if(local && seq[1] != seq[6]) return false;
            if(global && asString(chunk->constants[seq[1]])->str != asString(chunk->constants[seq[6]])->str) return false;

            if(chunk->constants[seq[3]].type != TYPE_INT) return false;

            uint8_t variable = seq[1];
            uint8_t step = seq[3];
            uint8_t op = seq[4] == OP_ADD
                ? (local? OP_INCREMENT_LOCAL: OP_INCREMENT_GLOBAL)
                : (local? OP_DECREMENT_LOCAL: OP_DECREMENT_GLOBAL);

            // replace the 8 byte sequence with 1 instruction
            for(int i = 0; i < 8; ++i) {
                chunk->code.pop_back();
                chunk->lines.pop_back();
            }
            emitBytes(op, variable);
            emitByte(step);

            return true;

        }

        void emitForLoop(uint8_t counterSlot, uint8_t limitSlot, uint8_t step, int bodyStart) {

            this->lastCmpOffset = -1;

            emitBytes(OP_FOR_LOOP, counterSlot);
            emitBytes(limitSlot, step);

            int offset = currentChunk()->code.size() - bodyStart + 2;

            emitByte((offset >> 8) & 0xff);
            emitByte(offset & 0xff);

        }

        void emitReturn() {
            emitBytes(OP_NULL, OP_RETURN);
        }


        // each part

        void makeGrouping(bool canAssign) {
            expression();
            consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
        }

        uint8_t makeConstant(Value value) {
            int constant = currentChunk()->addConstant(value);
            if(constant > UINT8_MAX) {
                error("Too many constants in one chunk.");
                return 0;
            }
            return (uint8_t)constant;
        }

        void emitComparison(uint8_t op) {
            this->lastCmpOffset = currentChunk()->code.size();
            emitByte(op);
        }

        void makeUnary(bool canAssign) {
            TokenType operatorType = this->previous.type;
            parsePrecedence(PREC_UNARY);    // compile the operand
            switch (operatorType) {    // emit the operator instruction
                case TOKEN_MINUS:  emitByte(OP_NEGATE); break;
                case TOKEN_BANG:   emitByte(OP_NOT);    break;
                case TOKEN_TYPEOF: emitByte(OP_TYPEOF); break;
                case TOKEN_SIZEOF: emitByte(OP_SIZEOF); break;
                default: return;    // unreachable.
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

                case TOKEN_EQUAL_EQUAL:   emitComparison(OP_EQUAL);         break;
                case TOKEN_BANG_EQUAL:    emitComparison(OP_NOT_EQUAL);     break;
                case TOKEN_LESS:          emitComparison(OP_LESS);          break;
                case TOKEN_LESS_EQUAL:    emitComparison(OP_LESS_EQUAL);    break;
                case TOKEN_GREATER:       emitComparison(OP_GREATER);       break;
                case TOKEN_GREATER_EQUAL: emitComparison(OP_GREATER_EQUAL); break;
                case TOKEN_SPACESHIP:     emitByte(OP_SPACESHIP);           break;
                
                default: return;    // unreachable

            }

        }
        
        void parseNumber(bool canAssign) {

            const string& text = this->previous.start;

            // non-decimal base, int only
            if(text.length() > 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'o' || text[1] == 'x')) {

                int base = text[1] == 'b'? 2: (text[1] == 'o'? 8: 16);
                string digits = text.substr(2);    // remove the prefix

                switch(text.back()) {
                    case 'u':
                        emitConstant(CaroNumber(TYPE_UINT, std::stoul(digits, nullptr, base)));
                        break;
                    case 'l':
                        if(text[text.length() - 2] == 'u') {
                            emitConstant(CaroNumber(TYPE_ULONG, std::stoull(digits, nullptr, base)));
                        }
                        else emitConstant(CaroNumber(TYPE_LONG, std::stoll(digits, nullptr, base)));
                        break;
                    default:
                        emitConstant(CaroNumber(TYPE_INT, std::stoll(digits, nullptr, base)));
                }

                return;
            }

            // decimal base, can be float
            switch(text.back()) {
                case 'b':
                    emitConstant(CaroNumber(TYPE_BYTE, std::stoul(text)));
                                                    // why does C++ not have a std::stou()???
                    break;
                case 'u':
                    emitConstant(CaroNumber(TYPE_UINT, std::stoul(text)));
                    break;
                case 'l':
                    if(text[text.length() - 2] == 'u') {
                        emitConstant(CaroNumber(TYPE_ULONG, std::stoull(text)));
                    }
                    else emitConstant(CaroNumber(TYPE_LONG, std::stoll(text)));
                    break;
                case 'f':
                    emitConstant(CaroNumber(TYPE_FLOAT, std::stof(text)));
                    break;
                case 'd':
                    emitConstant(CaroNumber(TYPE_DOUBLE, std::stod(text)));
                    break;
                default:
                    if(text.find('.') != string::npos) {
                        emitConstant(CaroNumber(TYPE_DOUBLE, std::stod(text)));
                        // emitting a float would be more consistent with the fact that ints become 32 bit ints
                        // but in C++ something like this becomes a double, and floats can lose precision
                    } else {
                        emitConstant(CaroNumber(TYPE_INT, std::stod(text)));
                    }
            }

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

        void parseArray(bool canAssign) {
            uint8_t elementCount = 0;
            if(!check(TOKEN_RIGHT_SQUARE)) {
                do{
                    expression();
                    if(elementCount == 255) {
                        error("An array literal can currently only have 255 elements.");
                    }
                        // todo: allow more than 255 elements in a literal
                    ++elementCount;
                } while(match(TOKEN_COMMA));
            }
            consume(TOKEN_RIGHT_SQUARE, "Expect ']' after array contents.");
            emitBytes(OP_MAKE_ARRAY, elementCount);
        }

        void parseDict(bool canAssign) {
            uint8_t elementCount = 0;
            if(!check(TOKEN_RIGHT_BRACE)) {
                do{
                    expression();    // key
                    consume(TOKEN_COLON, "Expect ':' between key and value.");
                    expression();    // value
                    if(elementCount == 255) {
                        error("A dict literal can currently only have 255 pairs.");
                    }
                        // todo: allow more than 255 pairs in a literal
                    ++elementCount;
                } while(match(TOKEN_COMMA));
            }
            consume(TOKEN_RIGHT_BRACE, "Expect '}' after dict contents.");
            emitBytes(OP_MAKE_DICT, elementCount);
        }

        void makeSubscript(bool canAssign) {
            expression();
            consume(TOKEN_RIGHT_SQUARE, "Expect ']' after index.");
            if(canAssign && match(TOKEN_EQUAL)) {
                expression();
                emitByte(OP_SET_INDEX);
            } else {
                emitByte(OP_GET_INDEX);
            }
        }


        // precedence

        void parsePrecedence(Precedence precedence) {
            advance();
            parseFromPrevious(precedence);
        }
        void parseFromPrevious(Precedence precedence) {
            // like parsePrecedence, but the expression's first token is already in previous

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
            if(match(TOKEN_NAME)) {
                nameStatement();
            } else if(match(TOKEN_DESC)) {
                descStatement();
            } else if(match(TOKEN_VERSION)) {
                versionStatement();
            } else if(match(TOKEN_USE)) {
                useStatement();
            } else if(match(TOKEN_FOR)) {
                forStatement();
            } else if(match(TOKEN_REPEAT)) {
                repeatStatement();
            } else if(match(TOKEN_IF)) {
                ifStatement();
            } else if (match(TOKEN_RETURN)) {
                returnStatement();
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

        void nameStatement() {

            consume(TOKEN_STRING, "Expect app name after 'set_name'.");
            if(this->panicMode) return;

            string name = this->previous.start.substr(1, this->previous.length - 2);
            emitBytes(OP_NAME, makeConstant(CaroObj(copyString(name))));

            consume(TOKEN_SEMICOLON, "Expect ';' after app name.");

        }
        void descStatement() {

            consume(TOKEN_STRING, "Expect app description after 'set_desc'.");
            if(this->panicMode) return;

            string desc = this->previous.start.substr(1, this->previous.length - 2);
            emitBytes(OP_DESC, makeConstant(CaroObj(copyString(desc))));

            consume(TOKEN_SEMICOLON, "Expect ';' after app description.");

        }
        void versionStatement() {

            consume(TOKEN_STRING, "Expect app version after 'set_version'.");
            if(this->panicMode) return;

            string version = this->previous.start.substr(1, this->previous.length - 2);
            emitBytes(OP_VERSION, makeConstant(CaroObj(copyString(version))));

            consume(TOKEN_SEMICOLON, "Expect ';' after app version.");

        }

        void useStatement() {

            consume(TOKEN_IDENTIFIER, "Expect module name after 'use'.");

            string name = this->previous.start;
            if(!modules.contains(name)) {
                error("Module " + name + " doesn't exist.");
            } else {
                this->usedModules.insert(name);
            }

            consume(TOKEN_SEMICOLON, "Expect ';' after module name.");

        }

        void forStatement() {

            beginScope();

            consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
            if (match(TOKEN_SEMICOLON)) {
                // no initializer
            } else if(check(TOKEN_IDENTIFIER)) {
                identifierStatement();
            } else {
                expressionStatement();
            }

            int loopStart = currentChunk()->code.size();

            int exitJump = -1;
            bool exitFused = false;

            if(!match(TOKEN_SEMICOLON)) {

                expression();
                consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

                // jump out of the loop if the condition is false
                ConditionJump condJump = emitConditionJump();
                exitJump = condJump.offset;
                exitFused = condJump.fused;
                if(!exitFused) emitByte(OP_POP);

            }

            vector<uint8_t>& code = currentChunk()->code;
            bool condIsForLoop = exitFused
                              && (int)code.size() - loopStart == 7
                              && code[loopStart] == OP_GET_LOCAL
                              && code[loopStart + 2] == OP_GET_LOCAL
                              && code[loopStart + 4] == OP_JUMP_IF_NOT_LESS;
            uint8_t counterSlot = condIsForLoop? code[loopStart + 1]: 0;
            uint8_t limitSlot   = condIsForLoop? code[loopStart + 3]: 0;

            bool isForLoop = false;
            uint8_t stepConstant = 0;

            if(!match(TOKEN_RIGHT_PAREN)) {

                int bodyJump = emitJump(OP_JUMP);
                int incrementStart = currentChunk()->code.size();
                expression();
                emitByte(OP_POP);
                bool incrementFused = tryFuseIncrement(incrementStart);
                consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

                isForLoop = condIsForLoop
                         && incrementFused
                         && code[incrementStart] == OP_INCREMENT_LOCAL
                         && code[incrementStart + 1] == counterSlot;

                if(isForLoop) {

                    // remove the fused increment (3 bytes) and the body jump (3 bytes)
                    // OP_FOR_LOOP does the increment itself, right before the body's loop-back
                    stepConstant = code[incrementStart + 2];
                    for(int i = 0; i < 6; ++i) {
                        code.pop_back();
                        currentChunk()->lines.pop_back();
                    }

                } else {

                    emitLoop(loopStart);
                    loopStart = incrementStart;
                    patchJump(bodyJump);

                }

            }

            int bodyStart = currentChunk()->code.size();
            statement();

            if(isForLoop) {
                emitForLoop(counterSlot, limitSlot, stepConstant, bodyStart);
            } else {
                emitLoop(loopStart);
            }

            if(exitJump != -1) {
                patchJump(exitJump);
                if(!exitFused) emitByte(OP_POP);
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
                consume(TOKEN_IDENTIFIER, "Expect variable name after ':'.");
                if(this->previous.start[0] == '$') error("The counter variable must be local.");
                declareVariable();
                markInitialized();
            } else {
                makeHiddenLocal('r');
            }
            uint8_t counterSlot = countSlot + 1;
            emitNumber(0);

            // check counter < count
            emitBytes(OP_GET_LOCAL, counterSlot);
            emitBytes(OP_GET_LOCAL, countSlot);
            int exitJump = emitJump(OP_JUMP_IF_NOT_LESS);

            // stuff inside the block
            int bodyStart = currentChunk()->code.size();
            statement();

            // increment counter
            emitForLoop(counterSlot, countSlot, makeConstant(CaroInt(1)), bodyStart);
            patchJump(exitJump);

            endScope();

        }

        void ifStatement() {

            consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
            expression();
            consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition."); 

            ConditionJump thenJump = emitConditionJump();
            if(!thenJump.fused) emitByte(OP_POP);
            statement();

            int elseJump = emitJump(OP_JUMP);
            patchJump(thenJump.offset);
            if(!thenJump.fused) emitByte(OP_POP);

            if(match(TOKEN_ELSE)) statement();
            patchJump(elseJump);
        
        }
        void patchJump(int offset) {

            int jump = currentChunk()->code.size() - offset - 2;
                // -2 to adjust for the bytecode for the jump offset itself

            currentChunk()->code[offset] = (jump >> 8) & 0xff;
            currentChunk()->code[offset + 1] = jump & 0xff;

            this->lastCmpOffset = -1;

        }

        void returnStatement() {

            if(cur().type == TYPE_SCRIPT) {
                error("Can't return from top-level code.");
            }

            if(match(TOKEN_SEMICOLON)) {
                emitReturn();
            } else {
                expression();
                consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
                emitByte(OP_RETURN);
            }

        }

        void whileStatement() {

            int loopStart = currentChunk()->code.size();

            consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
            expression();
            consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

            ConditionJump exitJump = emitConditionJump();
            if(!exitJump.fused) emitByte(OP_POP);
            statement();
            emitLoop(loopStart);

            patchJump(exitJump.offset);
            if(!exitJump.fused) emitByte(OP_POP);

        }

        void foreverStatement() {
            // derived from repeatStatement, not whileStatement, cuz it needs the counter variable thingy

            beginScope();

            // consume and declare the counter variable
            if(match(TOKEN_COLON)) {
                consume(TOKEN_IDENTIFIER, "Expect variable name after ':'.");
                if(this->previous.start[0] == '$') error("The counter variable must be local.");
                declareVariable();
                markInitialized();
            } else {
                makeHiddenLocal('r');
            }
            uint8_t counterSlot = cur().localCount - 1;
            emitNumber(0);

            int loopStart = currentChunk()->code.size();

            // stuff inside the block
            statement();

            // increment counter
            emitBytes(OP_INCREMENT_LOCAL, counterSlot);
            emitByte(makeConstant(CaroInt(1)));

            // loop
            emitLoop(loopStart);

            endScope();

        }

        void expressionStatement() {
            int start = currentChunk()->code.size();
            expression();
            consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
            emitByte(OP_POP);
            tryFuseIncrement(start);
        }


        // variables and declarations

        void declaration() {

            if(match(TOKEN_FUNC)) {
                funcDeclaration();
            } else if(check(TOKEN_IDENTIFIER)) {
                identifierStatement();
            } else {
                statement();
            }

            if(this->panicMode) synchronize();

        }

        void funcDeclaration() {
            consume(TOKEN_IDENTIFIER, "Expect function name.");
            if(this->previous.start[0] == '$') {
                error("Function names can't have a '$' as they are already global.");
            } else if(this->previous.start[0] == '#') {
                error("Function names can't have a '#' as they are already constant.");
            }
            uint8_t global = identifierConstant(&this->previous);
            makeFunction(TYPE_FUNCTION);
            emitBytes(OP_DEFINE_GLOBAL, global);
        }
            // functions are first-class values

        void identifierStatement() {

            advance();    // consume the identifier

            if(
                this->previous.start[0] != '$' && this->previous.start[0] != '#'
                && check(TOKEN_EQUAL) && resolveLocal(&this->previous) == -1
            ) {
                varDeclaration();
                return;
            }

            int start = currentChunk()->code.size();
            parseFromPrevious(PREC_ASSIGNMENT);
            consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
            emitByte(OP_POP);
            tryFuseIncrement(start);

        }

        void varDeclaration() {
            
            declareVariable();
            advance();    // consume =
            expression();
            consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
            markInitialized();

        }

        uint8_t parseVariable(string errorMessage) {
            consume(TOKEN_IDENTIFIER, errorMessage);
            if(this->previous.start[0] == '$' || this->previous.start[0] == '#') {
                error("Parameter names can't start with '$' or '#'");
            }
            declareVariable();
            if(cur().scopeDepth > 0) return 0;
            return identifierConstant(&this->previous);
        }
        uint8_t identifierConstant(Token* name) {
            return makeConstant(CaroObj(copyString(name->start)));
        }

        void declareVariable() {

            if(cur().scopeDepth == 0) return;
            Token* name = &this->previous;

            for(int i = cur().localCount - 1; i >= 0; --i) {
                Local* local = &cur().locals[i];
                if(local->depth != -1 && local->depth < cur().scopeDepth) {
                    break; 
                }
                if(name->start == local->name.start) {
                    error("Already a variable with this name in this scope.");
                }
            }

            addLocal(*name);

        }

        void defineVariable(uint8_t global) {
            if(cur().scopeDepth > 0) {
                markInitialized();
                return;
            }
            emitBytes(OP_DEFINE_GLOBAL, global);
        }
        void markInitialized() {
            if (cur().scopeDepth == 0) return;
            cur().locals[cur().localCount - 1].depth = cur().scopeDepth;
        }

        void addLocal(Token name) {
            // clox has a limit on the number of locals here but we don't because locals is a vector instead of an array
            cur().locals.push_back({name, -1});
            ++cur().localCount;
        }

        uint8_t makeHiddenLocal(char name) {
            addLocal({TOKEN_IDENTIFIER, string({' ', name}), 2, this->previous.line});
                // names start with a space so there can't be an actual local with the same name
            markInitialized();
            return cur().localCount - 1;
        }


        // synchronize

        void synchronize() {

            this->panicMode = false;

            while(this->current.type != TOKEN_EOF) {
                if(this->previous.type == TOKEN_SEMICOLON) return;
                switch(this->current.type) {

                    case TOKEN_CLASS:
                    case TOKEN_FUNC:
                    case TOKEN_FOR:
                    case TOKEN_REPEAT:
                    case TOKEN_IF:
                    case TOKEN_WHILE:
                    case TOKEN_TYPEOF:
                    case TOKEN_SIZEOF:
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
            cur().scopeDepth++;
        }
        void endScope() {
            cur().scopeDepth--;
            while(cur().localCount > 0 && cur().locals[cur().localCount - 1].depth > cur().scopeDepth) {
                emitByte(OP_POP);
                cur().locals.pop_back();
                cur().localCount--;
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

        void makeTernary(bool canAssign) {

            ConditionJump thenJump = emitConditionJump();
            if(!thenJump.fused) emitByte(OP_POP);

            parsePrecedence(PREC_TERNARY);    // if true
            int endJump = emitJump(OP_JUMP);

            patchJump(thenJump.offset);
            if(!thenJump.fused) emitByte(OP_POP);

            if(match(TOKEN_COLON)) {
                parsePrecedence(PREC_TERNARY);    // if false
            } else {
                emitByte(OP_NULL);    // false = null
            }

            patchJump(endJump);

        }

        // functions

        void makeFunction(FunctionType type) {

            // init compiler
            initCompiler(type);

            beginScope(); 

            // parameter list
            consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
            if(!check(TOKEN_RIGHT_PAREN)) {
                do{
                    cur().function->arity++;
                    uint8_t constant = parseVariable("Expect parameter name.");
                    defineVariable(constant);
                } while (match(TOKEN_COMMA));
            }
            consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

            // function body
            consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
            block();

            ObjFunction* function = endCompiler();
            emitBytes(OP_CONSTANT, makeConstant(CaroObj(function)));
            
        }

        void makeCall(bool canAssign) {
            uint8_t argCount = argumentList();
            emitBytes(OP_CALL, argCount);
        }
        uint8_t argumentList() {
            uint8_t argCount = 0;
            if(!check(TOKEN_RIGHT_PAREN)) {
                do{
                    expression();
                    if(argCount == 255) {
                        error("Can't have more than 255 arguments.");
                    }
                    argCount++;
                } while (match(TOKEN_COMMA));
            }
            consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
            return argCount;
        }


        // variables

        void makeVariable(bool canAssign) {

            // module function
            if(check(TOKEN_DOT) && modules.contains(this->previous.start)) {
                moduleFunction();
                return;
            }

            // actual variable
            namedVariable(this->previous, canAssign);

        }
        void namedVariable(Token name, bool canAssign) {

            uint8_t getOp, setOp;
            int arg;

            if(name.start[0] == '$' || name.start[0] == '#') {
                arg = identifierConstant(&name);
                getOp = OP_GET_GLOBAL;
                setOp = name.start[0] == '#'? OP_DEFINE_CONSTANT: OP_SET_GLOBAL;
            } else {
                arg = resolveLocal(&name);
                if(arg != -1) {
                    getOp = OP_GET_LOCAL;
                    setOp = OP_SET_LOCAL;
                } else {
                    if(canAssign && check(TOKEN_EQUAL)) {
                        error("Undefined variable '" + name.start + "'.");
                    }
                    arg = identifierConstant(&name);
                    getOp = OP_GET_GLOBAL;
                    setOp = OP_SET_GLOBAL;
                }
            }

            if(canAssign && match(TOKEN_EQUAL)) {
                expression();
                emitBytes(setOp, (uint8_t)arg);
            } else {
                emitBytes(getOp, (uint8_t)arg);
            }

        }

        void moduleFunction() {

            string module = this->previous.start;

            advance();    // eat .
            consume(TOKEN_IDENTIFIER, "Expect function name after '.'.");
            string name = module + '.' + this->previous.start;

            if(!this->usedModules.contains(module)) {
                error("Module " + module + " hasn't been included");
            } else if(!nativeNames.contains(name)) {
                error(name + "() doesn't exist.");
            }

            emitBytes(OP_GET_GLOBAL, makeConstant(CaroObj(copyString(name))));

        }

        int resolveLocal(Token* name) {
            for(int i = cur().localCount - 1; i >= 0; --i) {
                Local* local = &cur().locals[i];
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

        ObjFunction* endCompiler() {

            emitReturn();
            ObjFunction* function = cur().function;

            if(DEBUG_PRINT_CODE) {
                if(!this->hadError) {
                    currentChunk()->disassemble(function->name.empty()? "<script>": function->name);
                }
            }

            this->states.pop_back();
            return function;

        }


};


// parse rules

inline ParseRule rules[] = {

//   token                   prefix                    infix                  precedence

    // 1 char
    [TOKEN_LEFT_PAREN]    = { &Compiler::makeGrouping, &Compiler::makeCall,      PREC_CALL       },    // ( is an infix operator for function calls
    [TOKEN_RIGHT_PAREN]   = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_LEFT_SQUARE]   = { &Compiler::parseArray,   &Compiler::makeSubscript, PREC_CALL       },    // [ is an infix operator for indexing
    [TOKEN_RIGHT_SQUARE]  = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_LEFT_BRACE]    = { &Compiler::parseDict,    NULL,                     PREC_NONE       },
    [TOKEN_RIGHT_BRACE]   = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_DOT]           = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_COMMA]         = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_COLON]         = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_SEMICOLON]     = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_QUESTION]      = { NULL,                    &Compiler::makeTernary,   PREC_TERNARY    },
    [TOKEN_PLUS]          = { NULL,                    &Compiler::makeBinary,    PREC_TERM       },
    [TOKEN_MINUS]         = { &Compiler::makeUnary,    &Compiler::makeBinary,    PREC_TERM       },
    [TOKEN_STAR]          = { NULL,                    &Compiler::makeBinary,    PREC_FACTOR     },
    [TOKEN_SLASH]         = { NULL,                    &Compiler::makeBinary,    PREC_FACTOR     },
    [TOKEN_PERCENT]       = { NULL,                    &Compiler::makeBinary,    PREC_FACTOR     },
    [TOKEN_CARET]         = { NULL,                    &Compiler::makeBinary,    PREC_POWER      },
    [TOKEN_AMPERSAND]     = { NULL,                    &Compiler::makeAnd,       PREC_AND        },
    [TOKEN_PIPE]          = { NULL,                    &Compiler::makeOr,        PREC_OR         },

    // 1 or 2 chars
    [TOKEN_BANG]          = { &Compiler::makeUnary,    NULL,                     PREC_NONE       },
    [TOKEN_BANG_EQUAL]    = { NULL,                    &Compiler::makeBinary,    PREC_EQUALITY   },
    [TOKEN_EQUAL]         = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_EQUAL_EQUAL]   = { NULL,                    &Compiler::makeBinary,    PREC_EQUALITY   },
    [TOKEN_LESS]          = { NULL,                    &Compiler::makeBinary,    PREC_COMPARISON },
    [TOKEN_LESS_EQUAL]    = { NULL,                    &Compiler::makeBinary,    PREC_COMPARISON },
    [TOKEN_GREATER]       = { NULL,                    &Compiler::makeBinary,    PREC_COMPARISON },
    [TOKEN_GREATER_EQUAL] = { NULL,                    &Compiler::makeBinary,    PREC_COMPARISON },
    [TOKEN_SPACESHIP]     = { NULL,                    &Compiler::makeBinary,    PREC_COMPARISON },

    // literals
    [TOKEN_IDENTIFIER]    = { &Compiler::makeVariable, NULL,                     PREC_NONE       },
    [TOKEN_STRING]        = { &Compiler::parseString,  NULL,                     PREC_NONE       },
    [TOKEN_NUMBER]        = { &Compiler::parseNumber,  NULL,                     PREC_NONE       },

    // keywords
    [TOKEN_NAME]          = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_DESC]          = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_VERSION]       = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_USE]           = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_INCLUDE]       = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_FUNC]          = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_RETURN]        = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_CLASS]         = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_THIS]          = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_SUPER]         = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_IF]            = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_ELSE]          = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_FOR]           = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_WHILE]         = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_REPEAT]        = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_FOREVER]       = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_TRUE]          = { &Compiler::parseLiteral, NULL,                     PREC_NONE       },
    [TOKEN_FALSE]         = { &Compiler::parseLiteral, NULL,                     PREC_NONE       },
    [TOKEN_NULL]          = { &Compiler::parseLiteral, NULL,                     PREC_NONE       },
    [TOKEN_SMTH]          = { &Compiler::parseLiteral, NULL,                     PREC_NONE       },
    [TOKEN_TYPEOF]        = { &Compiler::makeUnary,    NULL,                     PREC_NONE       },
    [TOKEN_SIZEOF]        = { &Compiler::makeUnary,    NULL,                     PREC_NONE       },

    // misc
    [TOKEN_ERROR]         = { NULL,                    NULL,                     PREC_NONE       },
    [TOKEN_EOF]           = { NULL,                    NULL,                     PREC_NONE       },

};

inline ParseRule* Compiler::getRule(TokenType type) {
    return &rules[type];
}