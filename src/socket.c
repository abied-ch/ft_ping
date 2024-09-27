#include "ft_ping.h"
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

Result
init_socket(int *const sockfd) {
    *sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (*sockfd == -1) {
        return err(strerror(errno));
    }
    return ok(NULL);
}

Result
set_socket_options(const Args *const args) {
    struct timeval timeout;

    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    if (setsockopt(g_stats.sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        return err_fmt(2, "setsockopt (SO_RCVTIMEO): ", strerror(errno));
    }

    if (setsockopt(g_stats.sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
        return err_fmt(2, "setsockopt (SO_SNDTIMEO): ", strerror(errno));
    }

    if (args->cli.t != -1) {
        if (setsockopt(g_stats.sockfd, IPPROTO_IP, IP_TTL, &args->cli.t, sizeof(args->cli.t)) == -1) {
            return err_fmt(2, "setsockopt (IP_TTL): ", strerror(errno));
        }
    }
    return ok(NULL);
}