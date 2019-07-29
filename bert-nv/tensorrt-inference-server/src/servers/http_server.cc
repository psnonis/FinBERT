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

#include "src/servers/http_server.h"

#include <google/protobuf/text_format.h>
#include <algorithm>
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "evhtp/evhtp.h"
#include "libevent/include/event2/buffer.h"
#include "re2/re2.h"
#include "src/core/backend.h"
#include "src/core/constants.h"
#include "src/core/logging.h"
#include "src/core/provider_utils.h"
#include "src/core/request_status.h"
#include "src/core/server.h"

namespace nvidia { namespace inferenceserver {

// Handle HTTP requests
class HTTPServerImpl : public HTTPServer {
 public:
  explicit HTTPServerImpl(
      InferenceServer* server, const std::vector<std::string>& endpoints,
      int32_t port, int thread_cnt)
      : server_(server), endpoint_names_(endpoints), port_(port),
        thread_cnt_(thread_cnt),
        api_regex_(R"(/api/(health|profile|infer|status)(.*))"),
        health_regex_(R"(/(live|ready))"),
        infer_regex_(R"(/([^/]+)(?:/(\d+))?)"), status_regex_(R"(/(.*))")
  {
  }

  ~HTTPServerImpl() { Stop(); }

  static void Dispatch(evhtp_request_t* req, void* arg);

  Status Start() override;
  Status Stop() override;

 private:
  // Class object associated to evhtp thread, requests received are bounded
  // with the thread that accepts it. Need to keep track of that and let the
  // corresponding thread send back the reply
  class InferRequest {
   public:
    InferRequest(
        evhtp_request_t* req, uint64_t id,
        const std::shared_ptr<InferRequestProvider>& request_provider,
        const std::shared_ptr<HTTPInferResponseProvider>& response_provider,
        const std::shared_ptr<ModelInferStats>& infer_stats,
        const std::shared_ptr<ModelInferStats::ScopedTimer>& timer);

    evhtp_res FinalizeResponse();

   private:
    friend class HTTPServerImpl;
    evhtp_request_t* req_;
    evthr_t* thread_;
    uint64_t id_;
    RequestStatus request_status_;
    std::shared_ptr<InferRequestProvider> request_provider_;
    std::shared_ptr<HTTPInferResponseProvider> response_provider_;
    std::shared_ptr<ModelInferStats> infer_stats_;
    std::shared_ptr<ModelInferStats::ScopedTimer> timer_;
  };

  void Handle(evhtp_request_t* req);
  void HandleHealth(evhtp_request_t* req, const std::string& health_uri);
  void HandleProfile(evhtp_request_t* req, const std::string& profile_uri);
  void HandleInfer(evhtp_request_t* req, const std::string& infer_uri);
  void HandleStatus(evhtp_request_t* req, const std::string& status_uri);

  // Helper function that utilizes RETURN_IF_ERROR to avoid nested 'if'
  Status InferHelper(
      std::shared_ptr<ModelInferStats>& infer_stats,
      std::shared_ptr<ModelInferStats::ScopedTimer>& timer,
      const std::string& model_name, int64_t model_version,
      InferRequestHeader& request_header, evhtp_request_t* req);

  void FinishInferResponse(const std::shared_ptr<InferRequest>& req);
  static void OKReplyCallback(evthr_t* thr, void* arg, void* shared);
  static void BADReplyCallback(evthr_t* thr, void* arg, void* shared);

  static void StopCallback(int sock, short events, void* arg);

  InferenceServer* server_;
  std::vector<std::string> endpoint_names_;
  int32_t port_;
  int thread_cnt_;
  re2::RE2 api_regex_;
  re2::RE2 health_regex_;
  re2::RE2 infer_regex_;
  re2::RE2 status_regex_;

