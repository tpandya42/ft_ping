# Documentation Index

This directory contains the complete technical documentation for `ft_ping`, organized for
linear reading from fundamentals to implementation details.

| # | Document | Description |
|---|----------|-------------|
| 00 | [Index](00-index.md) | This page |
| 01 | [Overview](01-overview.md) | Project goals, feature matrix, high-level flow, glossary |
| 02 | [Networking Fundamentals](02-networking-fundamentals.md) | TCP/IP stack, routing, MTU, ICMP's role, RTT/loss concepts |
| 03 | [IPv4](03-ipv4.md) | IPv4 header layout, TTL, IP options, Linux raw sockets |
| 04 | [IPv6](04-ipv6.md) | Addressing, header, extension headers, ICMPv6, Linux specifics |
| 05 | [ICMP Protocol](05-icmp.md) | ICMPv4/v6 message formats, quoted-packet layout, error mappings |
| 06 | [Checksum Algorithm](06-checksum.md) | One's complement math, implementation walkthrough, RFC 1071 |
| 07 | [Architecture](07-architecture.md) | Module map, data structures, control flow, signal handling |
| 08 | [CLI Reference](08-cli-reference.md) | Every option: syntax, semantics, defaults, deviations |
| 09 | [Output Reference](09-output-reference.md) | Exact formats, reference comparison, worked examples |
| 10 | [Timing & Signals](10-timing-and-signals.md) | RTT method, cadence, -w/-W, SIGINT, retransmission removal |
| 11 | [DNS Resolution](11-dns-resolution.md) | getaddrinfo, v4/v6 paths, forward DNS, numeric mode |
| 12 | [Testing](12-testing.md) | Build matrix, test catalog, evaluation checklist |
| 13 | [Troubleshooting](13-troubleshooting.md) | Common errors and fixes |
| 14 | [References](14-references.md) | RFCs, inetutils-2.0 source, subject; links to `resources.md` |

---

## Reading order

**For evaluators**: 00 → 01 → 08 → 09 → 12 → 13  
**For implementers/maintainers**: 00 → 01 → 02 → 03 → 04 → 05 → 06 → 07 → 08 → 09 → 10 → 11 → 12 → 13 → 14

---

## See also

- [README.md](../README.md) — Quick start and build instructions
- [docs/resources.md](resources.md) — External learning resources (kept from original)