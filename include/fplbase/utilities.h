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

#ifndef FPLBASE_UTILITIES_H
#define FPLBASE_UTILITIES_H

#include <functional>
#include <string>
#include "fplbase/config.h"  // Must come first.
#include "fplbase/file_utilities.h"
#include "fplbase/logging.h"

#include "mathfu/utilities.h"

#if defined(__ANDROID__)
#include <jni.h>
#endif

namespace fplbase {

/// @file
/// @brief General utility functions, used by FPLBase, and that might be of use
/// to people using the library:
/// @addtogroup fplbase_utilities
/// @{

/// @brief Enum for use with the `Set/GetPerformanceMode()` functions.
enum PerformanceMode {
  // Normal mode.  No special actions taken.
  kNormalPerformance = 0,

  // High performance mode.  Attempt to keep the CPU frequency up.
  kHighPerformance
};

#ifdef __ANDROID__
/// @brief Used for Android to represent a Vsync callback function.
typedef void (*VsyncCallback)(void);

/// @brief Used for Android to simulate a keypress. Corresponds to `F24`, which
/// is unavailable on most keyboards.
const int kDefaultAndroidKeycode = 115;

/// @brief Used for Android. Corresponds to the time, in seconds, between
/// simulated keypresses.
const double kDefaultTimeBetweenPresses = 1.0;

/// @brief HighPerformanceParams are used on Android to configure simulated key
/// presses, in order to stop the CPU governor (see
/// https://www.kernel.org/doc/Documentation/cpu-freq/governors.txt) from
/// reducing the CPU frequency when a user is not providing tactile input.
///
/// For example, if a user is providing data to the device via a gyroscope, it
/// is possible for the CPU governor to reduce the CPU (and potentially the
/// GPU) frequency, reducing application performance.
struct HighPerformanceParams {
  HighPerformanceParams()
      : android_key_code(kDefaultAndroidKeycode),
        time_between_presses(kDefaultTimeBetweenPresses) {}
  HighPerformanceParams(int keycode, int presses)
      : android_key_code(keycode), time_between_presses(presses) {}

