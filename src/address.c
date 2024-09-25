#include "ft_ping.h"
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

Result
get_send_addr(const Args *const args) {
    struct addrinfo hints, *res;
    struct sockaddr_in *send_addr = calloc(sizeof(struct sockaddr_in), 1);
    if (!send_addr) {
        return err(strerror(errno));
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    int e = getaddrinfo(args->dest, NULL, &hints, &res);
    if (e != 0) {
        if (args->v) {
            printf("ft_ping: sockfd: %d (socktype SOCK_RAW), hints.ai_family: AF_INET\n\n", g_stats.sockfd);
        }
        free(send_addr);
        return err_fmt(5, "ft_ping: ", args->dest, ": ", gai_strerror(e), "\n");
    }

    struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
    send_addr->sin_family = addr->sin_family;
    send_addr->sin_addr = addr->sin_addr;

    freeaddrinfo(res);
    return ok(send_addr);
}