

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


// input

VM vm;

InterpretResult interpret(string source) {
    InterpretResult result = vm.interpret(source);
    return result;
}

void startingMessage() {
    cout << "\n  \033[1m\033[38:5:208mCarotene v" << VERSION << "\033[0m (" << VERSION_DATE << ")"
            "\n  https://github.com/anpang54/carotene\n";
}

void repl() {

    startingMessage();
    #define READLINE_START "\033[1m\033[38:5:208m> \033[0m"

    vm.replMode = true;

    for(;;) {

        cout << '\n';

        // get line
        string source;
        #ifdef __linux__
            // linux uses readline
            char* line = readline(READLINE_START);
            if(line == nullptr) break;
            if(*line) add_history(line);
            source = line;
            free(line);
        #else
            // windows doesn't have readline so it just gets plain getline
            cout << READLINE_START;
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
            cout << "Not implemented but the filename is " << filename << '\n';
            break;

        case 'h':
            cout << "\n"
                    "  \033[1mcaro   \033[0m              Opens the interactive console\n"
                    "  \033[1m       \033[0m file.caro    Runs a file\n"
                    "  \033[1m     -c\033[0m file.caro    Compiles a file into a bytecode file\n"
                    "  \033[1m     -r\033[0m app.reti     Runs a bytecode file\n"
                    "  \033[1m     -t\033[0m file.caro    Same as running with no argument, but leaves a bytecode file behind\n"
                    "  \033[1m     -h\033[0m              Shows this help menu\n"
                    "  \033[1m     -v\033[0m              Shows the Carotene version\n"
                    "\n"
                    "For more information, please consult the wiki at https://github.com/anpang54/carotene/wiki.\n";
            break;

        case 'v':
            startingMessage();
            break;

        default:
            cout << "Invalid option -" << option << ".\n";

    }

    return 0;
}