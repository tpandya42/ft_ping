#include "ft_ping.h"


int  ipv4_icmp_socket(t_config *conf)
{
	int fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (fd < 0)
	{
		// print ping's error log
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
