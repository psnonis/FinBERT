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
//

#include "src/core/model_repository_manager.h"

#include <algorithm>
#include <deque>
#include <stdexcept>
#include <thread>
#include "src/backends/caffe2/netdef_backend.pb.h"
#include "src/backends/caffe2/netdef_backend_factory.h"
#include "src/backends/custom/custom_backend.pb.h"
#include "src/backends/custom/custom_backend_factory.h"
#include "src/backends/ensemble/ensemble_backend.pb.h"
#include "src/backends/ensemble/ensemble_backend_factory.h"
#include "src/backends/onnx/onnx_backend.pb.h"
#include "src/backends/onnx/onnx_backend_factory.h"
#include "src/backends/pytorch/libtorch_backend.pb.h"
#include "src/backends/pytorch/libtorch_backend_factory.h"
#include "src/backends/tensorflow/graphdef_backend.pb.h"
#include "src/backends/tensorflow/graphdef_backend_factory.h"
#include "src/backends/tensorflow/savedmodel_backend.pb.h"
#include "src/backends/tensorflow/savedmodel_backend_factory.h"
#include "src/backends/tensorrt/plan_backend.pb.h"
#include "src/backends/tensorrt/plan_backend_factory.h"
#include "src/core/backend.h"
#include "src/core/constants.h"
#include "src/core/filesystem.h"
#include "src/core/logging.h"
#include "src/core/model_config_utils.h"
#include "src/core/server_status.h"

namespace nvidia { namespace inferenceserver {

namespace {

void
BuildPlatformConfigMap(
    const std::string& version, const std::string& model_store_path,
    const bool strict_model_config, const float tf_gpu_memory_fraction,
    const bool tf_allow_soft_placement, PlatformConfigMap* platform_configs)
{
  ::google::protobuf::Any platform_config;

  //// Tensorflow GraphDef
  {
    GraphDefPlatformConfig graphdef_config;

    graphdef_config.set_autofill(!strict_model_config);

    // Tensorflow session config
    if (tf_gpu_memory_fraction == 0.0) {
      graphdef_config.mutable_session_config()
          ->mutable_gpu_options()
          ->set_allow_growth(true);
    } else {
      graphdef_config.mutable_session_config()
          ->mutable_gpu_options()
          ->set_per_process_gpu_memory_fraction(tf_gpu_memory_fraction);
    }

    graphdef_config.mutable_session_config()->set_allow_soft_placement(
        tf_allow_soft_placement);
    platform_config.PackFrom(graphdef_config);
    (*platform_configs)[kTensorFlowGraphDefPlatform] = platform_config;
  }

  //// Tensorflow SavedModel
  {
    SavedModelPlatformConfig saved_model_config;

    saved_model_config.set_autofill(!strict_model_config);

    if (tf_gpu_memory_fraction == 0.0) {
      saved_model_config.mutable_session_config()
          ->mutable_gpu_options()
          ->set_allow_growth(true);
    } else {
      saved_model_config.mutable_session_config()
          ->mutable_gpu_options()
          ->set_per_process_gpu_memory_fraction(tf_gpu_memory_fraction);
    }

    saved_model_config.mutable_session_config()->set_allow_soft_placement(
        tf_allow_soft_placement);
    platform_config.PackFrom(saved_model_config);
    (*platform_configs)[kTensorFlowSavedModelPlatform] = platform_config;
  }

  //// Caffe NetDef
  {
    NetDefPlatformConfig netdef_config;
    netdef_config.set_autofill(!strict_model_config);
    platform_config.PackFrom(netdef_config);
    (*platform_configs)[kCaffe2NetDefPlatform] = platform_config;
  }

  //// TensorRT
  {
    PlanPlatformConfig plan_config;
    plan_config.set_autofill(!strict_model_config);
    platform_config.PackFrom(plan_config);
    (*platform_configs)[kTensorRTPlanPlatform] = platform_config;
  }

  //// Custom
  {
    CustomPlatformConfig custom_config;
    custom_config.set_inference_server_version(version);
    custom_config.set_model_repository_path(model_store_path);
    platform_config.PackFrom(custom_config);
    (*platform_configs)[kCustomPlatform] = platform_config;
  }

  //// Ensemble
  {
    EnsemblePlatformConfig ensemble_config;
    platform_config.PackFrom(ensemble_config);
    (*platform_configs)[kEnsemblePlatform] = platform_config;
  }

  //// Onnx Onnx
  {
    OnnxPlatformConfig onnx_config;
    onnx_config.set_autofill(!strict_model_config);
    platform_config.PackFrom(onnx_config);
    (*platform_configs)[kOnnxRuntimeOnnxPlatform] = platform_config;
  }

  //// PyTorch LibTorch
  {
    LibTorchPlatformConfig libtorch_config;
    libtorch_config.set_autofill(!strict_model_config);
    platform_config.PackFrom(libtorch_config);
    (*platform_configs)[kPyTorchLibTorchPlatform] = platform_config;
  }
}

int64_t
GetModifiedTime(const std::string& path)
{
  // If there is an error in any step the fall-back default
  // modification time is 0. This means that in error cases 'path'
  // will show as not modified. This is the safe fall-back to avoid
  // assuming a model is constantly being modified.
  bool path_is_dir;
  Status status = IsDirectory(path, &path_is_dir);
  if (!status.IsOk()) {
    LOG_ERROR << "Failed to determine modification time for '" << path
              << "': " << status.AsString();
    return 0;
  }

  // If 'path' is a file return its mtime. Otherwise, using the modification
  // time of the directory as baseline in case of file deletion
  int64_t mtime;
  status = FileModificationTime(path, &mtime);
  if (!status.IsOk()) {
    LOG_ERROR << "Failed to determine modification time for '" << path
              << "': " << status.AsString();
    return 0;
  }
  if (!path_is_dir) {
    return mtime;
  }

  // 'path' is a directory. Return the most recent mtime of the
  // contents of the directory.
  std::set<std::string> contents;
  status = GetDirectoryContents(path, &contents);
  if (!status.IsOk()) {
    LOG_ERROR << "Failed to determine modification time for '" << path
              << "': " << status.AsString();
    return 0;
  }

  for (const auto& child : contents) {
    const auto full_path = JoinPath({path, child});
    mtime = std::max(mtime, GetModifiedTime(full_path));
  }

  return mtime;
}
// Return true if any file in the subdirectory root at 'path' has been
// modified more recently than 'last'. Return the most-recent modified
// time in 'last'.
bool
IsModified(const std::string& path, int64_t* last_ns)
{
  const int64_t repo_ns = GetModifiedTime(path);
  bool modified = repo_ns > *last_ns;
  *last_ns = repo_ns;
  return modified;
}

// The implementation of backend handle, which provides callback hooks that
// will be triggered in different phase of the BackendHandle life cycle
class BackendHandleImpl : public ModelRepositoryManager::BackendHandle {
 public:
  BackendHandleImpl(
      std::unique_ptr<InferenceBackend> is,
      std::function<void(std::unique_ptr<InferenceBackend>)> OnDestroyBackend);

