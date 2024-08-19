#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define ICMP_ECHO 0

struct icmp {
    u_int8_t  type;
    u_int8_t  code;
    u_int16_t checksum;
    u_int16_t id;
    u_int16_t seq;
};

struct icmp icmp_header;

void init_icmp_header() {
    icmp_header.type = ICMP_ECHO;
    icmp_header.code = 0;
    icmp_header.id = getpid();
    icmp_header.seq = 0;
    icmp_header.checksum = 0;
}

int main(int ac, char **av) {
    if (ac < 2) {
        fprintf(stderr, "ft_ping: usage error: Destination address required\n");
        return EXIT_FAILURE;
    }
    init_icmp_header();
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }
    char buffer[1024];
    sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)av[1], sizeof(av[1]));
    close(sockfd);
    return 0;
}
