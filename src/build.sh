#!/bin/bash
COMPILER_FLAGS="-g -DDEBUG=0 -Ofast -DCOMPILER_GCC -Wall -Werror -Wpedantic -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-but-set-variable -Wno-write-strings"

mkdir -p ../build
g++ server_linux.cpp -o ../build/server_linux $COMPILER_FLAGS -lpthread


# in case carriage return characters are confusing bash, remove them with:
# sed -i -e 's/\r$//' build.sh
