#include <bits/types/clockid_t.h>
#include <bits/types/struct_iovec.h>
#include <errno.h>
#include <time.h>

void *
calloc(size_t __nmemb, size_t __size) {
    (void)__nmemb;
    (void)__size;
    errno = ENOMEM;
    return NULL;
}

