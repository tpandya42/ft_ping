#include "ft_ping.h"

int  ipv4_icmp_socket(t_config *conf)
{
	int fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (fd < 0)
	{
		perror("ft_ping: socket");
		return -1;
	}

	if (conf->ttl > 0)
	{
		int ttl = conf->ttl;
		if (setsockopt(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
			perror("ft_ping: setsockopt(IP_TTL)");
	}

	struct timeval tv;
	tv.tv_sec = conf->timeout_ms / 1000;
	tv.tv_usec = (conf->timeout_ms % 1000) * 1000;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		perror("ft_ping: setsockopt(SO_RCVTIMEO)");
	return fd;
}

int  ipv6_icmp_socket(t_config *conf)
{
	int fd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (fd < 0)
	{
		perror("ft_ping: socket");
		return -1;
	}

	if (conf->ttl > 0)
	{
		int ttl = conf->ttl;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl)) < 0)
			perror("ft_ping: setsockopt(IPV6_UNICAST_HOPS)");
	}

	// Enable receiving hop limit via cmsg so we can print ttl in replies
	int on = 1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on, sizeof(on)) < 0)
		perror("ft_ping: setsockopt(IPV6_RECVHOPLIMIT)");

	struct timeval tv;
	tv.tv_sec = conf->timeout_ms / 1000;
	tv.tv_usec = (conf->timeout_ms % 1000) * 1000;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		perror("ft_ping: setsockopt(SO_RCVTIMEO)");
	return fd;
}