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

// Sets start time, initializes socket file descriptor (into `g_stats.sockfd` to allow
// access in signal handlers) and parses command line arguments.
// .
// Returns:
// - `.type == OK`, `.val.val` type: `Args *` on success
// - `.type == ERR`, on failure
static Result
init(const int ac, char **av) {
    Result res;

    Args *args = calloc(sizeof(Args), 1);
    if (!args) {
        return err(strerror(errno));
    }

    args->recv_addr_len = sizeof(args->recv_addr);

    res = socket_init(&g_stats.sockfd);
    if (res.type == ERR) {
        return res;
    }

    g_stats.rtt_min = __builtin_inff64();

    if (signal(SIGINT, sigint) == SIG_ERR) {
        free(args);
        close(g_stats.sockfd);
        return err_fmt(2, "signal: ", strerror(errno));
    }

    return parse_cli_args(ac, av, args);
}

static bool
loop_condition(const Args *const args, const int seq) {
    if (args->cli.c != -1) {
        return seq <= args->cli.c;
    }

    return true;
}

// Calculates the time to sleep in order to maintain the same interval between each ping.
// .
// WHY `clock_nanosleep` IS NOT PROTECTED:
// Opening `man clock_nanosleep` shows that every possible error would be due to invalid input
// (except for `EINTR`, which would be handled by the signal handler). All input is sanitized
// at some point before being passed to `clock_nanosleep`, therefore no protection is needed.
static Result
adjust_sleep(struct timeval start_time, const double interval) {
    struct timespec end_time = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &end_time) == -1) {
        return err_fmt(2, "clock_gettime: ", strerror(errno));
    }

    double elapsed = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_usec) / 1000000000.0;
    double remaining = (interval - elapsed);

    if (remaining > 0) {
        struct timespec ts;
        ts.tv_sec = (time_t)floor(interval);
        ts.tv_nsec = (__syscall_slong_t)((interval - floor(interval)) * 1000000000);

        clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
    }
    return ok(NULL);
}

static Result
loop(const Args *const args) {
    Result res;

    if (gettimeofday(&g_stats.start_time, NULL) == -1) {
        return err(strerror(errno));
    }
    for (int seq = 1; loop_condition(args, seq); ++seq) {
        struct timeval trip_begin;
        if (gettimeofday(&trip_begin, NULL) == 1) {
            return err_fmt(2, "gettimeofday: ", strerror(errno));
        }

        icmp_init_header((Args *)args, seq);

        res = icmp_send_packet(args, (struct sockaddr_in *)&args->send_addr);
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

static Result
ping(const Args *const args) {
    Result res;

    if (!inet_ntop(AF_INET, &(args->send_addr.sin_addr), (char *)args->ip_str, INET_ADDRSTRLEN)) {
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

int
main(int ac, char **av) {
    Result res;

    res = init(ac, av);
    if (res.type == ERR) {
        err_unwrap(res, false);
        return cleanup(EXIT_FAILURE, NULL);
    }

    Args *args = (Args *)res.val.val;

    if (args->cli.h) {
        return cleanup(help(), args);
    }

    res = get_send_addr(args, &args->send_addr);
    if (res.type == ERR) {
        err_unwrap(res, false);
        return cleanup(EXIT_FAILURE, args);
    }

    init_local_ip();

    if (args->cli.v) {
        printf("ft_ping: sockfd: %d (socktype SOCK_RAW), hints.ai_family: AF_INET\n\n", g_stats.sockfd);
        printf("ai-ai_family: AF_INET, ai->ai_canonname: '%s'\n", args->cli.dest);
    }

    strncpy(g_stats.dest, args->cli.dest, sizeof(g_stats.dest));
    g_stats.dest[sizeof(g_stats.dest) - 1] = '\0';

    g_stats.args = args;

    res = ping(args);
    if (res.type == ERR) {
        err_unwrap(res, false);
        return cleanup(EXIT_FAILURE, args);
    }

    return cleanup(EXIT_SUCCESS, args);
}
