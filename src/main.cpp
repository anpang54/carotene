

// includes

#include "common.hpp"
#include "chunk.hpp"
#include "vm.hpp"

using std::cout, std::cin, std::string;


// version

#define VERSION      "0.1.0a"
#define VERSION_DATE "21 Aug 2026"


// main

int main(int argc, const char* argv[]) {
    

    VM vm;
    Chunk chunk;

    chunk.write(OP_CONSTANT, 123);
    chunk.write(chunk.addConstant(3), 123);
    chunk.write(OP_CONSTANT, 123);
    chunk.write(chunk.addConstant(5), 123);
    chunk.write(OP_EXPONENTIATE, 123);
    
    chunk.write(OP_RETURN, 123);

    vm.interpret(&chunk);

    return 0;


    // no arguments
    if(argc <= 1) {
        cout << "Please supply an argument. Use caro -h to get help.\n";
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