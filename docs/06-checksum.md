# Checksum Algorithm

## One's Complement Arithmetic

The Internet checksum (RFC 1071) uses **one's complement addition** on 16-bit
words. Unlike two's complement, one's complement has two representations of
zero (`0x0000` and `0xFFFF`), and carries from the MSB are added back into
the LSB (end-around carry).

### Addition Rules

```
  0xFFFF + 0x0001 = 0x0000 (with carry) -> end-around carry -> 0x0001
  0xFFFF + 0xFFFF = 0xFFFE (with carry) -> end-around carry -> 0xFFFF
```

The sum of all 16-bit words is computed, then the final carry bits are folded
back repeatedly until the result fits in 16 bits. The **one's complement**
(bitwise NOT) of that sum is the checksum.

## Algorithm Steps (RFC 1071)

1. **Pair bytes** into 16-bit words (big-endian / network byte order).
2. **Sum** all words using one's complement arithmetic (32-bit accumulator).
3. **Fold** carries: while sum > 0xFFFF, `sum = (sum & 0xFFFF) + (sum >> 16)`.
4. **Complement**: `checksum = ~sum` (one's complement).

If the data length is odd, the final byte is padded with a zero byte
(high-order byte of the last word) before summing.

## ft_ping Implementation

Located in `srcs/icmp.c`:

```c
static uint32_t checksumSum(const void *data, size_t len, uint32_t sum)
{
    const uint16_t *words = (const uint16_t *)data;
    while (len > 1) {
        sum += *words++;
        len -= 2;
    }
    if (len == 1) {
        uint16_t odd_byte = 0;
        *((uint8_t *)&odd_byte) = *(const uint8_t *)words;
        sum += odd_byte;
    }
    return sum;
}

static uint16_t checksumFold(uint32_t sum)
{
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

uint16_t calculateChecksum(const void *data, size_t l)
{
    return checksumFold(checksumSum(data, l, 0));
}
```

### `checksumSum()`

- Iterates over `uint16_t` words, accumulating into `uint32_t`.
- Handles odd length: last byte placed in low-order byte of a `uint16_t`,
  high-order byte zero.
- Returns running sum (allows chaining multiple buffers).

### `checksumFold()`

- Folds upper 16 bits into lower 16 bits repeatedly until no carry remains.
- Equivalent to `(sum & 0xFFFF) + (sum >> 16)` iterated.
- Returns one's complement (`~sum`).

### `calculateChecksum()`

- Entry point: zero initial sum, processes entire buffer, folds, complements.

## IPv6 Pseudo-Header Checksum (RFC 2460, Section 8.1)

ICMPv6 checksum includes a pseudo-header before the ICMPv6 message:

```
  Source Address (16 bytes)
  Destination Address (16 bytes)
  Upper-Layer Packet Length (4 bytes, network order)
  Zero (3 bytes)
  Next Header (1 byte) = 58 (ICMPv6)
```

Total: 40 bytes.

```c
static uint16_t icmp6_checksum(const struct in6_addr *src,
                               const struct in6_addr *dst,
                               const uint8_t *icmp, size_t icmp_len)
{
    struct {
        struct in6_addr src;
        struct in6_addr dst;
        uint32_t        len;
        uint8_t         zero[3];
        uint8_t         next;
    } pseudo = {
        .src = *src,
        .dst = *dst,
        .len = htonl(icmp_len),
        .zero = {0, 0, 0},
        .next = IPPROTO_ICMPV6
    };
    uint32_t sum = checksumSum(&pseudo, sizeof(pseudo), 0);
    sum = checksumSum(icmp, icmp_len, sum);
    return checksumFold(sum);
}
```

The `len` field is the ICMPv6 message length (header + data) in bytes,
network byte order.

## Kernel Behavior Difference

| Protocol | Who Computes Checksum? |
|----------|------------------------|
| ICMPv4 (raw) | **Application** (`ft_ping` calls `calculateChecksum()`). Kernel verifies on receive. |
| ICMPv6 (raw) | **Kernel** recomputes/overwrites on send (`rawv6_init_sk`: `checksum=1`, `offset=2`). Application checksum is ignored but `ft_ping` computes it anyway for standalone validity. |

This is why `buildEchoV6Packet()` still calls `icmp6_checksum()` even though
the kernel will replace it — the packet is correct on its own.

## Verification in ft_ping

- On send: checksum computed and placed in ICMP header.
- On receive: kernel verifies checksum; corrupted packets are dropped.
- `ft_ping` does **not** re-verify checksum on receive (trusts kernel).

## Edge Cases Handled

- **Odd-length payload**: `checksumSum()` pads the final byte correctly.
- **Large packets**: 32-bit accumulator avoids overflow during summation
  (max ~65KB fits easily in 32 bits before folding).
- **Zero-length data**: not used; minimum ICMP echo has 8-byte header.

## References

- RFC 1071 — Computing the Internet Checksum
- RFC 1141 — Incremental Updating of the Internet Checksum
- RFC 2460, Section 8.1 — ICMPv6 Pseudo-Header
- Linux kernel: `net/ipv4/ip_options.c`, `net/ipv6/raw.c` (rawv6_init_sk)

---

## See also

- [ICMP Protocol](05-icmp.md)
- [IPv6](04-ipv6.md)
- [Architecture — packet building](07-architecture.md)