#define _GNU_SOURCE
#include "ft_ping.h"

static volatile sig_atomic g_sig = 0; 

static void handle_sigint(int sig)
{
	(void)sig;
	write (STDOUT, "\n", 1); // print a new line to match iputils behaviour
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
	false;
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

	char from_addr[INET_ADDRSTRLEN];

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
		uint8_t packet[sizeof(struct icmphdr) + 1500]; // using raw bytes. 1500 is reserving space for ICMP payload in addition to header.
							       

		// build echo packet
		size_t icmp_len = buildEchoPacket(conf, packet, sizeof(packet), seq + 1, conf->packet_size, conf->pid);

		// send echo req 
		size_t sent_bytes = sento(fd, packet, icmp_len, 0, (struct sockaddr_in *)target_addr, sizeof(target_addr));
		if (sent_bytes < 0) perror("ft_ping: sento");
		else {
			logs.sent++;
			seq++;	
		}

		// receive and parse
		char	buf[2048]; //IOV max is 1024
		struct sockaddr_in src; memset(&src, 0, sizeof(src));
		struct msghdr rrecv; memset(&rrecv, 0, sizeof(rrecv));
		struct iovec	iov; memset(&io, 0, sizeof(rrecv));
		iov.iov_base = buf; iov.iov_len = sizeof(buf);
		rrecv.msg_iov = &iov; rrecv.msg_iovlen = 1; // number of elements in msg_iov
		rrecv.msg_name = &src; rrecv.msg_namelen = sizeof(src);
		ssize_t res = recvmsg(fd, &rrecv, 0);
		if (res >= (ssize_t)(sizeof(struct icmphdr) + sizeof(struct iphdr))) // check if packet is big enough
		{
			struct iphdr *ip = (struct iphdr *)buf;
			int 	iphdr_len = ip->ihl * 4;
			if (res < iphdr_len + (ssize_t)sizeof(struct icmphdr)) // making sure ICMP header actually exists
				goto wait_next;
			// Finding the ICMP header
			struct icmphdr *icmp = (struct icmphdr *)(buf + iphdr_len); // buf + iphdr_len = start of icmp hdr
			if (icmp->type == ICMP_ECHOREPLY && ntohs(icmp->un.echo.id) == conf->pid) // check if it is echo reply and if it is our ping (pid)
			{
				// record the timestamp of reply (for rtt)
				clock_gettime(CLOCK_MONOTONIC, &logs.end_time);
				// finding the timestamp in the ICMP PAYLOAD
				// buf + IP header + icmp header -> start of payload ( which is timestamp)
				/* ICMP payload: timespec.tv_sec
				 		 timespec.tv_nsec
				 		 other stuff */
				struct timespec *sent_ts = (struct timespec *)(buf + iphdr + len + sizeof(struct icmphr));
				double rtt_ms = 0.0;
				// check if received packet contain complete struct timespec
				if ((uint8_t*)sent_ts + sizeof(struct timespec) <= buf + res) // buf + res is end of packet
				{
					/*Calculating rtt*/
					struct timespec now;
					clock_gettime(CLOCK_MONOTONIC, &now);
					long sec = now.tv_sec - sent_ts->tv_sec;
					long nsec = now.tv_sec - sent_ts->tv_nsec;
					rtt_ms = sec * 1000.0 + nsec / 1e6;
				}
				logs.rec++; // Update recv stat
				// Update rtt_min and rtt_max
				if (logs.rec == 1 || rtt_ms < logs.rtt_min) logs.rtt_min = rtt_ms;
				if (logs.rec == 1 || rtt_ms > logs.rtt_max) logs.rtt_max = rtt_ms;

				// Getting the source IP address
				inet_ntop(AF_INET, &src.sin_addr, from_addr, sizeof(from_addr)); //src.sin_addr contain binary addr IPv4
				// Extract the sequence number
				uint16_t rseq = ntohs(icmp->un.echo.sequence);
				// Checking for duplicate 
				bool duplicate = false;
				if (seen_seq[rseq]){
					logs.duplicates++;
					duplicate = true;
				} else {
					seen_seq[rseq] = true;
				}
				// print the output here if the queit flag is NOT true;

			} else if (icmp->type != ICMP_ECHOREPLY){ // to show icmp error code for -v
				inet_ntop(AF_INET, &src.sin_addr, from_addr, sizeof(fromaddr));
				const char *error_msg = icmp_desc(icmp->type, icmp_code);
				// to handle verbose information
				char extra[128]; extra[0] = '\0';
				if (conf->verbose)
				{
					if (icmp->type == ICMP_REDIRECT) {
						struct in_addr gateway;
						gateway->s_addr = icmp->un.gateway;
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
				bool ours = parseSeqifOur(buf, res, iphdr_len, conf->pid, &ourSeq);
				if (ours){
					if (conf->verbose)
						printf("From %s icmp_seq=%u %s (type=%d code=%d)%s\n", from_addr, (unsigned)ourSeq, error_msg, icmp->type, icmp->code, extra);
					else
						printf("From %s %s\n", from_addr, error_msg);
				}
			}
		}
		wait_next;
		if (conf->count >= 0 && (int)logs.sent >= conf->count)
			break;
	}
	clock_gettime(CLOCK_MONOTONIC, &logs.end_time);
	print_summary(conf->hostname, &logs);
	close(fd);
	return (0);
}
