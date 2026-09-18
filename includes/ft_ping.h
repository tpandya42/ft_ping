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
	int count; //no. of echo req
	uint8_t	ttl; //time to live
	bool	verbose; // -v
	bool 	quiet; // -q
	bool	numeric; // -n
	int 	timeout_ms; // -W
	int 	packet_size; // -s
	bool 	ipv6;
} t_config;

typedef struct s_packet {
	struct icmphdr header;
	char 		payload[PAYLOAD_SIZE];
} 	t_packet;

typedef struct s_ping {
	char	*hostname;
	char 	ip_str[INET_ADDRSTRLEN];
	struct 	sockaddr_in 	target_addr;
	int 	verbose;
	int 	sockfd;
	uint16_t	pid;
	uint16_t	seq;
}		t_ping;

// parsing.c
t_config	parseInput(int argc, char **argv);
// dns.c
int	resolveDNS(t_ping *ping);
void buildPingPacket(t_packet *pkt, uint16_t pid, uint16_t seq);
#endif
