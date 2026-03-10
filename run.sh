#!/bin/bash

set -e

cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a
cmake --build build -j4

# Download Qualcomm Profiler. Then, run
# /opt/qcom/Shared/QualcommProfiler/API/target-la/InstallerLA

adb forward tcp:62472 tcp:62472

adb push build/qprof_example /data/local/tmp/

mkdir -p logs
OUTPUT_FILE="logs/profiler.log"
adb shell "export QMONITOR_BACKEND_LIB_PATH=/vendor/qprof/backends && export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/vendor/qprof/libs/ && /data/local/tmp/qprof_example"  2>&1 | tee "$OUTPUT_FILE"

python3 analyze_nsp_bw.py "$OUTPUT_FILE"