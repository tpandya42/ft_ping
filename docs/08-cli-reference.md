# CLI Reference

## Option Summary Table

| Short | Long | Argument | Description | Default |
|-------|------|----------|-------------|---------|
| `-v` | `--verbose` | — | Verbose output (ICMP errors, ID in header) | off |
| `-?`, `-h` | `--help` | — | Print full usage and exit | — |
| | `--usage` | — | Print short usage and exit | — |
| `-6` | — | — | Use IPv6 | off |
| `-c` | `--count` | NUMBER | Stop after sending NUMBER packets | unlimited |
| `-t` | `--ttl` | N | Set IP TTL / Hop Limit | 64 |
| `-T` | `--tos` | NUM | Set Type of Service (IPv4 only) | unset |
| | `--ip-timestamp` | FLAG | IP timestamp option: `tsonly` or `tsaddr` | off |
| `-s` | `--size` | NUMBER | Data bytes per packet | 56 |
| `-p` | `--pattern` | HEX | Fill payload with hex byte (e.g. `ff`) | `0x42` |
| `-r` | `--ignore-routing` | — | Set `SO_DONTROUTE` (bypass routing) | off |
| `-w` | `--timeout` | SECONDS | Deadline: stop after N seconds | 0 (none) |
| `-W` | `--linger` | SECONDS | Response wait time (SO_RCVTIMEO) | 1 |
| `-q` | `--quiet` | — | Suppress per-reply output | off |
| `-n` | `--numeric` | — | Numeric output only (no DNS) | off |

> **Note**: `--ttl` and `--ip-timestamp` are long-only in inetutils-2.0; `-t`
> is TTL alias in `ft_ping`; `-T` is TOS (inetutils `-T` = TOS, not timestamp).

## Mandatory Options (Subject)

| Option | Description | Implemented |
|--------|-------------|-------------|
| `-v` | Verbose | Yes |
| `-?` / `-h` | Help | Yes |

## Bonus Options (Subject List)

| Option | Subject Name | Status |
|--------|--------------|--------|
| `-f` | Flood | Not implemented |
| `-l` | Preload | Not implemented |
| `-n` | Numeric | Implemented (no-op; no rDNS by default) |
| `-w` | Deadline | Implemented |
| `-W` | Linger/timeout | Implemented |
| `-p` | Pattern | Implemented |
| `-r` | Ignore routing | Implemented |
| `-s` | Packet size | Implemented |
| `-T` / `--ip-timestamp` | IP timestamp | Implemented |
| `--ttl` | TTL | Implemented (`-t` also works) |

## Extra (Not in Subject Bonus List)

| Option | Description |
|--------|-------------|
| `-6` | IPv6 support |
| `--help` / `--usage` | Long help aliases |
| `-T` (TOS) | Type of Service (inetutils `-T`) |

## Detailed Descriptions

### `-v`, `--verbose`

- **Header**: appends `, id 0x%04x = %u` (hex and decimal of PID).
- **Errors**: prints ICMP errors even when they don't quote our packet.
- **Detail line**: on our errors, adds `ICMP: type N, code N, size N, id 0xXXXX, seq 0xXXXX`.
- **Duplicates**: `(DUP!)` printed on every duplicate (also without `-v`).

### `-?`, `-h`, `--help`, `--usage`

- `-?` / `-h` / `--help`: full usage text (stdout, exit 0).
- `--usage`: one-line short usage (stdout, exit 0).
- Unknown option → error message + `"Try 'ft_ping -?' for more information."` (stderr, exit 64).

### `-6`

- Selects IPv6 path: `resolveIPv6DNS()`, `ipv6_icmp_socket()`, `ft_pingIpv6()`.
- Mutually exclusive with IPv4 (last wins if both given — `getopt` order).

### `-c`, `--count` NUMBER

- Stop after **sending** `NUMBER` packets.
- `NUMBER = 0` → unlimited (same as not providing `-c`).
- Implemented as `conf->count`; loop breaks when `logs.sent >= count`.

