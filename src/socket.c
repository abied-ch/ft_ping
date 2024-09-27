#include "ping.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>

// Initializes a raw `ICMP` (`IPv4`) socket and stores its fd into `*sockfd`.
Result
socket_init(int *const sockfd) {
    *sockfd = socket(AF_INET, SOCK_RAW | SOCK_NONBLOCK, IPPROTO_ICMP);
    if (*sockfd == -1) {
        if (errno == EPERM) {
            return err("ft_ping: socket: Permission Denied: root privileges required for raw socket creation\n");
        }
        return err_fmt(3, "socket: ", strerror(errno), "\n");
    }

    return ok(NULL);
}

// Sets `SO_RCVTIMEO` & `SO_SNDTIMEO` (receive & send timeouts) on the socket.
// Also adds `IP_TTL` (IP time to live) if specified in the command line arguments.
Result
socket_set_options(const Args *const args) {
    struct timeval timeout;

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    if (setsockopt(g_stats.alloc.sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        return err_fmt(3, "setsockopt (SO_RCVTIMEO): ", strerror(errno), "\n");
    }

    if (setsockopt(g_stats.alloc.sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
        return err_fmt(3, "setsockopt (SO_SNDTIMEO): ", strerror(errno), "\n");
    }

    if (args->cli.t != -1) {
        if (setsockopt(g_stats.alloc.sockfd, IPPROTO_IP, IP_TTL, &args->cli.t, sizeof(args->cli.t)) == -1) {
            return err_fmt(3, "setsockopt (IP_TTL): ", strerror(errno), "\n");
        }
    }
    return ok(NULL);
}
