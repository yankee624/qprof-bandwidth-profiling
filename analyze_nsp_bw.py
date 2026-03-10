#!/usr/bin/env python3
"""Analyze NSP AXI Bandwidth from qprof log files.

Usage:
    python3 analyze_nsp_bw.py profiler.log

Parses AXI metrics (4141, 4142, 4144) which measure NSP DDR bandwidth.
Auto-detects the "active" window where an external NPU program is running.
"""
import sys
import re
import statistics

# AXI metrics to track
AXI_METRICS = {
    4141: "AXI 128B Read",
    4142: "AXI 128B Write",
    4144: "AXI 256B Read",
}
# Primary metric for active window detection
PRIMARY_METRIC = 4144

# Active window detection:
#   A sample is "active" if its primary metric (4144) > ACTIVE_THRESHOLD MBps.
#   A region is considered active if, within any WINDOW_SIZE consecutive samples,
#   at least MIN_ACTIVE_IN_WINDOW are active. This filters out sporadic spikes.
ACTIVE_THRESHOLD = 500.0   # MBps — samples above this are considered "active"
WINDOW_SIZE = 20           # sliding window size (number of samples)
MIN_ACTIVE_IN_WINDOW = 3   # minimum active samples within the window

SKIP_FIRST_SECS = 1.0     # skip initial seconds to avoid initialization noise

_METRIC_IDS_RE = "|".join(str(m) for m in AXI_METRICS)
_RE = re.compile(
    r'\[(\d{2}:\d{2}:\d{2}\.\d{3})\] Metric ID: ('
    + _METRIC_IDS_RE
    + r') \(.*?\), Value: ([0-9eE.+\-]+)'
)

def parse_log(path):
    """Parse log file, return list of (timestamp_str, metric_id, value)."""
    entries = []
    with open(path) as f:
        for line in f:
            m = _RE.search(line)
            if m:
                ts = m.group(1)
                mid = int(m.group(2))
                val = float(m.group(3))
                entries.append((ts, mid, val))
    return entries

def ts_to_secs(ts):
    """Convert HH:MM:SS.mmm to seconds since midnight."""
    h, m, rest = ts.split(':')
    s, ms = rest.split('.')
    return int(h) * 3600 + int(m) * 60 + int(s) + int(ms) / 1000.0

def detect_active_window(values):
    """Find the first and last sample indices of the active region.

    Scans forward to find the earliest window of WINDOW_SIZE samples containing
    at least MIN_ACTIVE_IN_WINDOW samples above ACTIVE_THRESHOLD, then scans
    backward to find the latest such window. Returns (first_idx, last_idx).
    """
    n = len(values)
    if n < WINDOW_SIZE:
        return None, None
    is_active = [v > ACTIVE_THRESHOLD for v in values]

    # Scan forward: find first window with enough active samples
    first = None
    count = sum(is_active[:WINDOW_SIZE])
    if count >= MIN_ACTIVE_IN_WINDOW:
        first = 0
    for i in range(1, n - WINDOW_SIZE + 1):
        count += is_active[i + WINDOW_SIZE - 1] - is_active[i - 1]
        if count >= MIN_ACTIVE_IN_WINDOW and first is None:
            first = i

    if first is None:
        return None, None

    # Scan backward: find last window with enough active samples
    last = None
    count = sum(is_active[n - WINDOW_SIZE:])
    if count >= MIN_ACTIVE_IN_WINDOW:
        last = n - 1
    for i in range(n - WINDOW_SIZE - 1, -1, -1):
        count += is_active[i] - is_active[i + WINDOW_SIZE]
        if count >= MIN_ACTIVE_IN_WINDOW and last is None:
            last = i + WINDOW_SIZE - 1

    return first, last

def print_stats(values, label=""):
    if not values:
        print(f"  {label}No data")
        return
    vs = sorted(values)
    n = len(vs)
    def pct(p):
        idx = int(p / 100.0 * (n - 1) + 0.5)
        return vs[min(idx, n - 1)]
    print(f"  {label}n={n}  mean={statistics.mean(values):.1f}"
          f"  p50={pct(50):.1f}  p90={pct(90):.1f}  p99={pct(99):.1f}"
          f"  max={pct(100):.1f}  MBps")

