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

#include <string>
#include <vector>
#include "src/core/api.pb.h"
#include "src/core/request_status.pb.h"
#include "src/core/server_status.pb.h"

namespace nvidia { namespace inferenceserver {

// From model_config.h, repeat here to avoid introducing dependence on
// that header.
using CorrelationID = uint64_t;
using DimsList = ::google::protobuf::RepeatedField<::google::protobuf::int64>;

}}  // namespace nvidia::inferenceserver

namespace nvidia { namespace inferenceserver { namespace client {

//==============================================================================
/// Error status reported by client API.
///
class Error {
 public:
  /// Create an error from a RequestStatus.
  /// \param status The RequestStatus object
  explicit Error(const RequestStatus& status);

  /// Create an error from a RequestStatusCode.
  /// \param code The status code for the error
  explicit Error(RequestStatusCode code = RequestStatusCode::SUCCESS);

  /// Create an error from a RequestStatusCode and a detailed message.
  /// \param code The status code for the error
  /// \param msg The detailed message for the error
  explicit Error(RequestStatusCode code, const std::string& msg);

  /// Accessor for the RequestStatusCode of this error.
  /// \return The RequestStatusCode for the error.
  RequestStatusCode Code() const { return code_; }

  /// Accessor for the message of this error.
  /// \return The detailed messsage for the error. Empty if no
  /// detailed message.
  const std::string& Message() const { return msg_; }

  /// Accessor for the ID of the inference server associated with this
  /// error.
  /// \return The ID of the inference server associated with this
  /// error, or empty-string if no inference server is associated with
  /// the error.
  const std::string& ServerId() const { return server_id_; }

  /// Accessor for the ID of the request associated with this error.
  /// \return The ID of the request associated with this error, or 0
  /// (zero) if no request ID is associated with the error.
  uint64_t RequestId() const { return request_id_; }

  /// Does this error indicate OK status?
  /// \return True if this error indicates "ok"/"success", false if
  /// error indicates a failure.
  bool IsOk() const { return code_ == RequestStatusCode::SUCCESS; }

  /// Convenience "success" value. Can be used as Error::Success to
  /// indicate no error.
  static const Error Success;

 private:
  friend std::ostream& operator<<(std::ostream&, const Error&);
  RequestStatusCode code_;
  std::string msg_;
  std::string server_id_;
  uint64_t request_id_;
};

//==============================================================================
/// A ServerHealthContext object is used to query an inference server
/// for health information. Once created a ServerHealthContext object
/// can be used repeatedly to get health from the server. A
/// ServerHealthContext object can use either HTTP protocol or GRPC
/// protocol depending on the Create function
/// (ServerHealthHttpContext::Create or
/// ServerHealthGrpcContext::Create). For example:
///
/// \code
/// std::unique_ptr<ServerHealthContext> ctx;
///   ServerHealthHttpContext::Create(&ctx, "localhost:8000");
///   bool ready;
///   ctx->GetReady(&ready);
///   ...
///   bool live;
///   ctx->GetLive(&live);
///   ...
/// \endcode
///
/// \note
///   ServerHealthContext::Create methods are thread-safe.
///   GetReady() and GetLive() are not thread-safe. For a given
///   ServerHealthContext, calls to GetReady() and GetLive() must be
///   serialized.
///
class ServerHealthContext {
 public:
  virtual ~ServerHealthContext() = 0;

  /// Contact the inference server and get readiness state.
  /// \param ready Returns the readiness state of the server.
  /// \return Error object indicating success or failure of the request.
  virtual Error GetReady(bool* ready) = 0;

