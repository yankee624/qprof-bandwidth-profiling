// android_ddr_bw_bench.cpp
// A self‑contained CPU DDR bandwidth microbenchmark for Android/Linux (AArch64-ready)
// Measures read, write, copy, and STREAM triad bandwidth with configurable
// working set, threads, stride, and duration.
//
// Build (Android NDK + CMake example):
//   # In your project root, create/extend CMakeLists.txt with:
//   # add_executable(android_ddr_bw_bench android_ddr_bw_bench.cpp)
//   # target_compile_features(android_ddr_bw_bench PRIVATE cxx_std_17)
//   # target_link_libraries(android_ddr_bw_bench PRIVATE log)
//   mkdir build && cd build
//   cmake -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
//         -DANDROID_PLATFORM=android-26 -DANDROID_ABI=arm64-v8a \
//         -DCMAKE_BUILD_TYPE=Release ..
//   cmake --build . --config Release -j
//   adb push ./android_ddr_bw_bench /data/local/tmp/ && \
//   adb shell chmod +x /data/local/tmp/android_ddr_bw_bench
//
// Run:
//   adb shell /data/local/tmp/android_ddr_bw_bench --mode=triad --size=1024 --threads=8 --seconds=5
//   adb shell /data/local/tmp/android_ddr_bw_bench --help
//
// Notes:
//   * Choose size (MB) >> LLC (e.g., 512–2048 MB) to minimize cache effects.
//   * Pin threads with --pin and optionally set a core mask via --first-core / --core-step.
//   * Run multiple modes; the max of write/copy/triad approximates sustained DDR BW.
//   * For cleaner results: set performance mode, keep device cool, and keep screen on.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <getopt.h>
#include <pthread.h>
#include <sched.h>
#include <string>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

#ifndef CACHELINE
#define CACHELINE 64
#endif

static inline double now_sec() {
  timespec ts; clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

struct Options {
  std::string mode = "triad"; // read|write|copy|triad
  size_t size_mb = 1024;       // per ARRAY size in MB (copy/triad allocate 2–3 arrays)
  int threads = std::max(1u, std::thread::hardware_concurrency());
  int seconds = 5;
  size_t stride = 1;           // elements (8 bytes each)
  bool pin = true;
  int first_core = 0;          // starting CPU for pinning
  int core_step = 1;           // step between cores when pinning
  bool huge = false;           // try huge pages via mmap transparently (best-effort)
};

static void set_affinity(int cpu) {
  cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu, &set);
  // Android NDK does not expose pthread_setaffinity_np.
  // Use sched_setaffinity on the current thread id (tid).
  pid_t tid = gettid();
  sched_setaffinity(tid, sizeof(set), &set);
}

static void touch_pages(uint8_t* p, size_t bytes) {
  const size_t page = 4096;
  for (size_t i = 0; i < bytes; i += page) p[i] = static_cast<uint8_t>(i);
}

struct ThreadResult { double bytes = 0; double secs = 0; };

// Worker kernels operate on uint64_t to encourage wider loads/stores
static ThreadResult run_read(uint64_t* A, size_t N, size_t stride, double end_time) {
  volatile uint64_t sink = 0; // prevent dead-code elimination
  size_t i = 0, s = stride;
  double bytes = 0; double t0 = now_sec();
  while (now_sec() < end_time) {
    // manual unroll
    for (int u = 0; u < 256; ++u) { sink += A[i]; i += s; if (i >= N) i -= N; }
    bytes += 256ull * sizeof(uint64_t);
  }
  (void)sink;
  double secs = now_sec() - t0; return {bytes, secs};
}

static ThreadResult run_write(uint64_t* A, size_t N, size_t stride, double end_time) {
  uint64_t val = 0xdeadbeefcafebabeull;
  size_t i = 0, s = stride; double bytes = 0; double t0 = now_sec();
  while (now_sec() < end_time) {
    for (int u = 0; u < 256; ++u) { A[i] = val; i += s; if (i >= N) i -= N; }
    bytes += 256ull * sizeof(uint64_t);
  }
  double secs = now_sec() - t0; return {bytes, secs};
}

