# References

## RFCs (Normative)

| RFC | Title | Relevance |
|-----|-------|-----------|
| RFC 791 | Internet Protocol (IPv4) | IPv4 header, TTL, fragmentation, options |
| RFC 792 | Internet Control Message Protocol (ICMPv4) | Echo request/reply, error messages |
| RFC 950 | Internet Standard Subnetting Procedure | Historical, CIDR precursor |
| RFC 1071 | Computing the Internet Checksum | One's complement algorithm |
| RFC 1141 | Incremental Updating of the Internet Checksum | Checksum optimization |
| RFC 1812 | Requirements for IP Version 4 Routers | TTL, ICMP errors, PMTUD |
| RFC 2460 | Internet Protocol, Version 6 (IPv6) | IPv6 header, extension headers, pseudo-header |
| RFC 4443 | Internet Control Message Protocol (ICMPv6) | ICMPv6 types, errors, echo, PMTUD |
| RFC 4884 | Extended ICMP to Support Multi-Part Messages | Modern ICMP extensions |
| RFC 781 | Specification of the Internet Protocol (IP) Timestamp Option | IP timestamp option format |
| RFC 1122 | Requirements for Internet Hosts — Communication Layers | ICMP processing rules |
| RFC 1123 | Requirements for Internet Hosts — Application and Support | Ping implementation notes |

## Standards & Specifications

- **POSIX.1-2008** — `getaddrinfo`, `socket`, `sendto`, `recvmsg`, `clock_gettime`
- **Linux man pages** — `socket(7)`, `raw(7)`, `ip(7)`, `ipv6(7)`, `icmp(7)`, `setsockopt(2)`
- **inetutils-2.0** — Behavioral reference (GNU inetutils `ping`)

## Source Code References

### ft_ping (this project)

- `includes/ft_ping.h` — Data structures, prototypes
- `srcs/parse.c` — Option parsing, usage text
- `srcs/dns.c` — IPv4/IPv6 resolution
- `srcs/icmp.c` — Checksum, packet building
- `srcs/socket.c` — Socket creation, options
- `srcs/ping4.c` — IPv4 loop, printing, stats
- `srcs/ping6.c` — IPv6 loop, printing, stats
- `srcs/main.c` — Entry point, dispatch
- `Makefile` — Build rules

### inetutils-2.0 (Behavioral Reference)

Source: <https://git.savannah.gnu.org/cgit/inetutils.git/tree/ping?h=v2.0>

Key files:
- `ping/ping.c` — Main, argp options, `ping_run`, `ping_finish`
- `ping/ping_echo.c` — Echo logic, `print_echo`, `print_icmp_header`, `print_ip_opt`
- `ping/ping_common.c` — Shared utilities, `print_ip_header`, `ping_cvt_time`
- `ping/libping.c` — Packet send/receive, sequence management

### Linux Kernel (Socket Behavior)

- `net/ipv4/af_inet.c`, `net/ipv4/raw.c` — IPv4 raw sockets
- `net/ipv6/af_inet6.c`, `net/ipv6/raw.c` — IPv6 raw sockets (`rawv6_init_sk`)
- `net/ipv4/ip_options.c` — IP options (timestamp, record route)
- `include/uapi/linux/icmp.h` — `struct icmphdr`
- `include/uapi/linux/icmpv6.h` — `struct icmp6_hdr`

## External Learning Resources

See also [`docs/resources.md`](resources.md) (kept from original project):

- IPv4 vs IPv6 header differences: <https://www.computernetworkingnotes.com/networking-tutorials/differences-between-the-ipv4-header-and-ipv6-header.html>
- IPv6 overview: <https://manpages.org/ipv6/7>, <https://learn.microsoft.com/en-us/dotnet/fundamentals/networking/ipv6-overview>, <https://en.wikipedia.org/wiki/IPv6>

## Additional References

- **TCP/IP Illustrated, Volume 1** (Stevens) — Chapters on IP, ICMP, Ping
- **UNIX Network Programming, Volume 1** (Stevens, Fenner, Rudoff) — Raw sockets
- **Linux Kernel Networking** (Rami Rosen) — Raw socket internals

## Version Information

- **Project**: ft_ping (42 Berlin)
- **Reference**: inetutils-2.0 (`ping` from GNU inetutils 2.0)
- **Target OS**: Linux (tested on Ubuntu 22.04, kernel 5.15+)
- **Language**: C99 (GNU extensions via `_GNU_SOURCE`)
- **Build**: `make` (GNU Make), `cc` (GCC/Clang)

## License

This project is a 42 school exercise. No external license applies to the
student code. Referenced standards (RFCs) are public documents. inetutils is
GPL-3.0+.

---

## See also

- [docs/resources.md](resources.md)
- [ICMP Protocol](05-icmp.md)
- [Checksum Algorithm](06-checksum.md)