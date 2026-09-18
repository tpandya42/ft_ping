#include "ft_ping.h"

t_config	parseInput(char **argv)
{
	t_config 	conf;
	memset(&conf, 0, sizeof(conf));
	conf.count = -1; // by default it is unlimited
	conf.ttl = 64; // default ttl
	conf.verbose = false;
	conf.quiet = false;
	conf.numeric = false;
	conf.timeout_ms = 1000; //1s default
	conf.packet_size = 56;
	conf.ipv6 = false;

	int		opt;
	while ((opt = getopt(argc, argv, "v?ctnqTWs")) != -1)
	{
		if (opt == 'v')
			conf->verbose = true;
		else if (opt == '?')
		{
			// print help text;
			exit(0);
		}
		else if (opt == 'n')
			conf->numeric = true;
		else if (opt == 'q')
			conf->quiet = true;
		else if (opt == 'c') {
			// parse count
		}
		else if (opt == 'T'){
			// parse time to live
		}
		else if (opt == 't'){
			// parse timeout
		}
		else if (opt == 'W'){
			// parse timout in ms
		}
		else if (opt == 's'){
			// parse packet size
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
	ping->hostname = argv[optind];
	return (0);
}
