#include "ping.h"
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

// Handles `SIGINT`.
// Displays the final stats (errors, rtt_(min, max, avg, mdev)) and frees up
// resources allocated during the ping process.
void
sigint(const int sig) {
    if (sig != SIGINT) {
        return;
    }

    stats_display_final();

    close(g_stats.sockfd);
    free(g_stats.args);

    exit(EXIT_SUCCESS);
}
