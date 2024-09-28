#include "ping.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
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

// Adds `IP_HDRINCL` to the socket options to prevent the kernel from creating the IP header.
Result
socket_set_options() {
    int on = 1;
    if (setsockopt(g_stats.alloc.sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
        return err_fmt(3, "setsockopt (IP_HDRINCL): ", strerror(errno), "\n");
    }

    return ok(NULL);
}
