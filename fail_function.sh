#!/bin/sh

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <function_name>"
    exit 1
fi

FUNCTION_NAME=$1

SRC="tests/overrides/${FUNCTION_NAME}.c"
OUT="tests/overrides/out/${FUNCTION_NAME}.so"

if [ ! -f "$SRC" ]; then 
    echo "error: $SRC not found"
    exit 1
fi 

if [ ! -d "tests/overrides/out" ]; then 
    echo "creating output directory"
    mkdir "tests/overrides/out"
fi

gcc -fPIC -shared -o "$OUT" "$SRC" -ldl

if [ $? -eq 0 ]; then 
    echo "compiled $SRC into $OUT"
    make
    echo "calling ft_ping with custom $FUNCTION_NAME"
    sudo LD_PRELOAD="./$OUT" ./ft_ping google.com
fi