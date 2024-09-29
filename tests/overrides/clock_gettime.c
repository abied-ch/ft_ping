#include <bits/types/clockid_t.h>
#include <time.h>

int
clock_gettime(clockid_t clockid, struct timespec *res) {
    (void)clockid;
    (void)res;
    return -1;
}
