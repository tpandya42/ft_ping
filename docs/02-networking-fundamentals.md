# Networking Fundamentals

## TCP/IP Stack and Where ICMP Lives

```
+------------------+     Application layer (ping, traceroute, etc.)
|   Application    |
+------------------+
|   Transport      |     TCP (streams), UDP (datagrams), ICMP (control)
+------------------+
|    Internet      |     IPv4 / IPv6 — addressing, routing, fragmentation
+------------------+
|   Link Layer     |     Ethernet, Wi-Fi, etc.
+------------------+
```

ICMP (Internet Control Message Protocol) is a **control protocol** that lives
at the Internet layer alongside IP. It does not carry user data; it reports
errors, performs diagnostics, and assists path MTU discovery.

Key RFCs:
- RFC 791 — Internet Protocol (IPv4)
- RFC 792 — Internet Control Message Protocol (ICMPv4)
- RFC 2460 — Internet Protocol, Version 6 (IPv6)
- RFC 4443 — ICMPv6

## IP Addressing and Routing (Brief)

- **IPv4**: 32-bit addresses, typically written `a.b.c.d`. Classes are historical;
  CIDR (`prefix/length`) is used today.
- **IPv6**: 128-bit addresses, eight groups of four hex digits. Scopes:
  link-local (`fe80::/10`), unique local (`fc00::/7`), global.
- **Routing**: Hosts consult their routing table; default gateway handles
  off-subnet destinations. Routers decrement TTL/Hop Limit and forward.

## MTU and Fragmentation

**MTU (Maximum Transmission Unit)** — largest packet a link-layer frame can carry.
Common MTUs: 1500 (Ethernet), 1492 (PPPoE), 1280 (IPv6 minimum).

If a packet exceeds the next-hop MTU and the **Don't Fragment (DF)** bit is
clear (IPv4), the router fragments it. If DF is set, the router drops the
packet and sends **ICMP Destination Unreachable / Fragmentation Needed** (type
3, code 4) containing the next-hop MTU. This is **Path MTU Discovery (PMTUD)**.

IPv6 routers never fragment; the source must perform PMTUD (RFC 8201).

## ICMP's Role

ICMP messages are encapsulated in IP packets (`protocol = 1` for v4,
`next-header = 58` for v6). They serve:

- **Echo Request / Reply** — reachability and RTT measurement (`ping`).
- **Destination Unreachable** — network, host, protocol, port, fragmentation.
- **Time Exceeded** — TTL/Hop Limit expired in transit, fragment reassembly timeout.
- **Redirect** — router suggests a better next hop.
- **Parameter Problem** — malformed header.

## RTT and Packet Loss

- **RTT (Round-Trip Time)**: `t_reply_received - t_request_sent`. Measured at
  the sender using a high-resolution monotonic clock.
- **Packet Loss**: `((sent - received) / sent) * 100%`. Duplicates are counted
  separately and do not reduce loss.
- **Duplicates**: Same sequence number received more than once (network
  duplication or retransmission artifact).

## How ping Works (Conceptual)

1. Build an ICMP Echo Request with identifier (typically PID) and sequence.
2. Include a timestamp in the payload for RTT calculation.
3. Send via raw socket (kernel adds IP header).
4. Wait for reply on the same socket (kernel delivers matching ICMP packets).
5. On Echo Reply: verify ID/seq, extract timestamp, compute RTT, print.
6. On ICMP Error: parse quoted original packet to identify our request, print.
7. Repeat at ~1s interval until count reached or interrupted.
8. Print summary: transmitted, received, loss%, RTT min/avg/max/stddev.

---

## See also

- [IPv4](03-ipv4.md)
- [IPv6](04-ipv6.md)
- [ICMP Protocol](05-icmp.md)
- [Timing & Signals](10-timing-and-signals.md)