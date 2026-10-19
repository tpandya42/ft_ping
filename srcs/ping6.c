#define _GNU_SOURCE
#include "ft_ping.h"
#include <time.h>
#include <math.h>

static volatile sig_atomic_t g_sig = 0; 

static void handle_sigint(int sig)
{
	(void)sig;
	// no i/o here (not async-signal-safe): the empty line before the
	g_sig = 1;
}

static const char *icmp6_desc(uint8_t type, uint8_t code)
{
	switch(type)
	{
		case ICMP6_DST_UNREACH:
			switch (code)
			{
				case ICMP6_DST_UNREACH_NOROUTE: return "Destination Unreachable: No route to destination";
				case ICMP6_DST_UNREACH_ADMIN:   return "Destination Unreachable: Administratively prohibited";
				case ICMP6_DST_UNREACH_BEYOND:  return "Destination Unreachable: Beyond scope of source address";
				case ICMP6_DST_UNREACH_ADDR:    return "Destination Unreachable: Address unreachable";
				case ICMP6_DST_UNREACH_NOPORT:  return "Destination Unreachable: Port unreachable";
				case ICMP6_DST_UNREACH_FAIL:    return "Destination Unreachable: Source address failed ingress/egress policy";
				case ICMP6_DST_UNREACH_REJECT:  return "Destination Unreachable: Reject route to destination";
				default: return "Destination Unreachable";
			}
		case ICMP6_PACKET_TOO_BIG:
			return "Packet Too Big";
		case ICMP6_TIME_EXCEEDED:
			return (code == ICMP6_TIME_EXCEED_TRANSIT) ? "Time Exceeded: Hop limit exceeded in transit" : "Time Exceeded: Fragment reassembly time exceeded";
		case ICMP6_PARAM_PROB:
			return "Parameter Problem";
		default:
			return "ICMPv6 error";
	}
}

/* ICMPv6 error packets contain the original packet that triggered the error.
 * Layout on Linux raw ICMPv6 sockets (no IPv6 header prepended):
 *   outer ICMPv6 header (8 bytes)
 *   inner IPv6 header (40 bytes, fixed)
 *   inner ICMPv6 header (8 bytes, includes our id/seq)
 *   payload (includes our timestamp) */
static bool	parseSeqifOur6(uint8_t *buf, ssize_t res, uint16_t pid, uint16_t *ourSeq)
{
	// buf starts at outer ICMPv6 header
	uint8_t *inner_ip = buf + sizeof(struct icmp6_hdr);
	// check inner IPv6 header + inner ICMPv6 header fits
	if (inner_ip + sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr) > (uint8_t *)buf + res)
		return false;
	struct ip6_hdr *inner_ip6 = (struct ip6_hdr *)inner_ip;
	// check next header is ICMPv6 (58)
	if (inner_ip6->ip6_nxt != IPPROTO_ICMPV6)
		return false;
	uint8_t *inner_icmp6_ptr = inner_ip + sizeof(struct ip6_hdr);
	struct icmp6_hdr *inner_icmp6 = (struct icmp6_hdr *)inner_icmp6_ptr;
	if (inner_icmp6->icmp6_type == ICMP6_ECHO_REQUEST && ntohs(inner_icmp6->icmp6_id) == pid)
	{
		*ourSeq = ntohs(inner_icmp6->icmp6_seq);
		return true;
	}
	return false;
}

