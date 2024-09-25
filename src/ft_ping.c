#include "ft_ping.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/floatn-common.h>
#include <bits/types/struct_iovec.h>
#include <bits/types/struct_timeval.h>
#include <ifaddrs.h>
#include <limits.h>
#include <linux/stddef.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

Stats g_stats = {0};

// Computes IP checksum (16 bit one's complement sum), ensuring packet integrity before accepting.
// .
// This ensures that the checksum result will not exceed the size of a 16 bit integer by
// wrapping around carry instead of making the number outgrow its bounds.
// .
// The sender calculates the checksum with '0' as a placeholder value in the checksum field. The receiver then
// calculates it including the checksum sent by the sender. Its one's complement sum is expected to be '0' -
// packets where this calculation fails are discarded.
// .
// https://web.archive.org/web/20020916085726/http://www.netfor2.com/checksum.html
static unsigned short
checksum(const void *const buffer, int len) {
    const unsigned short *buf = buffer;
    unsigned int sum = 0;

    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++;
    }
    if (len == 1) {
        sum += *(unsigned char *)buf;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return ~sum;
}

static void
init_icmp_header(struct icmp *const icmp_header, const int seq, const char *const packet, const int packet_len) {
    icmp_header->icmp_type = ICMP_ECHO;
    icmp_header->icmp_id = getpid();
    icmp_header->icmp_seq = seq;

    // `icmp_header` and `packet` point to the same memory address, they are just _cast to different types_.
    // Removing this line will result in the checksum not matching and all packets (except for the first
    // one) being lost!
    icmp_header->icmp_cksum = 0;
    icmp_header->icmp_cksum = checksum(packet, packet_len);
}

static void
sigint(const int sig) {
    if (sig != SIGINT) {
        return;
    }
    if (g_stats.sent < 1) {
        g_stats.sent = 1;
    }
    int loss = 100 - (g_stats.rcvd * 100) / g_stats.sent;
    struct timeval end;
    gettimeofday(&end, NULL);
    double tot_ms = (end.tv_sec - g_stats.start.tv_sec) * 1000.0 + (end.tv_usec - g_stats.start.tv_usec) / 1000.0;

    printf("\n--- %s ping statistics ---\n%u packets transmitted, %u received", g_stats.host, g_stats.sent, g_stats.rcvd);
    if (g_stats.errs != 0) {
        printf(", +%d errors", g_stats.errs);
    }

    printf(", %d%% packet loss time %dms\n", loss, (int)tot_ms);
    printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n", g_stats.rtt_min, g_stats.rtt_avg, g_stats.rtt_max, g_stats.rtt_mdev);
    close(g_stats.sockfd);
    exit(EXIT_SUCCESS);
}

static int
get_send_addr(const Args *const args, struct sockaddr_in *const send_addr) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    int err = getaddrinfo(args->dest, NULL, &hints, &res);
    if (err != 0) {
        if (args->v) {
            printf("ft_ping: sockfd: %d (socktype SOCK_RAW), hints.ai_family: AF_INET\n\n", g_stats.sockfd);
        }
        fprintf(stderr, "ft_ping: %s: %s\n", args->dest, gai_strerror(err));
        return -1;
    }
    struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
    send_addr->sin_family = addr->sin_family;
    send_addr->sin_addr = addr->sin_addr;
    freeaddrinfo(res);
    return 0;
}

static double
update_stats(const struct timeval *const trip_begin) {
    struct timeval trip_end;

    if (gettimeofday(&trip_end, NULL) == -1) {
        return -1;
    }
    const double rt_ms = (trip_end.tv_sec - trip_begin->tv_sec) * 1000.0 + (trip_end.tv_usec - trip_begin->tv_usec) / 1000.0;

    g_stats.rcvd++;
    g_stats.rtt_min = fmin(g_stats.rtt_min, rt_ms);
    g_stats.rtt_max = fmax(g_stats.rtt_max, rt_ms);
    g_stats.rtt_avg = ((g_stats.rtt_avg * (g_stats.rcvd - 1)) + rt_ms) / g_stats.rcvd;

    double sum_deviation = 0.0;

    for (size_t i = 0; i < g_stats.rcvd; ++i) {
        sum_deviation += fabs(g_stats.rtts[i] - g_stats.rtt_avg);
    }
    g_stats.rtt_mdev = sum_deviation / g_stats.rcvd;
    if (g_stats.rcvd < MAX_PINGS) {
        g_stats.rtts[g_stats.rcvd] = rt_ms;
    }
    return rt_ms;
}

static void
init_stats() {
    g_stats.rtt_min = __builtin_inff64();
    gettimeofday(&g_stats.start, NULL);
}

static int
send_packet(const char *const packet, const size_t packet_size, const struct sockaddr_in *const send_addr) {
    if (sendto(g_stats.sockfd, packet, packet_size, 0, (struct sockaddr *)send_addr, sizeof(*send_addr)) <= 0) {
        perror("sendto");
        return -1;
    }
    g_stats.sent++;
    return 0;
}

static void
display_rt_stats(const bool v, const char *const ip_str, const struct icmp *const icmp, const struct iphdr *const ip, const double ms) {
    printf("%d bytes from %s: imcp_seq=%u ", PACKET_SIZE, ip_str, icmp->icmp_seq);
    if (v) {
        printf("ident=%d ", icmp->icmp_id);
    }
    printf("ttl=%u time=%.3f ms\n", ip->ttl, ms);
}