def print_section(entries, section_name):
    """Print stats for a set of (ts, mid, val) entries."""
    # Per-metric values
    by_metric = {mid: [] for mid in AXI_METRICS}
    for _, mid, val in entries:
        by_metric[mid].append(val)

    # Total AXI BW per sample index (sum across metrics present at each sample)
    # Since metrics are individual samples (not grouped), compute total from per-metric means
    primary = by_metric[PRIMARY_METRIC]
    if not primary:
        print(f"  No {AXI_METRICS[PRIMARY_METRIC]} data")
        return

    print(f"\n--- {section_name} ---")
    # Primary metric (dominates total)
    print_stats(primary, f"{AXI_METRICS[PRIMARY_METRIC]}: ")

    # Other metrics
    for mid in AXI_METRICS:
        if mid == PRIMARY_METRIC:
            continue
        vals = by_metric[mid]
        if vals:
            print_stats(vals, f"{AXI_METRICS[mid]}: ")

    # Estimated total
    means = {mid: statistics.mean(vals) if vals else 0 for mid, vals in by_metric.items()}
    total_mean = sum(means.values())
    print(f"\n  Estimated Total AXI BW: {total_mean:.1f} MBps"
          f"  (read={means.get(4144,0) + means.get(4141,0):.1f}"
          f"  write={means.get(4142,0):.1f})")

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <logfile>")
        sys.exit(1)

    entries = parse_log(sys.argv[1])
    if not entries:
        print(f"No AXI metric entries found in {sys.argv[1]}")
        sys.exit(1)

    # Skip first N seconds
    if SKIP_FIRST_SECS > 0:
        t0 = ts_to_secs(entries[0][0])
        entries = [(ts, mid, val) for ts, mid, val in entries
                   if ts_to_secs(ts) - t0 >= SKIP_FIRST_SECS]

    if not entries:
        print(f"No entries left after skipping first {SKIP_FIRST_SECS}s")
        sys.exit(1)

    n_by_metric = {}
    for _, mid, _ in entries:
        n_by_metric[mid] = n_by_metric.get(mid, 0) + 1
    print(f"AXI metric entries (after skipping first {SKIP_FIRST_SECS}s):")
    for mid in sorted(n_by_metric):
        print(f"  {mid} ({AXI_METRICS[mid]}): {n_by_metric[mid]} samples")
    print(f"Time range: {entries[0][0]} - {entries[-1][0]}")

    # Full session stats
    print_section(entries, "Full session")

    # Detect active window using primary metric
    primary_entries = [(ts, mid, val) for ts, mid, val in entries if mid == PRIMARY_METRIC]
    primary_values = [val for _, _, val in primary_entries]
    first, last = detect_active_window(primary_values)

    if first is None:
        print(f"\nNo active window detected (no {AXI_METRICS[PRIMARY_METRIC]} values > {ACTIVE_THRESHOLD})")
        return

    # Map primary metric indices back to timestamps
    ts_first = primary_entries[first][0]
    ts_last = primary_entries[last][0]
    t_first = ts_to_secs(ts_first)
    t_last = ts_to_secs(ts_last)

    # Filter all metrics within the active time window
    active_entries = [(ts, mid, val) for ts, mid, val in entries
                      if t_first <= ts_to_secs(ts) <= t_last]

    print(f"\n  Active window: {ts_first} - {ts_last}")
    print_section(active_entries, f"Active window (threshold={ACTIVE_THRESHOLD})")

    # High values only within active window
    active_primary = [val for _, mid, val in active_entries if mid == PRIMARY_METRIC and val > ACTIVE_THRESHOLD]
    if active_primary:
        print(f"\n--- High {AXI_METRICS[PRIMARY_METRIC]} only (>{ACTIVE_THRESHOLD}) ---")
        print_stats(active_primary, f"{AXI_METRICS[PRIMARY_METRIC]}: ")

if __name__ == "__main__":
    main()
