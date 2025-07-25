#!/bin/bash
$CC $CFLAGS -c picohttpparser.c
llvm-ar rcs libfuzz.a *.o

$CC $CFLAGS $LIB_FUZZING_ENGINE $SRC/fuzzer.c -Wl,--whole-archive $SRC/picohttpparser/libfuzz.a -Wl,--allow-multiple-definition -I$SRC/picohttpparser/  -o $OUT/fuzzer
