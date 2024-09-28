#include "errno.h"
#include "ping.h"
#include <netdb.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Prints help message and returns `0`.
int
help() {
    fprintf(stderr, "\n"
                    "Usage: ./ft_ping [OPTION...] HOST\n"
                    "Send ICMP ECHO_REQUEST packets to network hosts.\n"
                    "\n"
                    "Options:\n"
                    "  -c, --count <n>      stop after n replies\n"
                    "  -d, --debug          set the SO_DEBUG socket option\n"
                    "  -D, --timestamp      print timestamps\n"
                    "  -h, -?, --help       print this help message\n"
                    "  -i, --interval <n>   wait n seconds between sending each packet\n"
                    "  -q, --quiet          quiet output\n"
                    "  -t, --ttl <n>        define time to live\n"
                    "  -T, --tos <n>        set type of service (TOS) to n\n"
                    "  -v, --verbose        verbose output\n"
                    "  -V, --verion         print program version\n"
                    "  -w, --timeout <n>    stop after n seconds\n"
                    "\n"
                    "Report bugs to <abied-ch@student.42vienna.com>\n");
    return 0;
}

int
version() {
    fprintf(stderr, "ft_ping (abied-ch 42 projects) 0.1\n"
                    "This is free software: you are free to change and redistribute it.\n"
                    "There is NO WARRANTY, I will not be held accountable for any stupid behavior.\n"
                    "\n"
                    "Written by Arthur Bied-Charreton.\n");
    return 0;
}

// Returns e `Result` struct containing the appropriate message based on the error.
Result
recv_error(const struct icmp *const icmp, const int seq, const int recv_len) {
    char seq_str[10];
    char icmp_code_str[10];

    sprintf(seq_str, "%d", seq);
    sprintf(icmp_code_str, "%d", icmp->icmp_code);

    if (icmp->icmp_type == ICMP_DEST_UNREACH) {
        g_stats.errs++;
        switch (icmp->icmp_code) {
        case ICMP_NET_UNREACH | ICMP_HOST_UNREACH:
            return err_fmt(5, "From ", g_stats.local_ip, " icmp_seq=", seq_str, " Destination Host Unreachable\n");
        case ICMP_FRAG_NEEDED:
            return err_fmt(5, "From ", g_stats.local_ip, " icmp_seq=", seq_str, " Fragmentation Needed\n");
        default:
            return err_fmt(7, "From ", g_stats.local_ip, " icmp_seq=", seq_str, " Destination unreachable, code: ", icmp_code_str, "\n");
        }
    } else if (icmp->icmp_type == ICMP_TIME_EXCEEDED) {
        g_stats.errs++;
        if (icmp->icmp_code == ICMP_EXC_TTL) {
            return err_fmt(5, "From ", g_stats.local_ip, " icmp_seq=", seq_str, " Time to live exceeded\n");
        } else {
            return err_fmt(5, "From ", g_stats.local_ip, " icmp_seq=", seq_str, " Time exceeded\n");
        }
    } else if (recv_len <= 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return err(NULL);
        }
    }
    return err_fmt(3, "recvfrom: ", strerror(errno), "\n");
}

// Performs full cleanup and returns `exit_code`.
int
cleanup(const int exit_code, Args *const args) {
    if (g_stats.alloc.sockfd != -1) {
        close(g_stats.alloc.sockfd);
    }
    if (args) {
        free(args);
    }
    return exit_code;
}
