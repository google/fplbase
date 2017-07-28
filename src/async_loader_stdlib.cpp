// Copyright 2015 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fplbase/async_loader.h"
#include "fplbase/utilities.h"
#include "precompiled.h"

namespace fplbase {

AsyncLoader::AsyncLoader() : loading_(nullptr), num_pending_requests_(0) {
}

AsyncLoader::~AsyncLoader() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
  }
  Stop();
}

void AsyncLoader::Stop() {
  if (worker_thread_.joinable()) {
    StopLoadingWhenComplete();
    worker_thread_.join();
  }
}

void AsyncLoader::QueueJob(AsyncAsset *res) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(res);
    ++num_pending_requests_;
  }
  job_cv_.notify_one();
}

void AsyncLoader::AbortJob(AsyncAsset *res) {
  bool was_loading = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    was_loading = loading_ == res;
  }

  if (was_loading) {
    PauseLoading();
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto iter = std::find(queue_.begin(), queue_.end(), res);
    if (iter != queue_.end()) {
      queue_.erase(iter);
      --num_pending_requests_;
    }

    iter = std::find(done_.begin(), done_.end(), res);
    if (iter != done_.end()) {
      done_.erase(iter);
      --num_pending_requests_;
    }
  }

  if (was_loading) {
    StartLoading();
  }
}

void AsyncLoader::StartLoading() {
  if (!worker_thread_.joinable()) {
    worker_thread_ = std::thread(AsyncLoader::LoaderThread, this);
  }
}

void AsyncLoader::PauseLoading() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_front(nullptr);
  }
  job_cv_.notify_one();
  worker_thread_.join();
}

void AsyncLoader::StopLoadingWhenComplete() { QueueJob(nullptr); }

bool AsyncLoader::TryFinalize() {
  for (;;) {
    AsyncAsset *resource = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (done_.size() > 0) resource = done_[0];
    }

    if (!resource) break;
    bool ok = resource->Finalize();
    if (!ok) {
      // Can't do much here, since res is already constructed. Caller has to
      // check IsValid() to know if resource can be used.
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      // It's possible that the resource was destroyed during its finalize
      // callbacks, so ensure that it's still the first item in done_.
      if (done_.size() > 0 && done_.front() == resource) {
        done_.pop_front();
      }
      --num_pending_requests_;
    }
  }
  bool finished;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    finished = num_pending_requests_ == 0;
  }
  return finished;
}

void AsyncLoader::LoaderWorker() {
  for (;;) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      job_cv_.wait(lock, [this]() { return queue_.size() > 0; });
      loading_ = queue_.front();
      queue_.pop_front();
    }

    if (!loading_) {
      break;
    }

    loading_->Load();
    std::lock_guard<std::mutex> lock(mutex_);
    done_.push_back(loading_);
    loading_ = nullptr;
  }
}

// static
int AsyncLoader::LoaderThread(void *user_data) {
  reinterpret_cast<AsyncLoader *>(user_data)->LoaderWorker();
  return 0;
}
}  // namespace fplbase
