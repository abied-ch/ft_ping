#include "ft_ping.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_iovec.h>
#include <bits/types/struct_timeval.h>
#include <errno.h>
#include <limits.h>
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

#define PACKET_SIZE 64
#define PAYLOAD_SIZE 56
#define MAX_PINGS 1024
double rtts[MAX_PINGS];

Stats          stats = {0};
struct timeval start_time;

void init_icmp_header(struct icmp* icmp_header, int seq, char* packet, int packet_len) {
    icmp_header->icmp_type = ICMP_ECHO;
    icmp_header->icmp_id   = getpid();
    icmp_header->icmp_seq  = seq;

    /*
    To future self: before thinking `this line is useless, I am overwriting the checksum
    anyway so why reset it beforehand`:

    `icmp_header` and `packet` point to the same memory address, they are just _cast to different types_.
    Removing this line will result in the checksum not matching and all packets (except for the first
    one) being lost!
    */
    icmp_header->icmp_cksum = 0;
    icmp_header->icmp_cksum = checksum(packet, packet_len);
}

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

int parse_args(int ac, char** av, Args* args) {
    int i = 0;

    while (++i < ac) {
        if (av[i][0] == '-') {
            if (!strcmp(av[i], "-v")) {
                args->verbose = true;
            } else if (!strcmp(av[i], "-h") || !strcmp(av[i], "-?")) {
                args->help = true;
                return EXIT_SUCCESS;
            } else {
                fprintf(stderr, "Unknown option: %s\n", av[i]);
                args->help = true;
                return ARG_ERR;
            }
        } else {
            if (args->dest == NULL) {
                args->dest = av[i];
            } else {
                fprintf(stderr, "Unexpected argument: %s\n", av[i]);
                args->help = true;
                return ARG_ERR;
            }
        }
    }
    if (!args->dest && !args->help) {
        fprintf(stderr, "Destination address required\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int help() {
    write(1, HELP, sizeof(HELP));
    return 2;
}

int get_send_addr(Args args, struct sockaddr_in* send_addr) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    int err = getaddrinfo(args.dest, NULL, &hints, &res);
    if (err != EXIT_SUCCESS) {
        if (args.verbose) {
            printf("ft_ping: sockfd: %d (socktype SOCK_RAW), hints.ai_family: AF_INET\n\n", stats.sockfd);
        }
        fprintf(stderr, "ft_ping: %s: %s\n", args.dest, gai_strerror(err));
        return EXIT_FAILURE;
    }
    struct sockaddr_in* addr = (struct sockaddr_in*)res->ai_addr;
    send_addr->sin_family    = addr->sin_family;
    send_addr->sin_addr      = addr->sin_addr;
    freeaddrinfo(res);
    return EXIT_SUCCESS;
}

int main(int ac, char** av) {
    stats.sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (stats.sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }
    Args args = {0};
    if (parse_args(ac, av, &args) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    if (args.help) {
        return help();
    }

    struct sockaddr_in send_addr = {0};
    if (get_send_addr(args, &send_addr) != 0) {
        return EXIT_FAILURE;
    }

    if (args.verbose) {
        printf("ft_ping: sockfd: %d (socktype SOCK_RAW), hints.ai_family: AF_INET\n\n", stats.sockfd);
        printf("ai-ai_family: AF_INET, ai->ai_canonname: '%s'\n", args.dest);
    }

    strncpy(stats.dest_host, args.dest, sizeof(stats.dest_host));
    stats.dest_host[sizeof(stats.dest_host) - 1] = '\0';

    char               buffer[1024];
    struct sockaddr_in recv_addr;
    socklen_t          addr_len = sizeof(recv_addr);
    gettimeofday(&start_time, NULL);
    stats.start_time = start_time;
    printf("PING %s %zu data bytes\n", args.dest, sizeof(struct icmp) + PAYLOAD_SIZE);
    stats.rtt_min = INFINITY;
    stats.rtt_max = 0.0;
    stats.rtt_avg = 0.0;

    char         packet[sizeof(struct icmp) + PAYLOAD_SIZE] = {0};
    struct icmp* icmp_header                                = (struct icmp*)packet;
    memset(packet + sizeof(struct icmp), 0x42, PAYLOAD_SIZE);
    init_icmp_header(icmp_header, 0, packet, sizeof(packet));

    struct timeval timeout;
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;
    if (setsockopt(stats.sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        return EXIT_FAILURE;
    }
    int failed_attempts = 0;
    signal(SIGINT, sigint);
    for (int count = 1; count; count++) {
        init_icmp_header(icmp_header, count, packet, sizeof(packet));

        if (sendto(stats.sockfd, packet, sizeof(packet), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) <= 0) {
            perror("sendto");
            failed_attempts++;
            if (failed_attempts >= 5) {
                fprintf(stderr, "too many consecutive failures, exiting.");
                break;
            }
            continue;
        }

        stats.transmitted++;

        struct timeval trip_begin, trip_end;
        gettimeofday(&trip_begin, NULL);

        bool received_reply = false;
        while (true) {
            ssize_t recv_len = recvfrom(stats.sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&recv_addr, &addr_len);
            if (recv_len <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (args.verbose) {
                        printf("Request timeout for icmp_sec %d\n", count);
                    }
                    break;
                } else {
                    perror("recvfrom");
                    break;
                }
            }
            struct iphdr* ip            = (struct iphdr*)buffer;
            size_t        ip_header_len = ip->ihl << 2;
            if (recv_len < (ssize_t)(ip_header_len + sizeof(struct icmp))) {
                fprintf(stderr, "Error: Received packet is too short to be valid\n");
                continue;
            }
            struct icmp* icmp = (struct icmp*)(buffer + ip_header_len);

            if (icmp->icmp_type == ICMP_ECHOREPLY && icmp->icmp_id == icmp_header->icmp_id && icmp->icmp_seq == count) {
                gettimeofday(&trip_end, NULL);
                double ttl_ms = (trip_end.tv_sec - trip_begin.tv_sec) * 1000.0 + (trip_end.tv_usec - trip_begin.tv_usec) / 1000.0;

                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(recv_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                if (args.verbose) {
                    printf("%d bytes from %s: icmp_seq=%u ident=%d ttl=%u time=%.3f ms\n", PACKET_SIZE, ip_str, icmp->icmp_seq, icmp->icmp_id, ip->ttl, ttl_ms);
                } else {
                    printf("%d bytes from %s: icmp_seq=%u ttl=%u time=%.3f ms\n", PACKET_SIZE, ip_str, icmp->icmp_seq, ip->ttl, ttl_ms);
                }

                if (stats.received < MAX_PINGS) {
                    rtts[stats.received] = ttl_ms;
                }

                stats.received++;
                stats.rtt_min = fmin(stats.rtt_min, ttl_ms);
                stats.rtt_max = fmax(stats.rtt_max, ttl_ms);
                stats.rtt_avg = ((stats.rtt_avg * (stats.received - 1)) + ttl_ms) / stats.received;

                double sum_deviation = 0.0;
                for (size_t i = 0; i < stats.received; ++i) {
                    sum_deviation += fabs(rtts[i] - stats.rtt_avg);
                }
                stats.rtt_mdev = sum_deviation / stats.received;

                received_reply = true;
                break;
            }
        }
        if (!received_reply) {
            failed_attempts++;
            if (failed_attempts >= 5) {
                fprintf(stderr, "Too many consecutive failures, exiting.\n");
                break;
            }
        } else {
            failed_attempts = 0;
        }
        usleep(PING_INTERVAL);
    }

    close(stats.sockfd);
    return 0;
}
