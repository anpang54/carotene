

// includes

#include <fstream>
#include <sstream>

#include "common.hpp"
#include "chunk.hpp"
#include "vm.hpp"
#include "compiler.hpp"

using std::cout, std::cin, std::cerr, std::string, std::ifstream, std::stringstream;


// version

#define VERSION      "0.1.0a"
#define VERSION_DATE "21 Aug 2026"


// interpret

InterpretResult interpret(string source) {
    VM vm;
    InterpretResult result = vm.interpret(source);
    freeObjects();
    return result;
}


// input

void repl() {
    string line;
    for(;;) {
        cout << "> ";
        std::getline(std::cin, line);
        interpret(line);
    }
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
    if(argv[1][0] == '-') {
        option = argv[1][1];
        if(argc >= 3) {
            filename = argv[2];
        }
    } else {
        filename = argv[1];
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