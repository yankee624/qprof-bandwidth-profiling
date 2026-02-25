#include "Profiler.hpp"

#include <cstring>
#include <cstdio>
#include <iostream>

#define PROFILER_CONFIG_MAX_LENGTH 1024

namespace qprof {

static std::vector<double> buffer_;
static std::vector<double> min_buffer_;
static std::vector<double> max_buffer_;
static std::vector<double> avg_buffer_;
static uint32_t count_ = 0;

Profiler::Profiler(void (*result_callback)(LpProfilingResult), void (*message_callback)(LpProfilingMessage)) {
  context_request_ = new ContextRequest();
  auto status = qp_initialize(context_request_, nullptr);
  if (status != RETURN_CODE_SUCCESS) {
    fprintf(stderr, "Failed to initialize QProfiler API: %d\n", static_cast<int>(status));
    abort();
  }

  auto capability_name = "profiler:apps-proc-ddr-metrics";

  start_config_ = new ProfilingEventStartConfiguration();
  start_config_->capabilityName.capabilityNameLen = snprintf((char*)start_config_->capabilityName.capabilityName,
                                                             CAPABILITY_NAME_LENGTH, "%s", capability_name);
  start_config_->metricIds.metricIdsLen = 3;
  start_config_->metricIds.metricIds[0] = 4661;  // APPS0: CPU small cluster bandwidth (MBps)
  start_config_->metricIds.metricIds[1] = 4662;  // APPS1: CPU big cluster bandwidth (MBps)
  start_config_->metricIds.metricIds[2] = 4663;  // GPU bandwidth (MBps)

  start_config_->streamingRate = 200;
  start_config_->samplingRate = 10;

  start_config_->resultType = RESULT_TYPE_GENERIC_STRUCT;
  start_config_->profilerConfig = nullptr;

  stop_config_ = new ProfilingEventStopConfiguration();
  stop_config_->capabilityName.capabilityNameLen = snprintf((char*)stop_config_->capabilityName.capabilityName,
                                                            CAPABILITY_NAME_LENGTH, "%s", capability_name);
  stop_config_->metricIds.metricIdsLen = 0;

  qp_setResultCallback(context_request_, result_callback);
  qp_setMessageCallback(context_request_, message_callback);
}

Profiler::~Profiler() {
  if (start_config_ != nullptr) {
    delete start_config_;
  }

  if (stop_config_ != nullptr) {
    delete stop_config_;
  }

  if (qp_destroy(context_request_) != RETURN_CODE_SUCCESS) {
    fprintf(stderr, "Failed to destroy QProfiler API context\n");
    abort();
  }

  if (context_request_ != nullptr) {
    delete context_request_;
  }

  fprintf(stdout, "Completed %u profiling iterations\n", count_);
}

void Profiler::Start() {
  auto status = qp_start(context_request_, start_config_);
  if (status != RETURN_CODE_SUCCESS) {
    fprintf(stderr, "Failed to start QProfiler API: %d\n", static_cast<int>(status));
    abort();
  }
}

void Profiler::Stop() {
  auto status = qp_stop(context_request_, stop_config_);
  if (status != RETURN_CODE_SUCCESS) {
    fprintf(stderr, "Failed to stop QProfiler API: %d\n", static_cast<int>(status));
    abort();
  }
  count_++;

  double max = std::numeric_limits<double>::lowest();
  double min = std::numeric_limits<double>::max();
  double sum = 0.0;
  for (auto value : buffer_) {
    if (value > max) {
      max = value;
    }

    if (value < min && value > 0.0) {
      min = value;
    }

    sum += value;
  }

  double avg = (buffer_.size() > 0) ? (sum / buffer_.size()) : 0.0;

  min_buffer_.push_back(min);
  max_buffer_.push_back(max);
  avg_buffer_.push_back(avg);

  if (file_ != nullptr) {
    *file_ << min << "," << max << "," << avg << ",";
    file_->flush();
  }

  buffer_.clear();
}

void Profiler::ResultCallback(LpProfilingResult profiling_result) {
  if (profiling_result == nullptr) {
    return;
  }

  for (uint32_t i = 0; i < profiling_result->profilingResultGeneric->metricResponseLen; i++) {
    auto& value = profiling_result->profilingResultGeneric->metricResponse[i].value.doubleValue;

    buffer_.push_back(value);
    // print the results
    std::cout << "Metric ID: " << profiling_result->profilingResultGeneric->metricResponse[i].metricId
              << ", Value: " << value << std::endl;
  }

  if (qp_freeProfilingResult(profiling_result) != RETURN_CODE_SUCCESS) {
    fprintf(stderr, "Failed to free profiling result");
    abort();
  }
}

void Profiler::MessageCallback(LpProfilingMessage profiling_message) {
  if (profiling_message == nullptr) {
    return;
  }

  std::cout << "Profiling Message: " << std::endl;
  std::cout << profiling_message->message << std::endl;
}

}  // namespace qprof