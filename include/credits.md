

## Crafting Interpreters

This project uses code from Robert Nystrom's [*Crafting Interpreters*](https://github.com/munificent/craftinginterpreters), specifically the *clox* section.


## Libraries

The following is a list of libraries used by Carotene. Most are used for the standard library, with only isocline being used for the core on a native build.

| Library                                       | License             | Usage                                  | Method      |
| --------------------------------------------- | ------------------- | -------------------------------------- | ----------- |
| [BeAPI](https://www.haiku-os.org/docs/api/)   | MIT                 | Haiku `gui`                            | Linked      |
| [Emscripten](https://github.com/emscripten-core/emscripten)         | MIT         | Web builds               | Compiled in |
| [GTK 4](https://gitlab.gnome.org/GNOME/gtk)   | LGPL 2.1+           | macOS, Linux, FreeBSD `gui`            | `dlopen()`  |
| [isocline](https://github.com/daanx/isocline) | MIT                 | REPL                                   | Bundled     |
| [libcurl](https://github.com/curl/curl)       | curl                | macOS, Linux, FreeBSD, Haiku `http`    | `dlopen()`  |
| [SHA](https://github.com/pr0f3ss/SHA)         | MIT                 | `hash` SHA functions                   | Bundled     |
| [Win32](https://learn.microsoft.com/en-us/windows/win32/api/)       | Proprietary | Windows `gui`            | Linked      |
| [WinHTTP](https://learn.microsoft.com/en-us/windows/win32/WinHttp/) | Proprietary | Windows `http`           | Linked      |
| [xxHash](https://github.com/cyan4973/xxhash)  | BSD 2-Clause        | `hash.xxhash()`                        | Bundled     |


## Licenses

And here are the required license texts.

### MIT License

Copyright (c) 2010-2014 Emscripten authors, see [AUTHORS](https://github.com/emscripten-core/emscripten/blob/main/AUTHORS) file.\
Copyright (c) 2015 Robert Nystrom\
Copyright (c) 2021 Daan Leijen\
Copyright (c) 2022 Filip Dobrosavljevic

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

### BSD 2-Clause License

Copyright (c) 2012-2023 Yann Collet

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

