# DNS Resolution

## Overview

`ft_ping` performs **forward DNS resolution** (hostname → IP address) using
`getaddrinfo()`. Reverse DNS (IP → hostname) is **never performed**; the
source address in replies is printed numerically. This matches the subject's
exemption: "except for the reverse DNS resolution."

## Resolution Functions

### `resolveDNS()` (IPv4) — `srcs/dns.c`

```c
int resolveDNS(t_config *conf) {
    struct addrinfo hints = {0}, *result;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_flags = AI_CANONNAME;

    if (getaddrinfo(conf->hostname, NULL, &hints, &result) != 0)
        return -1;  // "ft_ping: unknown host %s"

    for (struct addrinfo *rp = result; rp; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            conf->target_addr = *(struct sockaddr_in *)rp->ai_addr;
            inet_ntop(AF_INET, &conf->target_addr.sin_addr,
                      conf->ip_str, sizeof(conf->ip_str));
            freeaddrinfo(result);
            return 0;
        }
    }
    freeaddrinfo(result);
    return -1;  // "ft_ping: no IPv4 address for %s"
}
```

### `resolveIPv6DNS()` (IPv6) — `srcs/dns.c`

```c
int resolveIPv6DNS(t_config *conf) {
    struct addrinfo hints = {0}, *result;
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_flags = AI_CANONNAME;

    if (getaddrinfo(conf->hostname, NULL, &hints, &result) != 0)
        return -1;

    for (struct addrinfo *rp = result; rp; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET6) {
            conf->target_addr6 = *(struct sockaddr_in6 *)rp->ai_addr;
            inet_ntop(AF_INET6, &conf->target_addr6.sin6_addr,
                      conf->ip6_str, sizeof(conf->ip6_str));
            freeaddrinfo(result);
            return 0;
        }
    }
    freeaddrinfo(result);
    return -1;
}
```

## `getaddrinfo()` Hints

| Field | Value | Reason |
|-------|-------|--------|
| `ai_family` | `AF_INET` / `AF_INET6` | Force v4 or v6 |
| `ai_socktype` | `SOCK_RAW` | Match raw socket type |
| `ai_flags` | `AI_CANONNAME` | Request canonical name |
| `ai_protocol` | 0 (any) | Let system choose |

- `SOCK_RAW` ensures the returned address is suitable for raw socket bind.
- `AI_CANONNAME` populates `ai_canonname` (not currently used but available).

## Error Handling

| Error | Message | Exit |
|-------|---------|------|
| `getaddrinfo` fails | `ft_ping: unknown host %s` | 1 |
| No matching address family in results | `ft_ping: no IPv4/IPv6 address for %s` | 1 |
| `inet_ntop` fails | Falls back to hostname string | Continues |

## Numeric Mode (`-n`)

- `conf->numeric = true` when `-n` is given.
- `ft_ping` **never** performs reverse DNS (no `getnameinfo` on source IP).
- Source IP in replies is always printed via `inet_ntop()` → numeric.
- `-n` is effectively a no-op but present for compatibility.

## Hostname in Output

- Header: `PING %s (%s): ...` — `%s` = `conf->hostname` (original argument),
  second `%s` = `conf->ip_str` / `conf->ip6_str` (resolved IP).
- The hostname is the **original command-line argument**, not the canonical
  name from DNS. This matches inetutils-2.0 behavior.

## IPv6 Address Selection

- `getaddrinfo` may return multiple IPv6 addresses (global, link-local, etc.).
- Loop picks the **first `AF_INET6`** entry.
- For link-local targets, the caller must specify the interface (e.g.,
  `ping6 -I eth0 fe80::1`). `ft_ping` does not currently support `-I`
  interface specification; link-local may fail with "No route to host."

## Resolution Output

Resolution is silent. `main.c` passes the numeric IP string to the ping loop,
which prints it only in the `PING <host> (<ip>): ...` header. No extra debug
line is printed, keeping the output identical to inetutils-2.0.

## IPv4 vs IPv6 Resolution

| Aspect | IPv4 | IPv6 |
|--------|------|------|
| Function | `resolveDNS()` | `resolveIPv6DNS()` |
| `ai_family` | `AF_INET` | `AF_INET6` |
| `sockaddr` | `struct sockaddr_in` | `struct sockaddr_in6` |
| IP string buffer | `INET_ADDRSTRLEN` (16) | `INET6_ADDRSTRLEN` (46) |
| IP string | `conf.ip_str` (PING header) | `conf.ip6_str` (PING header) |

## No Reverse DNS — Design Decision

The subject states: "the result must have an indentation identical, except for
the RTT line and the reverse DNS resolution." This explicitly permits the
output to differ from the reference in whether hostnames or IPs are printed.
`ft_ping` chooses **numeric output always** for:
1. Simplicity and determinism.
   - No dependency on `/etc/hosts`, DNS PTR records, or NSS.
2. Speed — no additional DNS round-trip per reply.
3. Consistency — same format regardless of DNS configuration.

This is explicitly permitted by the subject.

## Testing DNS

```bash
# IPv4
sudo ./ft_ping -c 2 google.com
# Should print: PING google.com (142.250.x.x): 56 data bytes

# IPv6
sudo ./ft_ping -6 -c 2 google.com
# Should print: PING google.com (2a00:1450:4001:800::200e): 56 data bytes

# Numeric (no-op)
sudo ./ft_ping -n -c 2 8.8.8.8

# Unknown host
./ft_ping nonexistent.invalid
# ft_ping: unknown host nonexistent.invalid
```

---

## See also

- [Architecture](07-architecture.md)
- [CLI Reference — -n](08-cli-reference.md)
- [Output Reference](09-output-reference.md)