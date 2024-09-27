# /bin/sh

make

echo "test 1: calloc failure"
gcc -fPIC -shared -o tests/calloc.so tests/calloc.c -ldl
LD_PRELOAD=./tests/calloc.so ./ft_ping
echo $?


./ft_ping
