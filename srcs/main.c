#include "ft_ping.h"
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>


int 	main(int argc, char **argv)
{
	t_config	conf;
	conf = parseInput(argc, argv);

	int ret = 0;
	if(conf.ipv6){
		if (resolveIPv6DNS(&conf) != 0)
			return (1);
		ret = ft_pingIpv6(&conf, &conf.target_addr6, conf.ip6_str);	
	}
	else {
		if (resolveDNS(&conf) != 0)
			return (1);
		ret = ft_pingIpv4(&conf, &conf.target_addr, conf.ip_str);	
	}

	if (ret < 0)
		return (1);
	return (ret);
}