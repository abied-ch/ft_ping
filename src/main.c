#include <arpa/inet.h>
#include <limits.h>
#include <math.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_PINGS 1024
double rtts[MAX_PINGS];

unsigned short checksum(void* buffer, int len) {
    unsigned short* buf = buffer;  // convert the buffer into 16-bit chunks
    unsigned int    sum = 0;

    // iterate over buffer 2 bytes at a time
    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++;
    }
    if (len == 1) {
        sum += *(unsigned char*)buf;
    }

    // complement sum -> carry is added back to the lower 16 bits of the result
    // (sum & 0xFFFF) represents the lower 16 bits of `sum`
    // (sum >> 16) shifts the upper 16 bits to the lower 16 bits
    sum = (sum >> 16) + (sum & 0xFFFF);

    // add any overflow from previous addition to the sum
    sum += (sum >> 16);

    // invert all bits from the sum
    return ~sum;
}

struct stats {
    unsigned int transmitted;
    unsigned int received;
    unsigned int total_ms;
    double       rtt_min;
    double       rtt_avg;
    double       rtt_max;
    double       rtt_mdev;
    char*        dest_host;
    int          sockfd;
};

struct icmp_h {
    u_int8_t  type;
    u_int8_t  code;
    u_int16_t cksum;
    u_int16_t id;
    u_int16_t seq;
};

struct icmp_h icmp_header = {0};
struct stats  stats = {0};

void init_icmp_header() {
    icmp_header.type = ICMP_ECHO;
    icmp_header.id = getpid();
    icmp_header.seq = 1;
    icmp_header.cksum = checksum(&icmp_header, sizeof(icmp_header));
}

void sigint(int sig) {
    (void)sig;

    printf("\n--- %s ping statistics ---\n", stats.dest_host);
    int loss = 100 - (stats.received * 100) / stats.transmitted;
    printf("%u packets transmitted, %u received, %d%% packet loss, time %dms\n", stats.transmitted, stats.received, loss, stats.total_ms);
    printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n", stats.rtt_min, stats.rtt_avg, stats.rtt_max, stats.rtt_mdev);
    close(stats.sockfd);
    exit(EXIT_SUCCESS);
}

int main(int ac, char** av) {
    signal(SIGINT, sigint);
    if (ac < 2) {
        fprintf(stderr, "ft_ping: usage error: Destination address required\n");
        return EXIT_FAILURE;
    }

    init_icmp_header();

    stats.sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (stats.sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in send_addr;
    memset(&send_addr, 0, sizeof(send_addr));
    send_addr.sin_family = AF_INET;

    stats.dest_host = av[1];
    struct hostent* host = gethostbyname(av[1]);
    if (host == NULL) {
        fprintf(stderr, "ft_ping: %s: Name or service not known\n", av[1]);
        return EXIT_FAILURE;
    }
    send_addr.sin_addr = *(struct in_addr*)host->h_addr_list[0];

    char               buffer[1024];
    struct sockaddr_in recv_addr;
    socklen_t          addr_len = sizeof(recv_addr);
    clock_t            start_time = clock();
    printf("PING %s(%s) 56 data bytes\n", av[1], av[1]);
    int count = 0;
    stats.rtt_min = INFINITY;
    stats.rtt_max = 0.0;
    stats.rtt_avg = 0.0;

    while (1) {
        clock_t begin = clock();

        if (sendto(stats.sockfd, &icmp_header, sizeof(icmp_header), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) <= 0) {
            perror("sendto");
            return EXIT_FAILURE;
        }

        stats.transmitted++;
        ssize_t bytes_received = recvfrom(stats.sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&recv_addr, &addr_len);
        if (bytes_received <= 0) {
            perror("recvfrom");
            continue;
        }

        clock_t end = clock();
        double  ttl_ms = ((double)(end - begin) / CLOCKS_PER_SEC) * 100000.0;

        struct iphdr*  ip = (struct iphdr*)buffer;
        struct icmp_h* icmp = (struct icmp_h*)(buffer + (ip->ihl << 2));

        if (icmp->type == ICMP_ECHOREPLY && icmp->id == icmp_header.id) {
            printf("64 bytes from %s icmp_seq=%u ttl=%d time=%.3f ms\n", av[1], stats.transmitted, ip->ttl, ttl_ms);

            if (count < MAX_PINGS) {
                rtts[count++] = ttl_ms;
            }

            if (ttl_ms < stats.rtt_min) stats.rtt_min = ttl_ms;
            if (ttl_ms > stats.rtt_max) stats.rtt_max = ttl_ms;

            stats.received++;
            stats.rtt_avg = ((stats.rtt_avg * (stats.received - 1)) + ttl_ms) / stats.received;

            double sum_deviation = 0.0;
            for (int i = 0; i < count; i++) {
                sum_deviation += fabs(rtts[i] - stats.rtt_avg);
            }
            stats.rtt_mdev = sum_deviation / count;
        }

        stats.total_ms = (int)(((double)(clock() - start_time) / CLOCKS_PER_SEC) * 1000000.0);
        sleep(1);
    }
    close(stats.sockfd);
    return 0;
}