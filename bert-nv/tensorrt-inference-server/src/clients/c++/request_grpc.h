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

/// \file

#include "src/clients/c++/request.h"

namespace nvidia { namespace inferenceserver { namespace client {

//==============================================================================
/// ServerHealthGrpcContext is the GRPC instantiation of
/// ServerHealthContext.
///
class ServerHealthGrpcContext {
 public:
  /// Create a context that returns health information about server.
  /// \param ctx Returns a new ServerHealthGrpcContext object.
  /// \param server_url The inference server name and port.
  /// \param verbose If true generate verbose output when contacting
  /// the inference server.
  /// \return Error object indicating success or failure.
  static Error Create(
      std::unique_ptr<ServerHealthContext>* ctx, const std::string& server_url,
      bool verbose = false);
};

//==============================================================================
/// ServerStatusGrpcContext is the GRPC instantiation of
/// ServerStatusContext.
///
class ServerStatusGrpcContext {
 public:
  /// Create a context that returns information about an inference
  /// server and all models on the server using GRPC protocol.
  /// \param ctx Returns a new ServerStatusGrpcContext object.
  /// \param server_url The inference server name and port.
  /// \param verbose If true generate verbose output when contacting
  /// the inference server.
  /// \return Error object indicating success or failure.
  static Error Create(
      std::unique_ptr<ServerStatusContext>* ctx, const std::string& server_url,
      bool verbose = false);

  /// Create a context that returns information about an inference
  /// server and one model on the sever using GRPC protocol.
  /// \param ctx Returns a new ServerStatusGrpcContext object.
  /// \param server_url The inference server name and port.
  /// \param model_name The name of the model to get status for.
  /// \param verbose If true generate verbose output when contacting
  /// the inference server.
  /// \return Error object indicating success or failure.
  static Error Create(
      std::unique_ptr<ServerStatusContext>* ctx, const std::string& server_url,
      const std::string& model_name, bool verbose = false);
};

//==============================================================================
//// ProfileGrpcContext is the GRPC instantiation of ProfileContext.
////
class ProfileGrpcContext {
 public:
  /// Create context that controls profiling on a server using GRPC
  /// protocol.
  /// \param ctx Returns the new ProfileContext object.
  /// \param server_url The inference server name and port.
  /// \param verbose If true generate verbose output when contacting
  /// the inference server.
  /// \return Error object indicating success or failure.
  static Error Create(
      std::unique_ptr<ProfileContext>* ctx, const std::string& server_url,
      bool verbose = false);
};

//==============================================================================
/// InferGrpcContext is the GRPC instantiation of InferContext.
///
class InferGrpcContext {
 public:
  /// Create context that performs inference for a non-sequence model
  /// using the GRPC protocol.
  ///
  /// \param ctx Returns a new InferGrpcContext object.
  /// \param server_url The inference server name and port.
  /// \param model_name The name of the model to get status for.
  /// \param model_version The version of the model to use for inference,
  /// or -1 to indicate that the latest (i.e. highest version number)
  /// version should be used.
  /// \param verbose If true generate verbose output when contacting
  /// the inference server.
  /// \return Error object indicating success or failure.
  static Error Create(
      std::unique_ptr<InferContext>* ctx, const std::string& server_url,
      const std::string& model_name, int64_t model_version = -1,
      bool verbose = false);

  /// Create context that performs inference for a sequence model
  /// using a given correlation ID and the GRPC protocol.
  ///
  /// \param ctx Returns a new InferGrpcContext object.
  /// \param correlation_id The correlation ID to use for all
  /// inferences performed with this context. A value of 0 (zero)
  /// indicates that no correlation ID should be used.
  /// \param server_url The inference server name and port.
  /// \param model_name The name of the model to get status for.
  /// \param model_version The version of the model to use for inference,
  /// or -1 to indicate that the latest (i.e. highest version number)
  /// version should be used.
  /// \param verbose If true generate verbose output when contacting
  /// the inference server.
  /// \return Error object indicating success or failure.
  static Error Create(
      std::unique_ptr<InferContext>* ctx, CorrelationID correlation_id,
      const std::string& server_url, const std::string& model_name,
      int64_t model_version = -1, bool verbose = false);
};

//==============================================================================
/// InferGrpcStreamContext is the streaming instantiation of InferGrpcContext.
/// All synchronous and asynchronous requests sent from this context will be
/// sent in the same stream.
///
class InferGrpcStreamContext {
 public:
  /// Create streaming context that performs inference for a non-sequence model
  /// using the GRPC protocol.
  ///
  /// \param ctx Returns a new InferGrpcContext object.
  /// \param server_url The inference server name and port.
  /// \param model_name The name of the model to get status for.
  /// \param model_version The version of the model to use for inference,
  /// or -1 to indicate that the latest (i.e. highest version number)
  /// version should be used.
  /// \param verbose If true generate verbose output when contacting
  /// the inference server.
  /// \return Error object indicating success or failure.
  static Error Create(
      std::unique_ptr<InferContext>* ctx, const std::string& server_url,
      const std::string& model_name, int64_t model_version = -1,
      bool verbose = false);

  /// Create streaming context that performs inference for a sequence model
  /// using a given correlation ID and the GRPC protocol.
  ///
  /// \param ctx Returns a new InferGrpcContext object.
  /// \param correlation_id The correlation ID to use for all
  /// inferences performed with this context. A value of 0 (zero)
  /// indicates that no correlation ID should be used.
  /// \param server_url The inference server name and port.
  /// \param model_name The name of the model to get status for.
  /// \param model_version The version of the model to use for inference,
  /// or -1 to indicate that the latest (i.e. highest version number)
  /// version should be used.
  /// \param verbose If true generate verbose output when contacting
  /// the inference server.
  /// \return Error object indicating success or failure.
  static Error Create(
      std::unique_ptr<InferContext>* ctx, CorrelationID correlation_id,
      const std::string& server_url, const std::string& model_name,
      int64_t model_version = -1, bool verbose = false);
};

}}}  // namespace nvidia::inferenceserver::client
