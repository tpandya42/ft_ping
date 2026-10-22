#define _GNU_SOURCE
#include "ft_ping.h"
#include <time.h>
#include <math.h>

static void handle_sigint(int sig)
{
	(void)sig;
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
				case ICMP6_DST_UNREACH_BEYONDSCOPE:  return "Destination Unreachable: Beyond scope of source address";
				case ICMP6_DST_UNREACH_ADDR:    return "Destination Unreachable: Address unreachable";
				case ICMP6_DST_UNREACH_NOPORT:  return "Destination Unreachable: Port unreachable";
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

static bool	parseSeqifOur6(uint8_t *buf, ssize_t res, uint16_t pid, uint16_t *ourSeq)
{
	uint8_t *inner_ip = buf + sizeof(struct icmp6_hdr);
	if (inner_ip + sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr) > (uint8_t *)buf + res)
		return false;
	struct ip6_hdr *inner_ip6 = (struct ip6_hdr *)inner_ip;
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
	printf("PING %s (%s): %d data bytes", conf->hostname, ip_str, conf->packet_size);
	if (conf->verbose)
		printf(", id 0x%04x = %u", conf->pid, conf->pid);
	printf("\n");
}

static void	print_reply(t_config *conf, const char *from_addr, uint16_t seq,
		size_t bytes, unsigned ttl, double rtt_ms, bool duplicate)
{
	if (conf->quiet)
		return;
	printf("%zu bytes from %s: icmp_seq=%u ttl=%u time=%.3f ms",
		bytes, from_addr, (unsigned)seq, ttl, rtt_ms);
	if (duplicate)
		printf(" (DUP!)");
	printf("\n");
}

static void	print_summary(t_config *conf, t_logs *logs, double rtt_sq_sum,
		unsigned duplicates)
{
	int	loss = 0;
	if (logs->sent > 0)
		loss = (logs->sent - logs->rec) * 100 / logs->sent;
	printf("--- %s ping statistics ---\n", conf->hostname);
	if (duplicates > 0)
		printf("%d packets transmitted, %d received, +%u duplicates, %d%% packet loss\n",
			logs->sent, logs->rec, duplicates, loss);
	else
		printf("%d packets transmitted, %d received, %d%% packet loss\n",
			logs->sent, logs->rec, loss);
	if (logs->rec > 0)
	{
		unsigned	rtt_count = logs->rec + duplicates;
		double	avg = logs->rtt_sum / rtt_count;
		double	var = rtt_sq_sum / rtt_count - avg * avg;
		if (var < 0.0)
			var = 0.0;
		printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
			logs->rtt_min, avg, logs->rtt_max, sqrt(var));
	}
}

