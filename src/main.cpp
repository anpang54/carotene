

// includes

#include <fstream>
#include <sstream>

#ifdef __linux__
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "core/common.hpp"
#include "core/chunk.hpp"
#include "core/vm.hpp"
#include "core/compiler.hpp"

using std::cout, std::cin, std::cerr, std::string, std::ifstream, std::stringstream;


// version

#define VERSION      "0.1.0b"
#define VERSION_DATE "24 Aug 2026"


// input

VM vm;

InterpretResult interpret(string source) {
    InterpretResult result = vm.interpret(source);
    return result;
}

void repl() {
    for(;;) {

        // get line
        string source;
        #ifdef __linux__
            // linux uses readline
            char* line = readline("> ");
            if(line == nullptr) break;
            if(*line) add_history(line);
            source = line;
            free(line);
        #else
            // windows doesn't have readline so it just gets plain getline
            cout << "> ";
            if(!std::getline(cin, source)) break;
        #endif

        // interpret
        interpret(source);

    }
    freeObjects();
}

void runFile(string path) {

    ifstream file(path);

    if(!file.is_open()) {
        cerr << "Couldn't open the file!\n";
        exit(0);
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();

    interpret(content);

}


// main

int main(int argc, const char* argv[]) {
    
    // no arguments, repl
    if(argc <= 1) {
        repl();
        return 1;
    }

    // get arguments
    char option = ' ';
    string filename;
    uint moreArgumentsStart;
    if(argv[1][0] == '-') {
        option = argv[1][1];
        if(argc >= 3) {
            filename = argv[2];
        }
        moreArgumentsStart = 3;
    } else {
        filename = argv[1];
        moreArgumentsStart = 2;
    }

    // store additional arguments for the script
    for(int i = moreArgumentsStart; i < argc; ++i) {
        moreArguments.push_back(argv[i]);
    }

    // do something
    switch(option) {

        case ' ':
            runFile(filename);
            break;

        case 'c':
        case 'r':
        case 't':
        case 'h':
            cout << "Not implemented but the filename is " << filename << '\n';
            break;

        case 'v':
            cout << "Carotene v" << VERSION << " (" << VERSION_DATE << ")\n"
                    "https://github.com/anpang54/carotene\n";
            break;

        default:
            cout << "Invalid option -" << option << ".\n";

    }

    return 0;
}