#include "math.h"
#include "ping.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

double
stats_update(const struct timeval *const trip_begin) {
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

void
stats_display_final() {
    if (g_stats.sent < 1) {
        g_stats.sent = 1;
    }
    int loss = 100 - (g_stats.rcvd * 100) / g_stats.sent;
    struct timeval end;
    gettimeofday(&end, NULL);
    double tot_ms = (end.tv_sec - g_stats.start_time.tv_sec) * 1000.0 + (end.tv_usec - g_stats.start_time.tv_usec) / 1000.0;

    printf("\n--- %s ping statistics ---\n%u packets transmitted, %u received", g_stats.dest, g_stats.sent, g_stats.rcvd);
    if (g_stats.errs != 0) {
        printf(", +%d errors", g_stats.errs);
    }

    printf(", %d%% packet loss time %dms\n", loss, (int)tot_ms);

    if (g_stats.rcvd > 0) {
        printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n", g_stats.rtt_min, g_stats.rtt_avg, g_stats.rtt_max, g_stats.rtt_mdev);
    } else {
        printf("\n");
    }
}

void
stats_display_rt(const Args *const args, const struct icmp *const icmp, const struct iphdr *const ip, const double ms) {
    if (args->cli.q) {
        return;
    }

    printf("%d bytes from %s: imcp_seq=%u ", PACKET_SIZE, args->ip_str, icmp->icmp_seq);
    if (args->cli.v) {
        printf("ident=%d ", icmp->icmp_id);
    }
    printf("ttl=%u time=%.3f ms\n", ip->ttl, ms);
}
