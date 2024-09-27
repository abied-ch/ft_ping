#include "ping.h"
#include <bits/types/struct_iovec.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// Stores the destination address into `send_addr`.
// .
// Returns:
// `.type == OK` on success
// `.type == ERR` on failure (most likely name specified in CLI arguments not resolvable)
Result
get_send_addr(const Args *const args, struct sockaddr_in *const send_addr) {
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    int e = getaddrinfo(args->cli.dest, NULL, &hints, &res);
    if (e != 0) {
        if (args->cli.v) {
            printf("ft_ping: sockfd: %d (socktype SOCK_RAW), hints.ai_family: AF_INET\n\n", g_stats.sockfd);
        }
        return err_fmt(5, "ft_ping: ", args->cli.dest, ": ", gai_strerror(e), "\n");
    }

    struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
    send_addr->sin_family = addr->sin_family;
    send_addr->sin_addr = addr->sin_addr;

    freeaddrinfo(res);
    return ok(NULL);
}

// Gets the local IP address the ICMP packets are sent from.
// .
// Returns:
// - `.type == OK` if an address was found
// - `.type == ERR` on failure, this means that either the `getifaddrs` function failed
// (which would be specified in `.val.err`), or that no address was found.
static Result
get_local_ip(char *ip, size_t ip_len) {
    struct ifaddrs *ifaddr;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        return err_fmt(2, "getifaddrs: ", strerror(errno));
    }

    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                continue;
            }

            if (strcmp(ifa->ifa_name, "lo")) {
                if (strlen(host) < ip_len) {
                    strncpy(ip, host, ip_len);
                    freeifaddrs(ifaddr);
                    return ok(NULL);
                }
            }
        }
    }
    freeifaddrs(ifaddr);
    return err(NULL);
}

// Stores the IP used for sending ICMP packets into `g_stats.local_ip`. On failure to do so,
// sets it by default to `0.0.0.0`.
void
init_local_ip() {
    Result res;
    char local_ip[INET_ADDRSTRLEN];

    res = get_local_ip(local_ip, sizeof(local_ip));
    if (res.type == OK) {
        strncpy(g_stats.local_ip, local_ip, sizeof(g_stats.local_ip));
    } else {
        strncpy(g_stats.local_ip, "0.0.0.0", sizeof(g_stats.local_ip));
    }
    g_stats.local_ip[sizeof(g_stats.local_ip) - 1] = '\n';
}
