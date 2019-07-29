// Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
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

#include <unistd.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "src/core/request_inprocess.h"

namespace ni = nvidia::inferenceserver;
namespace nic = nvidia::inferenceserver::client;

#define FAIL_IF_ERR(X, MSG)                                        \
  do {                                                             \
    nic::Error err = (X);                                          \
    if (!err.IsOk()) {                                             \
      std::cerr << "error: " << (MSG) << ": " << err << std::endl; \
      exit(1);                                                     \
    }                                                              \
  } while (false)

namespace {

void
Usage(char** argv, const std::string& msg = std::string())
{
  if (!msg.empty()) {
    std::cerr << "error: " << msg << std::endl;
  }

  std::cerr << "Usage: " << argv[0] << " [options]" << std::endl;
  std::cerr << "\t-v" << std::endl;
  std::cerr << "\t-r [model repository absolute path]" << std::endl;

  exit(1);
}

}  // namespace

int
main(int argc, char** argv)
{
  bool verbose = false;
  std::string model_repository_path;

  // Parse commandline...
  int opt;
  while ((opt = getopt(argc, argv, "vr:")) != -1) {
    switch (opt) {
      case 'v':
        verbose = true;
        break;
      case 'r':
        model_repository_path = optarg;
        break;
      case '?':
        Usage(argv);
        break;
    }
  }

  if (model_repository_path.empty()) {
    Usage(argv, "-r must be used to specify model repository path");
  }

  // Set the options for inference server and then create the
  // inference server object.
  std::unique_ptr<nic::InferenceServerContext::Options> server_options;
  FAIL_IF_ERR(
      nic::InferenceServerContext::Options::Create(&server_options),
      "unable to create inference server options");
  server_options->SetModelRepositoryPath(model_repository_path);

  std::unique_ptr<nic::InferenceServerContext> server_ctx;
  FAIL_IF_ERR(
      nic::InferenceServerContext::Create(&server_ctx, server_options),
      "unable to create inference server context");

  // Wait until the server is both live and ready.
  std::unique_ptr<nic::ServerHealthContext> health_ctx;
  FAIL_IF_ERR(
      nic::ServerHealthInProcessContext::Create(
          &health_ctx, server_ctx, verbose),
      "unable to create health context");

  size_t health_iters = 0;
  while (true) {
    bool live, ready;
    FAIL_IF_ERR(health_ctx->GetLive(&live), "unable to get server liveness");
    FAIL_IF_ERR(health_ctx->GetReady(&ready), "unable to get server readiness");
    std::cout << "Server Health: live " << live << ", ready " << ready
              << std::endl;
    if (live && ready) {
      break;
    }

    if (++health_iters >= 10) {
      std::cerr << "failed to find healthy inference server" << std::endl;
      exit(1);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  // Check status of the server.
  std::unique_ptr<nic::ServerStatusContext> status_ctx;
  FAIL_IF_ERR(
      nic::ServerStatusInProcessContext::Create(
          &status_ctx, server_ctx, verbose),
      "unable to create status context");

  ni::ServerStatus server_status;
  FAIL_IF_ERR(
      status_ctx->GetServerStatus(&server_status),
      "unable to get server status");
  std::cout << "Server Status:" << std::endl;
  std::cout << server_status.DebugString() << std::endl;

  // Create an inference context for the model.  We use a simple model
  // that takes 2 input tensors of 16 integers each and returns 2
  // output tensors of 16 integers each. One output tensor is the
  // element-wise sum of the inputs and one output is the element-wise
  // difference.
  const std::string model_name("simple");

  std::unique_ptr<nic::InferContext> infer_ctx;
  FAIL_IF_ERR(
      nic::InferInProcessContext::Create(
          &infer_ctx, server_ctx, model_name, -1 /* model_version */, verbose),
      "unable to create inference context");

  std::unique_ptr<nic::InferContext::Options> infer_options;
  FAIL_IF_ERR(
      nic::InferContext::Options::Create(&infer_options),
      "unable to create inference options");

  const size_t batch_size = 1;
  infer_options->SetBatchSize(batch_size);
  for (const auto& output : infer_ctx->Outputs()) {
    infer_options->AddRawResult(output);
  }

  FAIL_IF_ERR(
      infer_ctx->SetRunOptions(*infer_options),
      "unable to set inference options");

  // Create the data for the two input tensors. Initialize the first
  // to unique integers and the second to all ones.
  std::vector<int32_t> input0_data(16);
  std::vector<int32_t> input1_data(16);
  for (size_t i = 0; i < 16; ++i) {
    input0_data[i] = i;
    input1_data[i] = 1;
  }

  // Initialize the inputs with the data.
  std::shared_ptr<nic::InferContext::Input> input0, input1;
  FAIL_IF_ERR(infer_ctx->GetInput("INPUT0", &input0), "unable to get INPUT0");
  FAIL_IF_ERR(infer_ctx->GetInput("INPUT1", &input1), "unable to get INPUT1");

  FAIL_IF_ERR(input0->Reset(), "unable to reset INPUT0");
  FAIL_IF_ERR(input1->Reset(), "unable to reset INPUT1");

  FAIL_IF_ERR(
      input0->SetRaw(
          reinterpret_cast<uint8_t*>(&input0_data[0]),
          input0_data.size() * sizeof(int32_t)),
      "unable to set data for INPUT0");
  FAIL_IF_ERR(
      input1->SetRaw(
          reinterpret_cast<uint8_t*>(&input1_data[0]),
          input1_data.size() * sizeof(int32_t)),
      "unable to set data for INPUT1");

  std::map<std::string, std::unique_ptr<nic::InferContext::Result>> results;
  FAIL_IF_ERR(infer_ctx->Run(&results), "unable to run model");

  // We expect there to be 2 results. Walk over all 16 result elements
  // and print the sum and difference calculated by the model.
  if (results.size() != 2) {
    std::cerr << "error: expected 2 results, got " << results.size()
              << std::endl;
  }

  for (size_t i = 0; i < 16; ++i) {
    int32_t r0, r1;
    FAIL_IF_ERR(
        results["OUTPUT0"]->GetRawAtCursor(0 /* batch idx */, &r0),
        "unable to get OUTPUT0 result at idx " + std::to_string(i));
    FAIL_IF_ERR(
        results["OUTPUT1"]->GetRawAtCursor(0 /* batch idx */, &r1),
        "unable to get OUTPUT1 result at idx " + std::to_string(i));
    std::cout << input0_data[i] << " + " << input1_data[i] << " = " << r0
              << std::endl;
    std::cout << input0_data[i] << " - " << input1_data[i] << " = " << r1
              << std::endl;

    if ((input0_data[i] + input1_data[i]) != r0) {
      std::cerr << "error: incorrect sum" << std::endl;
      exit(1);
    }
    if ((input0_data[i] - input1_data[i]) != r1) {
      std::cerr << "error: incorrect difference" << std::endl;
      exit(1);
    }
  }

  return 0;
}
