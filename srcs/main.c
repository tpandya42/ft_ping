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

	//resolve dns here
	if (resolveDNS(&conf) != 0)
		return (1);

	printf("Hostname's ip: %s\n", conf.ip_str);

	if(conf.ipv6){
		// handle ipv6 case here.
	}
	else {
		// handle ipv4 here
		// Things to do -> resolve dns, send ICMP packet, recieve, print
		ft_pingIpv4(&conf, &conf.target_addr, conf.ip_str);	
	}

	return (0);
}