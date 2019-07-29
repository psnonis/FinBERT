// Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#pragma once

#include <time.h>
#include <mutex>
#include "src/core/model_config.pb.h"
#include "src/core/model_repository_manager.h"
#include "src/core/server_status.pb.h"
#include "src/core/status.h"

namespace nvidia { namespace inferenceserver {

class MetricModelReporter;
class ServerStatusManager;

// Updates a server stat with duration measured by a C++ scope.
class ServerStatTimerScoped {
 public:
  enum Kind {
    // Stat for status request. Duration from request to response.
    STATUS,
    // Stat for profile request. Duration from request to response.
    PROFILE,
    // Stat for health request. Duration from request to response.
    HEALTH
  };

  // Start server timer for a given status 'kind'.
  ServerStatTimerScoped(
      const std::shared_ptr<ServerStatusManager>& status_manager, Kind kind)
      : status_manager_(status_manager), kind_(kind), enabled_(true)
  {
    clock_gettime(CLOCK_MONOTONIC, &start_);
  }

  // Stop the timer and record the duration, unless reporting has been
  // disabled.
  ~ServerStatTimerScoped();

  // Enable/Disable reporting for this timer. By default reporting is
  // enabled and so the server status is updated when this object is
  // destructed. Reporting may be enabled/disabled multiple times
  // while the timer is running without affecting the duration.
  void SetEnabled(bool enabled) { enabled_ = enabled; }

 private:
  std::shared_ptr<ServerStatusManager> status_manager_;
  const Kind kind_;
  bool enabled_;
  struct timespec start_;
};

// Stats collector for an inference request.
class ModelInferStats {
 public:
  // A timer that starts on construction and stops on destruction. Can
  // also be stopped and started manually and multiple times. The
  // measured time is the accumulation of start-stop durations.
  class ScopedTimer {
   public:
    ScopedTimer();
    ~ScopedTimer();

    struct timespec Start();
    void Stop();

    const struct timespec& StartTimeStamp() const { return start_; };

   private:
    friend class ModelInferStats;
    struct timespec start_;
    uint64_t cummulative_duration_ns_;
    uint64_t* duration_ptr_;
  };

 public:
  // Start model-specific timer for 'model_name' and a given status
  // 'kind'.
  ModelInferStats(
      const std::shared_ptr<ServerStatusManager>& status_manager,
      const std::string& model_name)
      : status_manager_(status_manager), model_name_(model_name),
        requested_model_version_(-1), batch_size_(0), gpu_device_(-1),
        failed_(false), execution_count_(0), request_duration_ns_(0),
        queue_duration_ns_(0), compute_duration_ns_(0)
  {
  }

  // Report collected statistics.
  ~ModelInferStats();

  // Mark inferencing request as failed / not-failed.
  void SetFailed(bool failed) { failed_ = failed; }

  // Set the model version explicitly requested for the inference, or
  // -1 if latest version was requested.
  void SetRequestedVersion(int64_t v) { requested_model_version_ = v; }

  // Set the metric reporter for the model.
  void SetMetricReporter(const std::shared_ptr<MetricModelReporter> m)
  {
    metric_reporter_ = m;
  }

  // Set batch size for the inference stats.
  void SetBatchSize(size_t bs) { batch_size_ = bs; }

  // Set CUDA GPU device index where inference was performed.
  void SetGPUDevice(int idx) { gpu_device_ = idx; }

  // Set the number of model executions that were performed for this
  // inference request. Can be zero if this request was dynamically
  // batched with another request (in dynamic batch case only one of
  // the batched requests will count the execution).
  void SetModelExecutionCount(uint32_t count) { execution_count_ = count; }

  // Get a ScopedTimer that measures entire inference request-response
  // duration. The lifetime of 'timer' must not exceed the
  // lifetime of 'this' object.
  struct timespec StartRequestTimer(ScopedTimer* timer) const;

  // Get a ScopedTimer that measures wait time spent in backend Run(),
  // including queuing, scheduling. The lifetime of 'timer' must not
  // exceed the lifetime of 'this' object.
  struct timespec StartQueueTimer(ScopedTimer* timer) const;

  // Get a ScopedTimer that measures model compute duration including
  // H2D, compute and D2H. The lifetime of 'timer' must not exceed the
  // lifetime of 'this' object.
  struct timespec StartComputeTimer(ScopedTimer* timer) const;

 private:
  std::shared_ptr<ServerStatusManager> status_manager_;
  std::shared_ptr<MetricModelReporter> metric_reporter_;
  const std::string model_name_;
  int64_t requested_model_version_;
  size_t batch_size_;
  int gpu_device_;
  bool failed_;

  uint32_t execution_count_;
  mutable uint64_t request_duration_ns_;
  mutable uint64_t queue_duration_ns_;
  mutable uint64_t compute_duration_ns_;
};

// Manage access and updates to server status information.
class ServerStatusManager {
 public:
  // Create a manager for server status
  explicit ServerStatusManager(const std::string& server_version);

  // Initialize status for a model.
  Status InitForModel(
      const std::string& model_name, const ModelConfig& model_config);

  // Update model config for an existing model.
  Status UpdateConfigForModel(
      const std::string& model_name, const ModelConfig& model_config);

  // Get the entire server status, including status for all models.
  Status Get(
      ServerStatus* server_status, const std::string& server_id,
      ServerReadyState server_ready_state, uint64_t server_uptime_ns,
      ModelRepositoryManager* model_repository_manager) const;

  // Get the server status and the status for a single model.
  Status Get(
      ServerStatus* server_status, const std::string& server_id,
      ServerReadyState server_ready_state, uint64_t server_uptime_ns,
      const std::string& model_name,
      ModelRepositoryManager* model_repository_manager) const;

  // Add a duration to the Server Stat specified by 'kind'.
  void UpdateServerStat(uint64_t duration, ServerStatTimerScoped::Kind kind);

  // Add durations to Infer stats for a failed inference request.
  void UpdateFailedInferStats(
      const std::string& model_name, const int64_t model_version,
      size_t batch_size, uint64_t request_duration_ns);

  // Add durations to Infer stats for a successful inference request.
  void UpdateSuccessInferStats(
      const std::string& model_name, const int64_t model_version,
      size_t batch_size, uint32_t execution_cnt, uint64_t request_duration_ns,
      uint64_t queue_duration_ns, uint64_t compute_duration_ns);

 private:
  mutable std::mutex mu_;
  ServerStatus server_status_;
};
}}  // namespace nvidia::inferenceserver
