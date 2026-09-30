#include "ft_ping.h"
#include <stdlib.h>

t_config	parseInput(int argc,char **argv)
{
	t_config 	conf;
	memset(&conf, 0, sizeof(conf));
	conf.count = -1; // by default it is unlimited
	conf.ttl = 64; // default ttl
	conf.verbose = false;
	conf.quiet = false;
	conf.numeric = false;
	conf.timeout_ms = 1000; //1s default -W
	conf.packet_size = 56;
	conf.ipv6 = false;
	conf.pid = (uint16_t)getpid();

	int		opt;
	while ((opt = getopt(argc, argv, "v?ctnqTWs")) != -1)
	{
		if (opt == 'v')
			conf.verbose = true;
		else if (opt == '?')
		{
			// print help text;
			exit(0);
		}
		else if (opt == '6') {
			conf.ipv6 = true;	
		}
		else if (opt == 'n')
			conf.numeric = true;
		else if (opt == 'q')
			conf.quiet = true;
		else if (opt == 'c') {
			// parse count
			conf.count = atoi(argv[3]);
		}
		
		else if (opt == 'T'){
			// do something	
		}
		else if (opt == 't'){
			// parse time to live
			conf.ttl = atoi(argv[3]);
		}
		else if (opt == 'W'){
			// parse timout in ms
			conf.timeout_ms = atoi(argv[3]);
		}
		else if (opt == 's'){
			// parse packet size
			conf.packet_size = atoi(argv[3]);
		}
		else {
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
	conf.hostname = argv[optind];
	if (!conf.hostname)
	{
		exit(EXIT_FAILURE);
	}
	return conf;
}
