#include "errno.h"
#include "ping.h"
#include <netdb.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <string.h>

Result
recv_error(const struct icmp *const icmp, const int seq, const int recv_len) {
    char seq_str[10];
    char icmp_code_str[10];

    sprintf(seq_str, "%d", seq);
    sprintf(icmp_code_str, "%d", icmp->icmp_code);

    if (icmp->icmp_type == ICMP_DEST_UNREACH) {
        switch (icmp->icmp_code) {
        case ICMP_NET_UNREACH:
            return err_fmt(5, "From ", g_stats.local_ip, " icmp_seq=", seq_str, " Destination Host Unreachable\n");
        case ICMP_HOST_UNREACH:
            return err_fmt(5, "From ", g_stats.local_ip, " icmp_seq=", seq_str, " Destination Host Unreachable\n");
        case ICMP_FRAG_NEEDED:
            return err_fmt(5, "From ", g_stats.local_ip, " icmp_seq=", seq_str, " Fragmentation Needed\n");
        default:
            return err_fmt(7, "From ", g_stats.local_ip, " icmp_seq=", seq_str, " Destination unreachable, code: ", icmp_code_str, "\n");
        }
        g_stats.errs++;
    } else if (icmp->icmp_type == ICMP_TIME_EXCEEDED) {
        if (icmp->icmp_code == ICMP_EXC_TTL) {
            return err_fmt(5, "From ", g_stats.local_ip, " icmp_seq=", seq_str, " Time to live exceeded\n");
        } else {
            return err_fmt(5, "From ", g_stats.local_ip, " icmp_seq=", seq_str, " Time exceeded\n");
        }
        g_stats.errs++;
    } else if (recv_len <= 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return err(NULL);
        }
    }
    return err_fmt(2, "recvfrom: ", strerror(errno));
}
