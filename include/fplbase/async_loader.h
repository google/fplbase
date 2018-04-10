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

#ifndef FPLBASE_ASYNC_LOADER_H
#define FPLBASE_ASYNC_LOADER_H

#include <stdint.h>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "fplbase/config.h"  // Must come first.
#include "fplbase/asset.h"

#ifdef FPLBASE_BACKEND_STDLIB
#include <mutex>
#include <thread>
#include <condition_variable>
#endif

namespace fplbase {

/// @file
/// @addtogroup fplbase_async_loader
/// @{

typedef void *Thread;
typedef void *Mutex;
typedef void *Semaphore;

class AsyncLoader;

/// @class AsyncResource
/// @brief Any resource that can be loaded asynchronously should inherit from
///        this.
class AsyncAsset : public Asset {
 public:
  /// @brief A function pointer to an asset loaded callback function.
  typedef std::function<void()> AssetFinalizedCallback;

  /// @brief Default constructor for an empty AsyncAsset.
  AsyncAsset() : data_(nullptr), finalized_(false) {}

  /// @brief Construct an AsyncAsset with a given file name.
  /// @param[in] filename A C-string corresponding to the name of the asset
  /// file.
  explicit AsyncAsset(const char *filename)
      : filename_(filename),
        data_(nullptr),
        finalize_callbacks_(0),
        finalized_(false) {}

  /// @brief AsyncAsset destructor.
  virtual ~AsyncAsset() {}

  /// @brief Override with the actual loading behavior.
  ///
  /// Load should perform the actual loading of filename_, and store the
  /// result in data_, or nullptr upon failure. It is called on the loader
  /// thread, so should not access any program state outside of this object.
  /// Since there will be only one loader thread, any libraries called by Load
  /// need not be MT-safe as long as they're not also called by the main thread.
  virtual void Load() = 0;

  /// @brief Override with converting the data into the resource.
  ///
  /// This should implement the behavior of turning data_ into the actual
  /// desired resource. Called on the main thread only.
  /// Should check if data_ is null.
  virtual bool Finalize() = 0;

  /// @brief Whether this object has been loaded and finalized. This does not
  /// signal success or not -- check IsValid for that.
  bool IsFinalized() const { return finalized_; }

  /// @brief Whether this object loaded and finalized correctly. Call after
  /// Finalize has been called (by AssetManager::TryFinalize).
  virtual bool IsValid() = 0;

  /// @brief Performs a synchronous load by calling Load & Finalize.
  ///
  /// Not used by the loader thread, should be called on the main thread.
  /// @return Returns false on failure.
  bool LoadNow() {
    Load();
    bool ok = data_ != nullptr;
    // Call this even if data_ is null, to enforce Finalize() checking for it.
    return Finalize() && ok;
  }

  /// @brief Sets the filename that should be loaded.
  ///
  /// Set the filename in situations where it can't be initialized in the
  /// constructor. Must be called before AsyncLoader::QueueJob().
  ///
  /// @param filename The name of the file to load.
  void set_filename(const std::string &filename) { filename_ = filename; }

  /// @brief The name of the file associated with the resource.
  ///
  /// @return Returns the filename.
  const std::string &filename() const { return filename_; }

  /// @brief Adds a callback to be called when the asset is finalized.
  ///
  /// Add a callback so logic can be executed when an asset is done loading.
  /// This does nothing if the asset has already been finalized.
  ///
  /// @param callback The function to be called.
  /// @return Returns true if the asset is not finalized and the callback was
  /// added.
  bool AddFinalizeCallback(AssetFinalizedCallback callback) {
    if (finalized_) {
      return false;
    }
    finalize_callbacks_.push_back(callback);
    return true;
  }

 protected:
  /// @brief Calls app callbacks when an asset is ready to be used.
  ///
  /// This should be called by descendants as soon as they are finalized.
  void CallFinalizeCallback() {
    auto callbacks = finalize_callbacks_;
    finalize_callbacks_.clear();
    for (auto it = callbacks.begin();
         it != callbacks.end(); ++it) {
      (*it)();
    }
    finalized_ = true;
  }

  /// @brief The resource file name.
  std::string filename_;
  /// @brief The resource data.
  const uint8_t *data_;

  /// @brief List of callbacks to be invoked when the asset is finalized.
  std::vector<AssetFinalizedCallback> finalize_callbacks_;
  /// @brief Whether the asset has been finalized.
  bool finalized_;

  friend class AsyncLoader;
};

/// @class AsyncLoader
/// @brief Handles loading AsyncAsset objects.
class AsyncLoader {
 public:
  AsyncLoader();
  ~AsyncLoader();

  /// @brief Queues AsyncResources to be loaded by StartLoading.
  ///
  /// Call this any number of times before StartLoading.
  ///
  /// @param res The resource to queue for loading.
  void QueueJob(AsyncAsset *res);

  /// @brief Aborts any pending operations for the given asset.
  ///
  /// If the given asset is currently loading, this blocks until it is complete.
  /// If it has not yet been loaded, this removes it from the queue.
  ///
  /// @param res The resource to abort performing any operations on.
  void AbortJob(AsyncAsset *res);

  /// @brief Launches the loading thread for the previously queued jobs.
  void StartLoading();

  /// @brief Pause the loading thread for previously queued jobs.
  ///
  /// Blocks until only the current job is finished loading. You can resume
  /// loading assets by calling StartLoading().
  void PauseLoading();

  /// @brief Ends the loading thread when all jobs are done.
  ///
  /// Cleans-up the background loading thread once all jobs have been completed.
  /// You can restart with StartLoading() if you like.
  void StopLoadingWhenComplete();

  /// @brief Call to Finalize any resources that have finished loading.
  ///
  /// Call this once per frame after StartLoading. Will call Finalize on any
  /// resources that have finished loading. One it returns true, that means
  /// the queue is empty, all resources have been processed, and the loading
  /// thread has terminated.
  ///
  /// @return Returns true once the queue is empty.
  bool TryFinalize();

  /// @brief Shuts down the loader after completing all pending loads.
  void Stop();

 private:
#ifdef FPLBASE_BACKEND_SDL
  void Lock(const std::function<void()> &body);
  template <typename T>
  T LockReturn(const std::function<T()> &body) {
    T ret;
    Lock([&ret, &body]() { ret = body(); });
    return ret;
  }
#endif

  void LoaderWorker();
  static int LoaderThread(void *user_data);

  std::deque<AsyncAsset *> queue_, done_;
  AsyncAsset *loading_;
  int num_pending_requests_;
#ifdef FPLBASE_BACKEND_SDL
  // Keep handle to the worker thread around so that we can wait for it to
  // finish before destroying the class.
  Thread worker_thread_;

  // This lock protects ALL state in this class, i.e. the two vectors.
  Mutex mutex_;

  // Kick-off the worker thread when a new job arrives.
  Semaphore job_semaphore_;
#elif defined(FPLBASE_BACKEND_STDLIB)
  std::thread worker_thread_;
  std::mutex mutex_;
  std::condition_variable job_cv_;
#else
#error Need to define FPLBASE_BACKEND_XXX
#endif
};

/// @}
}  // namespace fplbase

#endif  // FPLBASE_ASYNC_LOADER_H
