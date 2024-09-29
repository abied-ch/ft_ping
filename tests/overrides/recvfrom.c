#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>

extern ssize_t
recvfrom(int __fd, void *__restrict __buf, size_t __n, int __flags, __SOCKADDR_ARG __addr, socklen_t *__restrict __addr_len) {
    (void)__fd;
    (void)__buf;
    (void)__n;
    (void)__flags;
    (void)__addr;
    (void)__addr_len;
    errno = ENOMEM;
    return -1;
}
