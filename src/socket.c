#include "ft_ping.h"
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

Result
init_socket(int *const sockfd) {
    *sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (*sockfd == -1) {
        return err(strerror(errno));
    }
    return ok(NULL);
}