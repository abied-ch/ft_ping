#include "ping.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int ac, char **av) {
    Result res;

    res = ping_init(ac, av);
    if (res.type == ERR) {
        err_unwrap(res, false);
        return cleanup(EXIT_FAILURE, NULL);
    }

    Args *args = (Args *)res.val.val;

    if (args->cli.h) {
        return cleanup(help(), args);
    }

    res = get_send_addr(args, &args->send_addr);
    if (res.type == ERR) {
        err_unwrap(res, false);
        return cleanup(EXIT_FAILURE, args);
    }

    init_local_ip();

    if (args->cli.v) {
        printf("ft_ping: sockfd: %d (socktype SOCK_RAW), hints.ai_family: AF_INET\n\n", g_stats.sockfd);
        printf("ai-ai_family: AF_INET, ai->ai_canonname: '%s'\n", args->cli.dest);
    }

    strncpy(g_stats.dest, args->cli.dest, sizeof(g_stats.dest));
    g_stats.dest[sizeof(g_stats.dest) - 1] = '\0';

    g_stats.args = args;

    res = ping(args);
    if (res.type == ERR) {
        err_unwrap(res, false);
        return cleanup(EXIT_FAILURE, args);
    }

    return cleanup(EXIT_SUCCESS, args);
}
