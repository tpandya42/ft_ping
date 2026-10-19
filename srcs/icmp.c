#include "ft_ping.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* We gonna use 16 bit one's complement addition thingy to calculate cchecksum
 */

static uint32_t checksumSum(const void *data, size_t len, uint32_t sum)
{
	const uint16_t *words = (const uint16_t *)data;
	while (len > 1)
	{
		sum += *words++;
		len -= 2;
	}
	if (len == 1) {
		uint16_t odd_byte = 0;
		*((uint8_t *)&odd_byte) = *(const uint8_t *)words;
		sum += odd_byte;
	}
	return sum;
}

static uint16_t checksumFold(uint32_t sum)
{
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return (uint16_t)(~sum);
}

uint16_t calculateChecksum(const void *data, size_t l)
{
	return checksumFold(checksumSum(data, l, 0));
}

/* IPv6 pseudo-header for ICMPv6 checksum (RFC 2460 section 8.1):
 *   src (16) + dst (16) + length (4) + zero[3] (3) + next_header (1) = 40 bytes
 * Note: Linux kernel automatically computes/overwrites the ICMPv6 checksum
 * for raw ICMPv6 sockets (rawv6_init_sk sets checksum=1, offset=2).
 * We compute it anyway so the packet is correct on its own. */
static uint16_t icmp6_checksum(const struct in6_addr *src, const struct in6_addr *dst,
                               const uint8_t *icmp, size_t icmp_len)
{
	struct {
		struct in6_addr src;
		struct in6_addr dst;
		uint32_t        len;
		uint8_t         zero[3];
		uint8_t         next;
	} pseudo = {
		.src = *src,
		.dst = *dst,
		.len = htonl(icmp_len),
		.zero = {0, 0, 0},
		.next = IPPROTO_ICMPV6
	};
	uint32_t sum = checksumSum(&pseudo, sizeof(pseudo), 0);
	sum = checksumSum(icmp, icmp_len, sum);
	return checksumFold(sum);
}

size_t	buildEchoPacket(uint8_t *packet, int packetSize, uint16_t seq, uint16_t pid, struct timespec *st_time, uint8_t pattern)
{ 
	size_t icmp_len = sizeof(struct icmphdr) + packetSize;
	if (packetSize < 0 || (size_t)packetSize > 65535)
		return 0;
	struct icmphdr *hdr = (struct icmphdr *)packet;	
	hdr->type = ICMP_ECHO;
	hdr->code = 0;
	hdr->un.echo.id = htons(pid);
	hdr->un.echo.sequence = htons(seq);
	hdr->checksum = 0;

	uint8_t *payload = packet + sizeof(struct icmphdr);
	if (st_time && packetSize >= (int)sizeof(struct timespec))
	{
		memcpy(payload, st_time, sizeof(struct timespec));
		memset(payload + sizeof(struct timespec), pattern, packetSize - sizeof(struct timespec));
	}
	else
	{
		memset(payload, pattern, packetSize);
	}
	hdr->checksum = calculateChecksum(packet, icmp_len);
	return icmp_len;
}

size_t	buildEchoV6Packet(uint8_t *packet, int packetSize, uint16_t seq, uint16_t pid,
                          struct timespec *st_time, const struct in6_addr *src, const struct in6_addr *dst, uint8_t pattern)
{ 
	size_t icmp_len = sizeof(struct icmp6_hdr) + packetSize;
	if (packetSize < 0 || (size_t)packetSize > 65535)
		return 0;
	struct icmp6_hdr *hdr = (struct icmp6_hdr *)packet;
	hdr->icmp6_type = ICMP6_ECHO_REQUEST;
	hdr->icmp6_code = 0;
	hdr->icmp6_cksum = 0;
	hdr->icmp6_id = htons(pid);
	hdr->icmp6_seq = htons(seq);

	uint8_t *payload = packet + sizeof(struct icmp6_hdr);
	if (st_time && packetSize >= (int)sizeof(struct timespec))
	{
		memcpy(payload, st_time, sizeof(struct timespec));
		memset(payload + sizeof(struct timespec), pattern, packetSize - sizeof(struct timespec));
	}
	else
	{
		memset(payload, pattern, packetSize);
	}
	hdr->icmp6_cksum = icmp6_checksum(src, dst, packet, icmp_len);
	return icmp_len;
}