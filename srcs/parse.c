#include "ft_ping.h"
#include <stdlib.h>
#include <getopt.h>

static void print_usage(void)
{
	printf("Usage: ft_ping [OPTION...] HOST ...\n");
	printf("Send ICMP ECHO_REQUEST packets to network hosts.\n\n");
	printf("  -c, --count=NUMBER     stop after sending NUMBER packets\n");
	printf("  -n, --numeric          do not resolve host addresses\n");
	printf("  -q, --quiet            quiet output\n");
	printf("  -r, --ignore-routing   send directly to a host on an attached network\n");
	printf("  -s, --size=NUMBER      send NUMBER data octets\n");
	printf("  -t, --ttl=N            set time to live (IP TTL) to N\n");
	printf("  -T, --tos=NUM          set type of service (TOS) to NUM\n");
	printf("      --ip-timestamp=FLAG  IP timestamp of type FLAG, which is one of\n");
	printf("                           \"tsonly\" or \"tsaddr\"\n");
	printf("  -v, --verbose          verbose output\n");
	printf("  -w, --timeout=NUMBER   stop after NUMBER seconds\n");
	printf("  -W, --linger=NUMBER    number of seconds to wait for response\n");
	printf("  -p, --pattern=PATTERN  fill ICMP packet with given pattern (hex)\n");
	printf("  -6                     use IPv6\n");
	printf("  -?, --help             give this help list\n");
	printf("      --usage            give a short usage message\n");
}

static void print_short_usage(void)
{
	printf("Usage: ft_ping [-c COUNT] [-n] [-q] [-r] [-s SIZE]\n");
	printf("                [-t TTL] [-T TOS] [--ip-timestamp=FLAG] [-v] [-w TIMEOUT] [-W LINGER]\n");
	printf("                [-p PATTERN] [-6] [-?] HOST ...\n");
}

t_config	parseInput(int argc,char **argv)
{
	t_config 	conf;
	memset(&conf, 0, sizeof(conf));
	conf.count = -1;
	conf.ttl = 64;
	conf.verbose = false;
	conf.quiet = false;
	conf.numeric = false;
	conf.timeout_ms = 1000;
	conf.packet_size = 56;
	conf.ipv6 = false;
	conf.pid = (uint16_t)getpid();
	conf.pattern = 0x42;
	conf.dontroute = false;
	conf.deadline = 0;
	conf.tos = -1;
	conf.ip_ts = 0;
	conf.ip_ts_type = 0;

	static struct option long_options[] = {
		{"count", required_argument, 0, 'c'},
		{"numeric", no_argument, 0, 'n'},
		{"quiet", no_argument, 0, 'q'},
		{"ignore-routing", no_argument, 0, 'r'},
		{"size", required_argument, 0, 's'},
		{"ttl", required_argument, 0, 't'},
		{"tos", required_argument, 0, 'T'},
		{"ip-timestamp", required_argument, 0, 1000},
		{"verbose", no_argument, 0, 'v'},
		{"timeout", required_argument, 0, 'w'},
		{"linger", required_argument, 0, 'W'},
		{"pattern", required_argument, 0, 'p'},
		{"help", no_argument, 0, 1002},
		{"usage", no_argument, 0, 1001},
		{0, 0, 0, 0}
	};

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-?") == 0)
		{
			print_usage();
			exit(0);
		}
	}

	int opt;
	opterr = 0;
	while ((opt = getopt_long(argc, argv, ":v6c:t:W:w:s:qnp:rT:", long_options, NULL)) != -1)
	{
		if (opt == 'v')
			conf.verbose = true;
		else if (opt == ':')
		{
			fprintf(stderr, "ft_ping: option requires an argument -- '%c'\n", optopt);
			fprintf(stderr, "Try 'ft_ping -?' for more information.\n");
			exit(64);
		}
		else if (opt == '?')
		{
			fprintf(stderr, "ft_ping: unrecognized option '%s'\n", argv[optind - 1]);
			fprintf(stderr, "Try 'ft_ping -?' for more information.\n");
			exit(64);
		}
		else if (opt == 1002)
		{
			print_usage();
			exit(0);
		}
		else if (opt == 1001)
		{
			print_short_usage();
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
			conf.count = atoi(optarg);
		}
		else if (opt == 'T') {
			conf.tos = atoi(optarg);
		}
		else if (opt == 't') {
			conf.ttl = atoi(optarg);
		}
		else if (opt == 'W') {
			conf.timeout_ms = atoi(optarg) * 1000;
		}
		else if (opt == 'w') {
			conf.deadline = atoi(optarg);
		}
		else if (opt == 's') {
			conf.packet_size = atoi(optarg);
		}
		else if (opt == 'p') {
			conf.pattern = (uint8_t)strtol(optarg, NULL, 16);
		}
		else if (opt == 'r') {
			conf.dontroute = true;
		}
		else if (opt == 1000) {
			if (strcmp(optarg, "tsonly") == 0)
				conf.ip_ts_type = 1;
			else if (strcmp(optarg, "tsaddr") == 0)
				conf.ip_ts_type = 3;
			else {
				fprintf(stderr, "ft_ping: invalid timestamp flag: %s\n", optarg);
				exit(64);
			}
			conf.ip_ts = 1;
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