### `-t` N / `--ttl` N

- Sets IP TTL (v4) or Hop Limit (v6) via `IP_TTL` / `IPV6_UNICAST_HOPS`.
- Valid range: 1–255. Default 64.
- `-t` is an alias; `--ttl` is the inetutils long option.

### `-T` NUM (`--tos` NUM)

- Sets IPv4 Type of Service via `IP_TOS` socket option.
- **Not available for IPv6** (prints warning, ignored).
- inetutils `-T` = TOS; **not** timestamp (timestamp is `--ip-timestamp`).

### `--ip-timestamp=FLAG`

- Sets IPv4 `IP_OPTIONS` with Timestamp option.
- `FLAG` values:
  - `tsonly` — Timestamp Only (flag 0).
  - `tsaddr` — Timestamp and Address (flag 3 / prespecified).
- **Not available for IPv6** (prints warning, ignored).
- On replies with `-v`, `print_ip_opt()` prints collected timestamps.

### `-s`, `--size` NUMBER

- Data bytes in ICMP payload (excluding ICMP header).
- Total on wire: `20 (IP) + 8 (ICMP) + NUMBER` (v4); `40 + 8 + NUMBER` (v6).
- Default: 56. Max limited by buffer (1500 + header).

### `-p` HEX (`--pattern` HEX)

- Fill payload (after timestamp) with this byte.
- Hex string parsed by `strtol(optarg, NULL, 16)`.
- Default: `0x42` (`'B'`).
- Timestamp (first `sizeof(timespec)` bytes) is **not** overwritten.

### `-r`, `--ignore-routing`

- Sets `SO_DONTROUTE` on socket.
- Bypasses routing table; sends directly to attached network.
- Works on both IPv4 and IPv6.

### `-w` SECONDS (`--timeout`)

- **Deadline**: stop after `SECONDS` wall-clock time from start.
- Checked at top of main loop; silent break + summary.
- inetutils: `-w` = timeout (seconds).

### `-W` SECONDS (`--linger`)

- **Response wait time**: per-sequence receive timeout (`SO_RCVTIMEO`).
- Default: `1` second (converted to ms internally).
- inetutils: `-W` = linger (seconds to wait for response after count reached).
- **Important**: `ft_ping` uses it as per-packet timeout (converted to ms).
  This matches common `ping` behavior (iputils); inetutils uses `-W` for
  post-count linger. The subject bonus list includes `-W`; we implement the
  common per-packet semantics.

### `-q`, `--quiet`

- Suppresses per-reply lines (`print_reply()` returns early).
- Header and summary still printed.
- Errors still printed (unless quiet also gates them — currently it does not).

### `-n`, `--numeric`

- No reverse DNS resolution.
- `ft_ping` never performs reverse DNS (prints source IP directly), so this
  is effectively a no-op. Present for compatibility.

## Exit Codes

| Code | Condition |
|------|-----------|
| 0 | Success (at least one reply received) |
| 1 | Socket error, DNS failure, or zero replies |
| 64 | Usage error (unknown option, missing host) |

Matches inetutils argp conventions (EX_USAGE = 64).

## Deviations from inetutils-2.0

| Aspect | inetutils-2.0 | ft_ping | Note |
|--------|---------------|---------|------|
| `-T` | TOS | TOS | Same |
| `-t` | (unassigned) | TTL alias | Extra convenience |
| `--ip-timestamp` | Full option + reply print | Same | Full port |
| `-W` | Linger (post-count) | Per-packet timeout (ms) | Common ping semantics |
| IPv6 | Not in inetutils ping | `-6` extra | Subject extra |
| `-f`, `-l` | Flood, preload | Not implemented | Bonus, deferred |
| Output format | Reference | Matches (see [Output Reference](09-output-reference.md)) | Mandatory fidelity |

---

## See also

- [Output Reference](09-output-reference.md)
- [Architecture — option parsing](07-architecture.md)
- [IPv4 — socket options](03-ipv4.md)
- [IPv6 — socket options](04-ipv6.md)