  ~BackendHandleImpl();

  InferenceBackend* GetInferenceBackend() override { return is_.get(); }

 private:
  std::unique_ptr<InferenceBackend> is_;

  // Use to inform the BackendLifeCycle that the backend handle is destroyed
  std::function<void(std::unique_ptr<InferenceBackend>)> OnDestroyBackend_;
};

BackendHandleImpl::BackendHandleImpl(
    std::unique_ptr<InferenceBackend> is,
    std::function<void(std::unique_ptr<InferenceBackend>)> OnDestroyBackend)
    : is_(std::move(is)), OnDestroyBackend_(std::move(OnDestroyBackend))
{
}

BackendHandleImpl::~BackendHandleImpl()
{
  // [TODO] fix InferenceBackend destructor
  // The expectation is that the InferenceBackend can be released directly when
  // ~BackendHandleImpl() is called. But in fact, it is not "ready" to be
  // released if it just finished a request (Resource Deadlock error in
  // Scheduler's destructor). Now we have to defer InferenceBackend destruction.

  // Call callback with separate thread because the callback will try to acquire
  // the mutex of the corresponding backend_info. And this destructor is likely
  // to to be called within BackendLifeCycle class where the mutex is being hold
  std::thread worker(std::move(OnDestroyBackend_), std::move(is_));
  worker.detach();
}

}  // namespace

struct ModelRepositoryManager::ModelInfo {
  // [TODO] split modification time into versions' and model's
  // so that we have more information on whether the model reload
  // is necessary
  int64_t mtime_nsec_;
  ModelConfig model_config_;
  Platform platform_;
};

class ModelRepositoryManager::BackendLifeCycle {
 public:
  static Status Create(
      const PlatformConfigMap& platform_map, const std::string& repository_path,
      std::unique_ptr<BackendLifeCycle>* life_cycle);

  ~BackendLifeCycle();

  // Start loading model backends with specified versions asynchronously.
  // If 'force_unload', all versions that are being served will
  // be unloaded before loading the specified versions.
  Status AsyncLoad(
      const std::string& model_name, const std::vector<int64_t>& versions,
      const ModelConfig& model_config, bool force_unload = true);

  // Get specified model version's backend handle. Latest ready version will
  // be retrieved if 'version' is -1. Return error if the version specified is
  // not found or it is not ready.
  Status GetBackendHandle(
      const std::string& model_name, const int64_t version,
      std::shared_ptr<BackendHandle>* handle);

  // Get the ModelStateMap representation of the live backends. A backend is
  // live if at least one of the versions is not unknown nor unavailable.
  const ModelStateMap GetLiveBackendStates();

  // Get the VersionStateMap representation of the specified model.
  const VersionStateMap GetVersionStates(const std::string& model_name);

 private:
  struct BackendInfo {
    BackendInfo(
        const ModelReadyState state, const ActionType next_action,
        const ModelConfig& model_config)
        : platform_(GetPlatform(model_config.platform())), state_(state),
          next_action_(next_action), model_config_(model_config)
    {
    }