static int
set_socket_options(const Args *const args) {
    struct timeval timeout;

    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    if (setsockopt(g_stats.sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        perror("setsockopt (SO_RCVTIMEO)");
        return -1;
    }

    if (setsockopt(g_stats.sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
        perror("setsockopt (SO_SNDTIMEO)");
        return -1;
    }

    if (args->ttl != -1) {
        if (setsockopt(g_stats.sockfd, IPPROTO_IP, IP_TTL, &args->ttl, sizeof(args->ttl)) == -1) {
            perror("setsockopt (IP_TTL)");
            return -1;
        }
    }
    return 0;
}

static int
get_local_ip(char *ip, size_t ip_size) {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                continue;
            }

            // Skips loopback interface
            if (strcmp(ifa->ifa_name, "lo") != 0) {
                strncpy(ip, host, ip_size);
                ip[ip_size - 1] = '\0';
                freeifaddrs(ifaddr);
                return 0;
            }
        }
    }

    freeifaddrs(ifaddr);
    return -1;
}

static int
init_socket() {
    g_stats.sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (g_stats.sockfd == -1) {
        perror("socket");
        return -1;
    }
    return 0;
}

static void
init_local_ip(char *local_ip) {
    if (get_local_ip(local_ip, sizeof(local_ip)) == 0) {
        strncpy(g_stats.local_ip, local_ip, sizeof(g_stats.local_ip));
        g_stats.local_ip[sizeof(g_stats.local_ip) - 1] = '\0';
    } else {
        strncpy(g_stats.local_ip, "0.0.0.0", sizeof(g_stats.local_ip));
        g_stats.local_ip[sizeof(g_stats.local_ip) - 1] = '\0';
    }
}

static bool
is_unexpected_packet(struct icmp *icmp, struct icmp *icmp_header, const int count) {
    const bool is_echo_reply = icmp->icmp_type == ICMP_ECHOREPLY;
    const bool id_matches = icmp->icmp_id == icmp_header->icmp_id;
    const bool seq_matches = icmp->icmp_seq == count;

    return !is_echo_reply || !id_matches || !seq_matches;
}

// Try to receive packets from the sockfd until we get the one we are expecting
static int
receive_packet(char *const buf, const int buflen, Recv *recv, const int count, struct icmp *const icmp_header,
               const struct timeval trip_begin, const Args *const args, const char *const ip_str) {

    while (true) {
        ssize_t recv_len = recvfrom(g_stats.sockfd, buf, buflen, 0, (struct sockaddr *)&recv->addr, &recv->len);

        struct iphdr *ip = (struct iphdr *)buf;
        size_t ip_header_len = ip->ihl << 2;
        struct icmp *icmp = (struct icmp *)(buf + ip_header_len);

        if (recv_len <= 0) {
            recv_error(icmp, count, recv_len);
            return 0;
        }

        if (is_unexpected_packet(icmp, icmp_header, count)) {
            continue;
        }

        double rt_ms = update_stats(&trip_begin);
        if ((int)rt_ms == -1) {
            close(g_stats.sockfd);
            return -1;
        }
        display_rt_stats(args->v, ip_str, icmp, ip, rt_ms);

        return 0;
    }
}

static int
ping(const Args *const args, struct sockaddr_in *const send_addr) {
    
    char buf[1024];
    struct sockaddr_in recv_addr = {0};
    socklen_t addr_len = sizeof(recv_addr);
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(send_addr->sin_addr), ip_str, INET_ADDRSTRLEN);

    printf("PING %s (%s) %d(%zu) data bytes\n", args->dest, ip_str, PAYLOAD_SIZE, sizeof(struct icmp) + PAYLOAD_SIZE);

    char packet[sizeof(struct icmp) + PAYLOAD_SIZE] = {0};
    struct icmp *icmp_header = (struct icmp *)packet;
    memset(packet + sizeof(struct icmp), 0x42, PAYLOAD_SIZE);
    init_icmp_header(icmp_header, 0, packet, sizeof(packet));

    init_stats();
    if (set_socket_options(args) == -1) {
        close(g_stats.sockfd);
        return EXIT_FAILURE;
    }

    signal(SIGINT, sigint);

    for (int count = 1; count; count++) {
        init_icmp_header(icmp_header, count, packet, sizeof(packet));

        if (send_packet(packet, sizeof(packet), send_addr) == -1) {
            continue;
        }

        struct timeval trip_begin;
        if (gettimeofday(&trip_begin, NULL) == -1) {
            close(g_stats.sockfd);
            return EXIT_FAILURE;
        }

        Recv recv = {
            {buf, sizeof(buf)},
            recv_addr, addr_len
        };
        if (receive_packet(buf, sizeof(buf), &recv, count, icmp_header, trip_begin, args, ip_str) == -1) {
            return EXIT_FAILURE;
        }
        usleep(PING_INTERVAL);
    }
    close(g_stats.sockfd);
    return EXIT_SUCCESS;
}

int
main(int ac, char **av) {
    Args args = {0};
    if (init_socket() == -1) {
        return EXIT_FAILURE;
    }

    if (parse_args(ac, (const char **)av, &args) == -1) {
        close(g_stats.sockfd);
        return EXIT_FAILURE;
    }

    if (args.h) {
        return help();
    }

    struct sockaddr_in send_addr = {0};
    if (get_send_addr(&args, &send_addr) == -1) {
        close(g_stats.sockfd);
        return EXIT_FAILURE;
    }

    char local_ip[INET_ADDRSTRLEN];
    init_local_ip(local_ip);

    if (args.v) {
        printf("ft_ping: sockfd: %d (socktype SOCK_RAW), hints.ai_family: AF_INET\n\n", g_stats.sockfd);
        printf("ai-ai_family: AF_INET, ai->ai_canonname: '%s'\n", args.dest);
    }

    strncpy(g_stats.host, args.dest, sizeof(g_stats.host));
    g_stats.host[sizeof(g_stats.host) - 1] = '\0';

    if (ping(&args, &send_addr) == -1) {
        return EXIT_FAILURE;
    }
}