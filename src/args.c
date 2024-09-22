/*
Parses the arguments from the command line.
* .
* Supported options:
*   - `-v`: verbose
*   - `-(h|?)`: help
* .
* Anything without leading `-` is parsed as destination address.
* Expects exactly `1` destination address.
* .
* Returns `0` on success, `-1` on error.
*/
#include "ft_ping.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*option_handler)(Args*, const char*);

typedef struct {
    const char* option;
    option_handler handler;
    bool requires_arg;
} OptionEntry;

static int handle_v(Args* const args, const char* const arg) {
    (void)arg;
    args->v = true;
    return 0;
}

static int handle_h(Args* const args, const char* const arg) {
    (void)arg;
    args->h = true;
    return 0;
}

static int handle_ttl(Args* args, const char* arg) {
    char* endptr;
    long val = strtol(arg, &endptr, 10);

    if (*endptr != '\0' || val <= 0 || val > 255) {
        fprintf(stderr, "ft_ping: invalid argument: '%s'\n", arg);
        return -1;
    }

    args->ttl = (int)val;
    return 0;
}

static const OptionEntry option_map[] = {
    {"-v", handle_v, false}, {"-h", handle_h, false}, {"-?", handle_h, false}, {"--ttl", handle_ttl, true}, {NULL, NULL, false},
};

int help() {
    fprintf(stderr, "Usage: ./ft_ping [options] <destination>\n"
                    "Options:\n"
                    "  -v              Verbose output\n"
                    "  -h, -?          Show this help message\n"
                    "  --ttl <value>   Set the IP Time to Live\n");
    return 2;
}

int parse_args(const int ac, const char** const av, Args* const args) {
    args->ttl = -1;

    for (int idx = 1; idx < ac; ++idx) {
        if (av[idx][0] == '-') {
            const OptionEntry* entry = option_map;
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
            stats.dest = av[idx];
        }
    }
    return 0;
}