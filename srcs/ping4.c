#define _GNU_SOURCE
#include "ft_ping.h"
#include <time.h>
#include <math.h>

static volatile sig_atomic_t g_sig = 0; 

static void handle_sigint(int sig)
{
	(void)sig;
	// no i/o here (not async-signal-safe): the empty line before the
	// statistics is printed by print_summary, matching iputils
	g_sig = 1;
}

static const char *icmp_desc(uint8_t type, uint8_t code)
{
	switch(type)
	{
		case ICMP_DEST_UNREACH:
			switch (code)
			{
				case 0: return "Destination Net Unreachable";
				case 1: return "Destination Host Unreachable";
				case 2: return "Destination Protocol Unreachable";
				case 3: return "Destination Port Unreachable";
				case 4: return "Frag needed and DF set";
				case 5: return "Source route failed";
				case 6: return "Destination Network Unknown";
				case 7: return "Destination Host Unknown";
				case 9: return "Network administratively prohibited";
				case 10: return "Host administratively prohibited";
				case 13: return "Communication administratively prohibited";
				default: return "Destination Unreachable";
			}
		case ICMP_TIME_EXCEEDED:
			return (code == 1) ? "Fragment reassembly time exceeded" : "Time to live exceeded";
		case ICMP_REDIRECT:
			return "Redirect Message";
		case ICMP_PARAMETERPROB:
			return "Parameter Problem";
		default:
			return "ICMP error";
	}
}


// ICMP error itself has its own ICMP header, which doesn't contain sequence number of original
// ping request. But ICMP error message typically include the original IP header and part of the 
// original packet that trigger the error into the reply header.
// -> Outer header (IP header of machine sending the ICMP error)
// -> Outer ICMP HEADER (with code and error type etc.)
// -> Inner ipv4 header (IP header of original ping req)
// -> Inner ICMP header (some data about original req (includes seq, id and req type etc)
// -> Original ICMP payload.
static bool	parseSeqifOur(uint8_t *buf, ssize_t res, int outer_iphdr_len, uint16_t pid, uint16_t *ourSeq)
{
	uint8_t *outer_icmp = buf + outer_iphdr_len;
	uint8_t *inner_ip = outer_icmp + sizeof(struct icmphdr);
	// checking if the inner IPv4 header fits
	if (inner_ip  + sizeof(struct iphdr) > (uint8_t *)buf + res) return false;
	struct iphdr *inner_iphdr = (struct iphdr *)inner_ip;
	int inner_ip_hl = inner_iphdr->ihl * 4;
	if (inner_ip_hl < (int)sizeof(struct iphdr)) return false;
	// finding icmp header
	if (inner_ip + inner_ip_hl + sizeof(struct icmphdr) > (uint8_t *)buf + res) return false; // bound check
	struct icmphdr *inner_icmp = (struct icmphdr *)(inner_ip + inner_ip_hl);
	if(inner_icmp->type == ICMP_ECHO && ntohs(inner_icmp->un.echo.id) == pid) {
		*ourSeq = ntohs(inner_icmp->un.echo.sequence);
		return true;
	}
	return false;
}

/* -------------------- log output (inetutils-2.0 style) -------------------- */

static void	print_ping_header(t_config *conf, const char *ip_str)
{
	// note: iputils prints this header even with -q
	printf("PING %s (%s) %d(%d) bytes of data.\n",
		conf->hostname, ip_str, conf->packet_size,
		conf->packet_size + (int)sizeof(struct icmphdr) + (int)sizeof(struct iphdr));
}

static void	print_reply(t_config *conf, const char *from_addr, uint16_t seq,
		size_t bytes, unsigned ttl, double rtt_ms, bool duplicate)
{
	if (conf->quiet)
		return;
	printf("%zu bytes from %s: icmp_seq=%u ttl=%u time=%.3f ms",
		bytes, from_addr, (unsigned)seq, ttl, rtt_ms);
	if (duplicate && conf->verbose)
		printf(" (DUP!)");
	printf("\n");
}

