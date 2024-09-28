#include "ping.h"
#include <bits/types/struct_iovec.h>
#include <errno.h>
#include <float.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef Result (*option_handler)(Args *const, const char *);

typedef struct {
    const char *option_s;
    const char *option_l;
    option_handler handler;
    bool requires_arg;
} OptionEntry;

static Result
handle_v(Args *const args, const char *const arg) {
    (void)arg;
    args->cli.v = true;
    return ok(NULL);
}

static Result
handle_h(Args *const args, const char *const arg) {
    (void)arg;
    args->cli.h = true;
    return ok(NULL);
}

static Result
handle_q(Args *const args, const char *const arg) {
    (void)arg;
    args->cli.q = true;
    return ok(NULL);
}

static Result
handle_D(Args *const args, const char *const arg) {
    (void)arg;
    args->cli.D = true;
    return ok(NULL);
}

static Result
handle_t(Args *const args, const char *const arg) {
    char *endptr;
    long val = strtol(arg, &endptr, 10);

    if (*endptr != '\0') {
        return err_fmt(3, "ft_ping: invalid argument: '", arg, "'\n");
    } else if (val <= 0 || val > 255) {
        return err_fmt(3, "ft_ping: invalid argument: '", arg, "': out of range: 0 <= value <= 255");
    }

    args->cli.t = (int)val;
    return ok(NULL);
}

static Result
handle_c(Args *const args, const char *const arg) {
    char *endptr;
    int64_t val = strtol(arg, &endptr, 10);

    if (*endptr != '\0') {
        return err_fmt(3, "ft_ping: invalid argument: '", arg, "'\n");
    } else if (val < 1 || errno == ERANGE) {
        return err_fmt(3, "ft_ping: invalid argument: '", arg, "' out of range: 1 <= value <= 9223372036854775807\n");
    }

    args->cli.c = (int)val;
    return ok(NULL);
}

static Result
handle_i(Args *const args, const char *const arg) {
    char *endptr;
    double val = strtod(arg, &endptr);

    if (*endptr != '\0') {
        return err_fmt(3, "ft_ping: invalid argument: '", arg, "'\n");
    } else if (errno == ERANGE || strlen(arg) > sizeof("99999999999999999")) {
        return err_fmt(3, "ft_ping: invalid argument: '", arg, "' causes double overflow\n");
    }

    args->cli.i = DEFAULT_INTERVAL * val;
    return ok(NULL);
}

static Result
handle_w(Args *const args, const char *const arg) {
    char *endptr;
    int val = strtol(arg, &endptr, 10);

    if (*endptr != '\0') {
        return err_fmt(3, "ft_ping: invalid argument: '", arg, "'\n");
    } else if (errno == ERANGE || val < 1 || val > INT32_MAX) {
        return err_fmt(3, "ft_ping: invalid argument: '", arg, "' out of range: 1 <= value <= 2147483647\n");
    }

    args->cli.w = val;
    return ok(NULL);
}

static const OptionEntry option_map[] = {
    {"-v", "--verbose",   handle_v, false},
    {"-h", "--help",      handle_h, false},
    {"-?", "--help",      handle_h, false},
    {"-q", "--quiet",     handle_q, false},
    {"-D", "--timestamp", handle_D, false},
    {"-t", "--ttl",       handle_t, true },
    {"-c", "--count",     handle_c, true },
    {"-i", "--interval",  handle_i, true },
    {"-w", "--timeout",   handle_w, true },
    {NULL, NULL,          NULL,     false},
};

Result
parse_cli_args(const int ac, char **av, Args *const args) {
    args->cli.i = DEFAULT_INTERVAL;

    for (int idx = 1; idx < ac; ++idx) {
        if (av[idx][0] == '-') {
            const OptionEntry *entry = option_map;
            while (entry->option_s != NULL && strcmp(entry->option_s, av[idx]) != 0 && strcmp(entry->option_l, av[idx]) != 0) {
                entry++;
            }
            if (entry->option_s == NULL) {
                return err_fmt(3, "ft_ping: invalid option -- '", av[idx], "'\n");
            }
            if (entry->requires_arg) {
                if (++idx >= ac) {
                    return err("ft_ping: option requires an argument -- 'ttl'\n");
                }
            }
            Result res = entry->handler(args, entry->requires_arg ? av[idx] : NULL);
            if (res.type == ERR) {
                return res;
            }
        } else if (args->cli.dest == NULL) {
            args->cli.dest = av[idx];
        }
    }

    if (!args->cli.h && !args->cli.dest) {
        return err("ft_ping: usage error: Destination address required");
    }

    return ok(args);
}

// Sanitizes the `-i` / `--interval` input, ensuring it does not go bvelow `0.002s`.
Result
flood_check(const Args *const args) {
    if (args->cli.i < 0.002) {
        return err("ft_ping: cannot flood, minimal interval for user must be >= 2ms, use -i 0.002 (or higher)\n");
    }
    return ok(NULL);
}
