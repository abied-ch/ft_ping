#include "ft_ping.h"
#include "math.h"
#include <sys/time.h>
#include <time.h>

double
update_stats(const struct timeval *const trip_begin) {
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