static void	print_summary(t_config *conf, t_logs *logs, double rtt_sq_sum,
		unsigned duplicates)
{
	// iputils separates the replies from the statistics with an empty line
	printf("\n--- %s ping statistics ---\n", conf->hostname);
	long	sec = logs->end_time.tv_sec - logs->start_time.tv_sec;
	long	nsec = logs->end_time.tv_nsec - logs->start_time.tv_nsec;
	if (nsec < 0)
	{
		sec--;
		nsec += 1000000000L;
	}
	long	elapsed_ms = sec * 1000L + nsec / 1000000L;
	int	loss = 0;
	if (logs->sent > 0)
		loss = (logs->sent - logs->rec) * 100 / logs->sent;
	printf("%d packets transmitted, %d received, %d%% packet loss, time %ldms\n",
		logs->sent, logs->rec, loss, elapsed_ms);
	if (duplicates > 0 && conf->verbose)
		printf("%u duplicates\n", duplicates);
	if (logs->rec > 0)
	{
		// avg/mdev count every reply's rtt, duplicates included (see note in ft_pingIpv4)
		unsigned	rtt_count = logs->rec + duplicates;
		double	avg = logs->rtt_sum / rtt_count;
		double	var = rtt_sq_sum / rtt_count - avg * avg;
		if (var < 0.0)
			var = 0.0;
		printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n",
			logs->rtt_min, avg, logs->rtt_max, sqrt(var));
	}
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
	logs.rtt_mdev = 0;
	logs.sent = 0;
	logs.rec = 0;
	/*NOTE: If duplicate packets are received, they are not included in the packet loss calculation,
	 	although the rount trip time of these packets is used in calculating the min/max/avg/mdev rtt numbers. */
	uint16_t seq = 0; 
	unsigned	duplicates = 0;
	double		rtt_sq_sum = 0.0; // sum of squared rtt, for mdev
	bool		have_rtt = false;

	char from_addr[INET_ADDRSTRLEN];
	uint8_t packet[sizeof(struct icmphdr) + 1500]; // using raw bytes. 1500 is reserving space for ICMP payload in addition to header.

	if (sizeof(struct icmphdr) + (size_t)conf->packet_size > sizeof(packet))
	{
		fprintf(stderr, "ft_ping: -s %d: packet size too large\n",
			conf->packet_size);
		close(fd);
		return (-1);
	}

	print_ping_header(conf, ip_str);
	clock_gettime(CLOCK_MONOTONIC, &logs.start_time);
	/*NOTE: CLOCK_MONOTONIC is not affected by discontinous jumps
	 * in the system time (e.g. manual changes), but is affected by
	 * frequency adjustments.*/	
	bool seen_seq[65536];
	memset(seen_seq, 0, sizeof(seen_seq));
	while (!g_sig)
	{
		if (conf->count > 0 && logs.rec >= conf->count)
			break;

		// build echo packet
		struct timespec send_ts;
		clock_gettime(CLOCK_MONOTONIC, &send_ts);
		size_t icmp_len = buildEchoPacket(conf, packet, sizeof(packet), seq, conf->pid, &send_ts);
		if (icmp_len == 0)
		{
			fprintf(stderr, "ft_ping: failed to build icmp packet\n");
			break;
		}

		// send echo req 
		ssize_t sent_bytes = sendto(fd, packet, icmp_len, 0, (struct sockaddr *)target_addr, sizeof(*target_addr));
		if (sent_bytes < 0) perror("ft_ping: sendto");
		else {
			logs.sent++;
			seq++;	
		}

		// receive and parse. a raw socket also sees our own echo
		// requests reflected back (e.g. on loopback), so keep reading
		// until the matching reply or an icmp error for us arrives,
		// or the socket times out (SO_RCVTIMEO).
		while (!g_sig)
		{
			char	buf[2048]; //IOV max is 1024
			struct sockaddr_in src; memset(&src, 0, sizeof(src));
			struct msghdr rrecv; memset(&rrecv, 0, sizeof(rrecv));
			struct iovec	iov; memset(&iov, 0, sizeof(iov));
			iov.iov_base = buf; iov.iov_len = sizeof(buf);
			rrecv.msg_iov = &iov; rrecv.msg_iovlen = 1; // number of elements in msg_iov
			rrecv.msg_name = &src; rrecv.msg_namelen = sizeof(src);
			ssize_t res = recvmsg(fd, &rrecv, 0);
			if (res < (ssize_t)(sizeof(struct icmphdr) + sizeof(struct iphdr)))
				break; // timeout or nothing usable: move on to the next echo req
			struct iphdr *ip = (struct iphdr *)buf;
			int 	iphdr_len = ip->ihl * 4;
			if (iphdr_len < (int)sizeof(struct iphdr) // bogus ip header length
				|| res < iphdr_len + (ssize_t)sizeof(struct icmphdr)) // making sure ICMP header actually exists
				continue; // keep waiting for our reply
			// Finding the ICMP header
			struct icmphdr *icmp = (struct icmphdr *)(buf + iphdr_len); // buf + iphdr_len = start of icmp hdr
			if (icmp->type == ICMP_ECHOREPLY && ntohs(icmp->un.echo.id) == conf->pid) // check if it is echo reply and if it is our ping (pid)
			{
				double rtt_ms = 0.0;
				struct timespec sent_ts;
				// finding the timestamp in the ICMP PAYLOAD
				// buf + IP header + icmp header -> start of payload ( which is timestamp)
				/* ICMP payload: timespec.tv_sec
				 		 timespec.tv_nsec
				 		 other stuff */
				if (buf + iphdr_len + (int)sizeof(struct icmphdr) + (int)sizeof(struct timespec) <= buf + res)
				{
					memcpy(&sent_ts, buf + iphdr_len + sizeof(struct icmphdr), sizeof(sent_ts));
					/*Calculating rtt*/
					struct timespec now;
					clock_gettime(CLOCK_MONOTONIC, &now);
					long sec = now.tv_sec - sent_ts.tv_sec;
					long nsec = now.tv_nsec - sent_ts.tv_nsec;
					if (nsec < 0) // borrow from the seconds part
					{
						sec--;
						nsec += 1000000000L;
					}
					rtt_ms = sec * 1000.0 + nsec / 1e6;
					if (rtt_ms < 0.0)
						rtt_ms = 0.0;
				}
				// Getting the source IP address
				inet_ntop(AF_INET, &src.sin_addr, from_addr, sizeof(from_addr)); //src.sin_addr contain binary addr IPv4
				// Extract the sequence number
				uint16_t rseq = ntohs(icmp->un.echo.sequence);
				// Checking for duplicate 
				bool duplicate = seen_seq[rseq];
				seen_seq[rseq] = true;
				if (duplicate)
					duplicates++;
				else
					logs.rec++; // unique reply, counts toward packet loss stats
				// rtt of duplicates still feeds min/max/avg/mdev (see note above)
				logs.rtt_sum += rtt_ms;
				rtt_sq_sum += rtt_ms * rtt_ms;
				if (!have_rtt || rtt_ms < logs.rtt_min) logs.rtt_min = rtt_ms;
				if (!have_rtt || rtt_ms > logs.rtt_max) logs.rtt_max = rtt_ms;
				have_rtt = true;
				// print the output here if the queit flag is NOT true;
				print_reply(conf, from_addr, rseq, res - iphdr_len,
					ip->ttl, rtt_ms, duplicate);
				break; // got our reply for this round
			}
			if (icmp->type == ICMP_ECHO)
				continue; // our own echoed-back echo request, not a reply
			if (icmp->type != ICMP_ECHOREPLY){ // to show icmp error code for -v
				inet_ntop(AF_INET, &src.sin_addr, from_addr, sizeof(from_addr));
				const char *error_msg = icmp_desc(icmp->type, icmp->code);
				// to handle verbose information
				char extra[128]; extra[0] = '\0';
				if (conf->verbose)
				{
					if (icmp->type == ICMP_REDIRECT) {
						struct in_addr gateway;
						gateway.s_addr = icmp->un.gateway;
						char gwstr[INET_ADDRSTRLEN];
						if (inet_ntop(AF_INET, &gateway, gwstr, sizeof(gwstr)))
							snprintf(extra, sizeof(extra), " (new nexthop: %s)", gwstr);
					}
					else if (icmp->type == ICMP_DEST_UNREACH && icmp->code == 4) { // Path MTU Discover ( Fragementation needed) TODO: Read more about mtu
						unsigned mtu = ntohs(icmp->un.frag.mtu);
						if (mtu)
							snprintf(extra, sizeof(extra), " (mtu=%u)", mtu);
					}
				}
				// ICMP error packet usually contains part of the original packet that caused the error.
				uint16_t ourSeq = 0;
				bool ours = parseSeqifOur((uint8_t *)buf, res, iphdr_len, conf->pid, &ourSeq);
				if (ours){
					if (conf->verbose)
						printf("From %s icmp_seq=%u %s (type=%d code=%d)%s\n", from_addr, (unsigned)ourSeq, error_msg, icmp->type, icmp->code, extra);
					else
						printf("From %s %s\n", from_addr, error_msg);
				}
				// keep waiting: the error is not the echo reply we asked for
				continue;
			}
		}
		if (conf->count >= 0 && logs.sent >= conf->count)
			break;
	}
	clock_gettime(CLOCK_MONOTONIC, &logs.end_time);
	print_summary(conf, &logs, rtt_sq_sum, duplicates);
	close(fd);
	return (0);
}
