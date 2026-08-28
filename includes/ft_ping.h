#ifndef FT_PING_H
# define FT_PING_H

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

typedef struct s_ping {
	char	*hostname;
	char 	ip_str[INET_ADDRSTRLEN];
	struct 	sockaddr_in 	target_addr;
	int 	verbose;
	int 	sockfd;
	uint16_t	pid;
	uint16_t	seq;
}		t_ping;


#endif
