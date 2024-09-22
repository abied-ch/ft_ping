#include "errno.h"
#include "ft_ping.h"
#include <netdb.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>

void
recv_error(const struct icmp *const icmp, const int seq, const int recv_len) {
    g_stats.errs++;
    if (icmp->icmp_type == ICMP_DEST_UNREACH) {
        switch (icmp->icmp_code) {
        case ICMP_NET_UNREACH:
            fprintf(stderr, "From %s icmp_seq=%d Destination Network Unreachable\n", g_stats.local_ip, seq);
            break;
        case ICMP_HOST_UNREACH:
            fprintf(stderr, "From %s icmp_seq=%d Destination Host Unreachable\n", g_stats.local_ip, seq);
            break;
        case ICMP_FRAG_NEEDED:
            fprintf(stderr, "From %s icmp_seq=%d Fragmentation needed\n", g_stats.local_ip, seq);
            break;
        default:
            fprintf(stderr, "From %s icmp_seq=%d Destination unreachable, code: %d\n", g_stats.local_ip, seq, icmp->icmp_code);
            break;
        }
    } else if (icmp->icmp_type == ICMP_TIME_EXCEEDED) {
        if (icmp->icmp_code == ICMP_EXC_TTL) {
            fprintf(stderr, "From %s icmp_seq=%d Time to live exceeded\n", g_stats.local_ip, seq);
        } else {
            fprintf(stderr, "From %s icmp_seq=%d Time exceeded\n", g_stats.local_ip, seq);
        }
    } else if (recv_len <= 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stderr, "From %s icmp_seq=%d %s\n", g_stats.local_ip, seq, gai_strerror(errno));
        } else {
            perror("recvfrom");
        }
    }
}