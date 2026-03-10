TODO
- qprof cli로 qprof와 app을 동시에 돌리는 제대로 된 방법 쓰기 (launch-app)

Setting
- Device: Galaxy S25+
- Qualcomm Profiler version: v2.25.12.12

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
[03:51:00.380] Capabilities count: 12
[03:51:00.380] Capability 0: [profiler:apps-proc-mem-metrics]
[03:51:00.380]   samplingRates  (16): 50ms 60ms 70ms 80ms 90ms 100ms 110ms 120ms 130ms 140ms 150ms 160ms 170ms 180ms 190ms 200ms
[03:51:00.380]   streamingRates (2): 200ms 1000ms
[03:51:00.380] Capability 1: [profiler:apps-proc-process-metrics]
[03:51:00.380]   samplingRates  (2): 50ms 200ms
[03:51:00.380]   streamingRates (2): 200ms 1000ms
[03:51:00.380] Capability 2: [profiler:proc-gpu-specific-metrics]
[03:51:00.380]   samplingRates  (2): 50ms 70ms
[03:51:00.380]   streamingRates (2): 200ms 1000ms
[03:51:00.380] Capability 3: [profiler:apps-proc-thread-profiling]
[03:51:00.380]   samplingRates  (2): 100ms 200ms
[03:51:00.380]   streamingRates (2): 200ms 400ms
[03:51:00.380] Capability 4: [profiler:nsp-dsp-metrics]
[03:51:00.380]   samplingRates  (2): 1ms 10ms
[03:51:00.380]   streamingRates (2): 200ms 1000ms
[03:51:00.380] Capability 5: [profiler:apps-proc-cpu-metrics]
[03:51:00.380]   samplingRates  (2): 50ms 200ms
[03:51:00.380]   streamingRates (2): 200ms 1000ms
[03:51:00.380] Capability 6: [profiler:apps-proc-io-metrics]
[03:51:00.380]   samplingRates  (2): 200ms 1000ms
[03:51:00.380]   streamingRates (1): 1000ms
[03:51:00.380] Capability 7: [profiler:apps-proc-process-mem-metrics]
[03:51:00.380]   samplingRates  (2): 50ms 200ms
[03:51:00.380]   streamingRates (2): 200ms 1000ms
[03:51:00.381] Capability 8: [profiler:apps-proc-net-metrics]
[03:51:00.381]   samplingRates  (2): 50ms 200ms
[03:51:00.381]   streamingRates (2): 200ms 1000ms
[03:51:00.381] Capability 9: [profiler:apps-proc-ddr-metrics]
[03:51:00.381]   samplingRates  (1): 10ms
[03:51:00.381]   streamingRates (2): 200ms 500ms
[03:51:00.381] Capability 10: [profiler:apps-proc-thermal-metrics]
[03:51:00.381]   samplingRates  (16): 50ms 60ms 70ms 80ms 90ms 100ms 110ms 120ms 130ms 140ms 150ms 160ms 170ms 180ms 190ms 200ms
[03:51:00.381]   streamingRates (2): 200ms 1000ms
[03:51:00.381] Capability 11: [profiler:nsp-dsp-stats]
[03:51:00.381]   samplingRates  (2): 1000ms 2000ms
[03:51:00.381]   streamingRates (2): 1000ms 2000ms
[03:51:00.389] Warning: bw-profiler-ddr-metrics not supported on this device, skipping
```