
| [README](https://github.com/anpang54/carotene?tab=readme-ov-file) | [Source code](https://github.com/anpang54/carotene/tree/main/src) | [Releases](https://github.com/anpang54/carotene/releases) | [Documentation](https://github.com/anpang54/carotene/wiki)
| - | - | - | - |

# Carotene

A bytecode-interpreted programming language coded in C++23. It is currently functional, but doesn't have many features. It serves as a place for me to learn how to make a programming language, but the end goal is that it'll have built-in tools to make applications, both text-based and graphical.

The chemical Carotene, found in carrots, turns into Retinol, which maintains eye health, so that you can see the GUI programs made with (future) Carotene. Source code files are `.caro`, and bytecode files are `.reti`.

## Features

As of 29 August 2026, Carotene has:
- A working tokenizer, compiler, and VM, accessible by loading a file or using the REPL
- Functions
- Variables, scoped and global, as well as constants
- Booleans, 7 numeric types, strings, arrays, and dictionaries
- All the arithmetic and comparison operators you'd expect
- `for` and `while`, as well as a unique `repeat`
- A basic standard library with print/input, logging, time, random, hashing, and basic math

And a lot more is coming.

## Getting started

Carotene can run on Windows 10+ or Linux with glibc 2.28+ (pretty much anything from 2019 or later), on x86_64 (ie. most Intel/AMD CPUs). More build targets will be added later on.

Binaries are provided at `caro-release` (Linux) and `caro-release.exe` (Windows). In the examples below, append `.exe` like that if you are on Windows.

The build script uses `zig c++`, and is written in Carotene itself. Therefore, to compile Carotene, run:
```
./caro-release build.caro
```

Examples of usage:
```
./caro-release            Open the interactive REPL
./caro-release main.caro  Run the file "main.caro"
./caro-release -v         Get the current version
```

For some examples of Carotene code, please see the `examples/` folder.

## Development

Version 0.1.0 is scheduled to come out on the 31st.

I made the base for this by following the *clox* section of [Robert Nystrom](https://journal.stuffwithstuff.com/)'s [Crafting Interpreters](https://craftinginterpreters.com/), a book which he generously makes completely free.

The main difference, of course, is that Carotene uses C++ and therefore gets to use its features. Carotene also has a plethora of its own additions and changes, so much so that it is not backward compatible with clox.

The vast majority of the code is either adapted from Crafting Interpreters or written by me, as I only used AI for some debugging and optimization.

## Performance

Carotene is decently fast.

A benchmark, available at `tests/benchmarks/benchmark.py`, yields the following results on my PC:
```
Node.js, Lua, and PHP are in their non-JIT modes.
                  min      max
  Lua            27 ms    29 ms
  PHP            39 ms    42 ms
  Carotene       60 ms    61 ms
  Node.js        79 ms   100 ms
  Python        145 ms   152 ms
  Wren          146 ms   172 ms
  clox with %   164 ms   206 ms
```