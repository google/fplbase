// Copyright 2014 Google Inc. All rights reserved.
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
#include "fplbase/utilities.h"

namespace fplbase {

// Push this to signal the worker thread that it's time to quit.
class BookendAsyncResource : public AsyncAsset {
  static const char *kBookendFileName;

 public:
  BookendAsyncResource() : AsyncAsset(kBookendFileName) {}
  virtual ~BookendAsyncResource() {}
  virtual void Load() {}
  virtual bool Finalize() { return true; }
  virtual bool IsValid() { return true; }
  static bool IsBookend(const AsyncAsset &res) {
    return res.filename() == kBookendFileName;
  }
};

// static
const char *BookendAsyncResource::kBookendFileName = "bookend";

AsyncLoader::AsyncLoader()
    : loading_(nullptr), num_pending_requests_(0), worker_thread_(nullptr) {
  mutex_ = SDL_CreateMutex();
  job_semaphore_ = SDL_CreateSemaphore(0);
  assert(mutex_ && job_semaphore_);
}

AsyncLoader::~AsyncLoader() {
  Stop();
}

void AsyncLoader::Stop() {
  if (worker_thread_) {
    StopLoadingWhenComplete();
    SDL_WaitThread(static_cast<SDL_Thread *>(worker_thread_), nullptr);
    worker_thread_ = nullptr;

    if (mutex_) {
      SDL_DestroyMutex(static_cast<SDL_mutex *>(mutex_));
      mutex_ = nullptr;
    }
    if (job_semaphore_) {
      SDL_DestroySemaphore(static_cast<SDL_semaphore *>(job_semaphore_));
      job_semaphore_ = nullptr;
    }
  }
}

void AsyncLoader::QueueJob(AsyncAsset *res) {
  Lock([this, res]() {
    queue_.push_back(res);
    ++num_pending_requests_;
  });
  SDL_SemPost(static_cast<SDL_semaphore *>(job_semaphore_));
}

void AsyncLoader::AbortJob(AsyncAsset *res) {
  const bool was_loading =
      LockReturn<bool>([this, res]() { return loading_ == res; });

  if (was_loading) {
    PauseLoading();
  }

  Lock([this, res]() {
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
  });

  if (was_loading) {
    StartLoading();
  }
}

void AsyncLoader::LoaderWorker() {
  for (;;) {
    Lock([this]() { loading_ = queue_.empty() ? nullptr : queue_.front(); });
    if (!loading_) {
      SDL_SemWait(static_cast<SDL_semaphore *>(job_semaphore_));
      continue;
    }
    // Stop loading once we reach the bookend enqueued by
    // StopLoadingWhenComplete(). To start loading again, call StartLoading().
    if (BookendAsyncResource::IsBookend(*loading_)) break;
    LogInfo(kApplication, "async load: %s", loading_->filename_.c_str());
    loading_->Load();
    Lock([this]() {
      queue_.pop_front();
      done_.push_back(loading_);
      loading_ = nullptr;
    });
  }
}

int AsyncLoader::LoaderThread(void *user_data) {
  reinterpret_cast<AsyncLoader *>(user_data)->LoaderWorker();
  return 0;
}

void AsyncLoader::StartLoading() {
  worker_thread_ =
      SDL_CreateThread(AsyncLoader::LoaderThread, "FPL Loader Thread", this);
  assert(worker_thread_);
}

void AsyncLoader::PauseLoading() { assert(false); }

void AsyncLoader::StopLoadingWhenComplete() {
  // When the loader thread hits the bookend, it will exit.
  static BookendAsyncResource bookend;
  QueueJob(&bookend);
}

bool AsyncLoader::TryFinalize() {
  for (;;) {
    auto res = LockReturn<AsyncAsset *>(
        [this]() { return done_.empty() ? nullptr : done_.front(); });
    if (!res) break;
    bool ok = res->Finalize();
    if (!ok) {
      // Can't do much here, since res is already constructed. Caller has to
      // check IsValid() to know if resource can be used.
    }
    Lock([this, res]() {
      // It's possible that the resource was destroyed during its finalize
      // callbacks, so ensure that it's still the first item in done_.
      if (done_.size() > 0 && done_.front() == res) {
        done_.pop_front();
      }
      --num_pending_requests_;
    });
  }
  return LockReturn<bool>([this]() { return num_pending_requests_ == 0; });
}

void AsyncLoader::Lock(const std::function<void()> &body) {
  auto err = SDL_LockMutex(static_cast<SDL_mutex *>(mutex_));
  (void)err;
  assert(err == 0);
  body();
  SDL_UnlockMutex(static_cast<SDL_mutex *>(mutex_));
}

}  // namespace fplbase
