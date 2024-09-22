#include "ft_ping.h"
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*option_handler)(Args *const, const char *);

typedef struct {
    const char *option;
    option_handler handler;
    bool requires_arg;
} OptionEntry;

static int _handle_v(Args *const args, const char *const arg) {
    (void)arg;
    args->v = true;
    return 0;
}

static int _handle_h(Args *const args, const char *const arg) {
    (void)arg;
    args->h = true;
    return 0;
}

static int _handle_ttl(Args *args, const char *arg) {
    char *endptr;
    long val = strtol(arg, &endptr, 10);

    if (*endptr != '\0' || val <= 0 || val > 255) {
        fprintf(stderr, "ft_ping: invalid argument: '%s'\n", arg);
        return -1;
    }

    args->ttl = (int)val;
    return 0;
}

static const OptionEntry option_map[] = {
    {"-v",    _handle_v,   false},
    {"-h",    _handle_h,   false},
    {"-?",    _handle_h,   false},
    {"--ttl", _handle_ttl, true },
    {NULL,    NULL,       false},
};

int help() {
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
int _handle_extra_arg(Args *const args) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    int err = getaddrinfo(g_stats.dest, NULL, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "ft_ping: %s: %s\n", g_stats.dest, gai_strerror(err));
        return -1;
    } else {
        args->h = true;
        return 0;
    }
}

int parse_args(const int ac, const char **const av, Args *const args) {
    bool extra_arg = false;
    args->ttl = -1;

    for (int idx = 1; idx < ac; ++idx) {
        if (av[idx][0] == '-') {
            const OptionEntry *entry = option_map;
            while (entry->option != NULL && strcmp(entry->option, av[idx]) != 0) {
                entry++;
            }
            if (entry->option == NULL) {
                fprintf(stderr, "ft_ping: invalid option -- '%s'\n", av[idx]);
                return -1;
            }
            if (entry->requires_arg) {
                if (++idx >= ac) {
                    fprintf(stderr, "ft_ping: option requires an argument -- 'ttl'");
                    return -1;
                }
            }
            int result = entry->handler(args, entry->requires_arg ? av[idx] : NULL);
            if (result != 0) {
                return result;
            }
        } else {
            if (g_stats.dest != NULL) {
                extra_arg = true;
            }
            g_stats.dest = av[idx];
        }
    }
    if (extra_arg) {
        return _handle_extra_arg(args);
    }
    return 0;
}