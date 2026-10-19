# ft_ping

A from-scratch implementation of `ping` in C using Linux raw sockets,
built for the 42 Berlin curriculum with inetutils-2.0 as the behavioral reference.

## Features

| Category | Features |
|----------|----------|
| **Mandatory** | IPv4 echo, `-v` verbose, `-?` help, FQDN input |
| **Bonus** | `-w` deadline, `-W` timeout, `-p` pattern, `-r` ignore-routing, `-s` size, `-T` TOS, `--ip-timestamp`, `--ttl`, `-q` quiet, `-n` numeric |
| **Extra** | IPv6 (`-6`), long options (`--help`, `--usage`, `--ttl`, `--ip-timestamp`) |

## Requirements

- Linux (tested on Ubuntu 22.04+, kernel 5.15+)
- `gcc` or `clang` with C99 + GNU extensions
- `make` (GNU Make)
- **Root / `CAP_NET_RAW`** for raw sockets

> macOS cannot compile (Linux-specific headers: `struct icmphdr`, `IP_OPTIONS`, timestamp constants).

## Build

```bash
make            # Build ft_ping
make re         # Clean rebuild
make clean      # Remove objects
make fclean     # Remove objects + binary
```

## Quick Tests

```bash
# Basic IPv4
sudo ./ft_ping -c 3 8.8.8.8

# Verbose (shows ID in header)
sudo ./ft_ping -v -c 2 8.8.8.8

# TTL exceeded (forces ICMP error)
sudo ./ft_ping -t 1 -c 2 8.8.8.8
sudo ./ft_ping -t 1 -v -c 2 8.8.8.8   # with ICMP detail line

# IPv6
sudo ./ft_ping -6 -c 3 ::1
sudo ./ft_ping -6 -c 2 2001:4860:4860::8888

# Bonus options
sudo ./ft_ping -p ff -c 2 127.0.0.1     # pattern
sudo ./ft_ping -r -c 2 127.0.0.1        # ignore routing
sudo ./ft_ping -w 3 192.0.2.1           # deadline (3s)
sudo ./ft_ping -W 2 -c 2 127.0.0.1      # 2s per-packet timeout
sudo ./ft_ping --ttl 10 -c 2 8.8.8.8    # long option
sudo ./ft_ping -T 0x10 -c 2 127.0.0.1   # TOS
sudo ./ft_ping --ip-timestamp=tsonly -c 2 127.0.0.1  # timestamp option

# Help
./ft_ping -?
./ft_ping --help
./ft_ping --usage
```

## Options Summary

| Short | Long | Arg | Description |
|-------|------|-----|-------------|
| `-v` | `--verbose` | — | Verbose output |
| `-?` | `--help` | — | Full usage |
| | `--usage` | — | Short usage |
| `-6` | — | — | IPv6 mode |
| `-c` | `--count` | NUM | Stop after NUM packets |
| `-t` | `--ttl` | N | TTL / Hop Limit |
| `-T` | `--tos` | NUM | Type of Service (IPv4) |
| | `--ip-timestamp` | FLAG | `tsonly` or `tsaddr` |
| `-s` | `--size` | NUM | Data bytes |
| `-p` | `--pattern` | HEX | Payload fill byte |
| `-r` | `--ignore-routing` | — | `SO_DONTROUTE` |
| `-w` | `--timeout` | SEC | Deadline |
| `-W` | `--linger` | SEC | Per-packet timeout |
| `-q` | `--quiet` | — | Suppress replies |
| `-n` | `--numeric` | — | Numeric only |

## Output Format (inetutils-2.0 compatible)

```
PING google.com (142.250.185.206): 56 data bytes
64 bytes from 142.250.185.206: icmp_seq=0 ttl=117 time=12.345 ms
64 bytes from 142.250.185.206: icmp_seq=1 ttl=117 time=13.102 ms
^C
--- google.com ping statistics ---
3 packets transmitted, 3 received, 0% packet loss
round-trip min/avg/max/stddev = 12.345/12.723/13.102/0.378 ms
```

Key points:
- Header: `PING host (ip): 56 data bytes` (+ `, id 0xXXXX = NNNN` with `-v`)
- Reply: `N bytes from ip: icmp_seq=X ttl=Y time=Z ms` (+ `(DUP!)` if duplicate)
- Errors: `N bytes from ip: <message>` (+ detail line with `-v`)
- Summary: no leading blank line, no `time Xms`, inline `+N duplicates,`
- RTT line: `round-trip min/avg/max/stddev = ...` (exempt per subject)

## Project Structure

```
ft_ping/
├── Makefile
├── README.md
├── en.subject.pdf
├── docs/           # Detailed documentation
│   ├── 00-index.md … 14-references.md
│   └── resources.md
├── includes/
│   └── ft_ping.h
└── srcs/
    ├── main.c
    ├── parse.c
    ├── dns.c
    ├── icmp.c
    ├── socket.c
    ├── ping4.c
    └── ping6.c
```

## Documentation

See [`docs/00-index.md`](docs/00-index.md) for the complete documentation index:

- Networking fundamentals, IPv4, IPv6, ICMP
- Checksum algorithm, architecture, CLI reference
- Output format, timing, DNS, testing, troubleshooting
- Standards references

## Testing on Linux

```bash
# Build
make re

# Run test suite manually (requires sudo for raw sockets)
sudo ./ft_ping -c 3 8.8.8.8
sudo ./ft_ping -v -c 2 8.8.8.8
sudo ./ft_ping -t 1 -v -c 2 8.8.8.8
sudo ./ft_ping -6 -c 3 ::1
./ft_ping -?
```

## Reference

Behavioral reference: **inetutils-2.0 `ping`**  
Source: <https://git.savannah.gnu.org/cgit/inetutils.git/tree/ping?h=v2.0>

Subject: 42 Berlin `ft_ping` (mandatory IPv4 + `-v` + `-?`; IPv6 extra)