  /// Contact the inference server and get liveness state.
  /// \param ready Returns the liveness state of the server.
  /// \return Error object indicating success or failure of the request.
  virtual Error GetLive(bool* live) = 0;
};

//==============================================================================
/// A ServerStatusContext object is used to query an inference server
/// for status information, including information about the models
/// available on that server. Once created a ServerStatusContext object
/// can be used repeatedly to get status from the server.
/// A ServerStatusContext object can use either HTTP protocol or GRPC protocol
/// depending on the Create function (ServerStatusHttpContext::Create or
/// ServerStatusGrpcContext::Create). For example:
///
/// \code
///   std::unique_ptr<ServerStatusContext> ctx;
///   ServerStatusHttpContext::Create(&ctx, "localhost:8000");
///   ServerStatus status;
///   ctx->GetServerStatus(&status);
///   ...
///   ctx->GetServerStatus(&status);
///   ...
/// \endcode
///
/// \note
///   ServerStatusContext::Create methods are thread-safe.
///   GetServerStatus() is not thread-safe. For a given
///   ServerStatusContext, calls to GetServerStatus() must be
///   serialized.
///
class ServerStatusContext {
 public:
  virtual ~ServerStatusContext() = 0;

  /// Contact the inference server and get status.
  /// \param status Returns the status.
  /// \return Error object indicating success or failure of the request.
  virtual Error GetServerStatus(ServerStatus* status) = 0;
};

//==============================================================================
/// An InferContext object is used to run inference on an inference
/// server for a specific model. Once created an InferContext object
/// can be used repeatedly to perform inference using the
/// model. Options that control how inference is performed can be
/// changed in between inference runs.
///
/// A InferContext object can use either HTTP protocol or GRPC
/// protocol depending on the Create function
/// (InferHttpContext::Create, InferGrpcContext::Create or
/// InferGrpcStreamContext::Create). For example:
///
/// \code
///   std::unique_ptr<InferContext> ctx;
///   InferHttpContext::Create(&ctx, "localhost:8000", "mnist");
///   ...
///   std::unique_ptr<Options> options0;
///   Options::Create(&options0);
///   options->SetBatchSize(b);
///   options->AddClassResult(output, topk);
///   ctx->SetRunOptions(*options0);
///   ...
///   ctx->Run(&results0);  // run using options0
///   ctx->Run(&results1);  // run using options0
///   ...
///   std::unique_ptr<Options> options1;
///   Options::Create(&options1);
///   options->AddRawResult(output);
///   ctx->SetRunOptions(*options);
///   ...
///   ctx->Run(&results2);  // run using options1
///   ctx->Run(&results3);  // run using options1
///   ...
/// \endcode
///
/// \note
///   InferContext::Create methods are thread-safe.
///   All other InferContext methods, and nested class methods are not
///   thread-safe.
/// \par
///   The Run() calls are not thread-safe but a new Run() can
///   be invoked as soon as the previous completes. The returned result
///   objects are owned by the caller and may be retained and accessed
///   even after the InferContext object is destroyed.
/// \par
///   AsyncRun() and GetAsyncRunStatus() calls are not thread-safe.
///   What's more, calling one method while the other one is running
///   will result in undefined behavior given that they will modify the
///   shared data internally.
/// \par
///   For more parallelism multiple InferContext objects can access the
///   same inference server with no serialization requirements across
///   those objects.
/// \endcode
///
class InferContext {
 public:
  //==============
  /// An input to the model.
  class Input {
   public:
    virtual ~Input() = 0;

    /// \return The name of the input.
    virtual const std::string& Name() const = 0;

    /// \return The size in bytes of this input. This is the size for
    /// one instance of the input, not the entire size of a batched
    /// input. When the byte-size is not known, for example for
    /// non-fixed-sized types like TYPE_STRING or for inputs with
    /// variable-size dimensions, this will return -1.
    virtual int64_t ByteSize() const = 0;

    /// \return The size in bytes of entire batch of this input. For
    /// fixed-sized types this is just ByteSize() * batch-size, but
    /// for non-fixed-sized types like TYPE_STRING it is the only way
    /// to get the entire input size.
    virtual size_t TotalByteSize() const = 0;

    /// \return The data-type of the input.
    virtual DataType DType() const = 0;

    /// \return The format of the input.
    virtual ModelInput::Format Format() const = 0;

    /// \return The dimensions/shape of the input specified in the
    /// model configuration. Variable-size dimensions are reported as
    /// -1.
    virtual const DimsList& Dims() const = 0;

