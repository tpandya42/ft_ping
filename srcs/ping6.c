#include "ft_ping.h"

static volatile sig_atomic_t g_sig = 0; 

static void handle_sigint(int sig)
{
	(void)sig;
	// no i/o here (not async-signal-safe): the empty line before the
	// statistics is printed by print_summary, matching iputils
	g_sig = 1;
}

int	ft_pingIpv6(t_config *conf, struct sockaddr_in *target_addr, char *ip_str)
{
	// open socket
	int fd = ipv6_icmp_socket(conf);
	if (fd < 0) return -1;
	signal(SIGINT, handle_sigint);

	t_logs logs;
	memset(&logs, 0, sizeof(logs));
	
	logs.rtt_max = 0;
	logs.rtt_min = 0;
	logs.rtt_sum = 0;
	logs.rtt_mdev = 0;
	logs.sent = 0;
	logs.rec = 0;

	uint16_t seq = 0; 
	unsigned	duplicates = 0;
	double		rtt_sq_sum = 0.0; // sum of squared rtt, for mdev
	bool		have_rtt = false;

	char from_addr[INET6_ADDRSTRLEN];

	if (sizeof(struct icmphdr) + (size_t)conf->packet_size > sizeof(packet))
	{
		fprintf(stderr, "ft_ping: -s %d: packet size too large\n",
			conf->packet_size);
		close(fd);
		return (-1);
	}

	// print header here
	clock_gettime(CLOCK_MONOTONIC, &logs.start_time);
	bool	seen_seq[65536];
	memset(seen_seq, 0, sizeof(seen_seq));

	while(!g_sig){
		if (conf->count >= 0 && (int)logs.sent >= conf->count) break;

		// Build packet and send
		struct timespec send_ts;
		clock_gettime(CLOCK_MONOTONIC, &send_ts);
		uint8_t packet[sizeof(struct icmp6_hdr) + 1500];
		size_t	icmp_len = buildEchoV6Packet(packet, sizeof(packet), conf->pid, seq + 1, (size_t)conf->packet_size, &send_ts, conf, &local6.sin6_addr, &dst->sin6_addr);
		if (icmp_len == 0){
			fprintf(stderr, "ft_ping: packet build failed\n");
			break;	
		}
		ssize_t sret = send(fd, packet, icmp_len, 0);
		if (sret <0) {
			perror("ft_ping: send failed\n");
		} else {
			logs.sent++;
			seq++;
		}


		// Receive 
		uint8_t buf[4096];
		ssize_t rret = recv(fd, buf, sizeof(buf), 0);
		if (rret >= (ssize_t)sizeof(struct icmp6_hdr)){
			// the received buffer included the IPv6 header (40 bytes) for Raw ipv6 sockets.
			size_t off = use_raw ? sizeof(struct ip6_hdr) : 0;
			struct icmp6_hdr *icmp6 = (struct icmp6_hdr *)(buf + off);
			int hlim = conf->ttl; // hop limit
			if (off == sizeof(struct ip6_hdr)){
				struct ip6_hdr *ip6 = (struct ip6_hdr *)buf;
				hlim = ip6->ip6_ctlun.ip6_un1.ip6_un1_hlim;
			}
			
			if (icmp6->icmp6_type == ICMP6_ECHO_REPLY && ntohs(icmp6->icmp6_id) == conf->pid) {

			} else if (icmp6->icmp6_type != ICMP6_ECHO_REPLY) {
					
			} else if (rret < 0 && errno == EAGAIN && !use_raw) {
				
			}

			if (conf->count >= 0 && (int)logs.sent >= conf->count) break;
			nsleep6(conf->interval);
		}


	}

	clock_gettime(CLOCK_MONOTONIC, &logs.end_time);
	print_summary6(conf->hostname, &logs);
	close(fd);
	return (0);
}
