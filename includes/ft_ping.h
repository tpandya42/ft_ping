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
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <stdint.h>
#include <signal.h>
#include <stdbool.h>

typedef struct s_config {
	char 	ip_str[INET_ADDRSTRLEN];
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
	int	sent;
	int	rec;
	struct timespec start_time; // NOTE: time in seconds and nanoseconds.. Data type -> time_t (signed value usually long or long long).
	struct timespec end_time;
	double rtt_max;
	double rtt_min;
	double rtt_sum;
	double rtt_mdev; 
	/*mdev: Population standard deviation(mdev), essentially an average of how far each ping RTT is from the mean RTT.
	 * 	The higher mdev is, the more variable the RTT is (over time). With a high RTT variability, you will have speed issues with bulk transfers
	 * 	(They will take longer than is strictly speaking necessary, as the variability will eventually cause the sender to wait for ACKS) and 
	 * 	you will have middling to poor VoIP quality.*/
} t_logs;


// parsing.c
t_config	parseInput(int argc, char **argv);
// dns.c
int	resolveDNS(t_config *conf);
//icmp.c
size_t	buildEchoPacket(t_config *conf, uint8_t *packet, int packetSize, uint16_t seq, uint16_t pid, struct timespec *st_time);
//ping loop
int  ipv4_icmp_socket(t_config *conf);
int  ft_pingIpv4(t_config *conf, struct sockaddr_in *target_addr, char *ip_str);


#endif
