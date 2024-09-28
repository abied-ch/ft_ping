#include "ping.h"
#include "errno.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/floatn-common.h>
#include <bits/time.h>
#include <bits/types.h>
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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

Stats g_stats = {0};

// (probably overengineered)
// Checks whether the loop conditions are met. Can be called in the main loop's control structure.
// I originally thought there would be more to check than the `-c` argument, hence why I made an
// extra function for it.
static bool
loop_condition(const Args *const args, const int seq) {
    if (args->cli.c != 0) {
        return seq <= args->cli.c;
    }

    if (args->cli.w != 0) {
        struct timespec end_time = {0};
        if (clock_gettime(CLOCK_MONOTONIC, &end_time) == -1) {
            return false;
        }
        if (end_time.tv_sec - args->cli.w >= g_stats.start_time.tv_sec) {
            return false;
        }
    }

    return seq < MAX_PINGS;
}

// Calculates the time to sleep in order to maintain the same interval between each ping.
// .
// Returns:
// `.type == OK` on success
// `.type == ERR` on error (`clock_gettime` & `clock_nanosleep` are both subject to failure)
static Result
adjust_sleep(struct timespec start_time, const double interval) {
    struct timespec end_time = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &end_time) == -1) {
        return err_fmt(3, "clock_gettime: ", strerror(errno), "\n");
    }

    double elapsed = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    double remaining = (interval - elapsed);

    if (remaining > 0) {
        struct timespec ts;
        ts.tv_sec = (time_t)floor(remaining);
        ts.tv_nsec = ((remaining - floor(remaining)) * 1e9);

        if (clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL) == -1) {
            return err_fmt(3, "clock_nanosleep: ", strerror(errno), "\n");
        }
    }
    return ok(NULL);
}

static Result
fd_wait(const Args *const args, struct timespec trip_begin, const int seq) {
    fd_set readfds;
    struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};

    FD_ZERO(&readfds);
    FD_SET(g_stats.alloc.sockfd, &readfds);

    int ready = select(g_stats.alloc.sockfd + 1, &readfds, NULL, NULL, &timeout);
    if (ready < 0) {
        return err_fmt(3, "select: ", strerror(errno), "\n");
    } else if (ready == 0) {
        return ok(NULL);
    }

    return icmp_recv_packet((Args *)args, seq, &trip_begin);
}

static Result
loop(const Args *const args) {
    Result res;

    if (clock_gettime(CLOCK_MONOTONIC, &g_stats.start_time)) {
        return err_fmt(2, strerror(errno), "\n");
    }

    for (int seq = 1; seq; ++seq) {
        struct timespec trip_begin;
        if (clock_gettime(CLOCK_MONOTONIC, &trip_begin) == 1) {
            return err_fmt(3, "clock_gettime: ", strerror(errno), "\n");
        }

        icmp_init_header((Args *)args, seq);

        res = icmp_send_packet(args, (struct sockaddr_in *)&args->addr.send);
        if (res.type == ERR) {
            err_unwrap(res, args->cli.q);
            continue;
        }

        res = fd_wait(args, trip_begin, seq);
        if (res.type == ERR) {
            err_unwrap(res, args->cli.q);
            if (args->cli.v && res.val.err) {
                icmp_ip_hdr_dump(args->ip_h, args->icmp_h);
            }
        }

        res = adjust_sleep(trip_begin, args->cli.i);
        if (res.type == ERR) {
            return res;
        }

        if (!loop_condition(args, seq + 1)) {
            break;
        }
    }

    stats_display_final();

    return ok(NULL);
}

// Starts the ping loop.
Result
ping(const Args *const args) {
    Result res;

    if (!inet_ntop(AF_INET, &(args->addr.send.sin_addr), (char *)args->ip_str, INET_ADDRSTRLEN)) {
        return err_fmt(3, strerror(errno), "\n");
    }

    memset((void *)args->packet + (sizeof(struct icmp)), 0x42, PAYLOAD_SIZE);
    icmp_init_header((Args *)args, 0);

    fprintf(stdout, "PING %s (%s) %d data bytes", args->cli.dest, args->ip_str, PAYLOAD_SIZE);

    if (args->cli.v) {
        fprintf(stdout, ", id 0x%04x = %d", ntohs(args->ip_h->id), args->ip_h->id);
    }

    fprintf(stdout, "\n");

    res = flood_check(args);
    if (res.type == ERR) {
        return res;
    }

    res = socket_set_options();
    if (res.type == ERR) {
        return res;
    }

    res = loop(args);
    if (res.type == ERR) {
        return res;
    }

    return ok(NULL);
}

// Sets start time, initializes socket file descriptor (into `g_stats.sockfd` to allow
// access in signal handlers) and parses command line arguments.
// .
// Returns:
// - `.type == OK`, `.val.val == Args *` on success
// - `.type == ERR`, on failure
Result
ping_init(const int ac, char **av) {
    Result res;

    Args *args = calloc(sizeof(Args), 1);
    if (!args) {
        return err_fmt(3, "calloc: ", strerror(errno), "\n");
    }

    res = socket_init(&g_stats.alloc.sockfd);
    if (res.type == ERR) {
        return res;
    }

    g_stats.rtt.min = __builtin_inff64();

    if (signal(SIGINT, sigint) == SIG_ERR) {
        free(args);
        close(g_stats.alloc.sockfd);
        return err_fmt(3, "signal: ", strerror(errno), "\n");
    }

    res = parse_cli_args(ac, av, args);
    if (res.type == ERR) {
        free(args);
        close(g_stats.alloc.sockfd);
        return res;
    }

    return ok(args);
}
