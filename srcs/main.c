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

int		receivePingReply(t_ping *ping)
{
	char 	buf[1024];
	struct	sockaddr_in senderAddr;
	socklen_t	addr_len = sizeof(senderAddr);

	ssize_t bytes_rec = recvfrom(ping->sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&senderAddr, &addr_len);
	if (bytes_rec < 0)
	{
		printf("Request timeout for icmp_seq %d\n", ping->seq);
		return (-1);
	}

	struct timeval recvTime;
	gettimeofday(&recvTime, NULL);

	uint8_t ihl = (buf[0] & 0x0F) * 4;
	uint8_t ttl = buf[8];

	struct icmphdr *replyICMP = (struct icmphdr *)(buf + ihl);
	if (replyICMP->type != ICMP_ECHOREPLY){
		return (-1);
	}
	if (ntohs(replyICMP->un.echo.id) != ping->pid)
		return (-1);

	struct timeval sentTime;
	memcpy(&sentTime, buf + ihl + sizeof(struct icmphdr), sizeof(sentTime));

	// rrt
	double rtt = (recvTime.tv_sec - sentTime.tv_sec) * 1000.0 + (recvTime.tv_usec - sentTime.tv_usec) / 1000.0;
	
	// printing standard output
	printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
			bytes_rec - ihl,
			ping->ip_str,
			ntohs(replyICMP->un.echo.sequence),
			ttl,
			rtt
			);

	return (0);
}

int 	main(int argc, char **argv)
{
	t_ping 	ping;
	s_config	*conf;
	memset(&ping, 0, sizeof(ping));
	ping.pid = (uint16_t)getpid();
	conf = parseInput(argv);

	//resolve dns here
	if (resolveDNS(&ping) != 0)
		return (1);
	printf("Hostname's ip: %s\n", ping.ip_str);

	ping.sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP); 
	if (ping.sockfd == -1)
		return (-1);
	// generate ICMP header ig
	t_packet pkt;
	buildPingPacket(&pkt, ping.pid, ping.seq);
	sendPacket(&ping, &pkt);
	receivePingReply(&ping);
	return (0);
}
