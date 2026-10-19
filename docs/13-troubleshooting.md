# Troubleshooting

## Common Errors

### `ft_ping: socket: Operation not permitted`

**Cause**: Raw sockets require `CAP_NET_RAW` or root privileges.
**Fix**: Run with `sudo`:
```bash
sudo ./ft_ping -c 3 8.8.8.8
```
Or grant capability:
```bash
sudo setcap cap_net_raw+ep ./ft_ping
```

### `ft_ping: unknown host <name>`

**Cause**: `getaddrinfo()` failed to resolve the hostname.
**Possible reasons**:
- Hostname typo.
- No network connectivity.
- DNS server unreachable.
- Hostname has no A/AAAA record.

**Debug**:
```bash
getent ahosts <name>     # Check resolution
ping <name>              # System ping (uses same resolver)
```

### `ft_ping: no IPv4/IPv6 address for <name>`

**Cause**: DNS resolved the name but returned no address of the requested
family (e.g., `-6` given but host has only A record).

**Fix**: Use correct family or check DNS records:
```bash
dig <name> A      # IPv4
dig <name> AAAA   # IPv6
```

### `ft_ping: -s <N>: packet size too large`

**Cause**: Requested packet size exceeds internal buffer
(`sizeof(struct icmphdr) + packet_size > 1500 + header`).

**Fix**: Reduce `-s` value. Maximum data payload ≈ 1500 - 8 (ICMP) - 20 (IP) = 1472
bytes for IPv4 on Ethernet. For IPv6: 1500 - 8 - 40 = 1452.

### `ft_ping: sendto: Network is unreachable`

**Cause**: No route to destination (no default gateway, interface down,
wrong interface for link-local IPv6).

**Debug**:
```bash
ip route get <ip>           # Check routing
ip -6 route get <ipv6>      # IPv6
ping -I <iface> <ipv6>      # Specify interface for link-local
```

### `ft_ping: sendto: Invalid argument`

**Cause**: Invalid socket option value (e.g., TTL > 255, TOS out of range).

**Fix**: Check option values:
- TTL/Hop Limit: 1–255
- TOS: 0–255
- Packet size: within buffer limits

### `ft_ping: timeout, retransmitting seq=...` (not in current code)

If you see this, you are running an older version. Current code has no
retransmission; it moves to the next sequence after `SO_RCVTIMEO` expires.

### No output at all (hangs)

**Cause**: Waiting for reply that never arrives (firewall, ICMP blocked,
wrong IP version).

**Debug**:
```bash
sudo tcpdump -i any icmp      # See if packets leave/arrive
sudo ./ft_ping -v -c 1 <ip>   # Verbose shows errors
```

### `./ft_ping -?` prints nothing / exits immediately

**Cause**: Older version. Current code prints full usage and exits 0.

### IPv6 link-local fails

```bash
sudo ./ft_ping -6 fe80::1
# ft_ping: sendto: Invalid argument (or Network unreachable)
```

**Cause**: Link-local addresses require interface scope (`%iface`).
**Fix**: `ft_ping` does not yet support `-I` interface option. Use global
IPv6 or system `ping6 -I eth0 fe80::1`.

### macOS Compilation Errors

```
srcs/icmp.c:65:20: error: invalid application of 'sizeof' to an incomplete type 'struct icmphdr'
```

**Cause**: macOS uses `struct icmp` (from `netinet/ip_icmp.h`) with different
layout; `struct icmphdr` is Linux-specific. Also `IP_OPTIONS`, `IPOPT_*`
constants missing.

**Resolution**: This project targets Linux only. Build and test on Linux VM,
WSL 2, or Docker.

## VM / Container Specific

### WSL 2 (Windows Subsystem for Linux)

- Works natively; `sudo` works.
- Windows firewall may block ICMP; allow in Windows Defender.

### Docker

```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y build-essential
WORKDIR /ft_ping
COPY . .
RUN make
# Run as root (container default)
CMD ["./ft_ping", "-c", "3", "8.8.8.8"]
```

### VM Network Modes

| Mode | ICMP Behavior |
|------|---------------|
| NAT (default) | Host ↔ VM works; external may work |
| Bridged | VM on LAN; full ICMP |
| Host-only | VM ↔ Host only |

## Signal Artifacts

### `^C` Printed Twice

Terminal echoes `^C`; program prints summary on next line. Normal.

### Summary Appears Without Blank Line

Expected per inetutils-2.0 format (no leading `\n` before `---`).

### Exit Code 1 on Success?

Only if **zero** replies received. Check `logs.rec` in summary.

## Debugging Checklist

1. **Run with `-v`** — see errors and detail lines.
2. **Check `dmesg`** — kernel may log dropped packets or socket errors.
3. **Verify socket options** — `strace -e setsockopt,sendto,recvmsg ./ft_ping ...`
4. **Capture packets** — `tcpdump -n -i any icmp or icmp6`
5. **Test locally first** — `sudo ./ft_ping -c 3 127.0.0.1` / `::1`

---

## See also

- [Testing](12-testing.md)
- [CLI Reference](08-cli-reference.md)
- [Architecture](07-architecture.md)