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

// clang-format off
#include "fplbase/config.h"  // Must come first.
#include "fplbase/file_utilities.h"
#include "fplbase/logging.h"
// clang-format on

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

// clang-format off
#if defined(__APPLE__)
#include "TargetConditionals.h"
#include <CoreFoundation/CoreFoundation.h>
#endif  // defined(__APPLE__)
// clang-format on

#include "flatbuffers/util.h"
#include "fplutil/mutex.h"

namespace fplbase {

// Function called by LoadFile().
static fplutil::Mutex g_load_file_function_mutex_;
static LoadFileFunction g_load_file_function = LoadFileRaw;

LoadFileFunction SetLoadFileFunction(LoadFileFunction load_file_function) {
  fplutil::MutexLock lock(g_load_file_function_mutex_);
  LoadFileFunction previous_function = g_load_file_function;
  if (load_file_function) {
    g_load_file_function = load_file_function;
  } else {
    g_load_file_function = LoadFileRaw;
  }
  return previous_function;
}

bool LoadFile(const char *filename, std::string *dest) {
  LoadFileFunction load_file_function;
  {
    fplutil::MutexLock lock(g_load_file_function_mutex_);
    load_file_function = g_load_file_function;
  }
  assert(load_file_function);
  return load_file_function(filename, dest);
}

bool SaveFile(const char *filename, const std::string &src) {
  return SaveFile(filename, static_cast<const void *>(src.c_str()),
                  src.length());  // don't include the '\0'
}

#if defined(_WIN32)
inline char *getcwd(char *buffer, size_t maxlen) {
  return _getcwd(buffer, static_cast<int>(maxlen));
}

inline int chdir(const char *dirname) { return _chdir(dirname); }
#endif  // defined(_WIN32)

// Search up the directory tree from binary_dir for target_dir, changing the
// working directory to the target_dir and returning true if it's found,
// false otherwise.
bool ChangeToUpstreamDirDesktop(const char *const binary_dir,
                                const char *const target_dir) {
#if !defined(PLATFORM_MOBILE)
  {
    std::string target_dir_str(flatbuffers::PosixPath(target_dir));
    std::string current_dir(flatbuffers::PosixPath(binary_dir));
    std::string real_path;
    real_path.reserve(512);

    // Search up the tree from the directory containing the binary searching
    // for target_dir.
    for (;;) {
      size_t separator = current_dir.find_last_of(flatbuffers::kPathSeparator);
      if (separator == std::string::npos) break;
      current_dir = current_dir.substr(0, separator);
#ifdef _WIN32
      // On Windows, if you try to "cd c:" and you are already in a subdirectory
      // of c:, you will end up in the same directory. "cd c:\" will take you to
      // the root.
      if (current_dir.length() == 2) {
        current_dir.append("\\");
      }
#endif
      int chdir_error_code = chdir(current_dir.c_str());
      if (chdir_error_code) break;
      real_path[0] = '\0';
      const char* cwd = getcwd(&real_path[0], real_path.capacity());
      assert(cwd);  // cwd could be null if real_path is not long enough.
      current_dir = flatbuffers::PosixPath(cwd);
      std::string target =
        flatbuffers::ConCatPathFileName(current_dir, target_dir_str);
      chdir_error_code = chdir(target.c_str());
      if (chdir_error_code == 0) return true;
#ifdef _WIN32
      // If current_dir is now "c:\" on Windows, then we have tried and failed
      // to find the target directory. Windows guarantees single letter drive
      // names.
      if (current_dir.length() == 3) {
        break;
      }
#endif
    }
    return false;
  }
#else
  (void)binary_dir;
  (void)target_dir;
  return true;
#endif
}

bool ChangeToUpstreamDir(const char *const binary_dir,
                         const char *const target_dir) {
#if defined(__APPLE__)
  (void)binary_dir;
  // Get the root of the target directory from the Bundle instead of using the
  // directory specified by the client.
  {
    CFBundleRef main_bundle = CFBundleGetMainBundle();
    CFURLRef resources_url = CFBundleCopyResourcesDirectoryURL(main_bundle);
    char path[PATH_MAX];
    if (!CFURLGetFileSystemRepresentation(
            resources_url, true, reinterpret_cast<UInt8*>(path), PATH_MAX)) {
      LogError(kError, "Could not set the bundle directory");
      return false;
    }
    CFRelease(resources_url);
    int retval = chdir(path);
    if (retval == 0) {
      retval = chdir(target_dir);
    }
    return (retval == 0);
  }
#else
  extern bool ChangeToUpstreamDirDesktop(const char *const binary_dir,
                                         const char *const target_dir);
  return ChangeToUpstreamDirDesktop(binary_dir, target_dir);
#endif
}

#if defined(__ANDROID__)

static AAssetManager *g_asset_manager = nullptr;

void SetAAssetManager(AAssetManager *manager) { g_asset_manager = manager; }

AAssetManager *GetAAssetManager() { return g_asset_manager; }

#endif  // defined(__ANDROID__)

}  // namespace fplbase
