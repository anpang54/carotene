
# This will be replaced with a Carotene script once it can do that.

if [ "$1" = "windows" ]; then
    # Windows release
    zig c++ src/main.cpp -o caro.exe \
        -std=c++23 -target x86_64-windows-gnu -O2 -g0 -s\
        -Wno-c99-designator
elif [ "$1" = "linux" ]; then
    # Linux release
    zig c++ src/main.cpp -o caro\
        -std=c++23 -target x86_64-linux-gnu.2.28 -O2\
        -Wno-c99-designator\
        -idirafter /usr/include -L/usr/lib -lreadline
else
    # Linux dev
    zig c++ src/main.cpp -o caro\
        -std=c++23 -target x86_64-linux-gnu.2.28\
        -Wno-c99-designator\
        -idirafter /usr/include -L/usr/lib -lreadline
fi

# We compile with Zig, which in turn runs Clang, because it allows us to specify a glibc version.
# Clang support for C++26 is still rather incomplete, so this uses C++23.