    /// Prepare this input to receive new tensor values. Forget any
    /// existing values that were set by previous calls to
    /// SetRaw().
    /// \return Error object indicating success or failure.
    virtual Error Reset() = 0;

    /// Get the shape for this input that was most recently set by
    /// SetShape.
    /// \return The shape, or empty vector if SetShape has not been
    /// called.
    virtual const std::vector<int64_t>& Shape() const = 0;

    /// Set the shape for this input. The shape must be set for inputs
    /// that have variable-size dimensions and is optional for other
    /// inputs. The shape must be set before calling SetRaw or
    /// SetFromString.
    /// \param dims The dimensions of the shape.
    /// \return Error object indicating success or failure.
    virtual Error SetShape(const std::vector<int64_t>& dims) = 0;

    /// Set tensor values for this input from a byte array. The array
    /// is not copied and so it must not be modified or destroyed
    /// until this input is no longer needed (that is until the Run()
    /// call(s) that use the input have completed). For batched inputs
    /// this function must be called batch-size times to provide all
    /// tensor values for a batch of this input.
    /// \param input The pointer to the array holding the tensor value.
    /// \param input_byte_size The size of the array in bytes, must match
    /// the size expected by the input.
    /// \return Error object indicating success or failure.
    virtual Error SetRaw(const uint8_t* input, size_t input_byte_size) = 0;

    /// Set tensor values for this input from a byte vector. The vector
    /// is not copied and so it must not be modified or destroyed
    /// until this input is no longer needed (that is until the Run()
    /// call(s) that use the input have completed). For batched inputs
    /// this function must be called batch-size times to provide all
    /// tensor values for a batch of this input.
    /// \param input The vector holding tensor values.
    /// \return Error object indicating success or failure.
    virtual Error SetRaw(const std::vector<uint8_t>& input) = 0;

    /// Set tensor values for this input from a vector or
    /// strings. This method can only be used for tensors with STRING
    /// data-type. The strings are assigned in row-major order to the
    /// elements of the tensor. The strings are copied and so the
    /// 'input' does not need to be preserved as with SetRaw(). For
    /// batched inputs this function must be called batch-size times
    /// to provide all tensor values for a batch of this input.
    /// \param input The vector holding tensor string values.
    /// \return Error object indicating success or failure.
    virtual Error SetFromString(const std::vector<std::string>& input) = 0;
  };

  //==============
  /// An output from the model.
  class Output {
   public:
    virtual ~Output() = 0;

    /// \return The name of the output.
    virtual const std::string& Name() const = 0;

    /// \return The data-type of the output.
    virtual DataType DType() const = 0;

    /// \return The dimensions/shape of the output specified in the
    /// model configuration. Variable-size dimensions are reported as
    /// -1.
    virtual const DimsList& Dims() const = 0;
  };

  //==============
  /// An inference result corresponding to an output.
  class Result {
   public:
    virtual ~Result() = 0;

    /// Format in which result is returned.
    enum ResultFormat {
      /// RAW format is the entire result tensor of values.
      RAW = 0,

      /// CLASS format is the top-k highest probability values of the
      /// result and the associated class label (if provided by the
      /// model).
      CLASS = 1
    };

    /// \return The name of the model that produced this result.
    virtual const std::string& ModelName() const = 0;

    /// \return The version of the model that produced this result.
    virtual int64_t ModelVersion() const = 0;

    /// \return The Output object corresponding to this result.
    virtual const std::shared_ptr<Output> GetOutput() const = 0;

    /// Get the shape of a raw result. The shape does not include the
    /// batch dimension.
    /// \param shape Returns the shape.
    /// \return Error object indicating success or failure.
    virtual Error GetRawShape(std::vector<int64_t>* shape) const = 0;

    /// Get a reference to entire raw result data for a specific batch
    /// entry. Returns error if this result is not RAW format.
    /// WARNING: This call may require creation of a copy of the
    /// result data. To avoid this potential copy overhead use
    /// GetRaw(size_t, const uint8_t**, size_t*).
    /// \param batch_idx Returns the results for this entry of the batch.
    /// \param buf Returns the vector of result bytes.
    /// \return Error object indicating success or failure.
    virtual Error GetRaw(
        size_t batch_idx, const std::vector<uint8_t>** buf) const = 0;