static ThreadResult run_copy(uint64_t* __restrict__ A, uint64_t* __restrict__ B,
                             size_t N, size_t stride, double end_time) {
  size_t i = 0, s = stride; double bytes = 0; double t0 = now_sec();
  while (now_sec() < end_time) {
    for (int u = 0; u < 256; ++u) { A[i] = B[i]; i += s; if (i >= N) i -= N; }
    bytes += 256ull * sizeof(uint64_t) * 2ull;
  }
  double secs = now_sec() - t0; return {bytes, secs};
}

static ThreadResult run_triad(uint64_t* __restrict__ A, uint64_t* __restrict__ B,
                              uint64_t* __restrict__ C, size_t N, size_t stride,
                              double end_time) {
  const uint64_t sconst = 3;
  size_t i = 0, s = stride; double bytes = 0; double t0 = now_sec();
  while (now_sec() < end_time) {
    for (int u = 0; u < 256; ++u) { A[i] = B[i] + sconst * C[i]; i += s; if (i >= N) i -= N; }
    // triad moves 3 arrays (2 reads + 1 write)
    bytes += 256ull * sizeof(uint64_t) * 3ull;
  }
  double secs = now_sec() - t0; return {bytes, secs};
}

static void* huge_alloc(size_t bytes, bool huge) {
  // Best-effort transparent hugepages
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef MAP_HUGETLB
  if (huge) flags |= MAP_HUGETLB;
#endif
  void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, flags, -1, 0);
  if (p == MAP_FAILED) return nullptr;
  return p;
}

static void huge_free(void* p, size_t bytes) { if (p) munmap(p, bytes); }

static void usage(const char* prog) {
  printf("Usage: %s [--mode=read|write|copy|triad] --size=MB --threads=N --seconds=S [--stride=elts] [--pin] [--first-core=i] [--core-step=k] [--huge]\n", prog);
}