  evhtp_t* htp_;
  struct event_base* evbase_;
  std::thread worker_;
  int fds_[2];
  event* break_ev_;
};

Status
HTTPServerImpl::Start()
{
  if (!worker_.joinable()) {
    evbase_ = event_base_new();
    htp_ = evhtp_new(evbase_, NULL);
    evhtp_set_gencb(htp_, HTTPServerImpl::Dispatch, this);
    evhtp_use_threads_wexit(htp_, NULL, NULL, thread_cnt_, NULL);
    evhtp_bind_socket(htp_, "0.0.0.0", port_, 1024);
    // Set listening event for breaking event loop
    evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, fds_);
    break_ev_ = event_new(evbase_, fds_[0], EV_READ, StopCallback, evbase_);
    event_add(break_ev_, NULL);
    worker_ = std::thread(event_base_loop, evbase_, 0);
    return Status::Success;
  }

  return Status(
      RequestStatusCode::ALREADY_EXISTS, "HTTP server is already running.");
}

Status
HTTPServerImpl::Stop()
{
  if (worker_.joinable()) {
    // Notify event loop to break via fd write
    send(fds_[1], &evbase_, sizeof(event_base*), 0);
    worker_.join();
    event_free(break_ev_);
    evutil_closesocket(fds_[0]);
    evutil_closesocket(fds_[1]);
    evhtp_unbind_socket(htp_);
    evhtp_free(htp_);
    event_base_free(evbase_);
    return Status::Success;
  }

  return Status(RequestStatusCode::UNAVAILABLE, "HTTP server is not running.");
}

void
HTTPServerImpl::StopCallback(int sock, short events, void* arg)
{
  struct event_base* base = (struct event_base*)arg;
  event_base_loopbreak(base);
}

void
HTTPServerImpl::Dispatch(evhtp_request_t* req, void* arg)
{
  (static_cast<HTTPServerImpl*>(arg))->Handle(req);
}

void
HTTPServerImpl::Handle(evhtp_request_t* req)
{
  LOG_VERBOSE(1) << "HTTP request: " << req->method << " "
                 << req->uri->path->full;

  std::string endpoint, rest;
  if (RE2::FullMatch(
          std::string(req->uri->path->full), api_regex_, &endpoint, &rest)) {
    // status
    if (endpoint == "status" &&
        (std::find(endpoint_names_.begin(), endpoint_names_.end(), "status") !=
         endpoint_names_.end())) {
      HandleStatus(req, rest);
      return;
    }
    // health
    if (endpoint == "health" &&
        (std::find(endpoint_names_.begin(), endpoint_names_.end(), "health") !=
         endpoint_names_.end())) {
      HandleHealth(req, rest);
      return;
    }
    // profile
    if (endpoint == "profile" &&
        (std::find(endpoint_names_.begin(), endpoint_names_.end(), "profile") !=
         endpoint_names_.end())) {
      HandleProfile(req, rest);
      return;
    }
    // infer
    if (endpoint == "infer" &&
        (std::find(endpoint_names_.begin(), endpoint_names_.end(), "infer") !=
         endpoint_names_.end())) {
      HandleInfer(req, rest);
      return;
    }
  }

  LOG_VERBOSE(1) << "HTTP error: " << req->method << " " << req->uri->path->full
                 << " - " << static_cast<int>(EVHTP_RES_BADREQ);
  evhtp_send_reply(req, EVHTP_RES_BADREQ);
}

void
HTTPServerImpl::HandleHealth(
    evhtp_request_t* req, const std::string& health_uri)
{
  ServerStatTimerScoped timer(
      server_->StatusManager(), ServerStatTimerScoped::Kind::HEALTH);

  if (req->method != htp_method_GET) {
    evhtp_send_reply(req, EVHTP_RES_METHNALLOWED);
    return;
  }

  std::string mode;
  if (!health_uri.empty()) {
    if (!RE2::FullMatch(health_uri, health_regex_, &mode)) {
      evhtp_send_reply(req, EVHTP_RES_BADREQ);
      return;
    }
  }

  RequestStatus request_status;
  bool health;
  server_->HandleHealth(&request_status, &health, mode);

  evhtp_headers_add_header(
      req->headers_out,
      evhtp_header_new(
          kStatusHTTPHeader, request_status.ShortDebugString().c_str(), 1, 1));

  evhtp_send_reply(req, (health) ? EVHTP_RES_OK : EVHTP_RES_BADREQ);
}

void
HTTPServerImpl::HandleProfile(
    evhtp_request_t* req, const std::string& profile_uri)
{
  ServerStatTimerScoped timer(
      server_->StatusManager(), ServerStatTimerScoped::Kind::PROFILE);

  if (req->method != htp_method_GET) {
    evhtp_send_reply(req, EVHTP_RES_METHNALLOWED);
    return;
  }

  if (!profile_uri.empty() && (profile_uri != "/")) {
    evhtp_send_reply(req, EVHTP_RES_BADREQ);
    return;
  }

  std::string cmd;
  const char* cmd_c_str = evhtp_kv_find(req->uri->query, "cmd");
  if (cmd_c_str != NULL) {
    cmd = std::string(cmd_c_str);
  }

  RequestStatus request_status;
  server_->HandleProfile(&request_status, cmd);

  evhtp_headers_add_header(
      req->headers_out,
      evhtp_header_new(
          kStatusHTTPHeader, request_status.ShortDebugString().c_str(), 1, 1));

  evhtp_send_reply(
      req, (request_status.code() == RequestStatusCode::SUCCESS)
               ? EVHTP_RES_OK
               : EVHTP_RES_BADREQ);
}

void
HTTPServerImpl::HandleInfer(evhtp_request_t* req, const std::string& infer_uri)
{
  if (req->method != htp_method_POST) {
    evhtp_send_reply(req, EVHTP_RES_METHNALLOWED);
    return;
  }

  std::string model_name, model_version_str;
  if (!infer_uri.empty()) {
    if (!RE2::FullMatch(
            infer_uri, infer_regex_, &model_name, &model_version_str)) {
      evhtp_send_reply(req, EVHTP_RES_BADREQ);
      return;
    }
  }

  int64_t model_version = -1;
  if (!model_version_str.empty()) {
    model_version = std::atoll(model_version_str.c_str());
  }

  auto infer_stats =
      std::make_shared<ModelInferStats>(server_->StatusManager(), model_name);
  auto timer = std::make_shared<ModelInferStats::ScopedTimer>();
  infer_stats->StartRequestTimer(timer.get());
  infer_stats->SetRequestedVersion(model_version);

  absl::string_view infer_request_header = absl::string_view(
      evhtp_kv_find(req->headers_in, kInferRequestHTTPHeader));
  std::string infer_request_header_str(
      infer_request_header.data(), infer_request_header.size());

  InferRequestHeader request_header;
  google::protobuf::TextFormat::ParseFromString(
      infer_request_header_str, &request_header);

  Status status = InferHelper(
      infer_stats, timer, model_name, model_version, request_header, req);

  if (!status.IsOk()) {
    RequestStatus request_status;
    InferResponseHeader response_header;
    response_header.set_id(request_header.id());
    evhtp_headers_add_header(
        req->headers_out,
        evhtp_header_new(
            kInferResponseHTTPHeader,
            response_header.ShortDebugString().c_str(), 1, 1));
    LOG_VERBOSE(1) << "Infer failed: " << status.Message();
    infer_stats->SetFailed(true);
    RequestStatusFactory::Create(
        &request_status, 0 /* request_id */, server_->Id(), status);

    // this part still needs to be implemented in the completer
    evhtp_headers_add_header(
        req->headers_out, evhtp_header_new(
                              kStatusHTTPHeader,
                              request_status.ShortDebugString().c_str(), 1, 1));
    evhtp_headers_add_header(
        req->headers_out,
        evhtp_header_new("Content-Type", "application/octet-stream", 1, 1));

    evhtp_send_reply(
        req, (request_status.code() == RequestStatusCode::SUCCESS)
                 ? EVHTP_RES_OK
                 : EVHTP_RES_BADREQ);
  }
}

void
HTTPServerImpl::HandleStatus(
    evhtp_request_t* req, const std::string& status_uri)
{
  ServerStatTimerScoped timer(
      server_->StatusManager(), ServerStatTimerScoped::Kind::STATUS);

  if (req->method != htp_method_GET) {
    evhtp_send_reply(req, EVHTP_RES_METHNALLOWED);
    return;
  }

  std::string model_name;
  if (!status_uri.empty()) {
    if (!RE2::FullMatch(status_uri, status_regex_, &model_name)) {
      evhtp_send_reply(req, EVHTP_RES_BADREQ);
      return;
    }
  }

  RequestStatus request_status;
  ServerStatus server_status;
  server_->HandleStatus(&request_status, &server_status, model_name);

  // If got status successfully then send it...
  if (request_status.code() == RequestStatusCode::SUCCESS) {
    std::string format;
    const char* format_c_str = evhtp_kv_find(req->uri->query, "format");
    if (format_c_str != NULL) {
      format = std::string(format_c_str);
    } else {
      format = "text";
    }

    std::string server_status_str;
    if (format == "binary") {
      server_status.SerializeToString(&server_status_str);
      evbuffer_add(
          req->buffer_out, server_status_str.c_str(), server_status_str.size());
      evhtp_headers_add_header(
          req->headers_out,
          evhtp_header_new("Content-Type", "application/octet-stream", 1, 1));
    } else {
      server_status_str = server_status.DebugString();
      evbuffer_add(
          req->buffer_out, server_status_str.c_str(), server_status_str.size());
    }
  }

  evhtp_headers_add_header(
      req->headers_out,
      evhtp_header_new(
          kStatusHTTPHeader, request_status.ShortDebugString().c_str(), 1, 1));

  evhtp_send_reply(
      req, (request_status.code() == RequestStatusCode::SUCCESS)
               ? EVHTP_RES_OK
               : EVHTP_RES_BADREQ);
}

Status
HTTPServerImpl::InferHelper(
    std::shared_ptr<ModelInferStats>& infer_stats,
    std::shared_ptr<ModelInferStats::ScopedTimer>& timer,
    const std::string& model_name, int64_t model_version,
    InferRequestHeader& request_header, evhtp_request_t* req)
{
  std::shared_ptr<InferenceServer::InferBackendHandle> backend = nullptr;
  RETURN_IF_ERROR(InferenceServer::InferBackendHandle::Create(
      server_, model_name, model_version, &backend));
  infer_stats->SetMetricReporter(
      backend->GetInferenceBackend()->MetricReporter());

  std::unordered_map<std::string, std::shared_ptr<SystemMemory>> input_map;
  RETURN_IF_ERROR(
      NormalizeRequestHeader(*backend->GetInferenceBackend(), request_header));
  RETURN_IF_ERROR(EVBufferToInputMap(
      model_name, request_header, req->buffer_in, input_map));

  std::shared_ptr<InferRequestProvider> request_provider;
  RETURN_IF_ERROR(InferRequestProvider::Create(
      model_name, model_version, request_header, input_map, &request_provider));
  infer_stats->SetBatchSize(request_provider->RequestHeader().batch_size());

  std::shared_ptr<HTTPInferResponseProvider> response_provider;
  RETURN_IF_ERROR(HTTPInferResponseProvider::Create(
      req->buffer_out, *backend->GetInferenceBackend(),
      request_provider->RequestHeader(),
      backend->GetInferenceBackend()->GetLabelProvider(), &response_provider));

  std::shared_ptr<InferRequest> request(new InferRequest(
      req, request_header.id(), request_provider, response_provider,
      infer_stats, timer));
  server_->HandleInfer(
      &(request->request_status_), backend, request->request_provider_,
      request->response_provider_, infer_stats,
      [this, request]() mutable { this->FinishInferResponse(request); });

  return Status::Success;
}

void
HTTPServerImpl::OKReplyCallback(evthr_t* thr, void* arg, void* shared)
{
  evhtp_request_t* request = (evhtp_request_t*)arg;
  evhtp_send_reply(request, EVHTP_RES_OK);
  evhtp_request_resume(request);
}

void
HTTPServerImpl::BADReplyCallback(evthr_t* thr, void* arg, void* shared)
{
  evhtp_request_t* request = (evhtp_request_t*)arg;
  evhtp_send_reply(request, EVHTP_RES_BADREQ);
  evhtp_request_resume(request);
}

void
HTTPServerImpl::FinishInferResponse(const std::shared_ptr<InferRequest>& req)
{
  if (req->FinalizeResponse() == EVHTP_RES_OK) {
    evthr_defer(req->thread_, OKReplyCallback, req->req_);
  } else {
    evthr_defer(req->thread_, BADReplyCallback, req->req_);
  }
}

HTTPServerImpl::InferRequest::InferRequest(
    evhtp_request_t* req, uint64_t id,
    const std::shared_ptr<InferRequestProvider>& request_provider,
    const std::shared_ptr<HTTPInferResponseProvider>& response_provider,
    const std::shared_ptr<ModelInferStats>& infer_stats,
    const std::shared_ptr<ModelInferStats::ScopedTimer>& timer)
    : req_(req), id_(id), request_provider_(request_provider),
      response_provider_(response_provider), infer_stats_(infer_stats),
      timer_(timer)
{
  evhtp_connection_t* htpconn = evhtp_request_get_connection(req);
  thread_ = htpconn->thread;
  evhtp_request_pause(req);
}

evhtp_res
HTTPServerImpl::InferRequest::FinalizeResponse()
{
  InferResponseHeader* response_header =
      response_provider_->MutableResponseHeader();
  if (request_status_.code() == RequestStatusCode::SUCCESS) {
    std::string format;
    const char* format_c_str = evhtp_kv_find(req_->uri->query, "format");
    if (format_c_str != NULL) {
      format = std::string(format_c_str);
    } else {
      format = "text";
    }

    // The description of the raw outputs needs to go in
    // the kInferResponseHTTPHeader since it is needed to
    // interpret the body. The entire response (including
    // classifications) is serialized at the end of the
    // body.
    response_header->set_id(id_);

    std::string rstr;
    if (format == "binary") {
      response_header->SerializeToString(&rstr);
    } else {
      rstr = response_header->DebugString();
    }
    evbuffer_add(req_->buffer_out, rstr.c_str(), rstr.size());

    // We do this in destructive manner since we are the
    // last one to use response header from the provider.
    for (int i = 0; i < response_header->output_size(); ++i) {
      InferResponseHeader::Output* output = response_header->mutable_output(i);
      output->clear_batch_classes();
    }
  } else {
    evbuffer_drain(req_->buffer_out, -1);
    response_header->Clear();
    response_header->set_id(id_);
  }
  evhtp_headers_add_header(
      req_->headers_out,
      evhtp_header_new(
          kInferResponseHTTPHeader, response_header->ShortDebugString().c_str(),
          1, 1));
  evhtp_headers_add_header(
      req_->headers_out,
      evhtp_header_new(
          kStatusHTTPHeader, request_status_.ShortDebugString().c_str(), 1, 1));
  evhtp_headers_add_header(
      req_->headers_out,
      evhtp_header_new("Content-Type", "application/octet-stream", 1, 1));

  return (request_status_.code() == RequestStatusCode::SUCCESS)
             ? EVHTP_RES_OK
             : EVHTP_RES_BADREQ;
}

Status
HTTPServer::Create(
    InferenceServer* server,
    const std::map<int32_t, std::vector<std::string>>& port_map, int thread_cnt,
    std::vector<std::unique_ptr<HTTPServer>>* http_servers)
{
  if (port_map.empty()) {
    return Status(
        RequestStatusCode::INVALID_ARG,
        "HTTP is enabled but none of the service endpoints have a valid port "
        "assignment");
  }
  http_servers->clear();
  for (auto const& ep_map : port_map) {
    std::string addr = "0.0.0.0:" + std::to_string(ep_map.first);
    LOG_INFO << "Starting HTTPService at " << addr;
    http_servers->emplace_back(
        new HTTPServerImpl(server, ep_map.second, ep_map.first, thread_cnt));
  }

  return Status::Success;
}
}}  // namespace nvidia::inferenceserver
