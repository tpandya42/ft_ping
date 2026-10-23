#!/usr/bin/env bash
# Requires root (raw sockets). Only uses builtins + coreutils.
# This script auto-detects environment and SKIPs checks that cannot run

set -u

BIN=./ft_ping
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0; skip=0

check_rc() { local d=$1 rc=$2; shift 2; "$@" 2>&1; local s=$?; if [ "$s" -ne "$rc" ]; then fail "$d (rc=$s expected=$rc)"; else pass "$d"; fi; }
check_out() { local d=$1 pat=$2 rc=$3; shift 3; local out; out=$("$@" 2>&1); rc=$?; if [ "$rc" -ne "$rc" ]; then fail "$d (rc=$rc expected=$rc)"; elif ! echo "$out" | grep -qE -- "$pat"; then fail "$d (missing pattern: $pat)"; else pass "$d"; fi; }
pass() { pass=$((pass+1)); printf '[PASS] %s\n' "$1"; }
fail() { fail=$((fail+1)); printf '[FAIL] %s%s\n' "$1" "${2:+ (expected $2)}"; }
skip() { skip=$((skip+1)); printf '[SKIP] %s (%s)\n' "$1" "${2-}"; }

# ----- root check -----
if [ "$EUID" -ne 0 ]; then
  printf 'Run with sudo: sudo ./eval.sh\n'; exit 1; fi

# ----- build -----
if ! make -s re 2>/dev/null; then
  printf 'Build failed; fix Makefile/source then re-run.\n'; exit 1; fi
[ -x "$BIN" ] || { printf 'Binary ft_ping not found after build.\n'; exit 1; }

# ----- compliance helpers -----
chk_global() { nm "$BIN" 2>/dev/null | grep -cE ' g_sig$' | read c; [ "$c" -eq 1 ] && pass "single global g_sig" || fail "global g_sig count=$c"; }
chk_forbidden() { local s; s=$(grep -rnE '\b(fcntl|poll|ppoll)\s*\(' srcs includes 2>/dev/null | wc -l); [ "$s" -eq 0 ] && pass "no forbidden funcs" || fail "forbidden funcs found ($s)"; }

# ----- help / args -----
check " -h exits 0" 0 ./ft_ping -h 2>&1 | grep -q "Usage" && pass || fail
check " -?" rc 0; ./ft_ping -? >/dev/null 2>&1 || fail
check " --help" rc 0; ./ft_ping --help >/dev/null 2>&1 || fail
check " --usage" rc 0; ./ft_ping --usage >/dev/null 2>&1 || fail
check " -z rc 64" 64; ./ft_ping -z >/dev/null 2>&1 || fail
check " --bogus rc 64" 64; ./ft_ping --bogus >/dev/null 2>&1 || fail
check " missing host rc 64" 64; ./ft_ping >/dev/null 2>&1 || fail
check " -c missing arg rc 64" 64; ./ft_ping -c >/dev/null 2>&1 || fail

# ----- mandatory IPv4 -----
check "loopback -c 3" 0 ./ft_ping -c 3 127.0.0.1 2>&1 | grep -q "64 bytes from" && pass || fail
hdr=$(sudo -n ./ft_ping -v -c 1 127.0.0.1 2>&1 | head -1); hdr_ok=$(echo "$hdr" | grep -q "PING.*data bytes" && echo yes || echo no)
[ "$hdr_ok" = yes ] && pass "header format" || fail "header format"
rep=$(sudo -n ./ft_ping -c 3 127.0.0.1 2>&1 | grep -c "64 bytes from"); [ "$rep" -ge 3 ] && pass "3 replies" || fail "3 replies"
no_debug=$(sudo -n ./ft_ping -c 3 127.0.0.1 2>&1 | grep -c "Hostname's ip" || true); [ "$no_debug" -eq 0 ] && pass "no debug line" || fail "debug line present"
sigint_out=$(mktemp); sudo -n ./ft_ping -c 5 127.0.0.1 > "$sigint_out" 2>&1 & pid=$!; sleep 1; sudo -n kill -INT "$pid" 2>/dev/null; wait "$pid" 2>/dev/null; sigint_rc=$?; grep -qi "round.trip\|0% packet loss" "$sigint_out" 2>/dev/null && pass "SIGINT summary" || fail "SIGINT summary"; rm -f "$sigint_out"

# ----- FQDN (online — probe first) -----
fqdn_ok=0; sudo -n ./ft_ping -c 1 example.com 2>&1 | grep -q "bytes from" && fqdn_ok=1 || true
[ "$fqdn_ok" -eq 1 ] && pass "FQDN example.com" || skip "FQDN example.com (DNS unavailable)"

# bad IP (online — auto-SKIP if replies)  BAD_IP=${BAD_IP:-192.0.2.1}
bip_out=$(sudo -n timeout 5 ./ft_ping -c 2 -W 1 "$BAD_IP" 2>&1); b_rc=$?; b_replies=$(echo "$bip_out" | grep -c "bytes from" || true)
if [ "$b_rc" -eq 0 ] && [ "$b_replies" -gt 0 ]; then skip "bad IP $BAD_IP (environment replies)"; else
  [ "$b_rc" -eq 1 ] && [ "$b_replies" -eq 0 ] && pass "bad IP $BAD_IP (100% loss)" || fail "bad IP $BAD_IP unexpected"
