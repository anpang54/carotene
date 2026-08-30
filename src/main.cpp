

// INCLUDES

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
#include "core/serialize.hpp"

using std::ifstream, std::ofstream, std::stringstream;


// INPUT/OUTPUT

VM vm;

void startingMessage() {
    cout << "\n  \033[1m\033[38:5:208mCarotene v" << VERSION << "\033[0m (" << VERSION_DATE << ")"
            "\n  https://github.com/anpang54/carotene\n";
}


// repl

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
        vm.interpret(source);

    }
    
    freeObjects();

}


// read/write

string readFile(string filename) {

    ifstream file(filename);
    if(!file.is_open()) {
        cliError("Couldn't open the file!");
    }

    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

vector<uint8_t> readFileBytes(string filename) {

    ifstream file(filename, std::ios::binary);
    if(!file.is_open()) {
        cliError("Couldn't open the file!");
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();
    return vector<uint8_t>(content.begin(), content.end());

}

void writeFileBytes(string filename, vector<uint8_t> content) {

    ofstream file(filename, std::ios::binary);
    if(!file.is_open()) {
        cliError("Couldn't open the file!");
        return;
    }

    file.write(reinterpret_cast<const char*>(content.data()), content.size());

}


// COMPILE/RUN

string compile(string filename) {

    Compiler compiler;
    ObjFunction* function = compiler.compile(readFile(filename));
    if(function == nullptr) exit(1);

    string outname = filename.substr(0, filename.find_last_of('.')) + ".reti";
    writeFileBytes(outname, serializeApp(function, outname));

    freeObjects();

    cout << "Compiled to " + outname + "!\n";
    return outname;

}

void run(string filename) {

    ObjFunction* function = deserializeApp(readFileBytes(filename));
    vm.interpretBytecode(function);

    freeObjects();

}


// MAIN

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

        // compile/run
        case ' ':
            vm.interpret(readFile(filename));
            break;
        case 'c':
            compile(filename);
            break;
        case 'r':
            run(filename);
            break;
        case 't':
            run(compile(filename));
            break;

        // help
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

        // version
        case 'v':
            startingMessage();
            break;

        default:
            cliError("Invalid option -" + string(1, option) + ".");

    }

    return 0;
}