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
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int parse_args(const int ac, const char** const av, Args* const args) {
    int i = 0;
    args->ttl = -1;

    while (++i < ac) {
        if (av[i][0] == '-') {
            if (!strcmp(av[i], "-v")) {
                args->v = true;
            } else if (!strcmp(av[i], "-h") || !strcmp(av[i], "-?")) {
                /**
                 * Note: oh-my-zsh interprets '?' as a single-character wildcard
                 */
                args->h = true;
                return 0;
            } else if (!strcmp(av[i], "--ttl")) {
                if (i == ac - 1) {
                    fprintf(stderr, "ft_ping: option requires an argument -- 'ttl'");
                    args->h = true;
                    return 0;
                }
                i++;
                for (int c = 0; av[i][c]; ++c) {
                    if (!isdigit(av[i][c])) {
                        fprintf(stderr, "ft_ping: invalid argument: '%s'", av[i]);
                        return -1;
                    }
                }
                args->ttl = atoi(av[i]);
            } else {
                fprintf(stderr, "Unknown option: %s\n", av[i]);
                args->h = true;
                return 0;
            }
        } else {
            if (stats.dest == NULL) {
                stats.dest = av[i];
            } else {
                args->h = true;
                return 0;
            }
        }
    }
    if (!stats.dest && !args->h) {
        fprintf(stderr, "ft_ping: usage error: Destination address required\n");
        return -1;
    }
    return 0;
}