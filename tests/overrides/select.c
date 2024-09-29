#include <asm-generic/errno.h>
#include <errno.h>
#include <sys/select.h>

extern int
select(int __nfds, fd_set *__restrict __readfds, fd_set *__restrict __writefds, fd_set *__restrict __exceptfds, struct timeval *__restrict __timeout) {
    (void)__nfds;
    (void)__readfds;
    (void)__writefds;
    (void)__exceptfds;
    (void)__timeout;
    errno = EBADF;
    return -1;
}
