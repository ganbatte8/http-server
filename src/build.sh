#!/bin/bash

COMPILER_FLAGS="-g -DDEBUG -Wall -Werror -Wpedantic -Wextra -Wno-unused-parameter"

g++ server_linux.cpp -o ../build/server_linux $COMPILER_FLAGS