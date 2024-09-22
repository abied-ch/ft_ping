#ifndef FT_PING_H
#define FT_PING_H

#include <bits/types/struct_timeval.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <sys/types.h>

#define PING_INTERVAL 1000000
#define PACKET_SIZE 64
#define PAYLOAD_SIZE 64
#define MAX_PINGS 1024

typedef struct {
    unsigned int transmitted;
    unsigned int received;
    double rtt_min;
    double rtt_avg;
    double rtt_max;
    double rtt_mdev;
    double rtts[MAX_PINGS];
    char dest_host[256];
    int sockfd;
    struct timeval start_time;
    int errors;
    char local_ip[INET6_ADDRSTRLEN];
    const char* dest;
} Stats;

typedef struct {
    bool v;
    bool h;
    int ttl;
} Args;

typedef enum {
    ICMP_SEND_OK,
    ICMP_SEND_FAILURE,
    ICMP_SEND_MAX_RETRIES_REACHED,
} ICMPSendRes;

int parse_args(const int ac, const char** const av, Args* const args);

extern Stats stats;

#endif
