
| [README](https://github.com/anpang54/carotene?tab=readme-ov-file) | [Source code](https://github.com/anpang54/carotene/tree/main/src) | [Releases](https://github.com/anpang54/carotene/releases) | [Documentation](https://github.com/anpang54/carotene/wiki)
| - | - | - | - |

**Welcome to the Carotene source code!**

File structure:
- `main.cpp` - The command-line interface
- `core/`
  - `common.hpp` - Includes and helper functions
  - `scanner.hpp` - The scanner, turns source code into tokens
  - `compiler.hpp` - The compiler, turns those tokens into bytecode
  - `vm.hpp` - The virtual machine, runs that bytecode
  - `chunk.hpp` - Bytecode format and functions
  - `serialize.hpp` - Bytecode serialization
  - `value.hpp` - Types and normal objects
  - `object.hpp` - Heap-allocated objects
- `stdlib/`
  - `natives.hpp` - Parameter checking and helper macros
  - `main.hpp` - The functions that don't have a module
  - `x.hpp` - The functions in module `x`
