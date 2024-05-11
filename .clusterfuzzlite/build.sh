#!/bin/bash
find . -name "*.c" -exec $CC $CFLAGS -I./src -c {} \;
find . -name "*.o" -exec cp {} . \;

rm -f ./test*.o
llvm-ar rcs libfuzz.a *.o


$CC $CFLAGS $LIB_FUZZING_ENGINE $SRC/fuzzer.c -Wl,--whole-archive $SRC/picohttpparser/libfuzz.a -Wl,--allow-multiple-definition -I$SRC/picohttpparser/ -I$SRC/picohttpparser/picotest  -o $OUT/fuzzer