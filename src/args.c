#include "ft_ping.h"
#include <bits/types/struct_iovec.h>
#include <netdb.h>
#include <stdbool.h>
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
handle_ttl(Args *args, const char *arg) {
    char *endptr;
    long val = strtol(arg, &endptr, 10);

    if (*endptr != '\0' || val <= 0 || val > 255) {
        return err_fmt(3, "ft_ping: invalid argument: '", arg, "'\n");
    }

    args->cli.ttl = (int)val;
    return ok(NULL);
}

static const OptionEntry option_map[] = {
    {"-v",    handle_v,   false},
    {"-h",    handle_h,   false},
    {"-?",    handle_h,   false},
    {"--ttl", handle_ttl, true },
    {NULL,    NULL,       false},
};

// Prints help message and returns `2`.
int
help() {
    fprintf(stderr, "Usage: ./ft_ping [options] <destination>\n"
                    "Options:\n"
                    "  -v              Verbose output\n"
                    "  -h, -?          Show this help message\n"
                    "  --ttl <value>   Set the IP Time to Live\n");
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
    args->cli.ttl = -1;

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

    if (!args->cli.dest) {
        return err("ft_ping: usage error: Destination address required");
    }

    return ok(args);
}