#ifndef FT_PING_H
# define FT_PING_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <stdint.h>
#include <signal.h>
#include <stdbool.h>

typedef struct s_config {
	char 	ip_str[INET_ADDRSTRLEN];
	char 	ip6_str[INET6_ADDRSTRLEN];
	char 	*hostname;
	struct 	sockaddr_in target_addr;
	struct 	sockaddr_in6 target_addr6;
	int count;
	uint8_t	ttl;
	bool	verbose;
	bool 	quiet;
	bool	numeric;
	int 	timeout_ms;
	int 	packet_size;
	bool 	ipv6;
	uint16_t  pid;
	uint8_t  pattern;
	bool 	dontroute;
	int 	deadline;
	int 	tos;
	int 	ip_ts;
	int 	ip_ts_type;
} t_config;

typedef struct s_logs {
	int	sent;
	int	rec;
	struct timespec start_time;
	struct timespec end_time;
	double rtt_max;
	double rtt_min;
	double rtt_sum;
	double rtt_mdev; 
} t_logs;

extern volatile sig_atomic_t g_sig;

// parsing.c
t_config	parseInput(int argc, char **argv);
// dns.c
int	resolveDNS(t_config *conf);
int	resolveIPv6DNS(t_config *conf);
//icmp.c
size_t	buildEchoPacket(uint8_t *packet, int packetSize, uint16_t seq, uint16_t pid, struct timespec *st_time, uint8_t pattern);
size_t	buildEchoV6Packet(uint8_t *packet, int packetSize, uint16_t seq, uint16_t pid, struct timespec *st_time, const struct in6_addr *src, const struct in6_addr *dst, uint8_t pattern);
//ping loop
int  ipv4_icmp_socket(t_config *conf);
int  ipv6_icmp_socket(t_config *conf);
int  ft_pingIpv4(t_config *conf, struct sockaddr_in *target_addr, char *ip_str);
int  ft_pingIpv6(t_config *conf, struct sockaddr_in6 *target_addr, char *ip_str);

#endif
