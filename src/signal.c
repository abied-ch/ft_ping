#include "ft_ping.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

void
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
    double tot_ms = (end.tv_sec - g_stats.start_time->tv_sec) * 1000.0 + (end.tv_usec - g_stats.start_time->tv_usec) / 1000.0;

    printf("\n--- %s ping statistics ---\n%u packets transmitted, %u received", g_stats.dest, g_stats.sent, g_stats.rcvd);
    if (g_stats.errs != 0) {
        printf(", +%d errors", g_stats.errs);
    }

    printf(", %d%% packet loss time %dms\n", loss, (int)tot_ms);
    printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n", g_stats.rtt_min, g_stats.rtt_avg, g_stats.rtt_max, g_stats.rtt_mdev);
    close(g_stats.sockfd);
    exit(EXIT_SUCCESS);
}