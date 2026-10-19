# ICMP Protocol

## ICMPv4 Message Formats

All ICMPv4 messages share a common header:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     Type      |     Code      |           Checksum            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Message Body                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Echo Request (Type 8, Code 0) / Echo Reply (Type 0, Code 0)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     Type      |     Code      |           Checksum            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Identifier          |        Sequence Number        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Data (optional)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- **Identifier**: typically process ID (PID); allows demultiplexing concurrent pings.
- **Sequence Number**: incremented per echo; used to match replies.
- **Data**: payload; `ft_ping` puts a `struct timespec` at the start for RTT.

### Destination Unreachable (Type 3)

| Code | Meaning |
|------|---------|
| 0 | Net Unreachable |
| 1 | Host Unreachable |
| 2 | Protocol Unreachable |
| 3 | Port Unreachable |
| 4 | Fragmentation Needed and DF Set (includes MTU) |
| 5 | Source Route Failed |
| 6 | Destination Network Unknown |
| 7 | Destination Host Unknown |
| 8 | Source Host Isolated |
| 9 | Network Administratively Prohibited |
| 10 | Host Administratively Prohibited |
| 11 | Network Unreachable for TOS |
| 12 | Host Unreachable for TOS |
| 13 | Communication Administratively Prohibited |
| 14 | Host Precedence Violation |
| 15 | Precedence Cutoff |

### Time Exceeded (Type 11)

| Code | Meaning |
|------|---------|
| 0 | TTL Exceeded in Transit |
| 1 | Fragment Reassembly Time Exceeded |

### Redirect (Type 5)

| Code | Meaning |
|------|---------|
| 0 | Redirect for Network |
| 1 | Redirect for Host |
| 2 | Redirect for TOS and Network |
| 3 | Redirect for TOS and Host |

### Parameter Problem (Type 12)

| Code | Meaning |
|------|---------|
| 0 | Pointer indicates the error |
| 1 | Missing a required option |
| 2 | Bad length |

### Other Types

| Type | Name |
|------|------|
| 0 | Echo Reply |
| 4 | Source Quench (deprecated) |
| 8 | Echo Request |
| 9 | Router Advertisement |
| 10 | Router Solicitation |
| 13 | Timestamp Request |
| 14 | Timestamp Reply |
| 15 | Information Request (obsolete) |
| 16 | Information Reply (obsolete) |
| 17 | Address Mask Request |
| 18 | Address Mask Reply |

## ICMPv6 Message Formats

See [IPv6](04-ipv6.md) for the base header. Message bodies:

### Echo Request (Type 128) / Echo Reply (Type 129)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     Type      |     Code      |           Checksum            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Identifier          |        Sequence Number        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Data (optional)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

Identical semantics to v4 (identifier, sequence, data).

### Destination Unreachable (Type 1)

| Code | Meaning |
|------|---------|
| 0 | No route to destination |
| 1 | Administratively prohibited |
| 2 | Beyond scope of source address |
| 3 | Address unreachable |
| 4 | Port unreachable |
| 5 | Source address failed ingress/egress policy |
| 6 | Reject route to destination |

### Packet Too Big (Type 2, Code 0)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     Type=2    |     Code=0    |           Checksum            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                             MTU                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

Used for Path MTU Discovery (IPv6 routers never fragment).

### Time Exceeded (Type 3)

| Code | Meaning |
|------|---------|
| 0 | Hop Limit Exceeded in Transit |
| 1 | Fragment Reassembly Time Exceeded |

### Parameter Problem (Type 4)

| Code | Meaning |
|------|---------|
| 0 | Erroneous header field |
| 1 | Unrecognized Next Header |
| 2 | Unrecognized IPv6 option |

## Quoted Packet Layout in ICMP Errors

When a router or host generates an ICMP error, it includes the **original
packet that triggered the error** in the ICMP payload.

### IPv4 Error Payload Layout

```
Outer IP header (20+ bytes)
Outer ICMP header (8 bytes) — type 3, 11, 5, 12, etc.
Inner IP header (20+ bytes) — original packet's IP header
Inner ICMP header (8 bytes) — original ICMP header (type 8, our id/seq)
Inner payload — our timestamp + pattern
```

Total quoted portion: typically 28 bytes (IPv4 hdr 20 + ICMP hdr 8) minimum,
up to 576 bytes per RFC 792.

### IPv6 Error Payload Layout

