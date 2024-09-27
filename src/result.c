#include "ft_ping.h"
#include <bits/types/struct_iovec.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Result
ok(void *val) {
    Result res = {.type = OK, .val = {.val = val}, .on_heap = false};
    return res;
}

Result
err(char *err) {
    Result res = {.type = ERR, .val = {.err = err}, .on_heap = false};
    return res;
}

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

void
err_unwrap(Result err) {
    if (!err.val.err) {
        return;
    }
    fprintf(stderr, "%s", err.val.err);
    if (err.on_heap) {
        free(err.val.err);
    }
}
