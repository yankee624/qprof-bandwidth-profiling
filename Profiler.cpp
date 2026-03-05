#include "Profiler.hpp"

#include <cstdio>
#include <iostream>
#include <unordered_map>

namespace qprof {

static const std::unordered_map<uint32_t, const char*> kMetricNames = {
  // apps-proc-ddr-metrics
  {4625, "LLCC DDR Total Bandwidth"},
  {4626, "LLCC DDR Read Bandwidth"},
  {4627, "LLCC DDR Write Bandwidth"},
  {4628, "DDR Total Bandwidth"},
  {4629, "DDR Read Bandwidth"},
  {4630, "DDR Write Bandwidth"},
  {4661, "NOC DDR APPS0 Bandwidth"},
  {4662, "NOC DDR APPS1 Bandwidth"},
  {4663, "NOC DDR GPU Bandwidth"},
  {4664, "NOC DDR NSP Bandwidth (NSP0)"},
  {4665, "NOC DDR NSP1 Bandwidth"},
  {4666, "NOC DDR NSP2 Bandwidth"},
  {4667, "NOC DDR NSP3 Bandwidth"},
  {4668, "NOC DDR Total Bandwidth"},
  {4670, "LLC CPU Miss Rate"},
  {4671, "LLC GPU Miss Rate"},
  {4672, "LLC NSP Miss Rate"},
  // bw-profiler-ddr-metrics
  {5632, "GPU BW at DDR"},
  {5633, "CPU BW at DDR"},
  {5638, "NSP BW at DDR"},
  // nsp-dsp-metrics: DSP internal PMU counters
  // AXI bus bandwidth
  {4141, "AXI 128Byte read request"},
  {4142, "AXI 128Byte write request"},
  {4143, "AXI 256 Byte write request"},
  {4144, "AXI 256 Byte read request"},
  // IU (instruction unit) L2 cache
  {4152, "IU cache read from L2"},
  {4153, "IU cache read from DDR"},
  {4154, "IU cache read from TCM"},
  {4155, "IU cache read L2 miss ratio"},
  {4156, "IU cache prefetch from L2"},
  {4157, "IU cache prefetch from DDR"},
  {4158, "IU cache prefetch miss ratio"},
  // DU (data unit) L2 cache
  {4159, "DU cache read from L2"},
  {4160, "DU cache read from TCM"},
  {4161, "DU cache read from DDR"},
  {4162, "DU cache read L2 miss ratio"},
  {4163, "DU cache prefetch from DDR"},
  {4164, "DU cache prefetch from L2"},
  {4165, "DU cache prefetch miss ratio"},
  {4166, "DU cache write to L2"},
  {4167, "DU cache write to TCM"},
  {4168, "DU cache write to DDR"},
  {4169, "DU cache write miss ratio"},
  // L2FETCH engine
  {4170, "L2 FETCH engine access"},
  {4171, "L2 FETCH engine miss"},
  // BLC (bus latency counter)
  {4179, "BLC latency per read request"},
  {4181, "BLC latency per txn"},
  // HVX L2 bandwidth / miss (HVX_INSTRUCTION, HVX_L2 subcategories)
  {4369, "HVX L2 stores"},
  {4370, "HVX L2 store miss"},
  {4371, "HVX L2 loads"},
  {4372, "HVX L2 load miss"},
  {4373, "HVX L2 load miss ratio"},
  {4374, "HVX L2 store miss ratio"},
  {4387, "HVX L2 BW"},
  // DDR BW (128B bus-level, for roofline)
  {4388, "DDR BW 128B"},
  // HMX bandwidth / utilization (HMX subcategory)
  {4392, "HMX VTCM BW"},
  {4393, "HMX DDR BW"},
  {4480, "HMX Utilization"},
  {4481, "HMX Active"},
  {4521, "HMX Clock"},
  // uDMA bandwidth (UDMA subcategory)
  {4504, "UDMA L2 coherent WR"},
  {4505, "UDMA L2 coherent WR miss"},
  {4506, "UDMA L2 coherent RD"},
  {4507, "UDMA L2 coherent RD miss"},
  {4508, "UDMA L2 non-coherent WR"},
  {4509, "UDMA L2 non-coherent RD"},
  {4510, "UDMA VTCM store"},
  {4511, "UDMA VTCM reads"},
};

static uint32_t count_ = 0;

Profiler::Profiler(void (*result_callback)(LpProfilingResult), void (*message_callback)(LpProfilingMessage)) {
  context_request_ = new ContextRequest();
  auto status = qp_initialize(context_request_, nullptr);
  if (status != RETURN_CODE_SUCCESS) {
    fprintf(stderr, "Failed to initialize QProfiler API: %d\n", static_cast<int>(status));
    abort();
  }

  // --- Capability 1: apps-proc-ddr-metrics ---
  // NOC-level and LLCC/DDR controller bandwidth metrics
  start_config_ = new ProfilingEventStartConfiguration();
  start_config_->capabilityName.capabilityNameLen = snprintf((char*)start_config_->capabilityName.capabilityName,
                                                             CAPABILITY_NAME_LENGTH, "profiler:apps-proc-ddr-metrics");
  start_config_->metricIds.metricIdsLen = 17;
  start_config_->metricIds.metricIds[0]  = 4625;  // LLCC DDR Total Bandwidth     (DDR Controller Total)  MBps
  start_config_->metricIds.metricIds[1]  = 4626;  // LLCC DDR read bandwidth      (DDR Controller read)   MBps
  start_config_->metricIds.metricIds[2]  = 4627;  // LLCC DDR write bandwidth     (DDR Controller write)  MBps
  start_config_->metricIds.metricIds[3]  = 4628;  // DDR Total Bandwidth                                  MBps
  start_config_->metricIds.metricIds[4]  = 4629;  // DDR Read Bandwidth           (DDR Total Read)        MBps
  start_config_->metricIds.metricIds[5]  = 4630;  // DDR Write Bandwidth          (DDR Total Write)       MBps
  start_config_->metricIds.metricIds[6]  = 4661;  // NOC DDR APPS0 Bandwidth                              MBps
  start_config_->metricIds.metricIds[7]  = 4662;  // NOC DDR APPS1 Bandwidth                              MBps
  start_config_->metricIds.metricIds[8]  = 4663;  // NOC DDR GPU Bandwidth                                MBps
  start_config_->metricIds.metricIds[9]  = 4664;  // NOC DDR NSP Bandwidth  (NOC DDR NSP0)                MBps
  start_config_->metricIds.metricIds[10] = 4665;  // NOC DDR NSP1 Bandwidth                               MBps
  start_config_->metricIds.metricIds[11] = 4666;  // NOC DDR NSP2 Bandwidth                               MBps
  start_config_->metricIds.metricIds[12] = 4667;  // NOC DDR NSP3 Bandwidth                               MBps
  start_config_->metricIds.metricIds[13] = 4668;  // NOC DDR Total Bandwidth                              MBps
  start_config_->metricIds.metricIds[14] = 4670;  // LLC CPU Miss Rate                                    %
  start_config_->metricIds.metricIds[15] = 4671;  // LLC GPU Miss Rate                                    %
  start_config_->metricIds.metricIds[16] = 4672;  // LLC NSP Miss Rate                                    %

  start_config_->streamingRate = 200;
  start_config_->samplingRate = 10;
  start_config_->resultType = RESULT_TYPE_GENERIC_STRUCT;
  start_config_->profilerConfig = nullptr;

  stop_config_ = new ProfilingEventStopConfiguration();
  stop_config_->capabilityName.capabilityNameLen = snprintf((char*)stop_config_->capabilityName.capabilityName,
                                                            CAPABILITY_NAME_LENGTH, "profiler:apps-proc-ddr-metrics");
  stop_config_->metricIds.metricIdsLen = 0;

  // --- Capability 2: bw-profiler-ddr-metrics ---
  // DDRSS-level per-client bandwidth (CPU/GPU/NSP BW at DDR)
  start_config_bw_ = new ProfilingEventStartConfiguration();
  start_config_bw_->capabilityName.capabilityNameLen = snprintf((char*)start_config_bw_->capabilityName.capabilityName,
                                                                CAPABILITY_NAME_LENGTH, "profiler:bw-profiler-ddr-metrics");
  start_config_bw_->metricIds.metricIdsLen = 3;
  start_config_bw_->metricIds.metricIds[0] = 5632;  // GPU BW at DDR   MBps
  start_config_bw_->metricIds.metricIds[1] = 5633;  // CPU BW at DDR   MBps
  start_config_bw_->metricIds.metricIds[2] = 5638;  // NSP BW at DDR   MBps

  start_config_bw_->streamingRate = 200;
  start_config_bw_->samplingRate = 10;
  start_config_bw_->resultType = RESULT_TYPE_GENERIC_STRUCT;
  start_config_bw_->profilerConfig = nullptr;

  stop_config_bw_ = new ProfilingEventStopConfiguration();
  stop_config_bw_->capabilityName.capabilityNameLen = snprintf((char*)stop_config_bw_->capabilityName.capabilityName,
                                                               CAPABILITY_NAME_LENGTH, "profiler:bw-profiler-ddr-metrics");
  stop_config_bw_->metricIds.metricIdsLen = 0;

  // --- Capability 3: nsp-dsp-metrics ---
  // DSP internal PMU counters for NSP0 (NPU0) — fine-grained per-unit BW and L2 miss metrics
  start_config_nsp_ = new ProfilingEventStartConfiguration();
  start_config_nsp_->capabilityName.capabilityNameLen = snprintf((char*)start_config_nsp_->capabilityName.capabilityName,
                                                                 CAPABILITY_NAME_LENGTH, "profiler:nsp-dsp-metrics");
  start_config_nsp_->metricIds.metricIdsLen = 45;
  // AXI bus bandwidth (total L2<->DDR traffic at bus level)
  start_config_nsp_->metricIds.metricIds[0]  = 4141;  // AXI 128Byte read request        MBps
  start_config_nsp_->metricIds.metricIds[1]  = 4142;  // AXI 128Byte write request       MBps
  start_config_nsp_->metricIds.metricIds[2]  = 4143;  // AXI 256 Byte write request      MBps
  start_config_nsp_->metricIds.metricIds[3]  = 4144;  // AXI 256 Byte read request       MBps
  // IU (instruction unit) L2 cache bandwidth / miss
  start_config_nsp_->metricIds.metricIds[4]  = 4152;  // IU cache read from L2           MBps
  start_config_nsp_->metricIds.metricIds[5]  = 4153;  // IU cache read from DDR          MBps  (L2 miss → DDR)
  start_config_nsp_->metricIds.metricIds[6]  = 4154;  // IU cache read from TCM          MBps
  start_config_nsp_->metricIds.metricIds[7]  = 4155;  // IU cache read L2 miss ratio     %
  start_config_nsp_->metricIds.metricIds[8]  = 4156;  // IU cache prefetch from L2       MBps
  start_config_nsp_->metricIds.metricIds[9]  = 4157;  // IU cache prefetch from DDR      MBps  (L2 miss → DDR)
  start_config_nsp_->metricIds.metricIds[10] = 4158;  // IU cache prefetch miss ratio    %
  // DU (data unit) L2 cache bandwidth / miss
  start_config_nsp_->metricIds.metricIds[11] = 4159;  // DU cache read from L2           MBps
  start_config_nsp_->metricIds.metricIds[12] = 4160;  // DU cache read from TCM          MBps
  start_config_nsp_->metricIds.metricIds[13] = 4161;  // DU cache read from DDR          MBps  (L2 miss → DDR)
  start_config_nsp_->metricIds.metricIds[14] = 4162;  // DU cache read L2 miss ratio     %
  start_config_nsp_->metricIds.metricIds[15] = 4163;  // DU cache prefetch from DDR      MBps  (L2 miss → DDR)
  start_config_nsp_->metricIds.metricIds[16] = 4164;  // DU cache prefetch from L2       MBps
  start_config_nsp_->metricIds.metricIds[17] = 4165;  // DU cache prefetch miss ratio    %
  start_config_nsp_->metricIds.metricIds[18] = 4166;  // DU cache write to L2            MBps
  start_config_nsp_->metricIds.metricIds[19] = 4167;  // DU cache write to TCM           MBps
  start_config_nsp_->metricIds.metricIds[20] = 4168;  // DU cache write to DDR           MBps  (L2 miss → DDR)
  start_config_nsp_->metricIds.metricIds[21] = 4169;  // DU cache write miss ratio       %
  // L2FETCH engine
  start_config_nsp_->metricIds.metricIds[22] = 4170;  // L2 FETCH engine access          MBps
  start_config_nsp_->metricIds.metricIds[23] = 4171;  // L2 FETCH engine miss (DDR)      MBps
  // HVX L2 bandwidth / miss (HVX_INSTRUCTION, HVX_L2 subcategories)
  start_config_nsp_->metricIds.metricIds[24] = 4369;  // HVX L2 stores                   MBps
  start_config_nsp_->metricIds.metricIds[25] = 4370;  // HVX L2 store miss (→ DDR)       MBps
  start_config_nsp_->metricIds.metricIds[26] = 4371;  // HVX L2 loads                    MBps
  start_config_nsp_->metricIds.metricIds[27] = 4372;  // HVX L2 load miss (→ DDR)        MBps
  start_config_nsp_->metricIds.metricIds[28] = 4373;  // HVX L2 load miss ratio          %
  start_config_nsp_->metricIds.metricIds[29] = 4374;  // HVX L2 store miss ratio         %
  start_config_nsp_->metricIds.metricIds[30] = 4387;  // HVX L2 BW                       MBps
  // DDR BW 128B (bus-level, for roofline)
  start_config_nsp_->metricIds.metricIds[31] = 4388;  // DDR BW 128B                     MBps
  // HMX bandwidth (HMX subcategory)
  start_config_nsp_->metricIds.metricIds[32] = 4392;  // HMX VTCM BW                     MBps
  start_config_nsp_->metricIds.metricIds[33] = 4393;  // HMX DDR BW                      MBps
  // uDMA bandwidth (UDMA subcategory)
  start_config_nsp_->metricIds.metricIds[34] = 4504;  // UDMA L2 coherent WR             MBps
  start_config_nsp_->metricIds.metricIds[35] = 4505;  // UDMA L2 coherent WR miss        MBps
  start_config_nsp_->metricIds.metricIds[36] = 4506;  // UDMA L2 coherent RD             MBps
  start_config_nsp_->metricIds.metricIds[37] = 4507;  // UDMA L2 coherent RD miss        MBps
  start_config_nsp_->metricIds.metricIds[38] = 4508;  // UDMA L2 non-coherent WR         MBps
  start_config_nsp_->metricIds.metricIds[39] = 4509;  // UDMA L2 non-coherent RD         MBps
  start_config_nsp_->metricIds.metricIds[40] = 4510;  // UDMA VTCM store                 MBps
  start_config_nsp_->metricIds.metricIds[41] = 4511;  // UDMA VTCM reads                 MBps
  // HMX utilization / activity
  start_config_nsp_->metricIds.metricIds[42] = 4480;  // HMX Utilization                 %
  start_config_nsp_->metricIds.metricIds[43] = 4481;  // HMX Active                      MCPS
  start_config_nsp_->metricIds.metricIds[44] = 4521;  // HMX Clock                       MHz

  start_config_nsp_->streamingRate = 200;
  start_config_nsp_->samplingRate = 10;
  start_config_nsp_->resultType = RESULT_TYPE_GENERIC_STRUCT;
  start_config_nsp_->profilerConfig = nullptr;

  stop_config_nsp_ = new ProfilingEventStopConfiguration();
  stop_config_nsp_->capabilityName.capabilityNameLen = snprintf((char*)stop_config_nsp_->capabilityName.capabilityName,
                                                                CAPABILITY_NAME_LENGTH, "profiler:nsp-dsp-metrics");
  stop_config_nsp_->metricIds.metricIdsLen = 0;

  // --- Capability 4: nsp-dsp-stats ---
  // NSP bandwidth vote and measured bus clock (multi-field verbose struct)
  start_config_stats_ = new ProfilingEventStartConfiguration();
  start_config_stats_->capabilityName.capabilityNameLen = snprintf((char*)start_config_stats_->capabilityName.capabilityName,
                                                                   CAPABILITY_NAME_LENGTH, "profiler:nsp-dsp-stats");
  start_config_stats_->metricIds.metricIdsLen = 2;
  start_config_stats_->metricIds.metricIds[0] = 5889;  // Bandwidth Vote (SNOCVote, MEMNOCVote)        MHz
  start_config_stats_->metricIds.metricIds[1] = 5893;  // Measured Bus Clock (MeasuredMEMNOCClock, MeasuredBIMCClock) MHz

  start_config_stats_->streamingRate = 1000;
  start_config_stats_->samplingRate = 1000;
  start_config_stats_->resultType = RESULT_TYPE_VERBOSE_STRUCT;
  start_config_stats_->profilerConfig = nullptr;

  stop_config_stats_ = new ProfilingEventStopConfiguration();
  stop_config_stats_->capabilityName.capabilityNameLen = snprintf((char*)stop_config_stats_->capabilityName.capabilityName,
                                                                  CAPABILITY_NAME_LENGTH, "profiler:nsp-dsp-stats");
  stop_config_stats_->metricIds.metricIdsLen = 0;

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

  if (start_config_bw_ != nullptr) {
    delete start_config_bw_;
  }

  if (stop_config_bw_ != nullptr) {
    delete stop_config_bw_;
  }

  if (start_config_nsp_ != nullptr) {
    delete start_config_nsp_;
  }

  if (stop_config_nsp_ != nullptr) {
    delete stop_config_nsp_;
  }

  if (start_config_stats_ != nullptr) {
    delete start_config_stats_;
  }

  if (stop_config_stats_ != nullptr) {
    delete stop_config_stats_;
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

void Profiler::PrintCapabilities() {
  CapabilitiesResponse response = {};
  auto status = qp_getCapabilities(context_request_, &response);
  if (status != RETURN_CODE_SUCCESS) {
    fprintf(stderr, "Failed to get capabilities: %d\n", static_cast<int>(status));
    return;
  }

  for (uint8_t i = 0; i < response.capabilitiesLen; i++) {
    const auto& cap = response.capabilities[i];
    std::cout << "\n[" << cap.capabilityName.capabilityName << "]\n";

    std::cout << "  samplingRates  (" << cap.samplingRatesLen << "):";
    for (uint32_t j = 0; j < cap.samplingRatesLen; j++) {
      std::cout << " " << cap.samplingRates[j] << "ms";
    }
    std::cout << "\n";

    std::cout << "  streamingRates (" << cap.streamingRatesLen << "):";
    for (uint32_t j = 0; j < cap.streamingRatesLen; j++) {
      std::cout << " " << cap.streamingRates[j] << "ms";
    }
    std::cout << "\n";
  }
}

void Profiler::Start() {
  auto status = qp_start(context_request_, start_config_);
  if (status != RETURN_CODE_SUCCESS) {
    fprintf(stderr, "Failed to start apps-proc-ddr-metrics: %d\n", static_cast<int>(status));
    abort();
  }

  status = qp_start(context_request_, start_config_bw_);
  if (status == RETURN_CODE_WRONG_CAPABILITY) {
    fprintf(stderr, "Warning: bw-profiler-ddr-metrics not supported on this device, skipping\n");
    bw_started_ = false;
  } else if (status != RETURN_CODE_SUCCESS) {
    fprintf(stderr, "Failed to start bw-profiler-ddr-metrics: %d\n", static_cast<int>(status));
    abort();
  } else {
    bw_started_ = true;
  }

  status = qp_start(context_request_, start_config_nsp_);
  if (status == RETURN_CODE_WRONG_CAPABILITY) {
    fprintf(stderr, "Warning: nsp-dsp-metrics not supported on this device, skipping\n");
    nsp_started_ = false;
  } else if (status != RETURN_CODE_SUCCESS) {
    fprintf(stderr, "Failed to start nsp-dsp-metrics: %d\n", static_cast<int>(status));
    abort();
  } else {
    nsp_started_ = true;
  }

  status = qp_start(context_request_, start_config_stats_);
  if (status == RETURN_CODE_WRONG_CAPABILITY) {
    fprintf(stderr, "Warning: nsp-dsp-stats not supported on this device, skipping\n");
    stats_started_ = false;
  } else if (status != RETURN_CODE_SUCCESS) {
    fprintf(stderr, "Failed to start nsp-dsp-stats: %d\n", static_cast<int>(status));
    abort();
  } else {
    stats_started_ = true;
  }
}

void Profiler::Stop() {
  auto status = qp_stop(context_request_, stop_config_);
  if (status != RETURN_CODE_SUCCESS) {
    fprintf(stderr, "Failed to stop apps-proc-ddr-metrics: %d\n", static_cast<int>(status));
    abort();
  }

  if (bw_started_) {
    status = qp_stop(context_request_, stop_config_bw_);
    if (status != RETURN_CODE_SUCCESS) {
      fprintf(stderr, "Failed to stop bw-profiler-ddr-metrics: %d\n", static_cast<int>(status));
      abort();
    }
  }

  if (nsp_started_) {
    status = qp_stop(context_request_, stop_config_nsp_);
    if (status != RETURN_CODE_SUCCESS) {
      fprintf(stderr, "Failed to stop nsp-dsp-metrics: %d\n", static_cast<int>(status));
      abort();
    }
  }

  if (stats_started_) {
    status = qp_stop(context_request_, stop_config_stats_);
    if (status != RETURN_CODE_SUCCESS) {
      fprintf(stderr, "Failed to stop nsp-dsp-stats: %d\n", static_cast<int>(status));
      abort();
    }
  }

  count_++;
}

void Profiler::ResultCallback(LpProfilingResult profiling_result) {
  if (profiling_result == nullptr) {
    return;
  }

  if (profiling_result->resultType == RESULT_TYPE_VERBOSE_STRUCT) {
    // nsp-dsp-stats: multi-field verbose struct (e.g. Bandwidth Vote, Measured Bus Clock)
    auto* batch = profiling_result->profilingResultVerbose;
    if (batch != nullptr) {
      for (size_t i = 0; i < batch->num_results; i++) {
        auto* res = &batch->result_array[i];
        if (res->profileField == nullptr) continue;
        auto* field = res->profileField;
        std::cout << "Metric ID: " << field->fieldId
                  << " (" << (field->name ? field->name : "?") << ")";
        // multi-field dataset (e.g. SNOCVote, MEMNOCVote in Bandwidth Vote)
        for (size_t j = 0; j < field->num_fieldDataSet; j++) {
          auto& ds = field->fieldDataSet_array[j];
          std::cout << ", " << (ds.name ? ds.name : "?")
                    << ": " << (ds.value ? ds.value : "?")
                    << " " << (ds.unit ? ds.unit : "");
        }
        // field params (alternative multi-value layout)
        for (size_t j = 0; j < field->num_fieldParams; j++) {
          auto& p = field->fieldParams_array[j];
          if (p.value && p.valueLen > 0) {
            std::cout << ", " << (p.name ? p.name : "?")
                      << ": " << p.value
                      << " " << (p.unit ? p.unit : "");
          }
        }
        // single value fallback
        if (field->num_fieldDataSet == 0 && field->num_fieldParams == 0
            && field->fieldValue != nullptr && field->fieldValue->value != nullptr) {
          std::cout << ": " << field->fieldValue->value
                    << " " << (field->fieldValue->unit ? field->fieldValue->unit : "");
        }
        std::cout << std::endl;
      }
    }
  } else {
    // RESULT_TYPE_GENERIC_STRUCT: apps-proc-ddr-metrics, bw-profiler-ddr-metrics, nsp-dsp-metrics
    for (uint32_t i = 0; i < profiling_result->profilingResultGeneric->metricResponseLen; i++) {
      const auto& v = profiling_result->profilingResultGeneric->metricResponse[i].value;
      double value;
      switch (v.dataType) {
        case DATA_TYPE_DOUBLE: value = v.doubleValue; break;
        case DATA_TYPE_FLOAT:  value = static_cast<double>(v.floatValue); break;
        case DATA_TYPE_UINT32: value = static_cast<double>(v.uint32Value); break;
        case DATA_TYPE_UINT64: value = static_cast<double>(v.uint64Value); break;
        default:               value = 0.0; break;
      }
      uint32_t metric_id = profiling_result->profilingResultGeneric->metricResponse[i].metricId;
      auto it = kMetricNames.find(metric_id);
      const char* name = (it != kMetricNames.end()) ? it->second : "Unknown";
      std::cout << "Metric ID: " << metric_id << " (" << name << "), Value: " << value << std::endl;
    }
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
  std::cout << (const char*)profiling_message->message << std::endl;
}

}  // namespace qprof