#define _GNU_SOURCE
#include "ft_ping.h"
#include <time.h>
#include <math.h>
#include <netinet/ip.h>

static volatile sig_atomic_t g_sig = 0; 

static void handle_sigint(int sig)
{
	(void)sig;
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
				case 4: return "Fragmentation needed and DF set";
				case 5: return "Source Route Failed";
				case 6: return "Network Unknown";
				case 7: return "Host Unknown";
				case 8: return "Source Isolated";
				case 9: return "Network Administratively Prohibited";
				case 10: return "Host Administratively Prohibited";
				case 11: return "Network Unreachable For TOS";
				case 12: return "Host Unreachable For TOS";
				case 13: return "Communication Administratively Prohibited";
				case 14: return "Host Precedence Violation";
				case 15: return "Precedence Cutoff";
				default: return "Destination Unreachable";
			}
		case ICMP_TIME_EXCEEDED:
			return (code == 1) ? "Frag reassembly time exceeded" : "Time to live exceeded";
		case ICMP_REDIRECT:
			return "Redirect";
		case ICMP_SOURCE_QUENCH:
			return "Source Quench";
		case ICMP_PARAMETERPROB:
			return "Parameter Problem";
		default:
			return "ICMP error";
	}
}

static bool	parseSeqifOur(uint8_t *buf, ssize_t res, int outer_iphdr_len, uint16_t pid, uint16_t *ourSeq)
{
	uint8_t *outer_icmp = buf + outer_iphdr_len;
	uint8_t *inner_ip = outer_icmp + sizeof(struct icmphdr);
	if (inner_ip  + sizeof(struct iphdr) > (uint8_t *)buf + res) return false;
	struct iphdr *inner_iphdr = (struct iphdr *)inner_ip;
	int inner_ip_hl = inner_iphdr->ihl * 4;
	if (inner_ip_hl < (int)sizeof(struct iphdr)) return false;
	if (inner_ip + inner_ip_hl + sizeof(struct icmphdr) > (uint8_t *)buf + res) return false;
	struct icmphdr *inner_icmp = (struct icmphdr *)(inner_ip + inner_ip_hl);
	if(inner_icmp->type == ICMP_ECHO && ntohs(inner_icmp->un.echo.id) == pid) {
		*ourSeq = ntohs(inner_icmp->un.echo.sequence);
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

static void	print_ip_opt(struct iphdr *ip, int hlen)
{
	unsigned char *cp;
	struct in_addr ina;
	char *s;
	int i, j, k, l;
	cp = (unsigned char *)(ip + 1);

	for (; hlen > (int)sizeof(struct iphdr); --hlen, ++cp)
		switch (*cp)
		{
			case IPOPT_EOL:
				hlen = 0;
				break;

			case IPOPT_LSRR:
			case IPOPT_SSRR:
				printf("\n%cSRR: ", (*cp == IPOPT_LSRR) ? 'L' : 'S');
				hlen -= 2;
				j = *++cp;
				++cp;
				if (j > IPOPT_MINOFF)
					for (;;)
					{
						l = *++cp;
						ina.s_addr = htonl(l);
						s = inet_ntoa(ina);
						printf("\t%s", s);
						hlen -= 4;
						i -= 4;
						j += 4;
						if (i <= 0)
							break;
						if (j >= MAX_IPOPTLEN)
						{
							printf("\t (truncated route)");
							break;
						}
						putchar('\n');
					}
				break;

			case IPOPT_RR:
				j = *++cp;
				++cp;
				printf("\nRR: ");
				hlen -= 2;
				if (j > IPOPT_MINOFF)
					for (;;)
					{
						l = *++cp;
						ina.s_addr = htonl(l);
						s = inet_ntoa(ina);
						printf("\t%s", s);
						hlen -= 4;
						i -= 4;
						j += 4;
						if (i <= 0)
							break;
						if (j >= MAX_IPOPTLEN)
						{
							printf("\t (truncated route)");
							break;
						}
						putchar('\n');
					}
				break;

			case IPOPT_TS:
				j = *++cp;
				i = *++cp;
				hlen -= 2;
				if (i > j)
					i = j;
				if (j <= (int)(IPOPT_MINOFF + sizeof(uint32_t)))
					break;
				k = *++cp;
				++cp;
				hlen -= 2;
				printf("\nTS:");
				j = 5;
				for (;;)
				{
					if ((k & 0x0f) != IPOPT_TS_TSONLY
						&& ((j / 4) % 2 == 1))
					{
						struct in_addr ina;
						ina.s_addr = *((uint32_t *)cp);
						char *s = inet_ntoa(ina);
						printf("\t%s", s);
						hlen -= sizeof(uint32_t);
						cp += sizeof(uint32_t);
						j += sizeof(uint32_t);
					}
					else
					{
						uint32_t t = ntohl(*((uint32_t *)cp));
						char timestr[16];
						snprintf(timestr, sizeof(timestr), "%u", t);
						if (k & 0xf0)
							printf("\t%s ms = 0x%08x", timestr, t);
						else
							printf("\t%s ms", timestr);
						hlen -= sizeof(uint32_t);
						cp += sizeof(uint32_t);
						j += sizeof(uint32_t);
						putchar('\n');
					}
					if (j >= i)
						break;
				}
				if (k & 0xf0)
					printf("\t(%u overflowing hosts)", k >> 4);
				break;

			case IPOPT_NOP:
				printf("\nNOP");
				break;

			default:
				printf("\nunknown option %x", *cp);
				break;
		}
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
	long	sec = logs->end_time.tv_sec - logs->start_time.tv_sec;
	long	nsec = logs->end_time.tv_nsec - logs->start_time.tv_nsec;
	if (nsec < 0)
	{
		sec--;
		nsec += 1000000000L;
	}
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

static bool seen_seq[65536];

int  ft_pingIpv4(t_config *conf, struct sockaddr_in *target_addr, char *ip_str)
{
	int fd = ipv4_icmp_socket(conf);
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

	char from_addr[INET_ADDRSTRLEN];
	uint8_t packet[sizeof(struct icmphdr) + 1500];

	if (sizeof(struct icmphdr) + (size_t)conf->packet_size > sizeof(packet))
	{
		fprintf(stderr, "ft_ping: -s %d: packet size too large\n",
			conf->packet_size);
		close(fd);
		return (-1);
	}

	print_ping_header(conf, ip_str);
	clock_gettime(CLOCK_MONOTONIC, &logs.start_time);
	memset(seen_seq, 0, sizeof(seen_seq));

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
		size_t icmp_len = buildEchoPacket(packet, conf->packet_size, seq, conf->pid, &send_ts, conf->pattern);
		if (icmp_len == 0)
		{
			fprintf(stderr, "ft_ping: failed to build icmp packet\n");
			close(fd);
			return (-1);
		}

		ssize_t sent_bytes = sendto(fd, packet, icmp_len, 0, (struct sockaddr *)target_addr, sizeof(*target_addr));
		if (sent_bytes < 0) {
			perror("ft_ping: sendto");
			continue;
		}
		logs.sent++;

		while (!g_sig)
		{
			char	buf[2048];
			struct sockaddr_in src; memset(&src, 0, sizeof(src));
			struct msghdr rrecv; memset(&rrecv, 0, sizeof(rrecv));
			struct iovec	iov; memset(&iov, 0, sizeof(iov));
			iov.iov_base = buf; iov.iov_len = sizeof(buf);
			rrecv.msg_iov = &iov; rrecv.msg_iovlen = 1;
			rrecv.msg_name = &src; rrecv.msg_namelen = sizeof(src);
			ssize_t res = recvmsg(fd, &rrecv, 0);
			if (res < (ssize_t)(sizeof(struct icmphdr) + sizeof(struct iphdr)))
				break;
			struct iphdr *ip = (struct iphdr *)buf;
			int 	iphdr_len = ip->ihl * 4;
			if (iphdr_len < (int)sizeof(struct iphdr)
				|| res < iphdr_len + (ssize_t)sizeof(struct icmphdr))
				continue;
			struct icmphdr *icmp = (struct icmphdr *)(buf + iphdr_len);
			if (icmp->type == ICMP_ECHOREPLY && ntohs(icmp->un.echo.id) == conf->pid)
			{
				double rtt_ms = 0.0;
				if (buf + iphdr_len + (int)sizeof(struct icmphdr) + (int)sizeof(struct timespec) <= buf + res)
				{
					struct timespec sent_ts_reply;
					memcpy(&sent_ts_reply, buf + iphdr_len + sizeof(struct icmphdr), sizeof(sent_ts_reply));
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
				inet_ntop(AF_INET, &src.sin_addr, from_addr, sizeof(from_addr));
				uint16_t rseq = ntohs(icmp->un.echo.sequence);
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
				print_reply(conf, from_addr, rseq, res - iphdr_len,
					ip->ttl, rtt_ms, duplicate);
				if (conf->ip_ts)
					print_ip_opt(ip, iphdr_len);
				break;
			}
			if (icmp->type == ICMP_ECHO)
				continue;
			if (icmp->type != ICMP_ECHOREPLY)
			{
				inet_ntop(AF_INET, &src.sin_addr, from_addr, sizeof(from_addr));
				const char *error_msg = icmp_desc(icmp->type, icmp->code);
				uint16_t ourSeq = 0;
				bool ours = parseSeqifOur((uint8_t *)buf, res, iphdr_len, conf->pid, &ourSeq);
				if (ours || conf->verbose)
				{
					printf("%zu bytes from %s: %s\n", res - iphdr_len, from_addr, error_msg);
					if (conf->verbose && ours)
					{
						uint8_t *outer_icmp = (uint8_t *)buf + iphdr_len;
						uint8_t *inner_ip = outer_icmp + sizeof(struct icmphdr);
						if (inner_ip + sizeof(struct iphdr) + sizeof(struct icmphdr) <= (uint8_t *)buf + res)
						{
							struct iphdr *inner_iphdr = (struct iphdr *)inner_ip;
							int inner_ip_hl = inner_iphdr->ihl * 4;
							if (inner_ip + inner_ip_hl + sizeof(struct icmphdr) <= (uint8_t *)buf + res)
							{
								struct icmphdr *inner_icmp = (struct icmphdr *)(inner_ip + inner_ip_hl);
								unsigned size = ntohs(inner_iphdr->tot_len) - inner_ip_hl;
								printf("ICMP: type %u, code %u, size %u, id 0x%04x, seq 0x%04x\n",
									inner_icmp->type, inner_icmp->code, size,
									ntohs(inner_icmp->un.echo.id), ntohs(inner_icmp->un.echo.sequence));
							}
						}
					}
				}
				continue;
			}
		}

		seq++;

		if (!g_sig && (conf->count <= 0 || logs.sent < conf->count))
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
