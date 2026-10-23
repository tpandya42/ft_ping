#include "ft_ping.h"
#include <netinet/ip.h>

static void build_timestamp_option(uint8_t *opt, int type)
{
	int i;
	opt[0] = IPOPT_TIMESTAMP;
	opt[1] = 40;
	opt[2] = 5;
	opt[3] = type;
	for (i = 4; i < 40; i++)
		opt[i] = 0;
}

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

	if (conf->tos >= 0)
	{
		int tos = conf->tos;
		if (setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0)
			perror("ft_ping: setsockopt(IP_TOS)");
	}

	if (conf->ip_ts)
	{
		uint8_t opt[40];
		build_timestamp_option(opt, conf->ip_ts_type);
		if (setsockopt(fd, IPPROTO_IP, IP_OPTIONS, opt, 40) < 0)
			perror("ft_ping: setsockopt(IP_OPTIONS)");
	}

	if (conf->dontroute)
	{
		int on = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on)) < 0)
			perror("ft_ping: setsockopt(SO_DONTROUTE)");
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

	if (conf->dontroute)
	{
		int on = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on)) < 0)
			perror("ft_ping: setsockopt(SO_DONTROUTE)");
	}

	if (conf->tos >= 0)
	{
		fprintf(stderr, "ft_ping: -T/--tos is not available for IPv6\n");
	}

	if (conf->ip_ts)
	{
		fprintf(stderr, "ft_ping: --ip-timestamp is not available for IPv6\n");
	}

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