    /// Get a reference to entire raw result data for a specific batch
    /// entry. Returns error if this result is not RAW format.
    /// \param batch_idx Returns the results for this entry of the batch.
    /// \param buf Returns pointer to the buffer holding result bytes.
    /// \param byte_size Returns the size of the result buffer, in bytes.
    /// \return Error object indicating success or failure.
    virtual Error GetRaw(
        size_t batch_idx, const uint8_t** buf, size_t* byte_size) const = 0;

    /// Get a reference to raw result data for a specific batch entry
    /// at the current "cursor" and advance the cursor by the specified
    /// number of bytes. More typically use GetRawAtCursor<T>() method
    /// to return the data as a specific type T. Use ResetCursor() to
    /// reset the cursor to the beginning of the result. Returns error
    /// if this result is not RAW format.
    /// \param batch_idx Returns results for this entry of the batch.
    /// \param buf Returns pointer to 'adv_byte_size' bytes of data.
    /// \param adv_byte_size The number of bytes of data to get a reference to.
    /// \return Error object indicating success or failure.
    virtual Error GetRawAtCursor(
        size_t batch_idx, const uint8_t** buf, size_t adv_byte_size) = 0;

    /// Read a value for a specific batch entry at the current "cursor"
    /// from the result tensor as the specified type T and advance the
    /// cursor. Use ResetCursor() to reset the cursor to the beginning
    /// of the result. Returns error if this result is not RAW format.
    /// \param batch_idx Returns results for this entry of the batch.
    /// \param out Returns the value at the cursor.
    /// \return Error object indicating success or failure.
    template <typename T>
    Error GetRawAtCursor(size_t batch_idx, T* out);

    /// The result value for CLASS format results.
    struct ClassResult {
      /// The index of the class in the result vector.
      size_t idx;
      /// The value of the class.
      float value;
      /// The label for the class, if provided by the model.
      std::string label;
    };

    /// Get the number of class results for a batch. Returns error if
    /// this result is not CLASS format.
    /// \param batch_idx The index in the batch.
    /// \param cnt Returns the number of ClassResult entries for the
    /// batch entry.
    /// \return Error object indicating success or failure.
    virtual Error GetClassCount(size_t batch_idx, size_t* cnt) const = 0;

    /// Get the ClassResult result for a specific batch entry at the
    /// current cursor. Use ResetCursor() to reset the cursor to the
    /// beginning of the result. Returns error if this result is not
    /// CLASS format.
    /// \param batch_idx The index in the batch.
    /// \param result Returns the ClassResult value for the batch at the cursor.
    /// \return Error object indicating success or failure.
    virtual Error GetClassAtCursor(size_t batch_idx, ClassResult* result) = 0;

    /// Reset cursor to beginning of result for all batch entries.
    /// \return Error object indicating success or failure.
    virtual Error ResetCursors() = 0;

    /// Reset cursor to beginning of result for specified batch entry.
    /// \param batch_idx The index in the batch.
    /// \return Error object indicating success or failure.
    virtual Error ResetCursor(size_t batch_idx) = 0;
  };

  //==============
  /// Run options to be applied to all subsequent Run() invocations.
  class Options {
   public:
    virtual ~Options() = 0;

    /// Create a new Options object with default values.
    /// \return Error object indicating success or failure.
    static Error Create(std::unique_ptr<Options>* options);

    /// Get the value of a request flag being used for all subsequent
    /// inferences. Cannot be used with FLAG_NONE.
    /// \param flag The flag to get the value for.
    /// \return The true/false value currently set for the
    /// flag. If 'flag' is FLAG_NONE then return false.
    virtual bool Flag(InferRequestHeader::Flag flag) const = 0;

    /// Set a request flag to be used for all subsequent inferences.
    /// \param flag The flag to set. Cannot be used with FLAG_NONE.
    /// \param value The true/false value to set for the flag. If
    /// 'flag' is FLAG_NONE then do nothing.
    virtual void SetFlag(InferRequestHeader::Flag flag, bool value) = 0;