  /// @brief The key to press repeatedly, to keep the CPU active.
  int android_key_code;
  // @brief The time (in seconds) between keypress events.
  double time_between_presses;
};
#endif

/// @brief Load preference settings.
///
/// The API uses a dedicated API when an optimal API is available instead of
/// regular file IO APIs.
/// @param[in] filename A UTF-8 C-string representing the file to load
/// preferences from.
/// @param[out] dest A pointer to a `std::string` to capture the preferences
/// output.
/// @return Returns `false` if the file couldn't be loaded (usually means it's
/// not present, but can also mean there was a read error).
bool LoadPreferences(const char *filename, std::string *dest);

/// @brief Save preference settings.
///
/// The API uses a dedicated API when an optimal API is available instead of
/// regular file IO APIs.
/// @param[in] filename A UTF-8 C-string representing the file to save
/// preferences to.
/// @param[in] data A const void pointer to the data that should be written to
/// the file specified by `filename`.
/// @param[in] size The size of the `data` array to write to the file specified
/// by `filename`.
/// @return Returns `false` if the file couldn't be loaded (usually means it's
/// not present, but can also mean there was a read error).
bool SavePreferences(const char *filename, const void *data, size_t size);

/// @brief Load a single integer value to a preference.
/// @param[in] key The UTF-8 key for the preference.
/// @param[in] initial_value The default value to use if the preference is not
/// found.
/// @return Returns the preference value.
int32_t LoadPreference(const char *key, int32_t initial_value);

/// @brief Save a single integer value to a preference.
/// @param[in] key The UTF-8 key for the preference.
/// @param[in] value The value to save for the preference.
bool SavePreference(const char *key, int32_t value);

/// @brief Map a file into memory and returns its contents via pointer.
/// @details In contrast to `LoadFile()`, this method calls mmap API to map the
/// whole or a part of the file.
/// @param[in] filename A UTF-8 C-string representing the file to load.
/// @param[in] offset An offset of the file contents to map.
/// @param[in/out] size A size to map. A size of 0 indicates to map whole file.
/// returns a mapped size of the file.
/// @return Returns a mapped pointer. nullptr when failed to map the file.
const void *MapFile(const char *filename, int32_t offset, int32_t *size);

/// @brief Unmap a pointer that is mapped via MapFile() API.
/// @param[in] file A pointer to the file, returned via MapFile API.
/// @param[in] size A size to unmap.
void UnmapFile(const void *file, int32_t size);

/// @brief check if 16bpp MipMap is supported.
/// @return Return `true` if 16bpp MipMap generation is supported.
/// @note Basically always true, except on certain android devices.
bool MipmapGeneration16bppSupported();

/// @brief Get the system's RAM size.
/// @return Returns the system RAM size in MB.
int32_t GetSystemRamSize();

#if defined(__ANDROID__)
/// @brief Get the Android activity class.
/// @return Returns a pointer to the Java instance of the activity class
/// in an Android application.
jobject AndroidGetActivity(bool optional = false);

#ifdef FPLBASE_BACKEND_STDLIB
/// @brief Sets the Android activity class. Required by many of the functions
/// below, must be set before calling them.
void AndroidSetActivity(jobject activity);
#endif

/// @brief Get the Java native interface object (JNIEnv).
/// @return Returns a pointer to the Java native interface object (JNIEnv) of
/// the current thread on an Android application.
JNIEnv *AndroidGetJNIEnv();

/// @brief Register for handling vsync callbacks on android.
/// @note As with most callbacks, this will normally be called on a separate
/// thread, so you'll need to be  careful about thread-safety with anything you
/// do during the callback.
/// @param callback The VSync callback function to register.
/// @return Return value is whatever callback was previously registered. (Or
/// `nullptr` if there was none.)
VsyncCallback RegisterVsyncCallback(VsyncCallback callback);

/// @brief Call the current Vsync callback. Normally done by the native OnVsync
/// method.
void CallVsyncCallback();

/// @brief Blocks until the next time a VSync happens.
void WaitForVsync();

/// @brief Get Vsync frame id.
/// @return Returns a unique ID representing the frame. Guaranteed to change
/// every time the frame increments.
/// @warning May eventually wrap.
int GetVsyncFrameId();

/// @brief Triggers a keypress event on an Android device.
/// @param[in] android_keycode The key code corresponding to the
/// keypress that should be triggered.
void SendKeypressEventToAndroid(int android_keycode);

/// @brief Get the name of the current activity class.
/// @note This can be used by C++ code to determine how the application was
/// started.
/// @return Returns a `std::string` containing the Android activity name.
std::string AndroidGetActivityName();

/// @brief Determine whether the activity was started with `Intent.ACTION_VIEW`
/// and, if so, return the data the user wants to "view" in the application.
/// @return Returns a `std::string` contianing the View intent data.
std::string AndroidGetViewIntentData();
#endif  // __ANDROID__

#if defined(__ANDROID__) && defined(FPLBASE_BACKEND_STDLIB)
/// @brief Set global Java virtual machine object.
/// @note This method should be called only once.
void AndroidSetJavaVM(JavaVM* vm, jint jni_version);
#endif

/// @brief Retrieve a path where an app can store data files.
/// @param[in] app_name A C-string corresponding to the name of the
/// application.
/// @param[out] path A `std::string` to capture the storage path. Contains
/// `nullptr` if the function returns `false`.
/// @return Returns `true` if a storage path was found. Otherwise it
/// returns `false`.
bool GetStoragePath(const char *app_name, std::string *path);

/// @brief Checks whether Head Mounted Displays, such as Cardboard, are
/// supported by the system being run on.
/// @return Returns `true` if Head Mounted Displays are supported.
bool SupportsHeadMountedDisplay();

/// @brief Checks if the device has a touchscreen.
/// @return Returns `true` if the device has a touchscreen.
bool TouchScreenDevice();

/// @brief Checks whether the device we are running on is an Android TV device.
/// @return Returns `true` if the device is an Android TV device.
/// @note Always returns `false` when not running on Android.
bool IsTvDevice();

/// @brief Sets the performance mode.
/// @param[in] new_mode The `PerformanceMode` to set.
void SetPerformanceMode(PerformanceMode new_mode);

/// @brief Get the current performance mode.
/// @return Returns the current `PerformanceMode`.
PerformanceMode GetPerformanceMode();

/// @brief Relaunch the application.
void RelaunchApplication();

#ifdef __ANDROID__
/// @brief Sets the specific parameters used by high-performance mode on
/// Android.
/// @param[in] params A const reference to a `HighPerformanceParams` struct
/// to set for use with high-performance mode.
void SetHighPerformanceParameters(const HighPerformanceParams &params);

/// @brief Get the high performance parameters.
/// @return Returns the current performance parameters, in the form of a struct.
const HighPerformanceParams &GetHighPerformanceParameters();

/// @brief Get the Android device's model.
/// @return Returns the model of the Android device the app is currently being
/// run on.
std::string DeviceModel();

/// @brief Get the Android device's API level.
/// @return Returns the API level of the Android device the app is currently
/// being run on.
int32_t AndroidGetApiLevel();
#endif
/// @}
}  // namespace fplbase

#endif  // FPLBASE_UTILITIES_H
