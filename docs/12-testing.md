# Testing

## Build Matrix

| Environment | Expected Result |
|-------------|-----------------|
| Linux (x86_64, arm64) | Full compile, run with `sudo` |
| macOS | Compile fails (Linux headers: `struct icmphdr`, `IP_OPTIONS`, etc.) |
| WSL 2 (Ubuntu) | Full compile, run with `sudo` |
| Docker (ubuntu:latest) | Full compile, run as root |

> **Note**: macOS compilation fails because `netinet/ip_icmp.h` does not define
> `struct icmphdr` (uses `struct icmp`), and `IP_OPTIONS` / timestamp constants
> are Linux-specific. This is expected; the project targets Linux.

## Build Commands

```bash
# Clean build
make fclean && make

# Rebuild
make re

# Debug build (if needed)
CFLAGS="-Wall -Werror -Wextra -g" make re
```

## Privilege Requirements

- Raw sockets (`SOCK_RAW`) require `CAP_NET_RAW` or root.
- Run with `sudo`:
  ```bash
  sudo ./ft_ping -c 3 8.8.8.8
  ```
- Without `sudo`: `socket()` fails with `EPERM`; program prints
  `ft_ping: socket: Operation not permitted` and exits 1.

## Manual Test Catalog

### Mandatory (Subject Requirements)

| Test | Command | Expected |
|------|---------|----------|
| Basic IPv4 | `sudo ./ft_ping -c 3 8.8.8.8` | 3 replies, summary |
| Verbose | `sudo ./ft_ping -v -c 2 8.8.8.8` | Header with `id 0x...` |
| Help | `./ft_ping -?` / `-h` | Usage text, exit 0 |
| Missing host | `./ft_ping` | Error + hint, exit 64 |
| Unknown option | `./ft_ping -z` | Error + hint, exit 64 |

### IPv4 Options

| Test | Command | Expected |
|------|---------|----------|
| Count | `sudo ./ft_ping -c 4 127.0.0.1` | 4 sent, 4 received |
| TTL | `sudo ./ft_ping -t 1 -c 2 8.8.8.8` | "Time to live exceeded" |
| TTL verbose | `sudo ./ft_ping -t 1 -v -c 2 8.8.8.8` | Error + ICMP detail line |
| Size | `sudo ./ft_ping -s 100 -c 2 127.0.0.1` | 108 bytes from... |
| Pattern | `sudo ./ft_ping -p ff -c 2 127.0.0.1` | Payload filled with 0xff |
| Ignore routing | `sudo ./ft_ping -r -c 2 127.0.0.1` | Works on local |
| TOS | `sudo ./ft_ping -T 0x10 -c 2 127.0.0.1` | Sets IP_TOS |
| Timestamp | `sudo ./ft_ping --ip-timestamp=tsonly -c 2 127.0.0.1` | TS in reply (-v) |
| Deadline | `sudo ./ft_ping -w 2 192.0.2.1` | Stops ~2s, summary |
| Linger | `sudo ./ft_ping -W 2 -c 2 127.0.0.1` | 2s per-packet timeout |
| Quiet | `sudo ./ft_ping -q -c 3 8.8.8.8` | No reply lines, header+summary |
| Numeric | `sudo ./ft_ping -n -c 2 8.8.8.8` | Same as without -n |

### IPv6

| Test | Command | Expected |
|------|---------|----------|
| Loopback | `sudo ./ft_ping -6 -c 3 ::1` | 3 replies from ::1 |
| External | `sudo ./ft_ping -6 -c 2 2001:4860:4860::8888` | Google DNS |
| Verbose | `sudo ./ft_ping -6 -v -c 2 ::1` | Header with id |
| TTL | `sudo ./ft_ping -6 -t 1 -c 2 2001:4860:4860::8888` | Hop limit exceeded |
| Size | `sudo ./ft_ping -6 -s 100 -c 2 ::1` | 148 bytes from... |
| Pattern | `sudo ./ft_ping -6 -p aa -c 2 ::1` | Payload 0xaa |
| Deadline | `sudo ./ft_ping -6 -w 2 ::1` | Stops ~2s |

### Edge Cases

| Test | Command | Expected |
|------|---------|----------|
| Ctrl+C | `sudo ./ft_ping 8.8.8.8` then Ctrl+C | Summary, no leading blank line |
| No replies | `sudo ./ft_ping -c 3 192.0.2.1` | 3 transmitted, 0 received, 100% loss, exit 1 |
| Duplicate (loopback) | `sudo ./ft_ping -c 5 127.0.0.1` | Possible `(DUP!)` |
| Oversized packet | `sudo ./ft_ping -s 2000 -c 1 127.0.0.1` | "packet size too large" error |
| Unreachable host | `sudo ./ft_ping -c 2 192.0.2.1` | 100% loss, exit 1 |

## Evaluation Checklist (Mapped to Subject)

| Requirement | Test | Pass Criteria |
|-------------|------|---------------|
| Mandatory: IPv4 echo | `-c 3 8.8.8.8` | Replies + summary |
| Mandatory: `-v` | `-v -c 2` | Verbose header + error detail |
| Mandatory: `-?` | `-?` / `-h` | Usage text, exit 0 |
| Mandatory: FQDN input | `google.com` | Resolves + pings |
| Output indent | Compare with `inetutils-2.0 ping` | Identical (except RTT/rDNS) |
| RTT line exempt | Summary includes RTT line | Allowed to differ |
| Reverse DNS exempt | No reverse DNS in output | Allowed to differ |
| Bonus: `-W` | `-W 2` | 2s wait |
| Bonus: `-w` | `-w 3` | 3s deadline |
| Bonus: `-p` | `-p ff` | Pattern in payload |
| Bonus: `-r` | `-r` | SO_DONTROUTE set |
| Bonus: `-s` | `-s 100` | 100 data bytes |
| Bonus: `-T` (TOS) | `-T 0x10` | IP_TOS set |
| Bonus: `--ip-timestamp` | `--ip-timestamp=tsonly` | TS option + reply print |
| Bonus: `--ttl` | `--ttl 10` | TTL=10 |
| Extra: IPv6 | `-6` | Works |
| Extra: `--help`/`--usage` | Long options | Work |

## Automation Suggestion

For CI, run a subset that doesn't require network:

```bash
# Syntax + build only
make fclean && make

# Usage/help (no network)
./ft_ping -?
./ft_ping --help
./ft_ping --usage

# Error paths
./ft_ping                    # exit 64
./ft_ping -z                 # exit 64
./ft_ping -c 3               # exit 64 (missing host)
```

## Debugging Tips

- Add `-v` to see ICMP errors and detail lines.
- Use `strace -e trace=network ./ft_ping ...` to see syscalls.
- Use `tcpdump -i any icmp` or `tcpdump -i any icmp6` to capture wire traffic.
- Check `dmesg | tail` for kernel messages if socket options fail.

---

## See also

- [Troubleshooting](13-troubleshooting.md)
- [CLI Reference](08-cli-reference.md)
- [Architecture](07-architecture.md)