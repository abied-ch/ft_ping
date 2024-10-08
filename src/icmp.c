#include "ping.h"
#include <arpa/inet.h>
#include <bits/types/struct_iovec.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// This is ugly but it works perfectly and I do not feel bad.
void
icmp_ip_hdr_dump(const struct iphdr *const ip, const struct icmp *icmp) {
    char src_ip[INET_ADDRSTRLEN];
    char dst_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ip->saddr), src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip->daddr), dst_ip, INET_ADDRSTRLEN);

    u_int16_t *ip_hdr_u16 = (u_int16_t *)ip;
    int hdr_len = ip->ihl << 2;
    int n_u16_words = hdr_len / 2;

    fprintf(stderr, "IP Hdr Dump:\n");

    for (int idx = 0; idx < n_u16_words; ++idx) {
        fprintf(stderr, " %04x", ntohs(ip_hdr_u16[idx]));
    }

    fprintf(stderr, "\nVr HL TOS  Len   ID Flg  off TTL Pro  cks      Src      Dst     Data\n");
    fprintf(stderr, " %d", ip->version);
    fprintf(stderr, " %2d", n_u16_words / 2);
    fprintf(stderr, " %03d", ip->tos);
    fprintf(stderr, " %04d", hdr_len);
    fprintf(stderr, " %04x", ntohs(ip->id));
    // Move flags (top 3 bits) to the least significant position and isolate them with `& 00000111`
    fprintf(stderr, "%4d", (ntohs(ip->frag_off) >> 13) & 0x7);
    // `0x1FFF` -> 0001 1111 1111 1111 -> isolate lower 13 bits to get the fragment offset
    fprintf(stderr, " %04d", ntohs(ip->frag_off) & 0x1FFF);
    fprintf(stderr, " %03d", ip->ttl);
    fprintf(stderr, " %03d", ip->protocol);
    fprintf(stderr, " %04x", ntohs(ip->check));
    fprintf(stderr, " %s", src_ip);
    fprintf(stderr, "  %s\n", dst_ip);
    fprintf(stderr, "ICMP: type %d, ", icmp->icmp_type);
    fprintf(stderr, "code %d, size %d, ", icmp->icmp_code, PACKET_SIZE);
    fprintf(stderr, "id 0x%04x, seq 0x%04x\n", icmp->icmp_id, icmp->icmp_seq);
}

// Computes IP checksum (16 bit one's complement sum), ensuring packet integrity before accepting.
// .
// This ensures that the checksum result will not exceed the size of a 16 bit integer by
// wrapping around carry instead of making the number outgrow its bounds.
// .
// The sender calculates the checksum with '0' as a placeholder value in the checksum field. The receiver then
// calculates it including the checksum sent by the sender. Its one's complement sum is expected to be '0' -
// packets where this calculation fails are discarded.
// .
// https://web.archive.org/web/20020916085726/http://www.netfor2.com/checksum.html
static unsigned short
checksum(const void *const buffer, int len) {
    const unsigned short *buf = buffer;
    unsigned int sum = 0;

    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++;
    }
    if (len == 1) {
        sum += *(unsigned char *)buf;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return ~sum;
}

// Initializes a IP/ICMP header. Can be called on each iteration to refresh the `icmp` struct in `args`.
void
icmp_init_header(Args *const args, const int seq) {
    args->ip_h = (struct iphdr *)args->packet;
    args->icmp_h = (struct icmp *)(args->packet + sizeof(struct iphdr));

    args->ip_h->ihl = 5;
    args->ip_h->version = 4;
    args->ip_h->tos = args->cli.T;
    args->ip_h->tot_len = htons(PACKET_SIZE);
    args->ip_h->id = htons(getpid());
    args->ip_h->frag_off = 0;
    args->ip_h->ttl = args->cli.t == 0 ? 64 : args->cli.t;

    args->ip_h->protocol = IPPROTO_ICMP;
    args->ip_h->saddr = inet_addr(g_stats.local_ip);
    args->ip_h->daddr = inet_addr(args->ip_str);

    args->ip_h->check = 0;
    args->ip_h->check = checksum(args->ip_h, sizeof(struct iphdr));

    args->icmp_h->icmp_type = ICMP_ECHO;
    args->icmp_h->icmp_id = getpid();
    args->icmp_h->icmp_seq = seq;

    // `icmp_h` and `packet` point to the same memory address, they are just _cast to different types_.
    // Removing this line will result in the checksum not matching and all packets (except for the first
    // one) being lost!
    args->icmp_h->icmp_cksum = 0;
    args->icmp_h->icmp_cksum = checksum(args->packet, sizeof(args->packet));
}

// Sends the IMCP packet to the destination specified by `send_addr`.
// .
// Returns:
// - `Result.type == OK` on success
// - `Result.type == ERR` on failure to send the packet
Result
icmp_send_packet(const Args *const args, struct sockaddr_in *send_addr) {
    if (sendto(args->sockfd, args->packet, sizeof(args->packet), 0, (struct sockaddr *)send_addr, sizeof(*send_addr)) <= 0) {
        return err_fmt(3, "sendto: ", strerror(errno), "\n");
    }
    g_stats.sent++;
    return ok(NULL);
}

// Checks whether the packet matches with the one we are expecting.
// .
// Returns:
// - `false` if the packet matches
// - `true` if one of `is_echo_reply`, `id_matches` or `seq_matches` is false
static bool
icmp_packet_is_unexpected(struct icmp *icmp, struct icmp *icmp_header, const int seq) {
    const bool is_echo_reply = icmp->icmp_type == ICMP_ECHOREPLY;
    const bool id_matches = icmp->icmp_id == icmp_header->icmp_id;
    const bool seq_matches = icmp->icmp_seq == seq;

    return !is_echo_reply || !id_matches || !seq_matches;
}

// Tries to receive packets on `g_stats.sockfd` until finding the one corresponding to the
// sent echo request.
// Returns:
// - `Result.type == OK` on success
// - `Result.type == ERR` on error. Errors include: recv errors (see `recv_error` in `src/error.c`),
// internal failures (`gettimeofday` in `stats_update`)
Result
icmp_recv_packet(Args *const args, const int seq, const struct timespec *const trip_begin) {
    while (true) {
        socklen_t recv_addr_len = sizeof(args->addr.recv);

        ssize_t recv_len = recvfrom(args->sockfd, args->buf, sizeof(args->buf), 0, (struct sockaddr *)&args->addr.recv, &recv_addr_len);

        struct iphdr *ip = (struct iphdr *)args->buf;
        size_t iphdr_len = ip->ihl << 2;
        struct icmp *icmp = (struct icmp *)(args->buf + iphdr_len);
        if (recv_len <= 0) {
            return recv_error(icmp, seq, recv_len);
        }

        if (icmp_packet_is_unexpected(icmp, args->icmp_h, seq)) {
            continue;
        }

        double rt_ms = stats_update(trip_begin);
        if (rt_ms == -1) {
            return err(NULL);
        }

        stats_display_rt(args, icmp, ip, rt_ms);
    }

    return ok(NULL);
}