    /// Get the value of all request flags being used for all
    /// subsequent inferences.
    /// \return The bitwise-or of flag values as a single uint32_t
    /// value.
    virtual uint32_t Flags() const = 0;

    /// Set all request flags to be used for all subsequent inferences.
    /// \param flags The bitwise-or of flag values to set.
    virtual void SetFlags(uint32_t flags) = 0;

    /// \return The batch size to use for all subsequent inferences.
    virtual size_t BatchSize() const = 0;

    /// Set the batch size to use for all subsequent inferences.
    /// \param batch_size The batch size.
    virtual void SetBatchSize(size_t batch_size) = 0;

    /// Add 'output' to the list of requested RAW results. Run() will
    /// return the output's full tensor as a result.
    /// \param output The output.
    /// \return Error object indicating success or failure.
    virtual Error AddRawResult(
        const std::shared_ptr<InferContext::Output>& output) = 0;

    /// Add 'output' to the list of requested CLASS results. Run() will
    /// return the highest 'k' values of 'output' as a result.
    /// \param output The output.
    /// \param k Set how many class results to return for the output.
    /// \return Error object indicating success or failure.
    virtual Error AddClassResult(
        const std::shared_ptr<InferContext::Output>& output, uint64_t k) = 0;
  };

  //==============
  /// Handle to a inference request. The request handle is used to get
  /// request results if the request is sent by AsyncRun().
  class Request {
   public:
    virtual ~Request() = 0;

    /// \return The unique identifier of the request.
    virtual uint64_t Id() const = 0;
  };

  //==============
  /// Cumulative statistic of the InferContext.
  ///
  /// \note
  ///   For GRPC protocol, 'cumulative_send_time_ns' represents the
  ///   time for marshaling infer request.
  ///   'cumulative_receive_time_ns' represents the time for
  ///   unmarshaling infer response.
  struct Stat {
    /// Total number of requests completed.
    size_t completed_request_count;

    /// Time from the request start until the response is completely
    /// received.
    uint64_t cumulative_total_request_time_ns;

    /// Time from the request start until the last byte is sent.
    uint64_t cumulative_send_time_ns;

    /// Time from receiving first byte of the response until the
    /// response is completely received.
    uint64_t cumulative_receive_time_ns;

    /// Create a new Stat object with zero-ed statistics.
    Stat()
        : completed_request_count(0), cumulative_total_request_time_ns(0),
          cumulative_send_time_ns(0), cumulative_receive_time_ns(0)
    {
    }
  };

 public:
  using ResultMap = std::map<std::string, std::unique_ptr<Result>>;

  virtual ~InferContext() = 0;

  /// \return The name of the model being used for this context.
  virtual const std::string& ModelName() const = 0;

  /// \return The version of the model being used for this context. -1
  /// indicates that the latest (i.e. highest version number) version
  /// of that model is being used.
  virtual int64_t ModelVersion() const = 0;

  /// \return The maximum batch size supported by the context. A
  /// maximum batch size indicates that the context does not support
  /// batching and so only a single inference at a time can be
  /// performed.
  virtual uint64_t MaxBatchSize() const = 0;

  /// \return The correlation ID associated with the context.
  virtual CorrelationID CorrelationId() const = 0;

  /// \return The inputs of the model.
  virtual const std::vector<std::shared_ptr<Input>>& Inputs() const = 0;

  /// \return The outputs of the model.
  virtual const std::vector<std::shared_ptr<Output>>& Outputs() const = 0;

  /// Get a named input.
  /// \param name The name of the input.
  /// \param input Returns the Input object for 'name'.
  /// \return Error object indicating success or failure.
  virtual Error GetInput(
      const std::string& name, std::shared_ptr<Input>* input) const = 0;

  /// Get a named output.
  /// \param name The name of the output.
  /// \param output Returns the Output object for 'name'.
  /// \return Error object indicating success or failure.
  virtual Error GetOutput(
      const std::string& name, std::shared_ptr<Output>* output) const = 0;

