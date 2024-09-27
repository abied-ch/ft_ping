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
    if (args->cli.c != -1) {
        return seq <= args->cli.c;
    }

    return true;
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
        return err_fmt(2, "clock_gettime: ", strerror(errno));
    }

    double elapsed = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    double remaining = (interval - elapsed);

    if (remaining > 0) {
        struct timespec ts;
        ts.tv_sec = (time_t)floor(remaining);
        ts.tv_nsec = (__syscall_slong_t)((remaining - floor(remaining)) * 1e9);

        if (clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL) == -1) {
            return err_fmt(2, "clock_nanosleep: ", strerror(errno));
        }
    }
    return ok(NULL);
}

static Result
loop(const Args *const args) {
    Result res;

    if (clock_gettime(CLOCK_MONOTONIC, &g_stats.start_time)) {
        return err(strerror(errno));
    }

    for (int seq = 1; seq; ++seq) {

        struct timespec trip_begin;
        if (clock_gettime(CLOCK_MONOTONIC, &trip_begin) == 1) {
            return err_fmt(2, "clock_gettime: ", strerror(errno));
        }

        icmp_init_header((Args *)args, seq);

        res = icmp_send_packet(args, (struct sockaddr_in *)&args->addr.send);
        if (res.type == ERR) {
            continue;
        }

        res = icmp_recv_packet((Args *)args, seq, &trip_begin);
        if (res.type == ERR) {
            err_unwrap(res, args->cli.q);
        }
        if (!loop_condition(args, seq + 1)) {
            break;
        }

        res = adjust_sleep(trip_begin, args->cli.i);
        if (res.type == ERR) {
            return res;
        }
    }

    stats_display_final();

    return ok(NULL);
}

Result
ping(const Args *const args) {
    Result res;

    if (!inet_ntop(AF_INET, &(args->addr.send.sin_addr), (char *)args->ip_str, INET_ADDRSTRLEN)) {
        return err(strerror(errno));
    }

    printf("PING %s (%s) %d(%zu) data bytes\n", args->cli.dest, args->ip_str, PAYLOAD_SIZE, sizeof(struct icmp) + PAYLOAD_SIZE);

    res = flood_check(args);
    if (res.type == ERR) {
        return res;
    }

    memset((void *)args->packet + (sizeof(struct icmp)), 0x42, PAYLOAD_SIZE);
    icmp_init_header((Args *)args, 0);

    res = socket_set_options(args);
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
        return err(strerror(errno));
    }

    res = socket_init(&g_stats.alloc.sockfd);
    if (res.type == ERR) {
        return res;
    }

    g_stats.rtt.min = __builtin_inff64();

    if (signal(SIGINT, sigint) == SIG_ERR) {
        free(args);
        close(g_stats.alloc.sockfd);
        return err_fmt(2, "signal: ", strerror(errno));
    }

    return parse_cli_args(ac, av, args);
}
