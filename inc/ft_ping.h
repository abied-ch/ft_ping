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

#define PING_INTERVAL 1000000
#define PACKET_SIZE 64
#define PAYLOAD_SIZE 64
#define MAX_PINGS 1024

typedef struct {
    int errs;
    int sockfd;
    char dest[256];
    double rtt_min;
    double rtt_avg;
    double rtt_max;
    double rtt_mdev;
    unsigned int sent;
    unsigned int rcvd;
    struct timeval *start_time;
    double rtts[MAX_PINGS];
    char local_ip[INET6_ADDRSTRLEN];
} Stats;

typedef struct {

    struct {
        bool v;
        bool h;
        int ttl;
        const char *dest;
    } cli;

    struct sockaddr_in send_addr;
    struct sockaddr_in recv_addr;
    socklen_t recv_addr_len;
    char buf[1024];
    char packet[PAYLOAD_SIZE + sizeof(struct icmp)];
    struct icmp *icmp_h;
    char ip_str[INET_ADDRSTRLEN];
} Args;

typedef struct {

    struct {
        char *content;
        size_t len;
    } buf;

    struct sockaddr_in addr;
    socklen_t len;
} Recv;

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
int help();

// error.c
void recv_error(const struct icmp *const icmp, const int seq, const int recv_len);

// socket.c
Result init_socket(int *const sockfd);
Result set_socket_options(const Args *const args);

// result.c
Result ok(void *val);
Result err(char *err);
Result err_fmt(const int n_strs, ...);
void err_unwrap(Result err);

// address.c
Result get_send_addr(const Args *const args, struct sockaddr_in *const send_addr);
void init_local_ip();

// icmp.c
void init_icmp_header(const Args *const args, int seq);
Result send_packet(const Args *const args, struct sockaddr_in *send_addr);

    // signal.c
    void sigint(const int sig);

extern Stats g_stats;

#endif
