#include "ping.h"
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

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
