package main

/*
#include "../inc/ping.h"
*/
import "C"
import "unsafe"

func test_no_args() {
	args := []string{"./ft_ping"}

	cArgs := make([]*C.char, len(args))
	for idx, arg := range args {
		cArgs[idx] = C.CString(arg)
		defer C.free(unsafe.Pointer(cArgs[idx]))
	}

	result = 
}

func main() {
	test_no_args()
}