```
Outer IPv6 header (40 bytes) — not delivered to raw socket
Outer ICMPv6 header (8 bytes) — error type (1, 2, 3, 4)
Inner IPv6 header (40 bytes) — original packet's IPv6 header
Inner ICMPv6 header (8 bytes) — our echo request (type 128, our id/seq)
Inner payload — our timestamp + pattern
```

**Linux raw ICMPv6 sockets** do **not** deliver the outer IPv6 header.
The received buffer starts at the **outer ICMPv6 header**.

## ft_ping Error Parsing

### `parseSeqifOur()` (IPv4) — `srcs/ping4.c`

1. Locate outer ICMP header: `buf + outer_iphdr_len`.
2. Inner IP header follows outer ICMP: `outer_icmp + sizeof(struct icmphdr)`.
3. Verify inner IP header fits; `inner_iphdr->ihl * 4`.
4. Inner ICMP: `inner_ip + inner_ip_hl`.
5. Check `inner_icmp->type == ICMP_ECHO && inner_icmp->un.echo.id == pid`.
6. Return `true` with `*ourSeq = ntohs(inner_icmp->un.echo.sequence)`.

### `parseSeqifOur6()` (IPv6) — `srcs/ping6.c`

1. Buffer starts at outer ICMPv6 header.
2. Inner IPv6: `buf + sizeof(struct icmp6_hdr)`.
3. Verify `inner_ip6->ip6_nxt == IPPROTO_ICMPV6`.
4. Inner ICMPv6: `inner_ip + sizeof(struct ip6_hdr)`.
5. Check `inner_icmp6->icmp6_type == ICMP6_ECHO_REQUEST &&
   inner_icmp6->icmp6_id == pid`.
6. Return `true` with `*ourSeq = ntohs(inner_icmp6->icmp6_seq)`.

## Error Text Mapping

### `icmp_desc()` (IPv4) — `srcs/ping4.c`

Maps `(type, code)` to human-readable strings matching inetutils-2.0:

```c
ICMP_DEST_UNREACH, 0 -> "Destination Net Unreachable"
ICMP_DEST_UNREACH, 1 -> "Destination Host Unreachable"
ICMP_DEST_UNREACH, 2 -> "Destination Protocol Unreachable"
ICMP_DEST_UNREACH, 3 -> "Destination Port Unreachable"
ICMP_DEST_UNREACH, 4 -> "Fragmentation needed and DF set"
ICMP_TIME_EXCEEDED, 0 -> "Time to live exceeded"
ICMP_TIME_EXCEEDED, 1 -> "Frag reassembly time exceeded"
ICMP_REDIRECT, *   -> "Redirect"
ICMP_PARAMETERPROB, * -> "Parameter Problem"
```

### `icmp6_desc()` (IPv6) — `srcs/ping6.c`

Maps `(type, code)`:

```c
ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_NOROUTE  -> "Destination Unreachable: No route to destination"
ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_ADMIN   -> "Destination Unreachable: Administratively prohibited"
ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_BEYOND  -> "Destination Unreachable: Beyond scope of source address"
ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_ADDR    -> "Destination Unreachable: Address unreachable"
ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_NOPORT  -> "Destination Unreachable: Port unreachable"
ICMP6_TIME_EXCEEDED, ICMP6_TIME_EXCEED_TRANSIT -> "Time Exceeded: Hop limit exceeded in transit"
ICMP6_TIME_EXCEEDED, ICMP6_TIME_EXCEED_REASSEMBLY -> "Time Exceeded: Fragment reassembly time exceeded"
ICMP6_PACKET_TOO_BIG -> "Packet Too Big"
ICMP6_PARAM_PROB -> "Parameter Problem"
```

## ft_ping Output for Errors

When an ICMP error quoting our packet arrives (or `-v` for any error):

```
%zu bytes from %s: %s
```

With `-v` and if the error quotes our packet, an additional detail line:

```
ICMP: type %u, code %u, size %u, id 0x%04x, seq 0x%04x
```

- **size**: quoted ICMP payload length (inner IP total length - inner IP header length for v4; inner IPv6 payload length for v6).
- **id/seq**: from the inner ICMP header (our identifier and sequence).

---

## See also

- [IPv4](03-ipv4.md)
- [IPv6](04-ipv6.md)
- [Architecture — error handling](07-architecture.md)
- [Output Reference](09-output-reference.md)