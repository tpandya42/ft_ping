#define _GNU_SOURCE
#include "ft_ping.h"

static volatile sig_atomic g_sig = 0; 

static void handle_sigint(int sig)
{
	(void)sig;
	write (STDOUT, "\n", 1); // print a new line to match iputils behaviour
	g_sig = 1;
}

int  ft_pingIpv4(t_config *conf, struct sockaddr_in *target_addr, char *ip_str)
{
	// open socket
	int fd = ipv4_icmp_socket(conf);
	if (fd < 0) return -1;
	signal(SIGINT, handle_sigint); // ctr + c
	// initialize time stats for printing.
	t_logs logs;
	memset(&logs, 0, sizeof(logs));
	logs.rtt_max = 0;
	logs.rtt_min = 0;
	logs.rtt_sum = 0;
	logs.rtt_square_sum = 0;

	while (!g_sig)
	{
	}
	// build echo packet
	// send echo req 
	// receive and parse
	// and print info simultaneously

}
