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
size_t	buildEchoPacket(t_config *conf, uint8_t packet, int packetSize, uint16_t seq, int packet_size, uint16_t pid, struct timespec st_time)
{ 
	size_t icmp_len = sizeof(struct icmphdr) + packetSize;
	if (packetSize < icmp_len)
		return 0;
	struct icmphdr *hdr = (struct icmphdr *)packet;	
	hdr.type = ICMP_ECHO; // 8
	hdr.code = 0;
	hdr.un.echo.id = htons(pid); // our pid in network byte
	hdr.un.echo.sequence = htons(seq);
	hdr.checksum = 0;

	uint8_t payload = packet + sizeof(struct icmphdr);
	if (packetSize >= sizeof(timeval) && st_time)
	{
		memcpy(payload, &st_time, sizeof(st_time));
		memset(payload + sizeof(st_time), 0x42, packetSize - sizeof(st_time));
	}
	else
	{
		memset(payload, 0x42, packetSize - sizeof(st_time));
	}
	hdr.checksum = calculateChecksum(packet, icmp_len);
	return icmp_len;
}
