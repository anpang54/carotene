
<img src="https://raw.githubusercontent.com/anpang54/carotene/refs/heads/main/assets/carrot.svg" align="right" alt="Carrot" width="175">

| [README](https://github.com/anpang54/carotene?tab=readme-ov-file) | [Source code](https://github.com/anpang54/carotene/tree/main/src) | [Releases](https://github.com/anpang54/carotene/releases) | [Documentation](https://github.com/anpang54/carotene/wiki)
| - | - | - | - |


# Carotene

A bytecode-interpreted programming language coded in C++23. It is currently functional, but doesn't have many features. It serves as a place for me to learn how to make a programming language, but the end goal is that it'll have built-in tools to make applications, both text-based and graphical.

The chemical [Carotene](https://en.wikipedia.org/wiki/Carotene), found in carrots, turns into [Retinol](https://en.wikipedia.org/wiki/Retinol), which maintains eye health, so that you can see the GUI programs made with (future) Carotene. Source code files are `.caro`, and bytecode files are `.reti`.


## Features

Carotene currently has:
- A working tokenizer, compiler, and VM, accessible by loading a file or using the REPL
- Functions
- Variables, scoped and global, as well as constants
- Booleans, 7 numeric types, 6 vector types, strings, arrays, and dictionaries
- Type casting
- All the arithmetic and comparison operators you'd expect
- `for` and `while`, as well as unique `repeat` and `forever`, with `break` and `continue`
- A basic standard library with print/input, logging, time, random, hashing, and math

And a lot more is coming.


## Compatibility

Carotene can run on:
- Windows 10+ on x86_64
- macOS 13+ on Apple Silicon [(see note)](https://github.com/anpang54/carotene/wiki/Compatibility#macos)
- Linux from 2019 or later, on x86_64
- FreeBSD 14.0+ on x86_64
- Haiku R1/beta4+ on x86_64
- Web, via [WebAssembly](https://webassembly.org/)

See more at [Compatibility](https://github.com/anpang54/carotene/wiki/Compatibility).


## Getting started

Binaries are provided in [Releases](https://github.com/anpang54/carotene/releases).

The build script uses `zig c++` for Windows and Linux, and is written in Carotene itself. Therefore, to compile Carotene, run:
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

I made the base for this by following the [*clox*](https://craftinginterpreters.com/a-bytecode-virtual-machine.html) section of [Robert Nystrom](https://journal.stuffwithstuff.com/)'s [Crafting Interpreters](https://craftinginterpreters.com/), a book which he generously makes completely free. I am currently at chapter 26, opting to add some more stuff before classes.

The main difference, of course, is that Carotene uses C++ and therefore gets to use its features. Carotene also has a plethora of its own additions and changes, so much so that it is not backward compatible with clox.

My use of AI in this project is to let me skip the boring stuff like debugging and low-level optimization, so that I can do the fun stuff like adding features. Therefore, most of the code is either adapted from Crafting Interpreters or written by me.


## Performance

A benchmark, available at `tests/benchmarks/benchmark.py`, yields the following results for Carotene v0.1.2 on my PC:
```

This is a rudimentary benchmark where a few bytecode interpreters start and check whether 0 - 9,999 are prime.
Node.js, Lua, and PHP are in their non-JIT modes.

Trial 0 1 2 3 4 5 

                  min      max
  Lua            26 ms    28 ms
  PHP            39 ms    40 ms
  Node.js        79 ms    93 ms
  Carotene      100 ms   108 ms
  Python        145 ms   146 ms
  Wren          151 ms   170 ms
  clox with %   188 ms   207 ms

```

Do note that performance varies significantly between versions.

