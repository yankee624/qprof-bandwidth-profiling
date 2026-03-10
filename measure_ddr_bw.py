#!/usr/bin/env python3
"""Measure DRAM bandwidth via bw_hwmon_meas ftrace on Android.

Run your NPU workload separately, then run this script to measure DDR BW:

    # Terminal 1: NPU workload
    adb shell /data/local/tmp/your_npu_binary

    # Terminal 2: bandwidth monitor
    python3 measure_ddr_bw.py

Ctrl+C to stop and print final statistics.
"""
import subprocess
import re
import signal
import statistics
import time
import atexit
from collections import deque

# ── ftrace paths ──────────────────────────────────────────────────────────────
TRACING_ON   = "/sys/kernel/debug/tracing/tracing_on"
TRACE_BUF    = "/sys/kernel/debug/tracing/trace"
EVENT_ENABLE = "/sys/kernel/debug/tracing/events/dcvs/bw_hwmon_meas/enable"

# Only the gold monitor produces real readings (prime is always 0 on this device)
MONITOR_FILTER = "bwmon-llcc-gold"

# Poll interval in seconds. bw_hwmon samples every ~4ms; 100ms gives ~25 events/poll.
POLL_INTERVAL = 0.1

# Rolling average window in number of 4ms samples (~400ms)
ROLLING_N = 100

# ── regex ─────────────────────────────────────────────────────────────────────
# kworker/u16:0 [000] d..1. 781659.124893: bw_hwmon_meas: dev: 240b3400.qcom,bwmon-llcc-gold, mbps = 782, us = 4000, wake = 1
_RE = re.compile(
    r'(\d+\.\d+): bw_hwmon_meas: dev: (.+?), mbps = (\d+)'
)

# ── ADB helpers ───────────────────────────────────────────────────────────────
def adb(cmd):
    return subprocess.run(["adb", "shell", cmd],
                          capture_output=True, text=True).stdout

def adb_read_and_clear():
    """Atomically read trace buffer and clear it in one shell invocation."""
    # cat then clear — small race window is acceptable for BW estimation
    return adb(f"cat {TRACE_BUF}; echo > {TRACE_BUF}")

# ── setup / teardown ──────────────────────────────────────────────────────────
_running = True

def teardown():
    adb(f"echo 0 > {EVENT_ENABLE}")

atexit.register(teardown)

def on_signal(sig, frame):
    global _running
    _running = False

signal.signal(signal.SIGINT,  on_signal)
signal.signal(signal.SIGTERM, on_signal)

def setup():
    print("Enabling bw_hwmon_meas tracepoint...", flush=True)
    adb(f"echo 1 > {EVENT_ENABLE}")
    adb(f"echo 1 > {TRACING_ON}")
    adb(f"echo > {TRACE_BUF}")   # clear stale ring buffer

# ── stats ─────────────────────────────────────────────────────────────────────
def print_stats(values, label=""):
    if not values:
        print(f"  {label}No data")
        return
    vs = sorted(values)
    n  = len(vs)
    def p(pct): return vs[min(int(pct / 100 * (n - 1) + 0.5), n - 1)]
    print(f"  {label}n={n}  mean={statistics.mean(values):.0f}  "
          f"p50={p(50):.0f}  p90={p(90):.0f}  p99={p(99):.0f}  "
          f"max={p(100):.0f}  MBps")

# ── main ──────────────────────────────────────────────────────────────────────
def main():
    global _running

    all_mbps   = []
    rolling    = deque()
    last_print = time.time()
    seen_ts    = -1.0   # avoid duplicates across polls

    setup()

    print(f"Polling {MONITOR_FILTER} DDR BW every {int(POLL_INTERVAL*1000)}ms -- Ctrl+C to stop\n")
    print(f"{'Timestamp':>12}  {'BW (MBps)':>10}  {'Roll-avg (MBps)':>16}")
    print("-" * 44)

    while _running:
        time.sleep(POLL_INTERVAL)
        raw = adb_read_and_clear()

        for line in raw.splitlines():
            m = _RE.search(line)
            if not m:
                continue
            ts_str, dev, mbps_str = m.groups()
            if MONITOR_FILTER not in dev:
                continue

            ts = float(ts_str)
            if ts <= seen_ts:   # skip duplicates
                continue
            seen_ts = ts

            mbps = int(mbps_str)
            all_mbps.append(mbps)
            rolling.append(mbps)
            if len(rolling) > ROLLING_N:
                rolling.popleft()

        if all_mbps:
            roll_avg = int(statistics.mean(rolling))
            now = time.time()
            if now - last_print >= POLL_INTERVAL:
                print(f"{seen_ts:>12.3f}  {all_mbps[-1]:>10}  {roll_avg:>16}", flush=True)
                last_print = now

    if not all_mbps:
        print("No samples collected.")
        return

    print("\n" + "=" * 44)
    print("Final statistics (all samples):")
    print_stats(all_mbps)

    nonzero = [v for v in all_mbps if v > 0]
    if len(nonzero) < len(all_mbps):
        print(f"\nNon-zero samples only ({len(nonzero)}/{len(all_mbps)}):")
        print_stats(nonzero)

if __name__ == "__main__":
    main()
