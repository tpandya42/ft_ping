#include "ft_ping.h"
#include <bits/types/struct_timeval.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>


ssize_t		sendPacket(t_ping *ping, t_packet *pkt)
{
	ssize_t bytes_sent = sendto(ping->sockfd, pkt, sizeof(*pkt), 0, (struct sockaddr *)&ping->target_addr, sizeof(ping->target_addr));
	if (bytes_sent < 0)
	{
		perror("ft_ping: sendto failed");
		return (-1);
	}
	return bytes_sent;
}


int 	main(int argc, char **argv)
{
	t_config	*conf;
	conf = parseInput(argc, argv);

	//resolve dns here
	if (resolveDNS(conf) != 0)
		return (1);

	printf("Hostname's ip: %s\n", conf.hostname);

	if(conf.ipv6){
		// handle ipv6 case here.
	}
	else {
		// handle ipv4 here
		// Things to do -> resolve dns, send ICMP packet, recieve, print
		if (resolveDNS(conf) != 0) return 2;
		ft_pingIpv4(conf, conf->target_addr, conf->ip_str);	
	}

	return (0);
}