    Platform platform_;

    std::mutex mtx_;
    ModelReadyState state_;
    // next_action will be set in the case where a load / unload is requested
    // while the backend is already in loading / unloading state. Then the new
    // load / unload will be postponed as next action.
    ActionType next_action_;
    ModelConfig model_config_;

    std::shared_ptr<BackendHandle> handle_;
  };

  BackendLifeCycle(const std::string& repository_path);

  // Caller must obtain the mutex of 'backend_info' before calling this function
  Status Load(
      const std::string& model_name, const int64_t version,
      BackendInfo* backend_info);

  // Caller must obtain the mutex of 'backend_info' before calling this function
  Status Unload(
      const std::string& model_name, const int64_t version,
      BackendInfo* backend_info);

  Status CreateBackendHandle(
      const std::string& model_name, const int64_t version,
      BackendInfo* backend_info);

  // Function called after model load / unload.
  // Caller must obtain the mutex of 'backend_info' before calling this function
  Status TriggerNextAction(
      const std::string& model_name, const int64_t version,
      BackendInfo* backend_info);

  using VersionMap = std::map<int64_t, std::unique_ptr<BackendInfo>>;
  using BackendMap = std::map<std::string, VersionMap>;
  BackendMap map_;
  std::mutex map_mtx_;

  // Variables as workaround to issue mentioned in ~BackendHandleImpl()
  bool exiting_;
  std::thread release_thread_;
  std::mutex release_queue_mtx_;
  std::deque<std::unique_ptr<InferenceBackend>> release_queue_;

