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

#ifndef FPLBASE_FILE_UTILITIES_H
#define FPLBASE_FILE_UTILITIES_H

#include <functional>
#include <string>

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#endif

namespace fplbase {

/// @brief Called by `LoadFile()`.
typedef std::function<bool(const char *filename, std::string *dest)>
    LoadFileFunction;

/// @brief Checks if a file exists.
/// @param[in] filename A UTF-8 C-string representing the file to check.
/// @return Returns `true` if the file exists, false otherwise.
bool FileExistsRaw(const char *filename);

/// @brief Loads a file and returns its contents via string pointer.
/// @param[in] filename A UTF-8 C-string representing the file to load.
/// @param[out] dest A pointer to a `std::string` to capture the output of
/// the file.
/// @return Returns `false` if the file couldn't be loaded (usually means
/// it's not present, but can also mean there was a read error).
bool LoadFileRaw(const char *filename, std::string *dest);

/// @brief Loads a file and returns its contents via string pointer.
/// @details In contrast to `LoadFileRaw()`, this method simply calls the
/// function set by `SetLoadFileFunction()` to read the specified file.
/// @param[in] filename A UTF-8 C-string representing the file to load.
/// @param[out] dest A pointer to a `std::string` to capture the output of
/// the file.
/// @return Returns `false` if the file couldn't be loaded (usually means it's
/// not present, but can also mean there was a read error).
bool LoadFile(const char *filename, std::string *dest);

/// @brief Set the function called by `LoadFile()`.
/// @param[in] load_file_function The function to be used by `LoadFile()` to
/// read files.
/// @note If the specified function is nullptr, `LoadFileRaw()` is set as the
/// default function.
/// @return Returns the function previously set by `LoadFileFunction()`.
LoadFileFunction SetLoadFileFunction(LoadFileFunction load_file_function);

/// @brief Save a string to a file, overwriting the existing contents.
/// @param[in] filename A UTF-8 C-string representing the file to save to.
/// @param[in] data A const reference to a `std::string` containing the data
/// that should be written to the file specified by `filename`.
/// @return Returns `false` if the file could not be written.
bool SaveFile(const char *filename, const std::string &data);

/// @brief Save a string to a file, overwriting the existing contents.
/// @param[in] filename A UTF-8 C-string representing the file to save to.
/// @param[in] data A const void pointer to the data that should be written to
/// the file specified by `filename`.
/// @param[in] size The size of the `data` array to write to the file specified
/// by `filename`.
/// @return Returns `false` if the file could not be written.
bool SaveFile(const char *filename, const void *data, size_t size);

/// @brief Search and change to a given directory.
/// @param binary_dir A C-string corresponding to the current directory
/// to start searching from.
/// @param target_dir A C-string corresponding to the desired directory
/// that should be changed to, if found.
/// @return Returns `true` if it's found, `false` otherwise.
bool ChangeToUpstreamDir(const char *const binary_dir,
                         const char *const target_dir);

#if defined(__ANDROID__)

/// @brief Set an Android asset manager.
/// @param[in] manager A pointer to an already-created instance of
/// `AAssetManager`.
/// @note Must call this function once before loading any assets.
void SetAAssetManager(AAssetManager *manager);

/// @brief Returns the set Android asset manager.
/// @return Returns the AAssetManager that has been previously set.
AAssetManager *GetAAssetManager();

#endif  // defined(__ANDROID__)

}  // namespace fplbase

#endif  // FPLBASE_FILE_UTILITIES_H
