#include "ft_ping.h"
#include <bits/types/struct_iovec.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

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

// Gets the IP address used to send the ICMP packets.
// .
// Returns:
// - `Result.type == OK` if an address was found
// - `Result.type == ERR` on failure, this can mean either:
// .
// 1. The `getifaddrs` function failed
// 2. No address was found 
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