  const std::string& repository_path_;
  std::unique_ptr<NetDefBackendFactory> netdef_factory_;
  std::unique_ptr<CustomBackendFactory> custom_factory_;
  std::unique_ptr<EnsembleBackendFactory> ensemble_factory_;
  std::unique_ptr<GraphDefBackendFactory> graphdef_factory_;
  std::unique_ptr<SavedModelBackendFactory> savedmodel_factory_;
  std::unique_ptr<PlanBackendFactory> plan_factory_;
  std::unique_ptr<OnnxBackendFactory> onnx_factory_;
  std::unique_ptr<LibTorchBackendFactory> libtorch_factory_;
};

ModelRepositoryManager::BackendLifeCycle::BackendLifeCycle(
    const std::string& repository_path)
    : exiting_(false), repository_path_(repository_path)
{
  release_thread_ = std::thread([this]() {
    {
      std::vector<std::unique_ptr<InferenceBackend>> releasing_backend;
      {
        std::lock_guard<std::mutex> lock(this->release_queue_mtx_);
        if (this->exiting_ && this->release_queue_.empty()) {
          return;
        }
        while (!this->release_queue_.empty()) {
          releasing_backend.emplace_back(
              std::move(this->release_queue_.front()));
          this->release_queue_.pop_front();
        }
      }
      // Give some time so that the backend should be "ready" to be released
      std::this_thread::sleep_for(std::chrono::milliseconds(5000));
      releasing_backend.clear();
    }
  });
}

ModelRepositoryManager::BackendLifeCycle::~BackendLifeCycle()
{
  {
    std::lock_guard<std::mutex> lock(release_queue_mtx_);
    exiting_ = true;
  }
  if (release_thread_.joinable()) {
    release_thread_.join();
  }
}

Status
ModelRepositoryManager::BackendLifeCycle::Create(
    const PlatformConfigMap& platform_map, const std::string& repository_path,
    std::unique_ptr<BackendLifeCycle>* life_cycle)
{
  std::unique_ptr<BackendLifeCycle> local_life_cycle(
      new BackendLifeCycle(repository_path));

  {
    GraphDefPlatformConfig config;
    platform_map.find(kTensorFlowGraphDefPlatform)->second.UnpackTo(&config);
    RETURN_IF_ERROR(GraphDefBackendFactory::Create(
        config, &(local_life_cycle->graphdef_factory_)));
  }
  {
    SavedModelPlatformConfig config;
    platform_map.find(kTensorFlowSavedModelPlatform)->second.UnpackTo(&config);
    RETURN_IF_ERROR(SavedModelBackendFactory::Create(
        config, &(local_life_cycle->savedmodel_factory_)));
  }
  {
    NetDefPlatformConfig config;
    platform_map.find(kCaffe2NetDefPlatform)->second.UnpackTo(&config);
    RETURN_IF_ERROR(NetDefBackendFactory::Create(
        config, &(local_life_cycle->netdef_factory_)));
  }
  {
    PlanPlatformConfig config;
    platform_map.find(kTensorRTPlanPlatform)->second.UnpackTo(&config);
    RETURN_IF_ERROR(
        PlanBackendFactory::Create(config, &(local_life_cycle->plan_factory_)));
  }
  {
    CustomPlatformConfig config;
    platform_map.find(kCustomPlatform)->second.UnpackTo(&config);
    RETURN_IF_ERROR(CustomBackendFactory::Create(
        config, &(local_life_cycle->custom_factory_)));
  }
  {
    EnsemblePlatformConfig config;
    platform_map.find(kEnsemblePlatform)->second.UnpackTo(&config);
    RETURN_IF_ERROR(EnsembleBackendFactory::Create(
        config, &(local_life_cycle->ensemble_factory_)));
  }
  {
    OnnxPlatformConfig config;
    platform_map.find(kOnnxRuntimeOnnxPlatform)->second.UnpackTo(&config);
    RETURN_IF_ERROR(
        OnnxBackendFactory::Create(config, &(local_life_cycle->onnx_factory_)));
  }
  {
    LibTorchPlatformConfig config;
    platform_map.find(kPyTorchLibTorchPlatform)->second.UnpackTo(&config);
    RETURN_IF_ERROR(LibTorchBackendFactory::Create(
        config, &(local_life_cycle->libtorch_factory_)));
  }

  *life_cycle = std::move(local_life_cycle);
  return Status::Success;
}

const ModelRepositoryManager::ModelStateMap
ModelRepositoryManager::BackendLifeCycle::GetLiveBackendStates()
{
  LOG_VERBOSE(1) << "GetLiveBackendStates()";
  std::lock_guard<std::mutex> map_lock(map_mtx_);
  ModelStateMap live_backend_states;
  for (auto& model_version : map_) {
    bool live = false;
    VersionStateMap version_map;

    for (auto& version_backend : model_version.second) {
      std::lock_guard<std::mutex> lock(version_backend.second->mtx_);
      // At lease one version is live (ready / loading / unloading)
      if ((version_backend.second->state_ != ModelReadyState::MODEL_UNKNOWN) &&
          (version_backend.second->state_ !=
           ModelReadyState::MODEL_UNAVAILABLE)) {
        live = true;
        version_map[version_backend.first] = version_backend.second->state_;
      }
    }

    if (live) {
      live_backend_states[model_version.first] = version_map;
    }
  }
  return live_backend_states;
}

const ModelRepositoryManager::VersionStateMap
ModelRepositoryManager::BackendLifeCycle::GetVersionStates(
    const std::string& model_name)
{
  LOG_VERBOSE(1) << "GetVersionStates() '" << model_name << "'";
  std::lock_guard<std::mutex> map_lock(map_mtx_);
  VersionStateMap version_map;
  auto mit = map_.find(model_name);
  if (mit != map_.end()) {
    for (auto& version_backend : mit->second) {
      std::lock_guard<std::mutex> lock(version_backend.second->mtx_);
      version_map[version_backend.first] = version_backend.second->state_;
    }
  }

  return version_map;
}

Status
ModelRepositoryManager::BackendLifeCycle::GetBackendHandle(
    const std::string& model_name, const int64_t version,
    std::shared_ptr<BackendHandle>* handle)
{
  LOG_VERBOSE(1) << "GetBackendHandle() '" << model_name << "' version "
                 << version;
  std::lock_guard<std::mutex> map_lock(map_mtx_);
  auto mit = map_.find(model_name);
  if (mit == map_.end()) {
    return Status(
        RequestStatusCode::NOT_FOUND,
        "model '" + model_name + "' is not found");
  }

  auto vit = mit->second.find(version);
  if (vit == mit->second.end()) {
    // In case the request is asking for latest version
    int64_t latest = -1;
    if (version == -1) {
      for (auto& version_backend : mit->second) {
        if (version_backend.first > latest) {
          std::lock_guard<std::mutex> lock(version_backend.second->mtx_);
          if (version_backend.second->state_ == ModelReadyState::MODEL_READY) {
            latest = version_backend.first;
            // Tedious, but have to set handle for any "latest" version
            // at the moment to avoid edge case like the following:
            // "versions : 1 3 2", version 3 is latest but is requested
            // to be unloaded when the iterator is examining version 2.
            *handle = version_backend.second->handle_;
          }
        }
      }
    }
    if (latest == -1) {
      return Status(
          RequestStatusCode::NOT_FOUND, "model '" + model_name + "' version " +
                                            std::to_string(version) +
                                            " is not found");
    }
  } else {
    std::lock_guard<std::mutex> lock(vit->second->mtx_);
    if (vit->second->state_ == ModelReadyState::MODEL_READY) {
      *handle = vit->second->handle_;
    } else {
      return Status(
          RequestStatusCode::UNAVAILABLE,
          "model '" + model_name + "' version " + std::to_string(version) +
              " is not at ready state");
    }
  }
  return Status::Success;
}

Status
ModelRepositoryManager::BackendLifeCycle::AsyncLoad(
    const std::string& model_name, const std::vector<int64_t>& versions,
    const ModelConfig& model_config, bool force_unload)
{
  LOG_VERBOSE(1) << "AsyncLoad() '" << model_name << "'";
  std::lock_guard<std::mutex> map_lock(map_mtx_);
  auto it = map_.find(model_name);
  if (it == map_.end()) {
    it = map_.emplace(std::make_pair(model_name, VersionMap())).first;
  }

  if (force_unload) {
    for (auto& version_backend : it->second) {
      std::lock_guard<std::mutex> lock(version_backend.second->mtx_);
      Unload(model_name, version_backend.first, version_backend.second.get());
    }
  }

  for (const auto& version : versions) {
    auto vit = it->second.find(version);
    if (vit == it->second.end()) {
      vit =
          it->second
              .emplace(std::make_pair(version, std::unique_ptr<BackendInfo>()))
              .first;
      vit->second.reset(new BackendInfo(
          ModelReadyState::MODEL_UNKNOWN, ActionType::NO_ACTION, model_config));
    }

    // Update model config and reload model if it is being served
    std::lock_guard<std::mutex> lock(vit->second->mtx_);
    vit->second->model_config_ = model_config;
    Unload(model_name, version, vit->second.get());
    RETURN_IF_ERROR(Load(model_name, version, vit->second.get()));
  }

  return Status::Success;
}

Status
ModelRepositoryManager::BackendLifeCycle::Load(
    const std::string& model_name, const int64_t version,
    BackendInfo* backend_info)
{
  LOG_VERBOSE(1) << "Load() '" << model_name << "' version " << version;
  Status status = Status::Success;

  backend_info->next_action_ = ActionType::NO_ACTION;

  switch (backend_info->state_) {
    case ModelReadyState::MODEL_READY:
      status = Status(
          RequestStatusCode::ALREADY_EXISTS,
          "tried to load model '" + model_name + "' version " +
              std::to_string(version) + " which is being served");
      break;
    case ModelReadyState::MODEL_LOADING:
    case ModelReadyState::MODEL_UNLOADING:
      backend_info->next_action_ = ActionType::LOAD;
      break;
    default:
      LOG_INFO << "loading: " << model_name << ":" << version;
      backend_info->state_ = ModelReadyState::MODEL_LOADING;
      {
        std::thread worker(
            &ModelRepositoryManager::BackendLifeCycle::CreateBackendHandle,
            this, model_name, version, backend_info);
        worker.detach();
      }
      break;
  }

  return status;
}

Status
ModelRepositoryManager::BackendLifeCycle::Unload(
    const std::string& model_name, const int64_t version,
    BackendInfo* backend_info)
{
  LOG_VERBOSE(1) << "Unload() '" << model_name << "' version " << version;
  Status status = Status::Success;

  backend_info->next_action_ = ActionType::NO_ACTION;

  switch (backend_info->state_) {
    case ModelReadyState::MODEL_READY:
      LOG_INFO << "unloading: " << model_name << ":" << version;
      backend_info->state_ = ModelReadyState::MODEL_UNLOADING;
      backend_info->handle_.reset();
      break;
    case ModelReadyState::MODEL_LOADING:
    case ModelReadyState::MODEL_UNLOADING:
      backend_info->next_action_ = ActionType::UNLOAD;
      break;
    default:
      status = Status(
          RequestStatusCode::NOT_FOUND,
          "tried to unload model '" + model_name + "' version " +
              std::to_string(version) + " which is at model state: " +
              std::to_string(backend_info->state_));
      break;
  }

  return status;
}

Status
ModelRepositoryManager::BackendLifeCycle::CreateBackendHandle(
    const std::string& model_name, const int64_t version,
    BackendInfo* backend_info)
{
  LOG_VERBOSE(1) << "CreateBackendHandle() '" << model_name << "' version "
                 << version;
  const auto version_path =
      JoinPath({repository_path_, model_name, std::to_string(version)});
  // make copy of the current model config in case model config in backend info
  // is updated (another poll) during the creation of backend handle
  ModelConfig model_config;
  {
    std::lock_guard<std::mutex> lock(backend_info->mtx_);
    model_config = backend_info->model_config_;
  }

  // Create backend
  Status status;
  std::unique_ptr<InferenceBackend> is;
  switch (backend_info->platform_) {
    case Platform::PLATFORM_TENSORFLOW_GRAPHDEF:
      status =
          graphdef_factory_->CreateBackend(version_path, model_config, &is);
      break;
    case Platform::PLATFORM_TENSORFLOW_SAVEDMODEL:
      status =
          savedmodel_factory_->CreateBackend(version_path, model_config, &is);
      break;
    case Platform::PLATFORM_TENSORRT_PLAN:
      status = plan_factory_->CreateBackend(version_path, model_config, &is);
      break;
    case Platform::PLATFORM_CAFFE2_NETDEF:
      status = netdef_factory_->CreateBackend(version_path, model_config, &is);
      break;
    case Platform::PLATFORM_CUSTOM:
      status = custom_factory_->CreateBackend(version_path, model_config, &is);
      break;
    case Platform::PLATFORM_ENSEMBLE:
      status =
          ensemble_factory_->CreateBackend(version_path, model_config, &is);
      break;
    case Platform::PLATFORM_ONNXRUNTIME_ONNX:
      status = onnx_factory_->CreateBackend(version_path, model_config, &is);
      break;
    case Platform::PLATFORM_PYTORCH_LIBTORCH:
      status =
          libtorch_factory_->CreateBackend(version_path, model_config, &is);
      break;
    default:
      break;
  }

  // Update backend state
  std::lock_guard<std::mutex> lock(backend_info->mtx_);
  // Sanity check
  if (backend_info->handle_ != nullptr) {
    LOG_ERROR << "trying to load model '" << model_name << "' version "
              << version << " while it is being served";
  } else {
    if (status.IsOk()) {
      backend_info->state_ = ModelReadyState::MODEL_READY;
      // Unless the handle is nullptr, always reset handle out of the mutex,
      // otherwise the handle's destructor will try to acquire the mutex and
      // cause deadlock.
      backend_info->handle_.reset(new BackendHandleImpl(
          std::move(is), [this, model_name, version, backend_info](
                             std::unique_ptr<InferenceBackend> is) mutable {
            LOG_VERBOSE(1) << "OnDestroy callback() '" << model_name
                           << "' version " << version;
            LOG_INFO << "successfully unloaded '" << model_name << "' version "
                     << version;
            {
              std::lock_guard<std::mutex> lock(backend_info->mtx_);
              backend_info->state_ = ModelReadyState::MODEL_UNAVAILABLE;
              // Check if next action is requested
              this->TriggerNextAction(model_name, version, backend_info);
            }

            // workaround for issue mentioned in ~BackendHandle()
            {
              std::lock_guard<std::mutex> lock(this->release_queue_mtx_);
              this->release_queue_.emplace_back(std::move(is));
            }
          }));
      LOG_INFO << "successfully loaded '" << model_name << "' version "
               << version;
    } else {
      LOG_ERROR << "failed to load '" << model_name << "' version " << version
                << ": " << status.AsString();
      backend_info->state_ = ModelReadyState::MODEL_UNAVAILABLE;
    }
  }

  // Check if next action is requested
  return TriggerNextAction(model_name, version, backend_info);
}

Status
ModelRepositoryManager::BackendLifeCycle::TriggerNextAction(
    const std::string& model_name, const int64_t version,
    BackendInfo* backend_info)
{
  LOG_VERBOSE(1) << "TriggerNextAction() '" << model_name << "' version "
                 << version << ": "
                 << std::to_string(backend_info->next_action_);
  ActionType next_action = backend_info->next_action_;
  backend_info->next_action_ = ActionType::NO_ACTION;
  switch (next_action) {
    case ActionType::LOAD:
      Unload(model_name, version, backend_info);
      RETURN_IF_ERROR(Load(model_name, version, backend_info));
      break;
    case ActionType::UNLOAD:
      RETURN_IF_ERROR(Unload(model_name, version, backend_info));
      break;
    default:
      break;
  }

  return Status::Success;
}

ModelRepositoryManager::ModelRepositoryManager(
    const std::shared_ptr<ServerStatusManager>& status_manager,
    const std::string& repository_path,
    const PlatformConfigMap& platform_config_map, const bool autofill,
    const bool polling_enabled, std::unique_ptr<BackendLifeCycle> life_cycle)
    : repository_path_(repository_path),
      platform_config_map_(platform_config_map), autofill_(autofill),
      polling_enabled_(polling_enabled), status_manager_(status_manager),
      backend_life_cycle_(std::move(life_cycle))
{
}

ModelRepositoryManager::~ModelRepositoryManager() {}

Status
ModelRepositoryManager::Create(
    const std::string& server_version,
    const std::shared_ptr<ServerStatusManager>& status_manager,
    const std::string& repository_path, const bool strict_model_config,
    const float tf_gpu_memory_fraction, const bool tf_allow_soft_placement,
    const uint32_t repository_poll_secs, const bool polling_enabled,
    std::unique_ptr<ModelRepositoryManager>* model_repository_manager)
{
  // The rest only matters if repository path is valid directory
  bool path_is_dir;
  RETURN_IF_ERROR(IsDirectory(repository_path, &path_is_dir));
  if (!path_is_dir) {
    return Status(
        RequestStatusCode::INVALID_ARG,
        "repository path is not a valid directory");
  }

  PlatformConfigMap platform_config_map;

  BuildPlatformConfigMap(
      server_version, repository_path, strict_model_config,
      tf_gpu_memory_fraction, tf_allow_soft_placement, &platform_config_map);

  std::unique_ptr<BackendLifeCycle> life_cycle;
  RETURN_IF_ERROR(BackendLifeCycle::Create(
      platform_config_map, repository_path, &life_cycle));

  // Not setting the smart pointer directly to simplify clean up
  std::unique_ptr<ModelRepositoryManager> local_manager(
      new ModelRepositoryManager(
          status_manager, repository_path, platform_config_map,
          !strict_model_config, polling_enabled, std::move(life_cycle)));

  // Similar to PollAndUpdate(), but simplier
  std::set<std::string> added, deleted, modified, unmodified;
  if (polling_enabled) {
    RETURN_IF_ERROR(
        local_manager->Poll(&added, &deleted, &modified, &unmodified));
  }
  if (!deleted.empty() || !modified.empty() || !unmodified.empty()) {
    return Status(
        RequestStatusCode::INTERNAL,
        "Unexpected initial state for model repository");
  }

  for (const auto& name : added) {
    ModelConfig model_config;
    RETURN_IF_ERROR(local_manager->GetModelConfig(name, &model_config));
    RETURN_IF_ERROR(
        local_manager->status_manager_->InitForModel(name, model_config));

    std::vector<int64_t> versions;
    RETURN_IF_ERROR(
        local_manager->VersionsToLoad(name, model_config, versions));
    RETURN_IF_ERROR(local_manager->backend_life_cycle_->AsyncLoad(
        name, versions, model_config));
  }

  *model_repository_manager = std::move(local_manager);

  return Status::Success;
}

Status
ModelRepositoryManager::PollAndUpdate()
{
  if (!polling_enabled_) {
    return Status(RequestStatusCode::INVALID, "polling is disabled");
  }
  std::set<std::string> added, deleted, modified, unmodified;
  RETURN_IF_ERROR(Poll(&added, &deleted, &modified, &unmodified));
  // Nothing to do if no model adds, deletes or modifies.
  if (added.empty() && deleted.empty() && modified.empty()) {
    return Status::Success;
  }

  // Added models should be loaded and be initialized for status
  // reporting.
  for (const auto& name : added) {
    ModelConfig model_config;
    RETURN_IF_ERROR(GetModelConfig(name, &model_config));
    RETURN_IF_ERROR(status_manager_->InitForModel(name, model_config));

    std::vector<int64_t> versions;
    RETURN_IF_ERROR(VersionsToLoad(name, model_config, versions));
    backend_life_cycle_->AsyncLoad(name, versions, model_config);
  }

  // If there are any modified model, (re)load them to pick up
  // the changes. We want to keep the current status information
  // so don't re-init it.
  for (const auto& name : modified) {
    ModelConfig model_config;
    RETURN_IF_ERROR(GetModelConfig(name, &model_config));
    RETURN_IF_ERROR(status_manager_->UpdateConfigForModel(name, model_config));

    std::vector<int64_t> versions;
    RETURN_IF_ERROR(VersionsToLoad(name, model_config, versions));
    backend_life_cycle_->AsyncLoad(name, versions, model_config);
  }

  for (const auto& name : deleted) {
    ModelConfig model_config;
    std::vector<int64_t> versions;
    // Utilize "force_unload" of AsyncLoad()
    backend_life_cycle_->AsyncLoad(name, versions, model_config);
  }

  return Status::Success;
}

Status
ModelRepositoryManager::LoadUnloadModel(
    const std::string& model_name, ActionType type,
    std::function<void(Status)> OnCompleteUpdate)
{
  if (polling_enabled_) {
    return Status(
        RequestStatusCode::INVALID,
        "explicit model load / unload is not allowed if polling is enabled");
  }
  Status status = Status(RequestStatusCode::UNSUPPORTED, "not implemented");
  OnCompleteUpdate(status);
  return status;
}

Status
ModelRepositoryManager::UnloadAllModels()
{
  Status status;
  // Reload an empty version list to cause the model to unload.
  ModelConfig model_config;
  std::vector<int64_t> versions;
  for (const auto& name_info : infos_) {
    Status unload_status =
        backend_life_cycle_->AsyncLoad(name_info.first, versions, model_config);
    if (!unload_status.IsOk()) {
      status = Status(
          RequestStatusCode::INTERNAL,
          "Failed to gracefully unload models: " + unload_status.Message());
    }
  }
  return Status::Success;
}

const ModelRepositoryManager::ModelStateMap
ModelRepositoryManager::GetLiveBackendStates()
{
  return backend_life_cycle_->GetLiveBackendStates();
}

const ModelRepositoryManager::VersionStateMap
ModelRepositoryManager::GetVersionStates(const std::string& model_name)
{
  return backend_life_cycle_->GetVersionStates(model_name);
}

Status
ModelRepositoryManager::GetBackendHandle(
    const std::string& model_name, const int64_t model_version,
    std::shared_ptr<BackendHandle>* handle)
{
  Status status =
      backend_life_cycle_->GetBackendHandle(model_name, model_version, handle);
  if (!status.IsOk()) {
    handle->reset();
    status = Status(
        RequestStatusCode::UNAVAILABLE,
        "Inference request for unknown model '" + model_name + "'");
  }
  return status;
}

Status
ModelRepositoryManager::Poll(
    std::set<std::string>* added, std::set<std::string>* deleted,
    std::set<std::string>* modified, std::set<std::string>* unmodified)
{
  // Serialize all polling operation...
  std::lock_guard<std::mutex> lock(poll_mu_);

  added->clear();
  deleted->clear();
  modified->clear();
  unmodified->clear();

  // We don't modify 'infos_' in place to minimize how long we need to
  // hold the lock and also prevent any partial changes to do an error
  // during processing.
  ModelInfoMap new_infos;

  // Each subdirectory of repository path is a model directory from
  // which we read the model configuration.
  std::set<std::string> subdirs;
  RETURN_IF_ERROR(GetDirectorySubdirs(repository_path_, &subdirs));

  for (const auto& child : subdirs) {
    const auto full_path = JoinPath({repository_path_, child});

    // If 'child' is a new model or an existing model that has been
    // modified since the last time it was polled, then need to
    // (re)load, normalize and validate the configuration.
    bool need_load = false;
    int64_t mtime_ns;
    const auto iitr = infos_.find(child);
    if (iitr == infos_.end()) {
      added->insert(child);
      mtime_ns = GetModifiedTime(std::string(full_path));
      need_load = true;
    } else {
      mtime_ns = iitr->second->mtime_nsec_;
      if (IsModified(std::string(full_path), &mtime_ns)) {
        modified->insert(child);
        need_load = true;
      } else {
        unmodified->insert(child);
        const auto& ret = new_infos.emplace(child, nullptr);
        if (!ret.second) {
          return Status(
              RequestStatusCode::ALREADY_EXISTS,
              "unexpected model info for model '" + child + "'");
        }

        std::unique_ptr<ModelInfo>& model_info = ret.first->second;
        model_info.reset(new ModelInfo(*iitr->second));
      }
    }

    if (need_load) {
      const auto& ret = new_infos.emplace(child, nullptr);
      if (!ret.second) {
        return Status(
            RequestStatusCode::ALREADY_EXISTS,
            "unexpected model info for model '" + child + "'");
      }

      std::unique_ptr<ModelInfo>& model_info = ret.first->second;
      model_info.reset(new ModelInfo());
      ModelConfig& model_config = model_info->model_config_;
      model_info->mtime_nsec_ = mtime_ns;

      // If enabled, try to automatically generate missing parts of
      // the model configuration (autofill) from the model
      // definition. In all cases normalize and validate the config.
      RETURN_IF_ERROR(GetNormalizedModelConfig(
          full_path, platform_config_map_, autofill_, &model_config));
      RETURN_IF_ERROR(ValidateModelConfig(model_config, std::string()));

      model_info->platform_ = GetPlatform(model_config.platform());

      // Make sure the name of the model matches the name of the
      // directory. This is a somewhat arbitrary requirement but seems
      // like good practice to require it of the user. It also acts as a
      // check to make sure we don't have two different models with the
      // same name.
      if (model_config.name() != child) {
        return Status(
            RequestStatusCode::INVALID_ARG,
            "unexpected directory name '" + child + "' for model '" +
                model_config.name() +
                "', directory name must equal model name");
      }
    }
  }

  // Anything in 'infos_' that is not in "added", "modified", or
  // "unmodified" is deleted.
  for (const auto& pr : infos_) {
    if ((added->find(pr.first) == added->end()) &&
        (modified->find(pr.first) == modified->end()) &&
        (unmodified->find(pr.first) == unmodified->end())) {
      deleted->insert(pr.first);
    }
  }

  // Swap the new infos in place under a short-lived lock and only if
  // there were no errors encountered during polling.
  {
    std::lock_guard<std::mutex> lock(infos_mu_);
    infos_.swap(new_infos);
  }

  return Status::Success;
}


Status
ModelRepositoryManager::GetModelConfig(
    const std::string& name, ModelConfig* model_config)
{
  std::lock_guard<std::mutex> lock(infos_mu_);

  const auto itr = infos_.find(name);
  if (itr == infos_.end()) {
    return Status(
        RequestStatusCode::NOT_FOUND,
        "no configuration for model '" + name + "'");
  }

  *model_config = itr->second->model_config_;
  return Status::Success;
}

Status
ModelRepositoryManager::VersionsToLoad(
    const std::string& name, const ModelConfig& model_config,
    std::vector<int64_t>& versions)
{
  versions.clear();

  // Get integral number of the version directory
  const auto model_path = JoinPath({repository_path_, name});
  std::set<std::string> subdirs;
  RETURN_IF_ERROR(GetDirectorySubdirs(model_path, &subdirs));
  std::set<int64_t, std::greater<int64_t>> existing_versions;
  for (const auto& subdir : subdirs) {
    try {
      int64_t version = std::stoll(subdir);
      existing_versions.insert(version);
    }
    catch (const std::invalid_argument& ia) {
      LOG_ERROR << "failed to convert version directory '" << subdir
                << "' to integral number";
    }
  }

  if (model_config.version_policy().has_specific()) {
    for (const auto& v : model_config.version_policy().specific().versions()) {
      // Only load the specific versions that are presented in model directory
      bool version_not_exist = existing_versions.insert(v).second;
      if (!version_not_exist) {
        versions.push_back(v);
      } else {
        LOG_ERROR << "version " << v << " is specified for model '" << name
                  << "', but the version directory is not present";
      }
    }
  } else {
    if (model_config.version_policy().has_latest()) {
      // std::set is sorted with std::greater
      for (const auto& v : existing_versions) {
        if (versions.size() >=
            model_config.version_policy().latest().num_versions()) {
          break;
        }
        versions.push_back(v);
      }
    } else {
      // all
      versions.assign(existing_versions.begin(), existing_versions.end());
    }
  }

  return Status::Success;
}

}}  // namespace nvidia::inferenceserver
