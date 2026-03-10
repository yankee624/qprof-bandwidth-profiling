TODO
- qprof cli로 qprof와 app을 동시에 돌리는 제대로 된 방법 쓰기 (launch-app)

Setting
- Device: Galaxy S25+
- Qualcomm Profiler version: v2.25.8.4

Results
- NOC DDR NSP Bandwidth (4664)는 370000MBps 같은 이상한 숫자 나옴.
    - 1x8x8 같은 커널로 bandwidth 작게 쓰게 하고 돌려봐도 370000MBps 나옴.
        - Latency: 1x1024x4096 200번은 100ms (약 33GBps), 1x8x8 200번은 1ms (약 50MBps). 근데 370000 나오는 비율도 비슷.
- HMX util 보면 matvec도 HMX 씀.
- AXI read/write request가 L2+TCM도 포함하는 메트릭인듯.  
    - scaling tingcao 코드로 ddr to vtcm read benchmark하니까 비슷하게 60GBps 찍힘.
    - qnn 커널들은 qnn latency로 계산한 bandwidth 작게 찍힘.
        - 1x1024x4096 200회에서 8GBps와 24GBps가 번갈아가며 나옴. qnn profiling level client/basic로 했을때 latency 100ms (33GBps), detailed로 했을때 Accelerator (execute) time 90ms (37GBps) QNN (execute) time 170ms (20GBps)인거 고려하면 detailed 처럼 나온거 같은데, 실제 실행은 client로 했음.
        - 1x8x4096 200회에서는 최대 3GBps 정도. qnn profiling level client/basic로 했을때 latency 6ms (4GBps) 찍히는거 보면 비례해서 낮게 나오는듯.
    - 1ms 단위로 측정하게 하면 에러 날때도 있고 될때도 있고... 한 번 되면 계속 됨. 안될때 다른 qnn kernel 같은거 가지고 했다가 다시 돌아오면 되기도 하고...


qprof v2.25.8.4 기준 PrintCapabilities() 결과 (v2.25.12.12에서는 아예 안 나옴):
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