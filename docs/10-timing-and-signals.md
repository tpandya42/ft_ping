# Timing & Signals

## RTT Measurement

### Timestamp Method

`ft_ping` embeds a `struct timespec` (seconds + nanoseconds) at the start of
the ICMP payload:

```c
// Payload layout (bytes 0..packet_size-1)
| timespec (16 bytes on 64-bit) | pattern fill (packet_size - 16) |
```

- `buildEchoPacket()` / `buildEchoV6Packet()` capture `clock_gettime(CLOCK_MONOTONIC, &ts)`
  immediately before `sendto()` and write `ts` into the payload.
- On Echo Reply, the kernel echoes the entire payload back.
- Receiver extracts the timestamp from `buf + iphdr_len + sizeof(icmphdr)` (v4)
  or `buf + sizeof(icmp6_hdr)` (v6).
- RTT = `now - sent_ts` where `now` is captured after `recvmsg()` returns.

### Why `CLOCK_MONOTONIC`?

- `CLOCK_REALTIME` can jump (NTP, manual `date` changes) → negative or
  inflated RTT.
- `CLOCK_MONOTONIC` is guaranteed to never go backwards; only frequency
  adjustments (NTP slew) affect it.
- Suitable for elapsed-time measurements.

### Precision

- `struct timespec` provides nanosecond resolution.
- RTT computed as `double` milliseconds:
  ```c
  double rtt_ms = sec * 1000.0 + nsec / 1e6;
  ```
- Sub-millisecond precision achievable on modern hardware.

## Interval Timing (Cadence)

### Design Goal: ~1 Second Between Sends

The reference `inetutils-2.0` sends one Echo Request approximately every
second, regardless of whether the previous reply arrived. `ft_ping` matches
this behavior:

```
Send seq=0
  ├─ Reply arrives at 12ms → print → sleep 1s → Send seq=1
  └─ Timeout (1s) → print nothing → Send seq=1 immediately (no extra sleep)
```

### Implementation

1. **`SO_RCVTIMEO`** set to `conf->timeout_ms` (default 1000 ms). `recvmsg()`
   blocks up to this duration waiting for a packet.
2. On **reply received**: `got_reply = true`, break receive loop, then
   `nanosleep({1, 0})` before next sequence.
3. On **timeout** (`recvmsg()` returns -1/EAGAIN or 0): break receive loop
   **without** extra sleep — the timeout itself consumed ~1s.
4. Next sequence sent immediately.

This yields ~1s cadence in both success and loss scenarios, matching the
reference.

### Why No Retransmission?

The original code had a retransmit loop (up to 3 retries per sequence with
1s sleeps). This was removed because:
- **inetutils-2.0 does not retransmit**; it sends the next sequence on the
  next tick.
- Retransmission changes timing (up to 4s per lost packet) and packet counts
  (sent count increments only on first try, but time stretches).
- Subject requires output identical to inetutils-2.0; removing retransmission
  aligns timing and counts.

## Timeout Handling

### `SO_RCVTIMEO` (`-W`)

- Sets per-sequence receive timeout on the socket.
- Default: 1 second (`conf->timeout_ms = 1000`).
- When `recvmsg()` times out, it returns `-1` with `errno = EAGAIN` (or
  `EWOULDBLOCK`). The receive loop breaks, sequence advances.

### Deadline (`-w`)

- Absolute wall-clock limit from start.
- Checked at top of main loop:
  ```c
  if (conf->deadline > 0) {
      struct timespec now;
      clock_gettime(CLOCK_MONOTONIC, &now);
      if (now.tv_sec - logs.start_time.tv_sec >= conf->deadline)
          break;
  }
  ```
- Silent stop; summary printed with whatever was received.

## Signal Handling

### `SIGINT` (Ctrl+C)

```c
static volatile sig_atomic_t g_sig = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_sig = 1;
}
```

- `sig_atomic_t` ensures atomic access from signal handler.
- No I/O in handler (not async-signal-safe).
- Main loop checks `g_sig` at:
  - Top of main loop.
  - After `recvmsg()` timeout/return.
  - Before `nanosleep()`.
- On interrupt: breaks loops, prints summary, returns exit code.

### Why Not `SIGQUIT` / `SIGTERM`?

- Subject only requires `SIGINT` handling (standard ping behavior).
- `SIGQUIT` (Ctrl+\) typically dumps core; not required.

## Time Calculations

### Elapsed Time (Summary)

```c
long sec = logs.end_time.tv_sec - logs.start_time.tv_sec;
long nsec = logs.end_time.tv_nsec - logs.start_time.tv_nsec;
if (nsec < 0) { sec--; nsec += 1000000000L; }
// elapsed_ms = sec * 1000 + nsec / 1e6;
```

Used only for internal stats (not printed in summary per inetutils format).

### RTT Accumulators (for mdev)

- `logs.rtt_sum` — sum of all RTTs (including duplicates).
- `logs.rtt_sq_sum` — sum of squared RTTs.
- `logs.rtt_min` / `logs.rtt_max` — min/max.
- Count for avg/mdev = `logs.rec + duplicates` (duplicates included per
  inetutils behavior).

### Standard Deviation (mdev)

```c
unsigned rtt_count = logs.rec + duplicates;
double avg = logs.rtt_sum / rtt_count;
double var = logs.rtt_sq_sum / rtt_count - avg * avg;
if (var < 0.0) var = 0.0;  // numerical safety
double mdev = sqrt(var);
```

Population standard deviation (not sample). Matches inetutils.

## Summary of Timing Parameters

| Parameter | Source | Default | Units |
|-----------|--------|---------|-------|
| Interval | Hardcoded `nanosleep({1,0})` | 1 | seconds |
| Receive timeout (`-W`) | `conf->timeout_ms` | 1000 | milliseconds |
| Deadline (`-w`) | `conf->deadline` | 0 (none) | seconds |
| Timestamp clock | `CLOCK_MONOTONIC` | — | nanoseconds |
| RTT precision | `double` ms | — | ~µs |

## Comparison with inetutils-2.0

| Behavior | inetutils-2.0 | ft_ping |
|----------|---------------|---------|
| Interval | `PING_INTERVAL` default 1s | 1s `nanosleep` |
| Per-packet timeout | Not explicit (select loop with interval) | `SO_RCVTIMEO` = `-W` ms |
| Post-count linger | `-W` seconds after count reached | Not implemented (exits on count) |
| Retransmission | None | Removed (was 3x) |
| Deadline | `-w` seconds | Same |

---

## See also

- [Architecture](07-architecture.md)
- [CLI Reference — -w/-W](08-cli-reference.md)
- [Output Reference](09-output-reference.md)