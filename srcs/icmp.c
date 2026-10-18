#include "ft_ping.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* We gonna use 16 bit one's complement addition thingy to calculate cchecksum
 */

/* Step 1 is to pair bytes into 16 bit words
 * Step 2 is to sum all wrods into 32 bit integer 
 * Step 3 is to fold carry bits 
 * Step 4 is to do One's complement*/ 

static uint16_t calculateChecksum(const void *data, size_t l)
{
	const uint16_t *words = (const uint16_t *)data;
	uint32_t sum = 0;
	while (l > 1)
	{
		sum += *words++;
		l -=2;
	}
	// odd length handling
	if (l == 1) {
		uint16_t odd_byte = 0;
		// copying single remaining byte into low-address byte of odd_byte
		*((uint8_t *)&odd_byte) = *(const uint8_t *)words;
		sum += odd_byte;
	}
	// folding 32bit sum into 16bits
	while (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	return (uint16_t)(~sum);
}

// TODO: calculate time taken to send a packet.
size_t	buildEchoPacket(uint8_t *packet, int packetSize, uint16_t seq, uint16_t pid, struct timespec *st_time)
{ 
	size_t icmp_len = sizeof(struct icmphdr) + packetSize;
	if (packetSize < 0 || (size_t)packetSize > 65535)
		return 0;
	struct icmphdr *hdr = (struct icmphdr *)packet;	
	hdr->type = ICMP_ECHO; // 8
	hdr->code = 0;
	hdr->un.echo.id = htons(pid); // our pid in network byte
	hdr->un.echo.sequence = htons(seq);
	hdr->checksum = 0;

	uint8_t *payload = packet + sizeof(struct icmphdr);
	if (st_time && packetSize >= (int)sizeof(struct timespec))
	{
		memcpy(payload, st_time, sizeof(struct timespec));
		memset(payload + sizeof(struct timespec), 0x42, packetSize - sizeof(struct timespec));
	}
	else
	{
		memset(payload, 0x42, packetSize);
	}
	hdr->checksum = calculateChecksum(packet, icmp_len);
	return icmp_len;
}

static uint16_t icmp6_checksum(const struct in6_addr *src, const struct in6_addr *dst, const uint8_t *icmp, size_t icmp_len)
{

}

size_t	buildEchoV6Packet(uint8_t *packetBuf, size_t packet_len, uint16_t pid, uint16_t seq, size_t packet_size, const struct timespec *send_ts, const t_config *conf, const struct in6_addr *src, const struct in_6_addr *dst)
{
	size_t icmp_len = sizeof(struct icmp6_hdr) + packet_size;
	if (packet_len < icmp_len) return 0;

	struct icmp6_hdr *icmp6 = (struct icmp6_hdr *)packetBuf;
	icmp6->icmp6_type = ICMP6_ECHO_REQUEST;
	icmp6->icmp6_code = 0;
	icmp6->icmp6_cksum = 0;
	icmp6->icmp6_id = htons(pid);
	icmp6->icmp6_seq = htons(seq);

	uint8_t *payload = buf + sizeof(struct icmp6_hdr);
	size_t ts_len = sizeof(struct timesepc);
	if (packet_len >= ts_len && send_ts){
		memcpy(payload, send_ts, ts_len);
		size_t rem = packet_len - ts_len;
		if (conf && conf->patter_len > 0){
			for (size_t i = 0; i < rem; ++i)
				payload[ts_len + i] = conf->pattern[i % conf->pattern_len];
		} else {
			memset(payload + ts_len, 0x42, rem);	
		}
	} else {
		
		if (conf && conf->patter_len > 0){
			for (size_t i = 0; i < packet_len; ++i)
				payload[i] = conf->pattern[i % conf->pattern_len];
		} else {
		
			memset(payload + ts_len, 0x42, rem);	
		}
	}

	icmp6->icmp6_cksum = icmp6_checksum(src, dst, packetBuf, icmp_len);
	return icmp_len
}
