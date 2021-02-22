#!/bin/bash

COMPILER_FLAGS="-g -DDEBUG -DCOMPILER_GCC -Wall -Werror -Wpedantic -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-but-set-variable"

mkdir -p ../build
g++ server_linux.cpp -o ../build/server_linux $COMPILER_FLAGS