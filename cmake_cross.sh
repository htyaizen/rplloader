#!/bin/sh
cmake -DCMAKE_EXE_LINKER_FLAGS="-static" -DCMAKE_TOOLCHAIN_FILE=./Toolchain-PowerPC.cmake -DCMAKE_BUILD_TYPE=Debug .
