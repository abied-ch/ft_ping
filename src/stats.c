#include "math.h"
#include "ping.h"
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// Updates the stats with the data of the current round trip.
double
stats_update(const struct timespec *const trip_begin) {
    struct timespec trip_end;

    if (clock_gettime(CLOCK_MONOTONIC, &trip_end) == -1) {
        return -1;
    }

    double rt_ms = (trip_end.tv_sec - trip_begin->tv_sec) * 1e3 + (trip_end.tv_nsec - trip_begin->tv_nsec) / 1e6;

    g_stats.rcvd++;
    g_stats.rtt.min = fmin(g_stats.rtt.min, rt_ms);
    g_stats.rtt.max = fmax(g_stats.rtt.max, rt_ms);
    g_stats.rtt.avg = ((g_stats.rtt.avg * (g_stats.rcvd - 1)) + rt_ms) / g_stats.rcvd;

    double sum_deviation = 0.0;

    if (g_stats.rcvd < MAX_PINGS) {
        g_stats.rtts[g_stats.rcvd - 1] = rt_ms;
    }

    for (size_t i = 0; i < g_stats.rcvd; ++i) {
        sum_deviation += fabs(g_stats.rtts[i] - g_stats.rtt.avg);
    }
    g_stats.rtt.mdev = sum_deviation / g_stats.rcvd;

    return rt_ms;
}

// Displays the stats for the full ping lifetime.
void
stats_display_final() {
    if (g_stats.sent < 1) {
        g_stats.sent = 1;
    }
    int loss = 100 - (g_stats.rcvd * 100) / g_stats.sent;

    struct timespec end;
    if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
        perror("clock_gettime");
        return;
    }

    double tot_ms = (end.tv_sec - g_stats.start_time.tv_sec) * 1e3 + (end.tv_nsec - g_stats.start_time.tv_nsec) / 1e6;

    fprintf(stdout, "\n--- %s ping statistics ---\n%u packets transmitted, %u received", g_stats.dest, g_stats.sent, g_stats.rcvd);
    if (g_stats.errs != 0) {
        fprintf(stdout, ", +%d errors", g_stats.errs);
    }

    fprintf(stdout, ", %d%% packet loss, time %.0fms\n", loss, tot_ms);

    if (g_stats.rcvd > 0) {
        fprintf(stdout, "rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n", g_stats.rtt.min, g_stats.rtt.avg, g_stats.rtt.max, g_stats.rtt.mdev);
    } else {
        fprintf(stdout, "\n");
    }
}

// Displays the stats for the current roundtrip. May add a unix timestamp or `ident` field of the IP header depending on
// which CLI options are set.
void
stats_display_rt(const Args *const args, const struct icmp *const icmp, const struct iphdr *const ip, const double ms) {
    if (args->cli.q) {
        return;
    }

    if (args->cli.D) {
        struct timespec timestamp;
        if (clock_gettime(CLOCK_REALTIME, &timestamp) == -1) {
            perror("clock_gettime");
            return;
        }

        fprintf(stdout, "[%ld.%06ld] ", timestamp.tv_sec, timestamp.tv_nsec / 1000);
    }

    fprintf(stdout, "%d bytes from %s: icmp_seq=%u ", PACKET_SIZE, args->ip_str, icmp->icmp_seq);
    fprintf(stdout, "ttl=%u time=%.3f ms\n", ip->ttl, ms);
}