int  ft_pingIpv6(t_config *conf, struct sockaddr_in6 *target_addr, char *ip_str)
{
	bool seen_seq[65536];

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
	double		rtt_sq_sum = 0.0;
	bool		have_rtt = false;

	char from_addr[INET6_ADDRSTRLEN];
	uint8_t packet[sizeof(struct icmp6_hdr) + 1500];

	if (sizeof(struct icmp6_hdr) + (size_t)conf->packet_size > sizeof(packet))
	{
		fprintf(stderr, "ft_ping: -s %d: packet size too large\n",
			conf->packet_size);
		close(fd);
		return (-1);
	}

	print_ping_header(conf, ip_str);
	clock_gettime(CLOCK_MONOTONIC, &logs.start_time);
	memset(seen_seq, 0, sizeof(seen_seq));

	struct in6_addr src;
	memset(&src, 0, sizeof(src));
	struct in6_addr *dst = &target_addr->sin6_addr;

	while (!g_sig)
	{
		if (conf->count > 0 && logs.sent >= conf->count)
			break;

		if (conf->deadline > 0)
		{
			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);
			long elapsed = now.tv_sec - logs.start_time.tv_sec;
			if (elapsed >= conf->deadline)
				break;
		}

		struct timespec send_ts;
		clock_gettime(CLOCK_MONOTONIC, &send_ts);
		size_t icmp_len = buildEchoV6Packet(packet, conf->packet_size, seq, conf->pid, &send_ts, &src, dst, conf->pattern);
		if (icmp_len == 0)
		{
			fprintf(stderr, "ft_ping: failed to build icmp packet\n");
			close(fd);
			return (-1);
		}

		ssize_t sent_bytes = sendto(fd, packet, icmp_len, 0, (struct sockaddr *)target_addr, sizeof(*target_addr));
		if (sent_bytes < 0) {
			perror("ft_ping: sendto");
			logs.sent++;
			seq++;
			if (!g_sig && (conf->count <= 0 || logs.sent < conf->count))
			{
				struct timespec sleep_ts = {1, 0};
				nanosleep(&sleep_ts, NULL);
			}
			continue;
		}
		logs.sent++;

		bool got_reply = false;
		while (!g_sig)
		{
			char	buf[2048];
			struct sockaddr_in6 src; memset(&src, 0, sizeof(src));
			struct msghdr rrecv; memset(&rrecv, 0, sizeof(rrecv));
			struct iovec	iov; memset(&iov, 0, sizeof(iov));
			iov.iov_base = buf; iov.iov_len = sizeof(buf);
			rrecv.msg_iov = &iov; rrecv.msg_iovlen = 1;
			rrecv.msg_name = &src; rrecv.msg_namelen = sizeof(src);
			
			char control[CMSG_SPACE(sizeof(int))];
			rrecv.msg_control = control;
			rrecv.msg_controllen = sizeof(control);
			
			ssize_t res = recvmsg(fd, &rrecv, 0);
			if (res < (ssize_t)sizeof(struct icmp6_hdr))
				break;
			
			unsigned hoplimit = 0;
			for (struct cmsghdr *cm = CMSG_FIRSTHDR(&rrecv); cm; cm = CMSG_NXTHDR(&rrecv, cm))
			{
				if (cm->cmsg_level == IPPROTO_IPV6 && cm->cmsg_type == IPV6_HOPLIMIT)
				{
					memcpy(&hoplimit, CMSG_DATA(cm), sizeof(hoplimit));
					break;
				}
			}
			
			struct icmp6_hdr *icmp6 = (struct icmp6_hdr *)buf;
			if (icmp6->icmp6_type == ICMP6_ECHO_REPLY && ntohs(icmp6->icmp6_id) == conf->pid)
			{
				double rtt_ms = 0.0;
				if ((uint8_t *)buf + sizeof(struct icmp6_hdr) + sizeof(struct timespec) <= (uint8_t *)buf + res)
				{
					struct timespec sent_ts_reply;
					memcpy(&sent_ts_reply, buf + sizeof(struct icmp6_hdr), sizeof(sent_ts_reply));
					struct timespec now;
					clock_gettime(CLOCK_MONOTONIC, &now);
					long sec = now.tv_sec - sent_ts_reply.tv_sec;
					long nsec = now.tv_nsec - sent_ts_reply.tv_nsec;
					if (nsec < 0)
					{
						sec--;
						nsec += 1000000000L;
					}
					rtt_ms = sec * 1000.0 + nsec / 1e6;
					if (rtt_ms < 0.0)
						rtt_ms = 0.0;
				}
				inet_ntop(AF_INET6, &src.sin6_addr, from_addr, sizeof(from_addr));
				uint16_t rseq = ntohs(icmp6->icmp6_seq);
				bool duplicate = seen_seq[rseq];
				seen_seq[rseq] = true;
				if (duplicate)
					duplicates++;
				else
					logs.rec++;
				logs.rtt_sum += rtt_ms;
				rtt_sq_sum += rtt_ms * rtt_ms;
				if (!have_rtt || rtt_ms < logs.rtt_min) logs.rtt_min = rtt_ms;
				if (!have_rtt || rtt_ms > logs.rtt_max) logs.rtt_max = rtt_ms;
				have_rtt = true;
				print_reply(conf, from_addr, rseq, res,
					hoplimit, rtt_ms, duplicate);
				got_reply = true;
				break;
			}
			if (icmp6->icmp6_type == ICMP6_ECHO_REQUEST)
				continue;
			if (icmp6->icmp6_type != ICMP6_ECHO_REPLY)
			{
				inet_ntop(AF_INET6, &src.sin6_addr, from_addr, sizeof(from_addr));
				const char *error_msg = icmp6_desc(icmp6->icmp6_type, icmp6->icmp6_code);
				uint16_t ourSeq = 0;
				bool ours = parseSeqifOur6((uint8_t *)buf, res, conf->pid, &ourSeq);
				if (ours || conf->verbose)
				{
					printf("%zu bytes from %s: %s\n", res, from_addr, error_msg);
					if (conf->verbose && ours)
					{
						uint8_t *outer_icmp6 = (uint8_t *)buf;
						uint8_t *inner_ip = outer_icmp6 + sizeof(struct icmp6_hdr);
						if (inner_ip + sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr) <= (uint8_t *)buf + res)
						{
							struct ip6_hdr *inner_ip6 = (struct ip6_hdr *)inner_ip;
							if (inner_ip6->ip6_nxt == IPPROTO_ICMPV6)
							{
								uint8_t *inner_icmp6_ptr = inner_ip + sizeof(struct ip6_hdr);
								struct icmp6_hdr *inner_icmp6 = (struct icmp6_hdr *)inner_icmp6_ptr;
								unsigned size = ntohs(inner_ip6->ip6_plen);
								printf("ICMP: type %u, code %u, size %u, id 0x%04x, seq 0x%04x\n",
									inner_icmp6->icmp6_type, inner_icmp6->icmp6_code, size,
									ntohs(inner_icmp6->icmp6_id), ntohs(inner_icmp6->icmp6_seq));
							}
						}
					}
				}
				continue;
			}
		}

		seq++;

		if (!g_sig && got_reply && (conf->count <= 0 || logs.sent < conf->count))
		{
			struct timespec sleep_ts = {1, 0};
			nanosleep(&sleep_ts, NULL);
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &logs.end_time);
	print_summary(conf, &logs, rtt_sq_sum, duplicates);
	close(fd);
	return (logs.rec == 0 ? 1 : 0);
}
