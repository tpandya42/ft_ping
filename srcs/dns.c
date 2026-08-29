#include "ft_ping.h"

int	resolveDNS(t_ping *ping)
{
	struct addrinfo hints;
	struct addrinfo	*result;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_RAW;
	if (getaddrinfo(ping->hostname, NULL, &hints, &result) != 0){
		fprintf(stderr, "Unknown hostname '%s'\n", ping->hostname);
		return (-1);
	}
	struct sockaddr_in *ipv4 = (struct sockaddr_in *)result->ai_addr;
	ping->target_addr = *ipv4;
	inet_ntop(AF_INET, &(ipv4->sin_addr), ping->ip_str, sizeof(ping->ip_str));
	freeaddrinfo(result);
	return (0);
}
