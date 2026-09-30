#ifndef FT_PING_H
# define FT_PING_H

#define PAYLOAD_SIZE 56
#define PACKET_SIZE (sizeof(struct icmphdr) + PAYLOAD_SIZE)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <stdint.h>
#include <signal.h>
#include <stdbool.h>

typedef struct s_config {
	char 	ip_str[INET_ADDRSTRLEN]
	char 	*hostname;
	struct 	sockaddr_in target_addr;
	int count; //no. of echo req
	uint8_t	ttl; //time to live
	bool	verbose; // -v
	bool 	quiet; // -q
	bool	numeric; // -n
	int 	timeout_ms; // -W
	int 	packet_size; // -s
	bool 	ipv6;
	uint16_t  pid;
} t_config;

typedef struct s_logs {
	struct timespec start_time;
	struct timespec end_time;
	double rtt_max;
	double rtt_min;
	double rtt_sum;
	double rtt_square_sum;
} t_logs;

typedef struct s_packet {
	struct icmphdr header;
	char 		payload[PAYLOAD_SIZE];
} 	t_packet;


// parsing.c
t_config	parseInput(int argc, char **argv);
// dns.c
int	resolveDNS(t_conf *conf);
//ping loop
int  ipv4_icmp_socket(t_config *conf);
int  ft_pingIpv4(t_config *conf, struct sockaddr_in *target_addr, char *ip_str);


#endif