int main(int argc, char** argv) {
  Options opt;
  static struct option long_opts[] = {
    {"mode", required_argument, 0, 'm'},
    {"size", required_argument, 0, 's'},
    {"threads", required_argument, 0, 't'},
    {"seconds", required_argument, 0, 'd'},
    {"stride", required_argument, 0, 'r'},
    {"pin", no_argument, 0, 'p'},
    {"first-core", required_argument, 0, 'f'},
    {"core-step", required_argument, 0, 'c'},
    {"huge", no_argument, 0, 'h'},
    {"help", no_argument, 0, '?'},
    {0,0,0,0}
  };

  int c;
  while ((c = getopt_long(argc, argv, "", long_opts, nullptr)) != -1) {
    switch (c) {
      case 'm': opt.mode = optarg; break;
      case 's': opt.size_mb = std::strtoul(optarg, nullptr, 10); break;
      case 't': opt.threads = std::max(1, (int)std::strtol(optarg, nullptr, 10)); break;
      case 'd': opt.seconds = std::max(1, (int)std::strtol(optarg, nullptr, 10)); break;
      case 'r': opt.stride = std::max<size_t>(1, std::strtoull(optarg, nullptr, 10)); break;
      case 'p': opt.pin = true; break;
      case 'f': opt.first_core = std::strtol(optarg, nullptr, 10); break;
      case 'c': opt.core_step = std::max(1, (int)std::strtol(optarg, nullptr, 10)); break;
      case 'h': opt.huge = true; break;
      case '?': default: usage(argv[0]); return 0;
    }
  }

  // Raise priority to reduce jitter
  setpriority(PRIO_PROCESS, 0, -10);

  // Allocate arrays
  size_t N = (opt.size_mb * 1024ull * 1024ull) / sizeof(uint64_t);
  size_t bytesA = N * sizeof(uint64_t);
  size_t bytesB = (opt.mode == "copy" || opt.mode == "triad") ? bytesA : 0;
  size_t bytesC = (opt.mode == "triad") ? bytesA : 0;

  uint64_t* A = (uint64_t*) huge_alloc(bytesA, opt.huge);
  if (!A) { fprintf(stderr, "alloc A failed\n"); return 1; }
  uint64_t* B = nullptr; uint64_t* C = nullptr;
  if (bytesB) { B = (uint64_t*) huge_alloc(bytesB, opt.huge); if (!B) { fprintf(stderr, "alloc B failed\n"); return 1; } }
  if (bytesC) { C = (uint64_t*) huge_alloc(bytesC, opt.huge); if (!C) { fprintf(stderr, "alloc C failed\n"); return 1; } }

  // Touch pages to fault-in memory
  touch_pages((uint8_t*)A, bytesA); if (B) touch_pages((uint8_t*)B, bytesB); if (C) touch_pages((uint8_t*)C, bytesC);

  // Init contents
  for (size_t i = 0; i < N; ++i) A[i] = i;
  if (B) for (size_t i = 0; i < N; ++i) B[i] = 2*i+1;
  if (C) for (size_t i = 0; i < N; ++i) C[i] = 3*i+7;

  printf("mode=%s size=%zuMB stride=%zu elts threads=%d duration=%ds pin=%s\n",
         opt.mode.c_str(), opt.size_mb, opt.stride, opt.threads, opt.seconds, opt.pin?"yes":"no");

  // Launch workers
  std::vector<std::thread> th;
  std::vector<ThreadResult> res(opt.threads);
  std::atomic<bool> go{false};
  double start_time = 0.0;
  double end_time = 0.0;

  for (int tid = 0; tid < opt.threads; ++tid) {
    th.emplace_back([&, tid]() {
      if (opt.pin) {
        int cpu = opt.first_core + tid*opt.core_step;
        set_affinity(cpu);
      }
      // Stagger indices to reduce aliasing
      size_t offset = (N / opt.threads) * tid;
      uint64_t* Ap = A + offset;
      uint64_t* Bp = B ? (B + offset) : nullptr;
      uint64_t* Cp = C ? (C + offset) : nullptr;

      while (!go.load(std::memory_order_acquire)) {}
      double t_end = start_time + opt.seconds;

      if (opt.mode == "read") res[tid] = run_read(Ap, N - offset, opt.stride, t_end);
      else if (opt.mode == "write") res[tid] = run_write(Ap, N - offset, opt.stride, t_end);
      else if (opt.mode == "copy") res[tid] = run_copy(Ap, Bp, N - offset, opt.stride, t_end);
      else /*triad*/ res[tid] = run_triad(Ap, Bp, Cp, N - offset, opt.stride, t_end);
    });
  }

  // Warmup 0.5s then go
  usleep(200000);
  start_time = now_sec(); go.store(true, std::memory_order_release);
  end_time = start_time + opt.seconds;

  for (auto& t : th) t.join();

  // Aggregate
  double total_bytes = 0, max_gbps = 0; int tid_max = -1;
  for (int i = 0; i < opt.threads; ++i) {
    double gbps = (res[i].bytes / res[i].secs) / 1e9;
    printf("thread %2d: %.2f GB/s\n", i, gbps);
    total_bytes += res[i].bytes;
    if (gbps > max_gbps) { max_gbps = gbps; tid_max = i; }
  }
  double duration = opt.seconds; // approx, per-thread measures similar
  double total_gbps = (total_bytes / duration) / 1e9;
  printf("TOTAL: %.2f GB/s (mode=%s, threads=%d)\n", total_gbps, opt.mode.c_str(), opt.threads);

  huge_free(A, bytesA); if (B) huge_free(B, bytesB); if (C) huge_free(C, bytesC);
  return 0;
}
