#ifndef QPROF_PROFILER_H_
#define QPROF_PROFILER_H_

#include "QProfilerApi.h"

namespace qprof {

class Profiler {
 public:
  static Profiler& Get() {
    static Profiler instance;
    return instance;
  }

  Profiler(void (*result_callback)(LpProfilingResult) = ResultCallback,
           void (*message_callback)(LpProfilingMessage) = MessageCallback);
  ~Profiler();

  void Start();
  void Stop();
  void PrintCapabilities();

  static void ResultCallback(LpProfilingResult profiling_result);
  static void MessageCallback(LpProfilingMessage profiling_message);

 private:
  bool bw_started_ = false;
  bool nsp_started_ = false;
  bool stats_started_ = false;

  LpContextRequest context_request_ = nullptr;
  LpProfilingEventStartConfiguration start_config_ = nullptr;
  LpProfilingEventStopConfiguration stop_config_ = nullptr;
  LpProfilingEventStartConfiguration start_config_bw_ = nullptr;
  LpProfilingEventStopConfiguration stop_config_bw_ = nullptr;
  LpProfilingEventStartConfiguration start_config_nsp_ = nullptr;
  LpProfilingEventStopConfiguration stop_config_nsp_ = nullptr;
  LpProfilingEventStartConfiguration start_config_stats_ = nullptr;
  LpProfilingEventStopConfiguration stop_config_stats_ = nullptr;
};

}  // namespace qprof

#endif  // QPROF_PROFILER_H_