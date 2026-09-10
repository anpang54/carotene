

// INCLUDES

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define TokenType WindowsTokenType
#endif
#include "../include/isocline/src/isocline.c"
#ifdef _WIN32
    #undef TokenType
    #undef read
    #undef isatty
#endif

#include <fstream>
#include <sstream>

#include "core/common.hpp"
#include "core/chunk.hpp"
#include "core/vm.hpp"
#include "core/compiler.hpp"
#include "core/serialize.hpp"
#include "core/licenses.hpp"

using std::ifstream, std::ofstream, std::stringstream;


// INPUT/OUTPUT

VM vm;

void startingMessage() {
    cout << "\n  \033[1m\033[38;5;208mCarotene v" << VERSION << "\033[0m (" << VERSION_DATE << ")"
            "\n  https://github.com/anpang54/carotene\n";
}


// repl

void repl() {

    startingMessage();

    vm.replMode = true;

    ic_style_def("ic-prompt", "bold #ff8700");
    ic_set_prompt_marker("> ", NULL);
    ic_set_history(NULL, 1000);

    int consecutiveEmptyLines = 0;

    for(;;) {

        cout << '\n';

        char* input = ic_readline(NULL);
        if(input == NULL) break;

        if(input[0] == '\0') {
            free(input);
            if(++consecutiveEmptyLines >= 3) {
                cout << "\033[2mIf you would like to exit the REPL, please type exit(); or use Ctrl + D.\033[0m\n";
                consecutiveEmptyLines = 0;
            };
            continue;
        }
        consecutiveEmptyLines = 0;

        // isocline doesn't store 1 char lines in the history for whatever reason, so add it manually instead
        if(input[1] == '\0') ic_history_add(input);

        vm.interpret(string(input));
        free(input);

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

InterpretResult run(string filename) {

    ObjFunction* function = deserializeApp(readFileBytes(filename));
    InterpretResult result = vm.interpretBytecode(function);

    freeObjects();

    return result;

}


// MAIN

int main(int argc, const char* argv[]) {
    
    // no arguments, repl
    if(argc <= 1) {
        repl();
        return 0;
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
    InterpretResult result = INTERPRET_OK;
    switch(option) {

        // compile/run
        case ' ':
            result = vm.interpret(readFile(filename));
            break;
        case 'c':
            compile(filename);
            break;
        case 'r':
            result = run(filename);
            break;
        case 't':
            result = run(compile(filename));
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
                    "  \033[1m     -l\033[0m              Shows the licenses of Carotene and the libraries that it uses\n"
                    "\n"
                    "For more information, please consult the wiki at https://github.com/anpang54/carotene/wiki.\n";
            break;

        // version
        case 'v':
            startingMessage();
            break;

        // licenses
        case 'l':
            cout << LICENSES;
            break;

        default:
            cliError("Invalid option -" + string(1, option) + ".");

    }

    return result == INTERPRET_OK? 0: 1;
}