#include "errno.h"
#include "ft_ping.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/floatn-common.h>
#include <bits/types/struct_iovec.h>
#include <bits/types/struct_timeval.h>
#include <ifaddrs.h>
#include <limits.h>
#include <linux/stddef.h>
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
// - `Result.type == OK`, `Result.val.val` type: `Args *` on success
// - `Result.type == ERR`, error message stored in `Result.err.err` on failure
static Result
init(const int ac, char **av) {
    Result res;

    Args *args = calloc(sizeof(Args), 1);
    if (!args) {
        return err(strerror(errno));
    }

    args->recv_addr_len = sizeof(args->recv_addr);

    if (gettimeofday(&g_stats.start_time, NULL) == -1) {
        return err(strerror(errno));
    }
    res = init_socket(&g_stats.sockfd);
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

// Performs full cleanup and returns `exit_code`.
static int
cleanup(const int exit_code, Args *const args) {
    if (g_stats.sockfd != -1) {
        close(g_stats.sockfd);
    }
    if (args) {
        free(args);
    }
    return exit_code;
}

static Result
loop(const Args *const args) {
    Result res;

    for (int seq = 1; seq; ++seq) {
        init_icmp_header((Args *)args, seq);

        res = send_packet(args, (struct sockaddr_in *)&args->send_addr);
        if (res.type == ERR) {
            continue;
        }

        struct timeval trip_begin;
        if (gettimeofday(&trip_begin, NULL) == 1) {
            return err_fmt(2, "gettimeofday: ", strerror(errno));
        }

        res = receive_packet((Args *)args, seq, &trip_begin);

        usleep(PING_INTERVAL);
    }

    return ok(NULL);
}

static Result
ping(const Args *const args) {
    Result res;

    if (!inet_ntop(AF_INET, &(args->send_addr.sin_addr), (char *)args->ip_str, INET_ADDRSTRLEN)) {
        return err(strerror(errno));
    }

    printf("PING %s (%s) %d(%zu) data bytes\n", args->cli.dest, args->ip_str, PAYLOAD_SIZE, sizeof(struct icmp) + PAYLOAD_SIZE);

    memset((void *)args->packet + (sizeof(struct icmp)), 0x42, PAYLOAD_SIZE);
    init_icmp_header((Args *)args, 0);

    res = set_socket_options(args);
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
        err_unwrap(res);
        return cleanup(EXIT_FAILURE, NULL);
    }

    Args *args = (Args *)res.val.val;

    if (args->cli.h) {
        return cleanup(help(), args);
    }

    res = get_send_addr(args, &args->send_addr);
    if (res.type == ERR) {
        err_unwrap(res);
        return cleanup(EXIT_FAILURE, args);
    }

    init_local_ip();

    if (args->cli.v) {
        printf("ft_ping: sockfd: %d (socktype SOCK_RAW), hints.ai_family: AF_INET\n\n", g_stats.sockfd);
        printf("ai-ai_family: AF_INET, ai->ai_canonname: '%s'\n", args->cli.dest);
    }

    strncpy(g_stats.dest, args->cli.dest, sizeof(g_stats.dest));
    g_stats.dest[sizeof(g_stats.dest) - 1] = '\0';

    res = ping(args);
    if (res.type == ERR) {
        err_unwrap(res);
        return cleanup(EXIT_FAILURE, args);
    }

    return cleanup(EXIT_SUCCESS, args);
}