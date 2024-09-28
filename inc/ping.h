#ifndef FT_PING_H
#define FT_PING_H

#include <bits/types/struct_iovec.h>
#include <bits/types/struct_timeval.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#define DEFAULT_INTERVAL 1.0
#define PACKET_SIZE 56
#define PAYLOAD_SIZE 56
#define MAX_PINGS 16192

typedef struct {
    char buf[1024];
    char packet[PAYLOAD_SIZE + sizeof(struct icmp)];
    struct icmp *icmp_h;
    struct iphdr *ip_h;
    char ip_str[INET_ADDRSTRLEN];

    struct {
        int t;
        int c;
        bool v;
        bool h;
        bool q;
        bool D;
        double i;
        const char *dest;
    } cli;

    struct {
        struct sockaddr_in send;
        struct sockaddr_in recv;
    } addr;
} Args;

typedef struct {
    int errs;
    char dest[256];
    unsigned int sent;
    unsigned int rcvd;
    struct timespec start_time;
    double rtts[MAX_PINGS];
    char local_ip[INET6_ADDRSTRLEN];

    struct {
        double min;
        double avg;
        double max;
        double mdev;
    } rtt;

    struct {
        // sockfd needed to be closed in the signal handler
        // Pointer to the args struct needed to free it in the signal handler
        int sockfd;
        Args *args;
    } alloc;
} Stats;

typedef enum {
    OK = 0,
    ERR = -1,
} ResultType;

typedef union {
    const void *val;
    char *err;
} ResultValue;

typedef struct {
    ResultType type;
    ResultValue val;
    bool on_heap;
} Result;

// args.c
Result parse_cli_args(const int ac, char **av, Args *const args);
Result flood_check(const Args *const args);

// error.c
Result recv_error(const struct icmp *const icmp, const int seq, const int recv_len);
int cleanup(const int exit_code, Args *const args);
int help();

// socket.c
Result socket_init(int *const sockfd);
Result socket_set_options(const Args *const args);

// result.c
Result ok(void *val);
Result err(char *err);
Result err_fmt(const int n_strs, ...);
void err_unwrap(Result err, const bool quiet);

// address.c
Result get_send_addr(const Args *const args, struct sockaddr_in *const send_addr);
void init_local_ip();

// icmp.c
void icmp_init_header(Args *const args, int seq);
Result icmp_send_packet(const Args *const args, struct sockaddr_in *send_addr);
Result icmp_recv_packet(Args *const args, const int seq, const struct timespec *const trip_begin);

void icmp_ip_hdr_dump(const struct iphdr *const ip, const struct icmp *icmp);

// signal.c
void sigint(const int sig);

// stats.c
double stats_update(const struct timespec *const trip_begin);
void stats_display_rt(const Args *const args, const struct icmp *const icmp, const struct iphdr *const ip, const double ms);
void stats_display_final();

// ping.c
Result ping_init(const int ac, char **av);
Result ping(const Args *const args);

extern Stats g_stats;

#endif