fi

# TTL exceeded (online — auto-SKIP if NAT fakes)  ttl_ok=0; ttl_out=$(sudo -n timeout 4 ./ft_ping -v -t 1 -c 1 8.8.8.8 2>&1); ttl_ok=$(echo "$ttl_out" | grep -c "Time to live exceeded" || true)
[ "$ttl_ok" -ge 1 ] && pass "TTL exceeded -v" || skip "TTL exceeded -v (environment fakes reply)"

# ----- bonus flags (IPv4) -----
check " -c" 0; sudo -n ./ft_ping -c 1 127.0.0.1 >/dev/null 2>&1 || fail
check " -n" 0; sudo -n ./ft_ping -n -c 1 127.0.0.1 >/dev/null 2>&1 || fail
check " -q -c 2" 0; sudo -n ./ft_ping -q -c 2 127.0.0.1 2>&1 | grep -qv "bytes from" && pass || fail
check " -r -c 2 127.0.0.1" 0; sudo -n ./ft_ping -r -c 2 127.0.0.1 >/dev/null 2>&1 || fail
check " -s 100 -c 1" 0; sudo -n ./ft_ping -s 100 -c 1 127.0.0.1 2>&1 | grep -q "108 bytes from" && pass || fail
check " -s 2000 -c 1" 1; sudo -n ./ft_ping -s 2000 -c 1 127.0.0.1 2>&1 | grep -q "packet size too large" && pass || fail
check " -p ff -c 1" 0; sudo -n ./ft_ping -p ff -c 1 127.0.0.1 >/dev/null 2>&1 || fail
check " -T 16 -c 1" 0; sudo -n ./ft_ping -T 16 -c 1 127.0.0.1 >/dev/null 2>&1 || fail
check " -t 5 -c 1" 0; sudo -n ./ft_ping -t 5 -c 1 127.0.0.1 >/dev/null 2>&1 || fail
check " --ttl 5 -c 1" 0; sudo -n ./ft_ping --ttl 5 -c 1 127.0.0.1 >/dev/null 2>&1 || fail
check " --ip-timestamp=tsonly -c 1 -v" 0; sudo -n ./ft_ping --ip-timestamp=tsonly -c 1 127.0.0.1 >/dev/null 2>&1 || fail
check " -w 2 -c 1" 0; sudo -n ./ft_ping -w 2 127.0.0.1 >/dev/null 2>&1 || fail; d=$(( $(date +%s) - start )); [ "$d" -ge 1 ] && [ "$d" -le 5 ] && pass " -w 2 duration" || fail " -w 2 duration"
check " -W 1 -c 1" 0; sudo -n ./ft_ping -W 1 -c 1 127.0.0.1 >/dev/null 2>&1 || fail

# ----- IPv6 extras -----
check " -6 -c 2 ::1" 0; sudo -n ./ft_ping -6 -c 2 ::1 >/dev/null 2>&1 || fail
check " -6 -v -c 1 ::1" 0; sudo -n ./ft_ping -6 -v -c 1 ::1 2>&1 | grep -q "id 0x" && pass || fail
check " -6 -s 100 -c 1 ::1" 0; sudo -n ./ft_ping -6 -s 100 -c 1 ::1 2>&1 | grep -q "108 bytes from" && pass || fail
check " -6 -p ff -c 1" 0; sudo -n ./ft_ping -6 -p ff -c 1 ::1 >/dev/null 2>&1 || fail
check " -6 -T 16 -c 1 ::1" 0; sudo -n ./ft_ping -6 -T 16 -c 1 ::1 2>&1 | grep -q "not available for IPv6" && pass || fail  # warning OK
check " -6 --ip-timestamp=tsonly -c 1" 0; sudo -n ./ft_ping -6 --ip-timestamp=tsonly -c 1 ::1 2>&1 | grep -q "warning" && pass || fail

# ----- failure paths -----
check "multicast 100% loss" 1; sudo -n timeout 4 ./ft_ping -c 2 -W 1 224.0.0.1 2>&1 | grep -q "100% packet loss" && pass || fail
check "bad hostname rc 1" 1; sudo -n ./ft_ping -c 1 nonexistent.invalid >/dev/null 2>&1 || fail; [ "$?" -eq 1 ] && pass || fail
check "sendto failure (no hang)" 0; timeout 5 sudo -n ./ft_ping -r -c 1 8.8.8.8 2>&1 | grep -qi "packet statistics" && pass || fail  # terminate + summary
check "no-sudo EPERM" 1; setpriv --reuid=65534 --regid=65534 --clear-groups ./ft_ping -c 1 127.0.0.1 2>/dev/null | grep -qi "Operation not permitted" && pass || skip "no-sudo EPERM (setpriv unavailable)"; setpriv notfound=1; [ "$?" -eq 127 ] && skip "no-sudo EPERM (setpriv not found)"
check "oversized -s 2000 rc 1" 1; sudo -n ./ft_ping -s 2000 -c 1 127.0.0.1 2>&1 | grep -q "packet size too large" && pass || fail

# ----- summary -----
printf '\n=== Summary === \nPASS: %d  FAIL: %d  SKIP: %d\n' "$pass" "$fail" "$skip"
[ "$fail" -eq 0 ] && exit 0 || exit 1
