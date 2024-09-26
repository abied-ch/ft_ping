#include "ft_ping.h"
#include <netinet/ip_icmp.h>
#include <unistd.h>

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

void
init_icmp_header(const Args *const args, const int seq) {
    args->icmp_h->icmp_type = ICMP_ECHO;
    args->icmp_h->icmp_id = getpid();
    args->icmp_h->icmp_seq = seq;

    // `icmp_h` and `packet` point to the same memory address, they are just _cast to different types_.
    // Removing this line will result in the checksum not matching and all packets (except for the first
    // one) being lost!
    args->icmp_h->icmp_cksum = 0;
    args->icmp_h->icmp_cksum = checksum(args->packet, sizeof(args->packet));
}