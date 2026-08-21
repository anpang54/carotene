
zig c++ src/main.cpp -o caro -std=c++23 -target x86_64-linux-gnu.2.28

# We compile with Zig, which in turn runs Clang, because it allows us to specify a glibc version.
# Clang support for C++26 is still rather incomplete, so this uses C++23.
