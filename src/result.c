#include "ping.h"
#include <bits/types/struct_iovec.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Returns a `Result` struct with `.type` set to `OK` and `.val.val` set to `val`.
// Safety:
// - `val` is a void pointer to make the `Result` struct portable. It is therefore the caller's responsibility
// to make the expected type of `val` clear in the code.
Result
ok(void *val) {
    Result res = {.type = OK, .val = {.val = val}, .on_heap = false};
    return res;
}

// Returns a `Result` struct with `.type` set to `ERR` and `.val.err` set to `err`.
// Safety:
// - The `err` string will not be freed!
// - The `err` string is assumed to be a valid, null-terminated string.
Result
err(char *err) {
    Result res = {.type = ERR, .val = {.err = err}, .on_heap = false};
    return res;
}

// Returns a `Result` struct with `.type` set to `ERR` and `.val.err` set to all strings passed to this function
// joined together (without any separator).
// Safety:
// - The `n_strs` argument is expected to reflect the actual amount of strings passed to the variadic argument!
// - `.val.err` will be heap-allocated. Always call `err_unwrap` to free the memory!
Result
err_fmt(const int n_strs, ...) {
    va_list args;

    size_t len = 0;
    va_start(args, n_strs);

    for (int i = 0; i < n_strs; ++i) {
        const char *str = va_arg(args, const char *);
        len += strlen(str);
    }

    va_end(args);

    char *s = (char *)malloc(len + 1);
    if (!s) {
        perror("malloc");
        return err(NULL);
    }

    va_start(args, n_strs);

    s[0] = '\0';

    for (int i = 0; i < n_strs; ++i) {
        const char *str = va_arg(args, const char *);
        strncat(s, str, strlen(str));
    }

    va_end(args);

    Result res = {.type = ERR, {.err = s}, .on_heap = true};
    return res;
}

// "Unwraps" the `Result` struct value, printing `.val.err` value to `stderr` if `quiet` is set
// to `false` and freeing it if necessary.
// .
// Does nothing if `.type != ERR` or `.val.err == NULL`.
void
err_unwrap(Result err, const bool quiet) {
    if (err.type != ERR || !err.val.err) {
        return;
    }
    if (!quiet) {
        fprintf(stderr, "%s", err.val.err);
    }
    if (err.on_heap) {
        free(err.val.err);
    }
}
