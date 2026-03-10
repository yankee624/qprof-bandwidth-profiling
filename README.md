# Bandwidth Profiling with Qualcomm Profiler API

Qualcomm Profiler API example for measuring DDR bandwidth on Android.

Collects DSP PMU metrics (AXI bus, L2 cache, HVX, HMX, uDMA) and DDR bandwidth
counters, then analyzes the log to report active-window statistics.

## Setup

**Requirements**

- Android NDK (`ANDROID_NDK_HOME` set)
- Qualcomm Profiler SDK (automatically installed at `/opt/qcom/Shared/QualcommProfiler/`)
- Run `/opt/qcom/Shared/QualcommProfiler/API/target-la/InstallerLA` to start profiling server on the device.

## Usage

```bash
bash run.sh
```

This builds the binary, pushes it to `/data/local/tmp/`, runs it, saves the log
to `logs/profiler.log`, and runs `analyze_nsp_bw.py` on it.

The profiler runs for 5 seconds while the target workload should be running
separately in another shell. Alternatively, the qprof CLI supports a `launch-app`
command that starts profiling and launches the app simultaneously, which is the
more principled approach.

To analyze a saved log separately:

```bash
python3 analyze_nsp_bw.py logs/profiler.log
```

## Environment

- **Device**: Galaxy S25+
- **Qualcomm Profiler**: v2.25.8.4

## Workloads

All workloads are 200-layer matmul/matvec kernels compiled with QNN.
Dimensions are [M × K × N]:

| Kernel | Latency (QNN client) | BW implied by latency |
|---|---|---|
| 1×1024×4096 | ~100ms | ~33 GBps |
| 1×8×4096 | ~6ms | ~4 GBps |
| 1×8×8 | ~1ms | ~50 MBps |

## Findings

**NOC DDR NSP Bandwidth (metric 4664) reads bogus values**

Samples alternate between ~370,000 MBps and near-zero, regardless of workload
size. Both 1×1024×4096 (~33 GBps expected) and 1×8×8 (~50 MBps expected) show
the same ~370,000 / near-zero alternation, and the ratio of ~370,000 samples is
similar across workloads rather than scaling with bandwidth. The counter does not
appear to be measuring actual transfer bandwidth.

**matvec also uses HMX**

HMX utilization (metric 4480) is non-zero during matvec kernels, not just GEMM.

**AXI metrics (4141–4144) appear to include both L2 and VTCM traffic**

