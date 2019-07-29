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

#include "src/clients/c++/request_common.h"

namespace nvidia { namespace inferenceserver { namespace client {

//==============================================================================

Error
RequestTimers::Reset()
{
  request_start_.tv_sec = 0;
  request_end_.tv_sec = 0;
  send_start_.tv_sec = 0;
  send_end_.tv_sec = 0;
  receive_start_.tv_sec = 0;
  receive_end_.tv_sec = 0;
  request_start_.tv_nsec = 0;
  request_end_.tv_nsec = 0;
  send_start_.tv_nsec = 0;
  send_end_.tv_nsec = 0;
  receive_start_.tv_nsec = 0;
  receive_end_.tv_nsec = 0;
  return Error::Success;
}

Error
RequestTimers::Record(Kind kind)
{
  switch (kind) {
    case Kind::REQUEST_START:
      clock_gettime(CLOCK_MONOTONIC, &request_start_);
      break;
    case Kind::REQUEST_END:
      clock_gettime(CLOCK_MONOTONIC, &request_end_);
      break;
    case Kind::SEND_START:
      clock_gettime(CLOCK_MONOTONIC, &send_start_);
      break;
    case Kind::SEND_END:
      clock_gettime(CLOCK_MONOTONIC, &send_end_);
      break;
    case Kind::RECEIVE_START:
      clock_gettime(CLOCK_MONOTONIC, &receive_start_);
      break;
    case Kind::RECEIVE_END:
      clock_gettime(CLOCK_MONOTONIC, &receive_end_);
      break;
  }

  return Error::Success;
}

//==============================================================================

bool
OptionsImpl::Flag(InferRequestHeader::Flag flag) const
{
  if (flag == InferRequestHeader::FLAG_NONE) {
    return false;
  }

  uint32_t iflag = static_cast<uint32_t>(flag);
  return (flags_ & iflag) != 0;
}

void
OptionsImpl::SetFlag(InferRequestHeader::Flag flag, bool value)
{
  if (flag != InferRequestHeader::FLAG_NONE) {
    uint32_t iflag = static_cast<uint32_t>(flag);
    if (value) {
      flags_ = flags_ | iflag;
    } else {
      flags_ = flags_ & ~iflag;
    }
  }
}

Error
OptionsImpl::AddRawResult(const std::shared_ptr<InferContext::Output>& output)
{
  outputs_.emplace_back(std::make_pair(
      output, OutputOptions(InferContext::Result::ResultFormat::RAW)));
  return Error::Success;
}

Error
OptionsImpl::AddClassResult(
    const std::shared_ptr<InferContext::Output>& output, uint64_t k)
{
  outputs_.emplace_back(std::make_pair(
      output, OutputOptions(InferContext::Result::ResultFormat::CLASS, k)));
  return Error::Success;
}

Error
InferContext::Options::Create(std::unique_ptr<InferContext::Options>* options)
{
  options->reset(new OptionsImpl());
  return Error::Success;
}

//==============================================================================

InputImpl::InputImpl(const ModelInput& mio)
    : mio_(mio), total_byte_size_(0), needs_shape_(false), batch_size_(0),
      bufs_idx_(0), buf_pos_(0)
{
  if (GetElementCount(mio) == -1) {
    byte_size_ = -1;
    needs_shape_ = true;
  } else {
    byte_size_ = GetByteSize(mio);
  }
}

InputImpl::InputImpl(const InputImpl& obj)
    : mio_(obj.mio_), byte_size_(obj.byte_size_),
      total_byte_size_(obj.total_byte_size_), needs_shape_(obj.needs_shape_),
      shape_(obj.shape_), batch_size_(obj.batch_size_), bufs_idx_(0),
      buf_pos_(0), bufs_(obj.bufs_), buf_byte_sizes_(obj.buf_byte_sizes_),
      str_bufs_(obj.str_bufs_)
{
}

Error
InputImpl::SetShape(const std::vector<int64_t>& dims)
{
  // Make sure the shape does not contain any invalid dimensions
  for (const auto dim : dims) {
    if (dim < 0) {
      return Error(
          RequestStatusCode::INVALID_ARG,
          "attempt to set invalid shape dimension " + std::to_string(dim) +
              ", shape dimensions must be >= 0 for input '" + Name());
    }
  }

  needs_shape_ = false;
  shape_ = dims;

  byte_size_ = GetByteSize(DType(), dims);

  return Error::Success;
}

Error
InputImpl::SetRaw(const uint8_t* input, size_t input_byte_size)
{
  if (needs_shape_) {
    bufs_.clear();
    buf_byte_sizes_.clear();
    str_bufs_.clear();
    return Error(
        RequestStatusCode::INVALID_ARG,
        "must set shape for variable-size input '" + Name() +
            "' before setting input data");
  }

  if (IsFixedSizeDataType(DType()) && (input_byte_size != (size_t)byte_size_)) {
    bufs_.clear();
    buf_byte_sizes_.clear();
    str_bufs_.clear();
    return Error(
        RequestStatusCode::INVALID_ARG,
        "invalid size " + std::to_string(input_byte_size) +
            " bytes for input '" + Name() + "', expects " +
            std::to_string(byte_size_) + " bytes");
  }

  if (bufs_.size() >= batch_size_) {
    bufs_.clear();
    buf_byte_sizes_.clear();
    str_bufs_.clear();
    return Error(
        RequestStatusCode::INVALID_ARG,
        "expecting " + std::to_string(batch_size_) +
            " invocations of SetRaw for input '" + Name() +
            "', one per batch entry");
  }

  total_byte_size_ += input_byte_size;

  bufs_.push_back(input);
  buf_byte_sizes_.push_back(input_byte_size);

  return Error::Success;
}

Error
InputImpl::SetRaw(const std::vector<uint8_t>& input)
{
  return SetRaw(&input[0], input.size());
}

Error
InputImpl::SetFromString(const std::vector<std::string>& input)
{
  if (DType() != DataType::TYPE_STRING) {
    bufs_.clear();
    buf_byte_sizes_.clear();
    str_bufs_.clear();
    return Error(
        RequestStatusCode::INVALID_ARG,
        "non-string tensor '" + Name() + "' cannot be set from string data");
  }

  if (needs_shape_) {
    return Error(
        RequestStatusCode::INVALID_ARG,
        "must set shape for variable-size input '" + Name() +
            "' before setting input data");
  }

  const int64_t element_count =
      (!shape_.empty()) ? GetElementCount(shape_) : GetElementCount(mio_);

  if (input.size() != (size_t)element_count) {
    bufs_.clear();
    buf_byte_sizes_.clear();
    str_bufs_.clear();
    return Error(
        RequestStatusCode::INVALID_ARG,
        "expecting " + std::to_string(element_count) + " strings for input '" +
            Name() + "', got " + std::to_string(input.size()));
  }

  // Serialize the strings into a "raw" buffer. The first 4-bytes are
  // the length of the string length. Next are the actual string
  // characters. There is *not* a null-terminator on the string.
  str_bufs_.emplace_back();
  std::string& sbuf = str_bufs_.back();
  for (const auto& str : input) {
    uint32_t len = str.size();
    sbuf.append(reinterpret_cast<const char*>(&len), sizeof(uint32_t));
    sbuf.append(str);
  }

  return SetRaw(reinterpret_cast<const uint8_t*>(&sbuf[0]), sbuf.size());
}

Error
InputImpl::GetNext(
    uint8_t* buf, size_t size, size_t* input_bytes, bool* end_of_input)
{
  size_t total_size = 0;

  while ((bufs_idx_ < bufs_.size()) && (size > 0)) {
    const size_t buf_byte_size = buf_byte_sizes_[bufs_idx_];
    const size_t csz = std::min(buf_byte_size - buf_pos_, size);
    if (csz > 0) {
      const uint8_t* input_ptr = bufs_[bufs_idx_] + buf_pos_;
      std::copy(input_ptr, input_ptr + csz, buf);
      buf_pos_ += csz;
      buf += csz;
      size -= csz;
      total_size += csz;
    }

    if (buf_pos_ == buf_byte_size) {
      bufs_idx_++;
      buf_pos_ = 0;
    }
  }

  *input_bytes = total_size;
  *end_of_input = (bufs_idx_ >= bufs_.size());
  return Error::Success;
}

Error
InputImpl::GetRaw(
    size_t batch_idx, const uint8_t** buf, size_t* byte_size) const
{
  if (batch_idx >= batch_size_) {
    return Error(
        RequestStatusCode::INVALID_ARG,
        "unexpected batch entry " + std::to_string(batch_idx) +
            " requested for input '" + Name() + "', batch size is " +
            std::to_string(batch_size_));
  }

  *buf = bufs_[batch_idx];
  *byte_size = buf_byte_sizes_[batch_idx];

  return Error::Success;
}

Error
InputImpl::Reset()
{
  bufs_.clear();
  buf_byte_sizes_.clear();
  str_bufs_.clear();
  bufs_idx_ = 0;
  buf_pos_ = 0;
  total_byte_size_ = 0;

  return Error::Success;
}

Error
InputImpl::PrepareForRequest()
{
  if (bufs_.size() != batch_size_) {
    return Error(
        RequestStatusCode::INVALID_ARG,
        "expecting " + std::to_string(batch_size_) +
            " invocations of SetRaw for input '" + Name() + "', have " +
            std::to_string(bufs_.size()));
  }

  // Reset position so request sends entire input.
  bufs_idx_ = 0;
  buf_pos_ = 0;
  return Error::Success;
}

//==============================================================================

ResultImpl::ResultImpl(
    const std::shared_ptr<InferContext::Output>& output, uint64_t batch_size)
    : output_(output),
      result_format_(
          reinterpret_cast<OutputImpl*>(output.get())->ResultFormat()),
      batch_size_(batch_size), has_fixed_batch1_byte_size_(false),
      batch1_byte_size_(0), batch1_element_count_(0), inplace_(false),
      inplace_ptrs_(batch_size), buffers_(batch_size), bufs_idx_(0),
      bufs_pos_(batch_size), bufs_byte_size_(batch_size), class_pos_(batch_size)
{
}

Error
ResultImpl::GetRawShape(std::vector<int64_t>* shape) const
{
  shape->clear();

  if (result_format_ != InferContext::Result::ResultFormat::RAW) {
    return Error(
        RequestStatusCode::UNSUPPORTED,
        "raw shape not available for non-RAW output '" + output_->Name() + "'");
  }

  *shape = shape_;
  return Error::Success;
}

Error
ResultImpl::GetRaw(size_t batch_idx, const std::vector<uint8_t>** buf) const
{
  if (result_format_ != InferContext::Result::ResultFormat::RAW) {
    return Error(
        RequestStatusCode::UNSUPPORTED,
        "raw result not available for non-RAW output '" + output_->Name() +
            "'");
  }

  if (batch_idx >= batch_size_) {
    return Error(
        RequestStatusCode::INVALID_ARG,
        "unexpected batch entry " + std::to_string(batch_idx) +
            " requested for output '" + output_->Name() + "', batch size is " +
            std::to_string(batch_size_));
  }

  // If result is in-place then need to make a copy of the result
  // bytes so that it can be returned as a vector.
  if (inplace_ && buffers_[batch_idx].empty()) {
    buffers_[batch_idx].assign(
        inplace_ptrs_[batch_idx],
        inplace_ptrs_[batch_idx] + bufs_byte_size_[batch_idx]);
  }

  *buf = &buffers_[batch_idx];
  return Error::Success;
}

Error
ResultImpl::GetRaw(
    size_t batch_idx, const uint8_t** buf, size_t* byte_size) const
{
  if (result_format_ != InferContext::Result::ResultFormat::RAW) {
    return Error(
        RequestStatusCode::UNSUPPORTED,
        "raw result not available for non-RAW output '" + output_->Name() +
            "'");
  }

  if (batch_idx >= batch_size_) {
    return Error(
        RequestStatusCode::INVALID_ARG,
        "unexpected batch entry " + std::to_string(batch_idx) +
            " requested for output '" + output_->Name() + "', batch size is " +
            std::to_string(batch_size_));
  }

  *byte_size = bufs_byte_size_[batch_idx];

  if (inplace_) {
    *buf = inplace_ptrs_[batch_idx];
  } else {
    *buf = &(buffers_[batch_idx][0]);
  }

  return Error::Success;
}

Error
ResultImpl::GetRawAtCursor(
    size_t batch_idx, const uint8_t** buf, size_t adv_byte_size)
{
  if (result_format_ != InferContext::Result::ResultFormat::RAW) {
    return Error(
        RequestStatusCode::UNSUPPORTED,
        "raw result not available for non-RAW output '" + output_->Name() +
            "'");
  }

  if (batch_idx >= batch_size_) {
    return Error(
        RequestStatusCode::INVALID_ARG,
        "unexpected batch entry " + std::to_string(batch_idx) +
            " requested for output '" + output_->Name() + "', batch size is " +
            std::to_string(batch_size_));
  }

  if ((bufs_pos_[batch_idx] + adv_byte_size) > bufs_byte_size_[batch_idx]) {
    return Error(
        RequestStatusCode::UNSUPPORTED,
        "attempt to read beyond end of result for output '" + output_->Name() +
            "'");
  }

  if (inplace_) {
    *buf = inplace_ptrs_[batch_idx] + bufs_pos_[batch_idx];
  } else {
    *buf = &buffers_[batch_idx][bufs_pos_[batch_idx]];
  }

  bufs_pos_[batch_idx] += adv_byte_size;
  return Error::Success;
}

Error
ResultImpl::GetClassCount(size_t batch_idx, size_t* cnt) const
{
  if (result_format_ != InferContext::Result::ResultFormat::CLASS) {
    return Error(
        RequestStatusCode::UNSUPPORTED,
        "class result not available for non-CLASS output '" + output_->Name() +
            "'");
  }

  // Number of classifications should equal expected batch size but
  // check both to be careful and to protext class_pos_ accesses.
  if ((batch_idx >= (size_t)class_result_.batch_classes().size()) ||
      (batch_idx >= batch_size_)) {
    return Error(
        RequestStatusCode::INVALID_ARG,
        "unexpected batch entry " + std::to_string(batch_idx) +
            " requested for output '" + output_->Name() + "', batch size is " +
            std::to_string(batch_size_));
  }

  const InferResponseHeader::Output::Classes& classes =
      class_result_.batch_classes(batch_idx);

  *cnt = classes.cls().size();
  return Error::Success;
}

Error
ResultImpl::GetClassAtCursor(
    size_t batch_idx, InferContext::Result::ClassResult* result)
{
  if (result_format_ != InferContext::Result::ResultFormat::CLASS) {
    return Error(
        RequestStatusCode::UNSUPPORTED,
        "class result not available for non-CLASS output '" + output_->Name() +
            "'");
  }

  // Number of classifications should equal expected batch size but
  // check both to be careful and to protext class_pos_ accesses.
  if ((batch_idx >= (size_t)class_result_.batch_classes().size()) ||
      (batch_idx >= batch_size_)) {
    return Error(
        RequestStatusCode::INVALID_ARG,
        "unexpected batch entry " + std::to_string(batch_idx) +
            " requested for output '" + output_->Name() + "', batch size is " +
            std::to_string(batch_size_));
  }

  const InferResponseHeader::Output::Classes& classes =
      class_result_.batch_classes(batch_idx);

  if (class_pos_[batch_idx] >= (size_t)classes.cls().size()) {
    return Error(
        RequestStatusCode::UNSUPPORTED,
        "attempt to read beyond end of result for output output '" +
            output_->Name() + "'");
  }

  const InferResponseHeader::Output::Class& cls =
      classes.cls(class_pos_[batch_idx]);

  result->idx = cls.idx();
  result->value = cls.value();
  result->label = cls.label();

  class_pos_[batch_idx]++;
  return Error::Success;
}

Error
ResultImpl::ResetCursors()
{
  std::fill(bufs_pos_.begin(), bufs_pos_.end(), 0);
  std::fill(class_pos_.begin(), class_pos_.end(), 0);
  return Error::Success;
}

Error
ResultImpl::ResetCursor(size_t batch_idx)
{
  if (batch_idx >= batch_size_) {
    return Error(
        RequestStatusCode::INVALID_ARG,
        "unexpected batch entry " + std::to_string(batch_idx) +
            " requested for output '" + output_->Name() + "', batch size is " +
            std::to_string(batch_size_));
  }

  bufs_pos_[batch_idx] = 0;
  class_pos_[batch_idx] = 0;
  return Error::Success;
}

Error
ResultImpl::SetBatchRawResult(
    const size_t batch1_byte_size, const uint8_t* buf, size_t size,
    size_t* result_bytes)
{
  size_t total_size = 0;

  // If the batch1 size is 0, then we have an empty result tensor. We
  // don't need to do anything in this case except advance bufs_idx_
  // to show that all data has been read for the tensor.
  if (batch1_byte_size == 0) {
    bufs_idx_ = buffers_.size();
  }

  while ((bufs_idx_ < buffers_.size()) && (size > 0)) {
    const size_t csz = std::min(batch1_byte_size - bufs_pos_[bufs_idx_], size);
    if (csz > 0) {
      // If result is being used in-place just save a pointer to its
      // base. For in-place, 'buf' must be a single contiguous buffer
      // delivering the entire batch, but we don't need to check that
      // here since it is checked in SetNextRawResult.
      if (inplace_) {
        inplace_ptrs_[bufs_idx_] = buf;
      } else {
        std::copy(buf, buf + csz, std::back_inserter(buffers_[bufs_idx_]));
      }

      bufs_pos_[bufs_idx_] += csz;
      bufs_byte_size_[bufs_idx_] += csz;
      buf += csz;
      size -= csz;
      total_size += csz;
    }

    if (bufs_pos_[bufs_idx_] == batch1_byte_size) {
      bufs_idx_++;
    }
  }

  *result_bytes = total_size;
  return Error::Success;
}

Error
ResultImpl::SetNextRawResult(
    const uint8_t* buf, size_t size, const bool inplace, size_t* result_bytes)
{
  // 'inplace_' is initially false, so we use it to detect if called
  // more than once. With 'inplace' == true should only be called a
  // single time since that call should deliver a single contiguous
  // buffer that holds all results.
  if (inplace && inplace_) {
    return Error(
        RequestStatusCode::INTERNAL,
        "in-place results for '" + output_->Name() +
            "' must be delivered in a single continguous buffer");
  }

  inplace_ = inplace;

  // If output has a known batch1-byte-size (which is the same for
  // every item in the batch) then can directly assign the results to
  // the appropriate per-batch buffers.
  if (has_fixed_batch1_byte_size_) {
    return SetBatchRawResult(batch1_byte_size_, buf, size, result_bytes);
  }

  // Output is a non-fixed-sized datatype. For now we assume that it
  // is TYPE_STRING and so we need to parse buf to get the size for
  // each batch.
  if (bufs_idx_ == buffers_.size()) {
    *result_bytes = 0;
    return Error::Success;
  }

  // If there is partial batch data then append the new data to it and
  // parse it all together.
  const size_t orig_size = size;
  if (!pending_.empty()) {
    std::copy(buf, buf + size, std::back_inserter(pending_));
    buf = &pending_[0];
    size = pending_.size();
  }

  std::vector<size_t> batch_byte_sizes;
  size_t buf_offset = 0;
  for (size_t b = 0; b < batch_size_; ++b) {
    size_t batch_byte_size = 0;
    for (size_t e = 0; e < batch1_element_count_; ++e) {
      const uint32_t len =
          *(reinterpret_cast<const uint32_t*>(buf + buf_offset));
      batch_byte_size += sizeof(len) + len;
      buf_offset += sizeof(len) + len;
      if (buf_offset > size) {
        break;
      }
    }

    if (buf_offset > size) {
      break;
    }

    batch_byte_sizes.push_back(batch_byte_size);
  }

  // Don't assign batches until we have entire batch of tensor data.
  if (batch_byte_sizes.size() < batch_size_) {
    if (pending_.empty()) {
      std::copy(buf, buf + orig_size, std::back_inserter(pending_));
    }
    *result_bytes = orig_size;
  } else {
    for (size_t sz : batch_byte_sizes) {
      size_t batch1_result_bytes = 0;
      Error err = SetBatchRawResult(sz, buf, sz, &batch1_result_bytes);
      if (!err.IsOk()) {
        return err;
      }
      if (batch1_result_bytes != sz) {
        return Error(
            RequestStatusCode::INTERNAL,
            "output '" + output_->Name() + "' expecting batch size " +
                std::to_string(sz) + " for non-fixed-sized result, got " +
                std::to_string(batch1_result_bytes));
      }

      buf += sz;
    }

    if (bufs_idx_ != buffers_.size()) {
      return Error(
          RequestStatusCode::INTERNAL,
          "output '" + output_->Name() +
              "' failed to set result for entire batch");
    }

    pending_.clear();
    *result_bytes = orig_size - (size - buf_offset);
  }

  return Error::Success;
}

//==============================================================================

Error
RequestImpl::PostRunProcessing(
    const InferResponseHeader& infer_response,
    InferContext::ResultMap* results) const
{
  // At this point, the RAW requested results have their result values
  // set. Now need to initialize non-RAW results.
  for (auto& pr : *results) {
    ResultImpl* r = reinterpret_cast<ResultImpl*>(pr.second.get());
    r->SetModel(infer_response.model_name(), infer_response.model_version());
    switch (r->ResultFormat()) {
      case InferContext::Result::ResultFormat::RAW:
        r->ResetCursors();
        break;

      case InferContext::Result::ResultFormat::CLASS: {
        for (const auto& ir : infer_response.output()) {
          if (ir.name() == r->GetOutput()->Name()) {
            r->SetClassResult(ir);
            break;
          }
        }
        break;
      }
    }
  }

  return Error::Success;
}

//==============================================================================

InferContextImpl::InferContextImpl(
    const std::string& model_name, int64_t model_version,
    CorrelationID correlation_id, bool verbose)
    : model_name_(model_name), model_version_(model_version),
      correlation_id_(correlation_id), verbose_(verbose), batch_size_(0),
      async_request_id_(1), worker_(), exiting_(false)
{
}

Error
InferContextImpl::Init(std::unique_ptr<ServerStatusContext> sctx)
{
  // Get status of the model and create the inputs and outputs.
  ServerStatus server_status;
  Error err = sctx->GetServerStatus(&server_status);
  if (err.IsOk()) {
    const auto& itr = server_status.model_status().find(model_name_);
    if (itr == server_status.model_status().end()) {
      err = Error(
          RequestStatusCode::INTERNAL,
          "unable to find status information for \"" + model_name_ + "\"");
    } else {
      const ModelConfig& model_info = itr->second.config();

      max_batch_size_ =
          static_cast<uint64_t>(std::max(0, model_info.max_batch_size()));

      // Create inputs and outputs
      for (const auto& io : model_info.input()) {
        inputs_.emplace_back(std::make_shared<InputImpl>(io));
      }
      for (const auto& io : model_info.output()) {
        outputs_.emplace_back(std::make_shared<OutputImpl>(io));
      }
    }
  }

  return err;
}

Error
InferContextImpl::GetInput(
    const std::string& name, std::shared_ptr<Input>* input) const
{
  for (const auto& io : inputs_) {
    if (io->Name() == name) {
      *input = io;
      return Error::Success;
    }
  }

  return Error(
      RequestStatusCode::INVALID_ARG,
      "unknown input '" + name + "' for '" + model_name_ + "'");
}

Error
InferContextImpl::GetOutput(
    const std::string& name, std::shared_ptr<Output>* output) const
{
  for (const auto& io : outputs_) {
    if (io->Name() == name) {
      *output = io;
      return Error::Success;
    }
  }

  return Error(
      RequestStatusCode::INVALID_ARG,
      "unknown output '" + name + "' for '" + model_name_ + "'");
}

Error
InferContextImpl::SetRunOptions(const InferContext::Options& boptions)
{
  const OptionsImpl& options = reinterpret_cast<const OptionsImpl&>(boptions);

  // If the model doesn't support batching (i.e. max_batch_size_ == 0)
  // then still allow batch size of 1 to be specified.
  uint64_t effective_max_batch_size = std::max((uint64_t)1, max_batch_size_);
  if (options.BatchSize() > effective_max_batch_size) {
    return Error(
        RequestStatusCode::INVALID_ARG,
        "run batch-size " + std::to_string(options.BatchSize()) +
            " exceeds maximum batch size " +
            std::to_string(effective_max_batch_size) + " allowed for model '" +
            model_name_ + "'");
  }

  // If batch-size 0 was requested (no batching) treat it like
  // batch-size 1.
  batch_size_ = std::max((uint64_t)1, options.BatchSize());

  // Create the InferRequestHeader protobuf. This protobuf will be
  // used for all subsequent requests.
  infer_request_.Clear();
  infer_request_.set_flags(options.Flags());
  infer_request_.set_batch_size(batch_size_);
  infer_request_.set_correlation_id(correlation_id_);

  for (const auto& io : inputs_) {
    reinterpret_cast<InputImpl*>(io.get())->SetBatchSize(batch_size_);
  }

  for (const auto& p : options.Outputs()) {
    const std::shared_ptr<Output>& output = p.first;
    const OptionsImpl::OutputOptions& ooptions = p.second;

    reinterpret_cast<OutputImpl*>(output.get())
        ->SetResultFormat(ooptions.result_format);

    auto routput = infer_request_.add_output();
    routput->set_name(output->Name());
    if (ooptions.result_format == Result::ResultFormat::CLASS) {
      routput->mutable_cls()->set_count(ooptions.u64);
    }
  }

  return Error::Success;
}

Error
InferContextImpl::GetStat(Stat* stat) const
{
  stat->completed_request_count = context_stat_.completed_request_count;
  stat->cumulative_total_request_time_ns =
      context_stat_.cumulative_total_request_time_ns;
  stat->cumulative_send_time_ns = context_stat_.cumulative_send_time_ns;
  stat->cumulative_receive_time_ns = context_stat_.cumulative_receive_time_ns;
  return Error::Success;
}

Error
InferContextImpl::UpdateStat(const RequestTimers& timer)
{
  uint64_t request_start_ns = timer.request_start_.tv_sec * NANOS_PER_SECOND +
                              timer.request_start_.tv_nsec;
  uint64_t request_end_ns =
      timer.request_end_.tv_sec * NANOS_PER_SECOND + timer.request_end_.tv_nsec;
  uint64_t send_start_ns =
      timer.send_start_.tv_sec * NANOS_PER_SECOND + timer.send_start_.tv_nsec;
  uint64_t send_end_ns =
      timer.send_end_.tv_sec * NANOS_PER_SECOND + timer.send_end_.tv_nsec;
  uint64_t receive_start_ns = timer.receive_start_.tv_sec * NANOS_PER_SECOND +
                              timer.receive_start_.tv_nsec;
  uint64_t receive_end_ns =
      timer.receive_end_.tv_sec * NANOS_PER_SECOND + timer.receive_end_.tv_nsec;
  if ((request_start_ns > request_end_ns) || (send_start_ns > send_end_ns) ||
      (receive_start_ns > receive_end_ns)) {
    return Error(
        RequestStatusCode::INVALID_ARG,
        "Timer not set correctly." +
            ((request_start_ns > request_end_ns)
                 ? (" Request time from " + std::to_string(request_start_ns) +
                    " to " + std::to_string(request_end_ns) + ".")
                 : "") +
            ((send_start_ns > send_end_ns)
                 ? (" Send time from " + std::to_string(send_start_ns) +
                    " to " + std::to_string(send_end_ns) + ".")
                 : "") +
            ((receive_start_ns > receive_end_ns)
                 ? (" Receive time from " + std::to_string(receive_start_ns) +
                    " to " + std::to_string(receive_end_ns) + ".")
                 : ""));
  }

  uint64_t request_time_ns = request_end_ns - request_start_ns;
  uint64_t send_time_ns = send_end_ns - send_start_ns;
  uint64_t receive_time_ns = receive_end_ns - receive_start_ns;

  context_stat_.completed_request_count++;
  context_stat_.cumulative_total_request_time_ns += request_time_ns;
  context_stat_.cumulative_send_time_ns += send_time_ns;
  context_stat_.cumulative_receive_time_ns += receive_time_ns;
  return Error::Success;
}

Error
InferContextImpl::GetReadyAsyncRequest(
    std::shared_ptr<Request>* request, bool* is_ready, bool wait)
{
  *is_ready = false;
  std::unique_lock<std::mutex> lock(mutex_);
  if (ongoing_async_requests_.size() == 0) {
    return Error(
        RequestStatusCode::UNAVAILABLE,
        "No asynchronous requests have been sent");
  }

  cv_.wait(lock, [is_ready, request, this, wait] {
    for (auto& ongoing_async_request : this->ongoing_async_requests_) {
      if (std::static_pointer_cast<RequestImpl>(ongoing_async_request.second)
              ->IsReady()) {
        *request = ongoing_async_request.second;
        *is_ready = true;
        return true;
      }
    }

    if (!wait) {
      return true;
    } else {
      return false;
    }
  });

  return Error::Success;
}

Error
InferContextImpl::IsRequestReady(
    const std::shared_ptr<Request>& async_request, bool* is_ready, bool wait)
{
  *is_ready = false;
  std::unique_lock<std::mutex> lock(mutex_);
  if (ongoing_async_requests_.size() == 0) {
    return Error(
        RequestStatusCode::INVALID_ARG,
        "No asynchronous requests have been sent");
  }

  std::shared_ptr<RequestImpl> request =
      std::static_pointer_cast<RequestImpl>(async_request);

  auto itr = ongoing_async_requests_.find(request->RunIndex());
  if (itr == ongoing_async_requests_.end()) {
    return Error(
        RequestStatusCode::INVALID_ARG,
        "No matched asynchronous request found.");
  }

  cv_.wait(lock, [is_ready, &request, wait] {
    if (!request->IsReady()) {
      if (wait) {
        return false;
      }
    } else {
      *is_ready = true;
    }
    return true;
  });

  return Error::Success;
}

}}}  // namespace nvidia::inferenceserver::client
