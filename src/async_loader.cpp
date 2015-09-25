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

#ifndef FPL_BASE_BACKEND_SDL
#error This version of AsyncLoader depends on SDL.
#endif

namespace fpl {

// Push this to signal the worker thread that it's time to quit.
class BookendAsyncResource : public AsyncResource {
  static std::string kBookendFileName;

 public:
  BookendAsyncResource() : AsyncResource(kBookendFileName) {}
  virtual ~BookendAsyncResource() {}
  virtual void Load() {}
  virtual void Finalize() {}
  static bool IsBookend(const AsyncResource &res) {
    return res.filename() == kBookendFileName;
  }
};

// static
std::string BookendAsyncResource::kBookendFileName = "bookend";

AsyncLoader::AsyncLoader() : worker_thread_(nullptr) {
  mutex_ = SDL_CreateMutex();
  job_semaphore_ = SDL_CreateSemaphore(0);
  assert(mutex_ && job_semaphore_);
}

AsyncLoader::~AsyncLoader() {
  StopLoadingWhenComplete();
  SDL_WaitThread(static_cast<SDL_Thread *>(worker_thread_), nullptr);

  if (mutex_) {
    SDL_DestroyMutex(static_cast<SDL_mutex *>(mutex_));
    mutex_ = nullptr;
  }
  if (job_semaphore_) {
    SDL_DestroySemaphore(static_cast<SDL_semaphore *>(job_semaphore_));
    job_semaphore_ = nullptr;
  }
}

void AsyncLoader::QueueJob(AsyncResource *res) {
  Lock([this, res]() { queue_.push_back(res); });
  SDL_SemPost(static_cast<SDL_semaphore *>(job_semaphore_));
}

void AsyncLoader::LoaderWorker() {
  for (;;) {
    auto res = LockReturn<AsyncResource *>(
        [this]() { return queue_.empty() ? nullptr : queue_[0]; });
    if (!res) {
      SDL_SemWait(static_cast<SDL_semaphore *>(job_semaphore_));
      continue;
    }
    // Stop loading once we reach the bookend enqueued by
    // StopLoadingWhenComplete(). To start loading again, call StartLoading().
    if (BookendAsyncResource::IsBookend(*res)) break;
    LogInfo(kApplication, "async load: %s", res->filename_.c_str());
    res->Load();
    Lock([this, res]() {
      queue_.erase(queue_.begin());
      done_.push_back(res);
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

void AsyncLoader::StopLoadingWhenComplete() {
  // When the loader thread hits the bookend, it will exit.
  static BookendAsyncResource bookend;
  QueueJob(&bookend);
}

bool AsyncLoader::TryFinalize() {
  for (;;) {
    auto res = LockReturn<AsyncResource *>(
        [this]() { return done_.empty() ? nullptr : done_[0]; });
    if (!res) break;
    res->Finalize();
    Lock([this]() { done_.erase(done_.begin()); });
  }
  return LockReturn<bool>([this]() { return queue_.empty() && done_.empty(); });
}

void AsyncLoader::Lock(const std::function<void()> &body) {
  auto err = SDL_LockMutex(static_cast<SDL_mutex *>(mutex_));
  (void)err;
  assert(err == 0);
  body();
  SDL_UnlockMutex(static_cast<SDL_mutex *>(mutex_));
}

}  // namespace fpl
