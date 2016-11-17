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

#include "precompiled.h"
#include "fplbase/async_loader.h"

#ifndef FPL_BASE_BACKEND_STDLIB
#error This version of AsyncLoader is designed for use with the C++ library.
#endif

namespace fplbase {

AsyncLoader::AsyncLoader() {}

AsyncLoader::~AsyncLoader() {
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
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.push_back(res);
  }
  job_cv_.notify_one();
}

void AsyncLoader::StartLoading() {
  worker_thread_ = std::thread(AsyncLoader::LoaderThread, this);
}

void AsyncLoader::StopLoadingWhenComplete() { QueueJob(nullptr); }

bool AsyncLoader::TryFinalize() {
  for (;;) {
    AsyncAsset *resource = nullptr;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (done_.size() > 0) resource = done_[0];
    }

    if (!resource) break;
    resource->Finalize();

    {
      std::unique_lock<std::mutex> lock(mutex_);
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
      resource = queue_[0];
    }

    if (resource) resource->Load();

    {
      std::unique_lock<std::mutex> lock(mutex_);
      queue_.erase(queue_.begin());
      if (resource) done_.push_back(resource);
    }

    // We assume a nullptr inserted in to the queue is used as a termination
    // signal.
    if (!resource) break;
  }
}

// static
int AsyncLoader::LoaderThread(void *user_data) {
  reinterpret_cast<AsyncLoader *>(user_data)->LoaderWorker();
  return 0;
}
}  // namespace fplbase
