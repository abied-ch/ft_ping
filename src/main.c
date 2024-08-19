#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>
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
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_PINGS 1024
#define PAYLOAD_SIZE 56
double rtts[MAX_PINGS];

unsigned short checksum(void *buffer, int len) {
    unsigned short *buf = buffer;
    unsigned int    sum = 0;

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

struct stats {
    unsigned int   transmitted;
    unsigned int   received;
    double         rtt_min;
    double         rtt_avg;
    double         rtt_max;
    double         rtt_mdev;
    char          *dest_host;
    int            sockfd;
    struct timeval start_time;
};

struct icmp_h {
    u_int8_t  type;
    u_int8_t  code;
    u_int16_t cksum;
    u_int16_t id;
    u_int16_t seq;
};

struct stats   stats = {0};
struct timeval start_time;

void init_icmp_header(struct icmp_h *icmp_header) {
    icmp_header->type = ICMP_ECHO;
    icmp_header->id = getpid();
    icmp_header->seq = 1;
}

#define SIGINT_MSG "\n--- %s ping statistics ---\n%u packets transmitted, %u received, %d%% packet loss time %dms\nrtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n"

void sigint(int sig) {
    if (sig != SIGINT) {
        return;
    }
    int            loss = 100 - (stats.received * 100) / stats.transmitted;
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_ms = (end_time.tv_sec - stats.start_time.tv_sec) * 1000.0 + (end_time.tv_usec - stats.start_time.tv_usec) / 1000.0;

    printf(SIGINT_MSG, stats.dest_host, stats.transmitted, stats.received, loss, (int)total_ms, stats.rtt_min, stats.rtt_avg, stats.rtt_max, stats.rtt_mdev);
    close(stats.sockfd);
    exit(EXIT_SUCCESS);
}

#define USLEEP_TIME 500000

int main(int ac, char **av) {
    unsigned int interval = 1000000;
    signal(SIGINT, sigint);
    if (ac < 2) {
        fprintf(stderr, "ft_ping: usage error: Destination address required\n");
        return EXIT_FAILURE;
    }
    char name[100] = {0};
    gethostname(name, sizeof(name));
    struct hostent *localhost_pub = gethostbyname(name);
    printf("%s\n", localhost_pub->h_addr_list[0]);
    printf("hostname: %s\n", name);
    if (!strncmp(av[1], "localhost", 9) || !strncmp(av[1], "127.0.0.1", 9) || !strncmp(name, av[1], strlen(name))) {
        interval /= 2;
    }
    stats.sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (stats.sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in send_addr;
    memset(&send_addr, 0, sizeof(send_addr));
    send_addr.sin_family = AF_INET;

    stats.dest_host = av[1];
    struct hostent *host = gethostbyname(av[1]);
    if (host == NULL) {
        fprintf(stderr, "ft_ping: %s: Name or service not known\n", av[1]);
        return EXIT_FAILURE;
    }
    send_addr.sin_addr = *(struct in_addr *)host->h_addr_list[0];

    char               buffer[1024];
    struct sockaddr_in recv_addr;
    socklen_t          addr_len = sizeof(recv_addr);
    gettimeofday(&start_time, NULL);
    stats.start_time = start_time;
    printf("PING %s %zu data bytes\n", av[1], sizeof(struct icmp_h) + PAYLOAD_SIZE);
    stats.rtt_min = INFINITY;
    stats.rtt_max = 0.0;
    stats.rtt_avg = 0.0;

    char           packet[sizeof(struct icmp_h) + PAYLOAD_SIZE];
    struct icmp_h *icmp_header = (struct icmp_h *)packet;
    init_icmp_header(icmp_header);
    memset(packet + sizeof(struct icmp_h), 0x42, PAYLOAD_SIZE);
    icmp_header->cksum = 0;
    icmp_header->cksum = checksum(packet, sizeof(packet));

    for (int count = 1; count; count++) {
        struct timeval trip_begin, trip_end;
        gettimeofday(&trip_begin, NULL);

        icmp_header->seq = count;
        icmp_header->cksum = 0;
        icmp_header->cksum = checksum(packet, sizeof(packet));

        if (sendto(stats.sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&send_addr, sizeof(send_addr)) <= 0) {
            perror("sendto");
            return EXIT_FAILURE;
        }

        ssize_t recv_len = recvfrom(stats.sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&recv_addr, &addr_len);
        if (recv_len <= 0) {
            perror("recvfrom");
            continue;
        }

        gettimeofday(&trip_end, NULL);
        double ttl_ms = (trip_end.tv_sec - trip_begin.tv_sec) * 1000.0 + (trip_end.tv_usec - trip_begin.tv_usec) / 1000.0;

        struct iphdr  *ip = (struct iphdr *)buffer;
        struct icmp_h *icmp = (struct icmp_h *)(buffer + (ip->ihl << 2));

        if (icmp->type == ICMP_ECHOREPLY && icmp->id == icmp_header->id) {
            printf("64 bytes from %s icmp_seq=%u ttl=%d time=%.3f ms\n", av[1], icmp->seq, ip->ttl, ttl_ms);

            if (count < MAX_PINGS) rtts[count - 1] = ttl_ms;

            stats.rtt_min = fmin(stats.rtt_min, ttl_ms);
            stats.rtt_max = fmax(stats.rtt_max, ttl_ms);

            stats.received++;
            stats.rtt_avg = ((stats.rtt_avg * (stats.received - 1)) + ttl_ms) / stats.received;

            double sum_deviation = 0.0;
            for (unsigned int i = 0; i < stats.received; i++) {
                sum_deviation += fabs(rtts[i] - stats.rtt_avg);
            }
            stats.rtt_mdev = sum_deviation / stats.received;
        }

        usleep(interval);
    }

    close(stats.sockfd);
    return 0;
}