No official documentation; the following is inferred from experiments. AXI metrics
seem to count all DDR-bound requests from the NSP side, including both L2 cache
miss fills and VTCM fills from DDR. Running DDR→VTCM read benchmark (from https://github.com/haozixu/llama.cpp-npu) showed ~60 GBps on AXI metrics, consistent with the bandwidth implied by latency.

QNN kernel observations (AXI 256B read, metric 4144):

| Kernel | Latency (QNN client) | BW implied by latency | AXI-measured BW |
|---|---|---|---|
| 1×1024×4096 | ~100ms | ~33 GBps | 8–24 GBps (alternating) |
| 1×8×4096 | ~6ms | ~4 GBps | ≤3 GBps |

AXI-measured BW is consistently lower than the latency-implied value.

**`nsp-dsp-metrics` at 1ms sampling rate is flaky**

At 1ms sampling, the profiler sometimes returns errors on the first attempt.
Workarounds:
- Retry: it tends to succeed consistently once it works the first time
- Switch to a different QNN kernel and switch back
- Switch to 10ms sampling rate and switch back

## Device Capabilities (qprof v2.25.8.4)

`PrintCapabilities()` output. Note: v2.25.12.12 returns no capabilities on this device.

```
[04:04:43.799] Capabilities count: 12
[04:04:43.799] Capability 0: [profiler:apps-proc-mem-metrics]
[04:04:43.799]   samplingRates  (16): 50ms 60ms 70ms 80ms 90ms 100ms 110ms 120ms 130ms 140ms 150ms 160ms 170ms 180ms 190ms 200ms
[04:04:43.799]   streamingRates (2): 200ms 1000ms
[04:04:43.799]   metricIds      (8): 4639-4641 4643-4645 4648-4649
[04:04:43.799] Capability 1: [profiler:apps-proc-process-metrics]
[04:04:43.799]   samplingRates  (2): 50ms 200ms
[04:04:43.799]   streamingRates (2): 200ms 1000ms
[04:04:43.799]   metricIds      (1): 4642
[04:04:43.799] Capability 2: [profiler:proc-gpu-specific-metrics]
[04:04:43.799]   samplingRates  (2): 50ms 70ms
[04:04:43.799]   streamingRates (2): 200ms 1000ms
[04:04:43.799]   metricIds      (5): 4864-4868
[04:04:43.799] Capability 3: [profiler:apps-proc-thread-profiling]
[04:04:43.799]   samplingRates  (2): 100ms 200ms
[04:04:43.799]   streamingRates (2): 200ms 400ms
[04:04:43.799]   metricIds      (1): 4660
[04:04:43.799] Capability 4: [profiler:nsp-dsp-metrics]
[04:04:43.799]   samplingRates  (2): 1ms 10ms
[04:04:43.799]   streamingRates (2): 200ms 1000ms
[04:04:43.799]   metricIds      (164): 4096-4184 4187-4188 4190-4192 4195 4198-4205 4236-4241 4352 4356 4358 4360-4362 4366-4374 4377-4385 4480-4481 4496-4524
[04:04:43.799] Capability 5: [profiler:apps-proc-cpu-metrics]
[04:04:43.799]   samplingRates  (2): 50ms 200ms
[04:04:43.799]   streamingRates (2): 200ms 1000ms
[04:04:43.799]   metricIds      (3): 4608 4616-4617
[04:04:43.799] Capability 6: [profiler:apps-proc-io-metrics]
[04:04:43.799]   samplingRates  (2): 200ms 1000ms
[04:04:43.799]   streamingRates (1): 1000ms
[04:04:43.799]   metricIds      (2): 4646-4647
[04:04:43.799] Capability 7: [profiler:apps-proc-process-mem-metrics]
[04:04:43.799]   samplingRates  (2): 50ms 200ms
[04:04:43.799]   streamingRates (2): 200ms 1000ms
[04:04:43.799]   metricIds      (4): 4683-4686
[04:04:43.799] Capability 8: [profiler:apps-proc-net-metrics]
[04:04:43.799]   samplingRates  (2): 50ms 200ms
[04:04:43.799]   streamingRates (2): 200ms 1000ms
[04:04:43.799]   metricIds      (9): 4656-4659 4678-4682
[04:04:43.799] Capability 9: [profiler:apps-proc-ddr-metrics]
[04:04:43.799]   samplingRates  (1): 10ms
[04:04:43.799]   streamingRates (2): 200ms 500ms
[04:04:43.799]   metricIds      (4): 4661-4664
[04:04:43.799] Capability 10: [profiler:apps-proc-thermal-metrics]
[04:04:43.799]   samplingRates  (16): 50ms 60ms 70ms 80ms 90ms 100ms 110ms 120ms 130ms 140ms 150ms 160ms 170ms 180ms 190ms 200ms
[04:04:43.799]   streamingRates (2): 200ms 1000ms
[04:04:43.799]   metricIds      (2): 6464-6465
[04:04:43.799] Capability 11: [profiler:nsp-dsp-stats]
[04:04:43.799]   samplingRates  (2): 1000ms 2000ms
[04:04:43.799]   streamingRates (2): 1000ms 2000ms
[04:04:43.799]   metricIds      (21): 5888-5890 5893-5900 5903-5907 5909-5912 5914
[04:04:43.807] Warning: bw-profiler-ddr-metrics not supported on this device, skipping
```
