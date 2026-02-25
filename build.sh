#!/bin/bash

set -e

cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a
cmake --build build -j4

# Download Qualcomm Profiler. Then,
# Run /opt/qcom/Shared/QualcommProfiler/API/target-la/InstallerLA

adb forward tcp:62472 tcp:62472

adb push build/qprof_example /data/local/tmp/
adb shell "export QMONITOR_BACKEND_LIB_PATH=/vendor/qprof/backends && export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/vendor/qprof/libs/ && /data/local/tmp/qprof_example"


########## IGNORE BELOW ##########

# cmake -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
#       -DANDROID_PLATFORM=android-26 -DANDROID_ABI=arm64-v8a \
#       -DCMAKE_BUILD_TYPE=Release ..
# cmake --build . --config Release -j
# adb push ./android_ddr_bw_bench /data/local/tmp/
# adb shell chmod +x /data/local/tmp/android_ddr_bw_bench
# adb shell /data/local/tmp/android_ddr_bw_bench --mode=triad --size=1024 --threads=8 --seconds=5
# adb shell /data/local/tmp/android_ddr_bw_bench --mode=read  --size=2048 --threads=8 --seconds=5
# adb shell /data/local/tmp/android_ddr_bw_bench --mode=write --size=2048 --threads=8 --seconds=5
# adb shell /data/local/tmp/android_ddr_bw_bench --mode=copy  --size=2048 --threads=8 --seconds=5


# adb shell simpleperf stat -e armv8_pmuv3/l2d_cache_refill/ -- /data/local/tmp/android_ddr_bw_bench --mode=read  --size=2048 --threads=1 --seconds=5

# adb shell simpleperf stat \
#   -e armv8_pmuv3/l2d_cache_refill/,armv8_pmuv3/bus_access/,armv8_pmuv3/stall_backend_mem -- /data/local/tmp/android_ddr_bw_bench --mode=read  --size=2048 --threads=1 --seconds=5

# adb shell simpleperf stat -a -e llcc-pmu/config=0x1000/ --duration 5  -- /data/local/tmp/android_ddr_bw_bench --mode=read  --size=2048 --threads=1 --seconds=5


# adb shell simpleperf stat -- /data/local/tmp/android_ddr_bw_bench --mode=read  --size=2048 --threads=1 --seconds=5