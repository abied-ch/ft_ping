#include "ft_ping.h"
#include <bits/types/struct_iovec.h>
#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef Result (*option_handler)(Args *const, const char *);

typedef struct {
    const char *option;
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
        return err_fmt(3, "ft_ping: invalid argument: '", arg, "' out of range: : 1 <= value <= 9223372036854775807\n");
    }

    args->cli.c = (int)val;
    return ok(NULL);
}

static Result
handle_i(Args *const args, const char *const arg) {
    char *endptr;
    double val = strtod(arg, &endptr);

    if (*endptr != '\0') {
        return err_fmt(3, "ft_ping: invalid argument: '", args, "'\n");
    }

    args->cli.i = DEFAULT_INTERVAL * val;
    return ok(NULL);
}

static const OptionEntry option_map[] = {
    {"-v",         handle_v, false},
    {"--verbose",  handle_v, false},
    {"-h",         handle_h, false},
    {"--help",     handle_h, false},
    {"-?",         handle_h, false},
    {"--ttl",      handle_t, true },
    {"-t",         handle_t, true },
    {"-c",         handle_c, true },
    {"--count",    handle_c, true },
    {"-i",         handle_i, true },
    {"--interval", handle_i, true },
    {NULL,         NULL,     false},
};

// Prints help message and returns `2`.
int
help() {
    fprintf(stderr, "\n"
                    "Usage:\n"
                    "  ./ft_ping [options] <destination>\n"
                    "\n"
                    "Options:\n"
                    "  <destination>               DNS name or IP address\n"
                    "  -c <count>                  stop after <count> replies\n"
                    "  -h | -?                     print help and exit\n"
                    "  -i <interval>               seconds between sending each packet\n"
                    "  -t                          define time to live\n"
                    "  -v                          verbose output\n");
    return 2;
}

// ping handles extra arguments weirdly:
// - If multiple destination hosts are provided, only the last one will be stored, then:
//
// - If it is resolvable:
//      -> Print help message
// - Else:
//      -> Print error message alone
static Result
handle_extra_arg(Args *const args) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    int e = getaddrinfo(args->cli.dest, NULL, &hints, &res);
    if (e != 0) {

        size_t n = strlen(args->cli.dest);
        char dest[n + 1];
        strncpy(dest, args->cli.dest, n);
        dest[n] = '\0';
        free(args);

        return err_fmt(5, "ft_ping: ", dest, ": ", gai_strerror(e), "\n");
    } else {
        args->cli.h = true;
        freeaddrinfo(res);
        return ok(args);
    }
    freeaddrinfo(res);
    return ok(args);
}

Result
parse_cli_args(const int ac, char **av, Args *const args) {
    bool extra_arg = false;
    args->cli.t = -1;
    args->cli.c = -1;
    args->cli.i = DEFAULT_INTERVAL;

    for (int idx = 1; idx < ac; ++idx) {
        if (av[idx][0] == '-') {
            const OptionEntry *entry = option_map;
            while (entry->option != NULL && strcmp(entry->option, av[idx]) != 0) {
                entry++;
            }
            if (entry->option == NULL) {
                free(args);
                return err_fmt(3, "ft_ping: invalid option -- '", av[idx], "'\n");
            }
            if (entry->requires_arg) {
                if (++idx >= ac) {
                    free(args);
                    return err("ft_ping: option requires an argument -- 'ttl'\n");
                }
            }
            Result res = entry->handler(args, entry->requires_arg ? av[idx] : NULL);
            if (res.type == ERR) {
                return res;
            }
        } else {
            if (args->cli.dest != NULL) {
                extra_arg = true;
            }
            args->cli.dest = av[idx];
        }
    }
    if (extra_arg) {
        return handle_extra_arg(args);
    }

    if (!args->cli.h && !args->cli.dest) {
        return err("ft_ping: usage error: Destination address required");
    }

    return ok(args);
}
