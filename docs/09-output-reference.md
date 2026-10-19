# Output Reference

## Format Strings (Exact)

All output uses `printf` with the exact format strings below. No extra spaces
or newlines unless shown.

### Header (IPv4 & IPv6)

```c
// Without -v
printf("PING %s (%s): %d data bytes\n", hostname, ip_str, packet_size);

// With -v
printf("PING %s (%s): %d data bytes, id 0x%04x = %u\n",
       hostname, ip_str, packet_size, pid, pid);
```

**Representative example:**

```
PING google.com (142.250.185.206): 56 data bytes
PING google.com (142.250.185.206): 56 data bytes, id 0x1234 = 4660
```

### Reply (Echo Reply)

```c
printf("%zu bytes from %s: icmp_seq=%u ttl=%u time=%.3f ms",
       bytes, from_addr, seq, ttl, rtt_ms);
if (duplicate)
    printf(" (DUP!)");
printf("\n");
```

**Representative example:**

```
64 bytes from 142.250.185.206: icmp_seq=0 ttl=117 time=12.345 ms
64 bytes from 142.250.185.206: icmp_seq=5 ttl=117 time=13.102 ms (DUP!)
```

**Notes:**
- `bytes` = received length - IP header length (typically 64 for default 56).
- `ttl` = received TTL (v4) or Hop Limit (v6).
- `(DUP!)` printed **always** on duplicates (not gated by `-v`).
- IPv6: `ttl` field shows Hop Limit from `IPV6_RECVHOPLIMIT` cmsg.

### Timestamp Option Reply (IPv4, `-v` with `--ip-timestamp`)

After the reply line, `print_ip_opt()` prints collected timestamps:

```
TS:	12345 ms
	12356 ms
	12367 ms	(2 overflowing hosts)
```

Format per `print_ip_opt()` (ported from inetutils-2.0):
- `tsonly` (flag 0): timestamps only.
- `tsaddr` (flag 3): address + timestamp per hop.

### ICMP Error (Our Packet)

```c
printf("%zu bytes from %s: %s\n", bytes, from_addr, error_msg);
```

**Representative example:**

```
56 bytes from 192.0.2.1: Time to live exceeded
```

### ICMP Error (Verbose + Our Packet) — Extra Detail Line

```c
printf("ICMP: type %u, code %u, size %u, id 0x%04x, seq 0x%04x\n",
       inner_type, inner_code, size, inner_id, inner_seq);
```

**Representative example:**

```
56 bytes from 192.0.2.1: Time to live exceeded
ICMP: type 8, code 0, size 64, id 0x1234, seq 0x0005
```

**Fields:**
- `type/code`: inner ICMP header (our Echo Request = 8/0 or 128/0).
- `size`: inner payload length (inner IP total len - inner header len).
- `id/seq`: our identifier and sequence from inner ICMP header.

### ICMP Error (Verbose, Not Our Packet)

Only the first line is printed (no detail line):

```
56 bytes from 192.0.2.1: Destination Host Unreachable
```

### Summary (End of Run or SIGINT)

```c
printf("--- %s ping statistics ---\n", hostname);
if (duplicates > 0)
    printf("%d packets transmitted, %d received, +%u duplicates, %d%% packet loss\n",
           sent, rec, duplicates, loss);
else
    printf("%d packets transmitted, %d received, %d%% packet loss\n",
           sent, rec, loss);
if (rec > 0) {
    printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
           rtt_min, avg, rtt_max, mdev);
}
```

**Representative example (no duplicates):**

```
--- google.com ping statistics ---
4 packets transmitted, 4 received, 0% packet loss
round-trip min/avg/max/stddev = 11.234/12.567/14.890/1.234 ms
```

**Representative example (with duplicates):**

```
--- google.com ping statistics ---
5 packets transmitted, 4 received, +1 duplicates, 20% packet loss
round-trip min/avg/max/stddev = 11.234/12.567/14.890/1.234 ms
```

**Notes:**
- **No leading blank line** before `---` (unlike iputils).
- **No `time Xms`** in summary line (inetutils-2.0 omits it).
- Duplicates inline as `+N duplicates, ` (only when > 0).
- **Loss** = `(sent - rec) * 100 / sent` (integer arithmetic; duplicates
  excluded from `rec`).
- **RTT line label**: `round-trip min/avg/max/stddev` (not `rtt min/avg/max/mdev`).
- **mdev** = population standard deviation: `sqrt(E[x^2] - E[x]^2)`.
  `rtt_sq_sum` accumulates `rtt^2`; `avg = rtt_sum / (rec + duplicates)`;
  `var = rtt_sq_sum / (rec + duplicates) - avg^2`; `mdev = sqrt(var)`.

### RTT Line (Exempt per Subject)

The subject allows the RTT line to differ from inetutils. `ft_ping` uses
the inetutils label `round-trip min/avg/max/stddev` for consistency.

## Side-by-Side: inetutils-2.0 vs ft_ping

| Line | inetutils-2.0 | ft_ping | Status |
|------|---------------|---------|--------|
| Header (no -v) | `PING host (ip): 56 data bytes` | Same | Match |
| Header (-v) | `PING host (ip): 56 data bytes, id 0x1234 = 4660` | Same | Match |
| Reply | `64 bytes from ip: icmp_seq=0 ttl=117 time=12.345 ms` | Same | Match |
| Reply (DUP) | `64 bytes from ip: icmp_seq=5 ttl=117 time=13.102 ms (DUP!)` | Same | Match |
| Error (no -v) | `56 bytes from ip: Time to live exceeded` | Same | Match |
| Error (-v) | `56 bytes from ip: Time to live exceeded` + detail line | Same | Match |
| Summary | `--- host ping statistics ---`<br>`4 transmitted, 4 received, 0% loss` | Same | Match |
| Summary (dups) | `+1 duplicates, ` inline | Same | Match |
| Summary (time) | **No** `time Xms` | **No** `time Xms` | Match |
| RTT line | `round-trip min/avg/max/stddev = ...` | Same label | Match (exempt) |

## Deviations (Intentional or Unavoidable)

| Aspect | Difference |
|--------|------------|
| Debug line | `ft_ping` prints `Hostname's ip: <ip>` after DNS (subject permits). |
| `-v` ID | `ft_ping` uses `pid & 0xFFFF`; inetutils uses same. Match. |
| `-W` unit | `ft_ping` uses `-W` as per-packet timeout (ms); inetutils uses seconds linger. |
| IPv6 header | inetutils has no IPv6 ping; `ft_ping -6` uses same style. |
| Timestamp reply | `print_ip_opt` ported; output matches when kernel echoes option. |

---

## See also

- [CLI Reference](08-cli-reference.md)
- [Architecture — printing](07-architecture.md)
- [ICMP Protocol — error detail](05-icmp.md)