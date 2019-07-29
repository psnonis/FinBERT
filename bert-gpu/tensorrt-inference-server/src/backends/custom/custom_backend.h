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

#include "src/backends/custom/custom.h"
#include "src/core/backend.h"
#include "src/core/model_config.pb.h"
#include "src/core/scheduler.h"
#include "src/core/status.h"

namespace nvidia { namespace inferenceserver {

class CustomBackend : public InferenceBackend {
 public:
  CustomBackend() = default;
  CustomBackend(CustomBackend&&) = default;

  Status Init(
      const std::string& path, const std::vector<std::string>& server_params,
      const ModelConfig& config);

  // Create a context for execution for each instance for the custom
  // 'models'.
  Status CreateExecutionContexts(
      const std::unordered_map<std::string, std::string>& libraries);
  Status CreateExecutionContext(
      const std::string& instance_name, const int gpu_device,
      const std::unordered_map<std::string, std::string>& libraries);

 private:
  // Init model on the context associated with 'runner_idx'.
  Status InitBackend(uint32_t runner_idx);

  // Run model on the context associated with 'runner_idx' to
  // execute for one or more requests.
  void RunBackend(
      uint32_t runner_idx, std::vector<Scheduler::Payload>* payloads,
      std::function<void(Status)> OnCompleteQueuedPayloads);

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomBackend);
  friend std::ostream& operator<<(std::ostream&, const CustomBackend&);
  friend bool CustomGetNextInput(void*, const char*, const void**, uint64_t*);
  friend bool CustomGetOutput(
      void*, const char*, size_t, int64_t*, uint64_t, void**);

  // For each model instance there is a context.
  struct Context {
    // GPU device number that indicates that no gpu is available for a
    // context
    static constexpr int NO_GPU_DEVICE = -1;

    // Max batch size value that indicates batching is not supported.
    static constexpr int NO_BATCHING = 0;

    using IOSizeMap = std::unordered_map<std::string, size_t>;

    Context(
        const std::string& name, const int gpu_device,
        const int max_batch_size);
    ~Context();

    DISALLOW_MOVE(Context);
    DISALLOW_COPY_AND_ASSIGN(Context);

    // Return the shared library reported error string for 'err'.
    std::string LibraryErrorString(const int err);

    // Run model to execute for one or more requests. This function
    // assumes that it is only called by the single runner thread that
    // is assigned to this context. A non-OK return status indicates
    // an internal error that prevents any of the of requests from
    // completing. If an error is isolate to a single request payload
    // it will be reported in that payload.
    Status Run(CustomBackend* base, std::vector<Scheduler::Payload>* payloads);

    struct GetInputOutputContext {
      GetInputOutputContext(
          CustomBackend::Context* context, Scheduler::Payload* payload)
          : context_(context), payload_(payload)
      {
      }
      CustomBackend::Context* context_;
      Scheduler::Payload* payload_;
    };

    // Callback used by custom backends to get the next block of input
    // for a 'name'd input tensor.
    bool GetNextInput(
        GetInputOutputContext* input_context, const char* name,
        const void** content, uint64_t* content_byte_size);

    // Callback used by custom backends to get the output buffer for a
    // 'name'd output tensor.
    bool GetOutput(
        GetInputOutputContext* output_context, const char* name,
        size_t shape_dim_cnt, int64_t* shape_dims, uint64_t content_byte_size,
        void** content);

    // Name of the model instance
    std::string name_;

    // The GPU index active when this context was created.
    int gpu_device_;

    // Maximum batch size to allow. NO_BATCHING indicates that
    // batching is not supported.
    int max_batch_size_;

    // The handle to the shared library associated with this context.
    void* library_handle_;

    // The handle to the custom shared library context associated with
    // this context.
    void* library_context_handle_;

    // The functions from the shared library.
    CustomInitializeFn_t InitializeFn_;
    CustomFinalizeFn_t FinalizeFn_;
    CustomErrorStringFn_t ErrorStringFn_;
    CustomExecuteFn_t ExecuteFn_;
  };

  std::vector<std::string> server_params_;
  std::vector<std::unique_ptr<Context>> contexts_;
};

std::ostream& operator<<(std::ostream& out, const CustomBackend& pb);

// Callback used by custom backends to get the next block of input for
// a 'name'd input tensor.
bool CustomGetNextInput(
    void* input_context, const char* name, const void** content,
    uint64_t* content_byte_size);

// Callback used by custom backends to get the output buffer for a
// 'name'd output tensor.
bool CustomGetOutput(
    void* output_context, const char* name, size_t shape_dim_cnt,
    int64_t* shape_dims, uint64_t content_byte_size, void** content);

}}  // namespace nvidia::inferenceserver
