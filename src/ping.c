#include "ft_ping.h"
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

Stats g_stats;

static Result
init(const int ac, char **av) {
    Result res;
    Stats g_stats = {0};

    g_stats.sockfd = -1;

    if (gettimeofday(g_stats.start_time, NULL) == -1) {
        return err(strerror(errno));
    }
    res = init_socket(&g_stats.sockfd);
    if (res.type == ERR) {
        return res;
    }

    return parse_args(ac, av);
}

int
main(int ac, char **av) {
    Result res;

    res = init(ac, av);
    if (res.type == ERR) {
        err_unwrap(res);
        if (g_stats.sockfd != -1) {
            close(g_stats.sockfd);
        }
        return EXIT_FAILURE;
    }

    Args *args = (Args *)res.val.val;

    if (args->h) {
        close(g_stats.sockfd);
        free(args);
        return help();
    }

    res = get_send_addr(args);
    if (res.type == ERR) {
        err_unwrap(res);
        close(g_stats.sockfd);
        free(args);
        return EXIT_FAILURE;
    }

    struct sockaddr_in *send_addr = (struct sockaddr_in *)res.val.val;

    close(g_stats.sockfd);
    free(args);
    free(send_addr);
}