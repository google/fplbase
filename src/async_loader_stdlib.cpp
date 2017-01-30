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

#ifndef FPL_BASE_BACKEND_STDLIB
#error This version of AsyncLoader is designed for use with the C++ library.
#endif

namespace fplbase {

AsyncLoader::AsyncLoader() {}

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
  }
  job_cv_.notify_one();
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
      done_.erase(done_.begin());
    }
  }
  bool finished;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    finished = queue_.empty() && done_.empty();
  }
  return finished;
}

void AsyncLoader::LoaderWorker() {
  for (;;) {
    AsyncAsset *resource = nullptr;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      job_cv_.wait(lock, [this]() { return queue_.size() > 0; });
      resource = queue_.front();
      queue_.pop_front();
    }

    if (!resource) {
      break;
    }

    resource->Load();
    std::lock_guard<std::mutex> lock(mutex_);
    done_.push_back(resource);
  }
}

// static
int AsyncLoader::LoaderThread(void *user_data) {
  reinterpret_cast<AsyncLoader *>(user_data)->LoaderWorker();
  return 0;
}
}  // namespace fplbase
