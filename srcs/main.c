#include "ft_ping.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int		parseInput(int argc, char **argv, t_ping *ping)
{
	int		opt;
	while ((opt = getopt(argc, argv, "v?")) != -1)
	{
		if (opt == 'v')
			ping->verbose = 1;
		else if (opt == '?')
		{
			// print help text;
			exit(0);
		} else {
			fprintf(stderr, "ft_ping: unrecognized option '%s'\n", argv[optind - 1]);
			fprintf(stderr, "Try 'ft_ping -?' for more information.\n");
			exit(64);
		}
	}

	if (optind >= argc)
	{
		fprintf(stderr,"ft_ping: missing host operand\n");
		fprintf(stderr, "Try 'ft_ping -?' for more information.\n");
		exit(64);
	}
	ping->hostname = argv[optind];
	return (0);
}

int 	main(int argc, char **argv)
{
	t_ping 	ping;

	memset(&ping, 0, sizeof(ping));
	ping.pid = (uint16_t)getpid();
	parseInput(argc, argv, &ping);
	//resolve dns here
	if (resolveDNS(&ping) != 0)
		return (1);
	printf("Hostname's ip: %s\n", ping.ip_str);
	return (0);
}
