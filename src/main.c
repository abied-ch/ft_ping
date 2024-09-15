#include "ft_ping.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_iovec.h>
#include <bits/types/struct_timeval.h>
#include <limits.h>
#include <math.h>
#include <netdb.h>
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

#define MAX_PINGS 1024
#define PAYLOAD_SIZE 56
double rtts[MAX_PINGS];

struct stats   stats = {0};
struct timeval start_time;
struct args    args = {0};

void init_icmp_header(struct icmp_h* icmp_header) {
    icmp_header->type = ICMP_ECHO;
    icmp_header->id   = getpid();
    icmp_header->seq  = 1;
}

// SAFETY:
// It is the caller's responsibility to ensure that `s` is
// pointing to a valid, NULL-terminated string.
size_t ft_strlen(const char* s) {
    size_t len = 0;

    while (s[len]) {
        ++len;
    }
    return len;
}

#define SIGINT_MSG                                                                                                                                             \
    "\n--- %s ping statistics ---\n%u packets transmitted, %u received, %d%% "                                                                                 \
    "packet loss time %dms\nrtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n"

void sigint(int sig) {
    if (sig != SIGINT) {
        return;
    }
    if (stats.transmitted < 1) {
        stats.transmitted = 1;
    }
    int            loss = 100 - (stats.received * 100) / stats.transmitted;
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_ms = (end_time.tv_sec - stats.start_time.tv_sec) * 1000.0 + (end_time.tv_usec - stats.start_time.tv_usec) / 1000.0;

    printf(SIGINT_MSG, stats.dest_host, stats.transmitted, stats.received, loss, (int)total_ms, stats.rtt_min, stats.rtt_avg, stats.rtt_max, stats.rtt_mdev);
    close(stats.sockfd);
    exit(EXIT_SUCCESS);
}

int parse_args(int ac, char** av) {
    if (ac < 2) {
        return EXIT_FAILURE;
    }
    for (int i = 0; av[i]; ++i) {
        switch (av[i][0]) {
        case '-':
            if (ft_strlen(av[i]) == 1) {
                args.dest = av[i];
                break;
            } else if (av[i][1] == 'v') {
                args.verbose = true;
                break;
            } else if (av[i][1] == 'h') {
                args.help = true;
                break;
            } else {
                args.help = true;
                return ARG_ERR;
            }
        default:
            args.dest = av[i];
            break;
        }
    }
    return args.dest == NULL && !args.help;
}

int help() {
    write(1, HELP, sizeof(HELP));
    return 2;
}

int main(int ac, char** av) {
    int res = parse_args(ac, ++av);
    if (res == EXIT_FAILURE) {
        fprintf(stderr, "ft_ping: usage error: Destination address required\n");
        return res;
    }

    if (args.help) {
        return help();
    }

    stats.sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (stats.sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in send_addr = {0};
    send_addr.sin_family         = AF_INET;

    struct hostent* host = gethostbyname(args.dest);
    if (host == NULL) {
        fprintf(stderr, "ft_ping: %s: Name or service not known\n", av[1]);
        return EXIT_FAILURE;
    }
    send_addr.sin_addr = *(struct in_addr*)host->h_addr_list[0];

    char               buffer[1024];
    struct sockaddr_in recv_addr;
    socklen_t          addr_len = sizeof(recv_addr);
    gettimeofday(&start_time, NULL);
    stats.start_time = start_time;
    printf("PING %s %zu data bytes\n", args.dest, sizeof(struct icmp_h) + PAYLOAD_SIZE);
    stats.rtt_min = INFINITY;
    stats.rtt_max = 0.0;
    stats.rtt_avg = 0.0;

    char           packet[sizeof(struct icmp_h) + PAYLOAD_SIZE] = {0};
    struct icmp_h* icmp_header                                  = (struct icmp_h*)packet;
    init_icmp_header(icmp_header);
    memset(packet + sizeof(struct icmp_h), 0x42, PAYLOAD_SIZE);
    icmp_header->cksum = 0;
    icmp_header->cksum = checksum(packet, sizeof(packet));

    signal(SIGINT, sigint);
    for (int count = 1; count; count++) {
        int failed_attempts = 0;

        struct timeval trip_begin, trip_end;
        gettimeofday(&trip_begin, NULL);

        icmp_header->seq   = count;
        icmp_header->cksum = 0;
        icmp_header->cksum = checksum(packet, sizeof(packet));

        if (sendto(stats.sockfd, packet, sizeof(packet), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) <= 0) {
            perror("sendto");
            failed_attempts++;
            if (failed_attempts >= 5) {
                fprintf(stderr, "too many consecutive failures, exiting.");
                break;
            }
            continue;
        }
        stats.transmitted += 1;

        ssize_t recv_len = recvfrom(stats.sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&recv_addr, &addr_len);
        if (recv_len <= 0) {
            perror("recvfrom");
            failed_attempts++;
            continue;
        }
        failed_attempts = 0;
        struct timeval timeout;
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;
        if (setsockopt(stats.sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("setsockopt");
            return EXIT_FAILURE;
        }

        gettimeofday(&trip_end, NULL);
        double ttl_ms = (trip_end.tv_sec - trip_begin.tv_sec) * 1000.0 + (trip_end.tv_usec - trip_begin.tv_usec) / 1000.0;

        struct iphdr* ip            = (struct iphdr*)buffer;
        size_t        ip_header_len = ip->ihl << 2;
        if (recv_len < (ssize_t)(ip_header_len + sizeof(struct icmp_h))) {
            fprintf(stderr, "error: received packet is too short to be valid");
            continue;
        }
        struct icmp_h* icmp = (struct icmp_h*)(buffer + (ip->ihl << 2));

        if (icmp->type == ICMP_ECHOREPLY && icmp->id == icmp_header->id) {
            printf("64 bytes from %s icmp_seq=%u ttl=%d time=%.3f ms\n", av[1], icmp->seq, ip->ttl, ttl_ms);

            if (count < MAX_PINGS) {
                rtts[count - 1] = ttl_ms;
            }

            stats.rtt_min = fmin(stats.rtt_min, ttl_ms);
            stats.rtt_max = fmax(stats.rtt_max, ttl_ms);

            stats.received++;
            stats.rtt_avg = ((stats.rtt_avg * (stats.received - 1)) + ttl_ms) / stats.received;

            double sum_deviation = 0.0;
            for (unsigned int i = 0; i < stats.received; i++) {
                sum_deviation += fabs(rtts[i] - stats.rtt_avg);
            }
            stats.rtt_mdev = sum_deviation / stats.received;
        }

        usleep(PING_INTERVAL);
    }

    close(stats.sockfd);
    return 0;
}
