#ifndef QPROF_PROFILER_H_
#define QPROF_PROFILER_H_

#include "QProfilerApi.h"

#include <vector>
#include <ostream>

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
  void SetFile(std::ostream& file) {
    file_ = &file;
  }

  static void ResultCallback(LpProfilingResult profiling_result);
  static void MessageCallback(LpProfilingMessage profiling_message);

 private:
  std::ostream* file_ = nullptr;

  LpContextRequest context_request_ = nullptr;
  LpProfilingEventStartConfiguration start_config_ = nullptr;
  LpProfilingEventStopConfiguration stop_config_ = nullptr;
};

}  // namespace qprof

#endif  // QPROF_PROFILER_H_