static void	print_ping_header(t_config *conf, const char *ip_str)
{
	// inetutils-2.0 style for IPv6: "PING hostname (ipv6): 56 data bytes"
	printf("PING %s (%s): %d data bytes\n",
		conf->hostname, ip_str, conf->packet_size);
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

/* 
 * seen_seq tracks which sequence numbers we've already received a reply for.
 * Using static storage to avoid 64KB stack allocation.
 * Size 65536 covers all possible uint16_t sequence numbers.
 */
static bool seen_seq[65536];

int  ft_pingIpv6(t_config *conf, struct sockaddr_in6 *target_addr, char *ip_str)
{
	// open socket
	int fd = ipv6_icmp_socket(conf);
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

	char from_addr[INET6_ADDRSTRLEN];
	uint8_t packet[sizeof(struct icmp6_hdr) + 1500]; // using raw bytes. 1500 is reserving space for ICMP payload in addition to header.

	if (sizeof(struct icmp6_hdr) + (size_t)conf->packet_size > sizeof(packet))
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
	memset(seen_seq, 0, sizeof(seen_seq));

	// Source address for checksum pseudo-header.
	// Linux kernel will recompute/overwrite the ICMPv6 checksum for raw sockets.
	// We use zeroed source (::) here and let the kernel fill in the real source.
	struct in6_addr src;
	memset(&src, 0, sizeof(src));
	struct in6_addr *dst = &target_addr->sin6_addr;

	// Main ping loop: send packet, wait for reply, sleep 1s, repeat
	while (!g_sig)
	{
		// Check count limit BEFORE sending (use sent count, not received)
		if (conf->count > 0 && logs.sent >= (unsigned)conf->count)
			break;

		// Retransmit state for current sequence
		int retransmit = 0;
		const int max_retransmit = 3; // max retransmits per sequence
		bool got_reply = false;
		struct timespec send_ts;
		size_t icmp_len = 0;

		// Inner loop: send and wait for reply (with retransmits on timeout)
		while (!g_sig && !got_reply && retransmit <= max_retransmit)
		{
			// Build echo packet (on first attempt or retransmit)
			if (retransmit == 0 || icmp_len == 0) {
				clock_gettime(CLOCK_MONOTONIC, &send_ts);
				icmp_len = buildEchoV6Packet(packet, sizeof(packet), seq, conf->pid, &send_ts, &src, dst);
				if (icmp_len == 0)
				{
					fprintf(stderr, "ft_ping: failed to build icmp packet\n");
					got_reply = true; // break out of inner loop
					break;
				}
			}

			// send echo req 
			ssize_t sent_bytes = sendto(fd, packet, icmp_len, 0, (struct sockaddr *)target_addr, sizeof(*target_addr));
			if (sent_bytes < 0) {
				perror("ft_ping: sendto");
				// Don't count failed sends, just retry
				continue;
			}
			
			// Only increment sent count on first transmission (not retransmits)
			if (retransmit == 0) {
				logs.sent++;
			}

			// receive and parse. a raw socket also sees our own echo
			// requests reflected back (e.g. on loopback), so keep reading
			// until the matching reply or an icmp error for us arrives,
			// or the socket times out (SO_RCVTIMEO).
			while (!g_sig)
			{
				char	buf[2048];
				struct sockaddr_in6 src; memset(&src, 0, sizeof(src));
				struct msghdr rrecv; memset(&rrecv, 0, sizeof(rrecv));
				struct iovec	iov; memset(&iov, 0, sizeof(iov));
				iov.iov_base = buf; iov.iov_len = sizeof(buf);
				rrecv.msg_iov = &iov; rrecv.msg_iovlen = 1;
				rrecv.msg_name = &src; rrecv.msg_namelen = sizeof(src);
				
				// Control buffer for IPV6_HOPLIMIT cmsg
				char control[CMSG_SPACE(sizeof(int))];
				rrecv.msg_control = control;
				rrecv.msg_controllen = sizeof(control);
				
				ssize_t res = recvmsg(fd, &rrecv, 0);
				if (res < (ssize_t)sizeof(struct icmp6_hdr))
					break; // timeout or nothing usable: will retransmit or move to next seq
				
				// Extract hop limit from cmsg (if available)
				unsigned hoplimit = 0;
				for (struct cmsghdr *cm = CMSG_FIRSTHDR(&rrecv); cm; cm = CMSG_NXTHDR(&rrecv, cm))
				{
					if (cm->cmsg_level == IPPROTO_IPV6 && cm->cmsg_type == IPV6_HOPLIMIT)
					{
						memcpy(&hoplimit, CMSG_DATA(cm), sizeof(hoplimit));
						break;
					}
				}
				
				struct icmp6_hdr *icmp6 = (struct icmp6_hdr *)buf; // no IPv6 header on Linux raw ICMPv6
				if (icmp6->icmp6_type == ICMP6_ECHO_REPLY && ntohs(icmp6->icmp6_id) == conf->pid)
				{
					double rtt_ms = 0.0;
					// finding the timestamp in the ICMP PAYLOAD
					// buf + ICMPv6 header -> start of payload (timestamp)
					if ((uint8_t *)buf + sizeof(struct icmp6_hdr) + sizeof(struct timespec) <= (uint8_t *)buf + res)
					{
						struct timespec sent_ts_reply;
						memcpy(&sent_ts_reply, buf + sizeof(struct icmp6_hdr), sizeof(sent_ts_reply));
						/*Calculating rtt*/
						struct timespec now;
						clock_gettime(CLOCK_MONOTONIC, &now);
						long sec = now.tv_sec - sent_ts_reply.tv_sec;
						long nsec = now.tv_nsec - sent_ts_reply.tv_nsec;
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
					inet_ntop(AF_INET6, &src.sin6_addr, from_addr, sizeof(from_addr));
					// Extract the sequence number
					uint16_t rseq = ntohs(icmp6->icmp6_seq);
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
					// print the output here if the quiet flag is NOT true;
					print_reply(conf, from_addr, rseq, res,
						hoplimit, rtt_ms, duplicate);
					got_reply = true; // got our reply for this sequence
					break; // break out of receive loop
				}
				if (icmp6->icmp6_type == ICMP6_ECHO_REQUEST)
					continue; // our own echoed-back echo request, not a reply
				if (icmp6->icmp6_type != ICMP6_ECHO_REPLY){ // to show icmp error code for -v
					inet_ntop(AF_INET6, &src.sin6_addr, from_addr, sizeof(from_addr));
					const char *error_msg = icmp6_desc(icmp6->icmp6_type, icmp6->icmp6_code);
					// to handle verbose information
					char extra[128]; extra[0] = '\0';
					if (conf->verbose)
					{
						if (icmp6->icmp6_type == ICMP6_PACKET_TOO_BIG) {
							unsigned mtu = ntohl(icmp6->icmp6_mtu);
							if (mtu)
								snprintf(extra, sizeof(extra), " (mtu=%u)", mtu);
						}
						else if (icmp6->icmp6_type == ICMP6_PARAM_PROB) {
							unsigned ptr = ntohl(icmp6->icmp6_pptr);
							snprintf(extra, sizeof(extra), " (ptr=%u)", ptr);
						}
					}
					// ICMPv6 error packet usually contains part of the original packet that caused the error.
					uint16_t ourSeq = 0;
					bool ours = parseSeqifOur6((uint8_t *)buf, res, conf->pid, &ourSeq);
					if (ours){
						if (conf->verbose)
							printf("From %s icmp_seq=%u %s (type=%d code=%d)%s\n", from_addr, (unsigned)ourSeq, error_msg, icmp6->icmp6_type, icmp6->icmp6_code, extra);
						else
							printf("From %s %s\n", from_addr, error_msg);
					}
					// keep waiting: the error is not the echo reply we asked for
					continue;
				}
			} // end receive loop

			if (!got_reply) {
				retransmit++;
				if (retransmit <= max_retransmit && !g_sig) {
					// Timeout occurred - will retransmit same sequence
					if (conf->verbose) {
						fprintf(stderr, "ft_ping: timeout, retransmitting seq=%u (attempt %d/%d)\n", 
							seq, retransmit, max_retransmit);
					}
				}
			}
		} // end retransmit loop

		// If we exhausted retransmits without reply, that's a lost packet
		if (!got_reply && retransmit > max_retransmit) {
			if (conf->verbose) {
				fprintf(stderr, "ft_ping: seq=%u lost after %d retransmits\n", seq, max_retransmit);
			}
		}

		seq++; // move to next sequence number

		// Sleep ~1 second before next ping (standard ping interval)
		// Only sleep if not exiting and not the last packet
		if (!g_sig && (conf->count <= 0 || logs.sent < (unsigned)conf->count)) {
			struct timespec sleep_ts = {1, 0}; // 1 second
			nanosleep(&sleep_ts, NULL);
		}
	} // end main while loop

	clock_gettime(CLOCK_MONOTONIC, &logs.end_time);
	print_summary(conf, &logs, rtt_sq_sum, duplicates);
	close(fd);
	return (0);
}
