#include <bits/types/clockid_t.h>
#include <errno.h>
#include <time.h>

extern int
clock_nanosleep(clockid_t __clock_id, int __flags, const struct timespec *__req, struct timespec *__rem) {
    (void)__clock_id;
    (void)__flags;
    (void)__req;
    (void)__rem;
    errno = ETIMEDOUT;
    return -1;
}