  /// Set the options to use for all subsequent Run() invocations.
  /// \param options The options.
  /// \return Error object indicating success or failure.
  virtual Error SetRunOptions(const Options& options) = 0;

  /// Get the current statistics of the InferContext.
  /// \param stat Returns the Stat object holding the statistics.
  /// \return Error object indicating success or failure.
  virtual Error GetStat(Stat* stat) const = 0;

  /// Send a synchronous request to the inference server to perform an
  /// inference to produce results for the outputs specified in the
  /// most recent call to SetRunOptions().
  /// \param results Returns Result objects holding inference results
  /// as a map from output name to Result object.
  /// \return Error object indicating success or failure.
  virtual Error Run(ResultMap* results) = 0;

  /// Send an asynchronous request to the inference server to perform
  /// an inference to produce results for the outputs specified in the
  /// most recent call to SetRunOptions().
  /// \param async_request Returns a Request object that can be used
  /// to retrieve the inference results for the request.
  /// \return Error object indicating success or failure.
  virtual Error AsyncRun(std::shared_ptr<Request>* async_request) = 0;

  /// Get the results of the asynchronous request referenced by
  /// 'async_request'.
  /// \param results Returns Result objects holding inference results
  /// as a map from output name to Result object.
  /// \param is_ready Returns boolean that indicates if the results are ready,
  /// the results are valid only if is_ready returns true.
  /// \param async_request Request handle to retrieve results.
  /// \param wait If true, block until the request completes. Otherwise, return
  /// immediately.
  /// \return Error object indicating success or failure.
  virtual Error GetAsyncRunResults(
      ResultMap* results, bool* is_ready,
      const std::shared_ptr<Request>& async_request, bool wait) = 0;

  /// Get any one completed asynchronous request.
  /// \param async_request Returns the Request object holding the
  /// completed request.
  /// \param is_ready Returns boolean that indicates if the request is ready,
  /// the async_request returned is valid only if is_ready returns true.
  /// \param wait If true, block until the request completes. Otherwise, return
  /// immediately.
  /// \return Error object indicating success or failure.
  virtual Error GetReadyAsyncRequest(
      std::shared_ptr<Request>* async_request, bool* is_ready, bool wait) = 0;
};

//==============================================================================
/// A ProfileContext object is used to control profiling on the
/// inference server. Once created a ProfileContext object can be used
/// repeatedly.
///
/// A ProfileContext object can use either HTTP protocol or GRPC protocol
/// depending on the Create function (ProfileHttpContext::Create or
/// ProfileGrpcContext::Create). For example:
///
/// \code
///   std::unique_ptr<ProfileContext> ctx;
///   ProfileGrpcContext::Create(&ctx, "localhost:8000");
///   ctx->StartProfile();
///   ...
///   ctx->StopProfile();
///   ...
/// \endcode
///
/// \note
///   ProfileContext::Create methods are thread-safe.  StartProfiling()
///   and StopProfiling() are not thread-safe. For a given
///   ProfileContext, calls to these methods must be serialized.
///
class ProfileContext {
 public:
  virtual ~ProfileContext() = 0;

  /// Start profiling on the inference server.
  /// \return Error object indicating success or failure.
  virtual Error StartProfile() = 0;

  /// Stop profiling on the inference server.
  // \return Error object indicating success or failure.
  virtual Error StopProfile() = 0;
};

//==============================================================================

std::ostream& operator<<(std::ostream&, const Error&);

template <>
Error InferContext::Result::GetRawAtCursor(size_t batch_idx, std::string* out);

template <typename T>
Error
InferContext::Result::GetRawAtCursor(size_t batch_idx, T* out)
{
  const uint8_t* buf;
  Error err = GetRawAtCursor(batch_idx, &buf, sizeof(T));
  if (!err.IsOk()) {
    return err;
  }

  std::copy(buf, buf + sizeof(T), reinterpret_cast<uint8_t*>(out));
  return Error::Success;
}

}}}  // namespace nvidia::inferenceserver::client
