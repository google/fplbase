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
#include "precompiled.h"
#include "fplbase/utilities.h"
#include "fplutil/mutex.h"
// clang-format on

// Header files for mmap API.
#ifdef _WIN32
#else
// Platforms with POSIX
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#endif  // _WIN32

#include <stdarg.h>

namespace fplbase {

const void *MapFile(const char *filename, int32_t offset, int32_t *size) {
#ifdef _WIN32
  (void)filename;
  (void)offset;
  (void)size;
  LogError(kError, "MapFile unimplemented on Win32.");
  return nullptr;
#else
  // POSIX implementation.
  auto fd = open(filename, O_RDONLY);
  if (fd == -1) {
    LogError("Can't open the file for mmap: %s\n", filename);
    return nullptr;
  }
  struct stat sb;
  if (fstat(fd, &sb) == -1) {
    close(fd);
    return nullptr;
  }
  auto s = *size ? *size : sb.st_size;
  auto p = mmap(0, s, PROT_READ, MAP_SHARED, fd, offset);
  if (p == MAP_FAILED) {
    LogError("Can't map the file: %s\n", filename);
    close(fd);
    return nullptr;
  }
  // Now we can close the file.
  close(fd);
  *size = sb.st_size;
  return p;
#endif // _WIN32
}

void UnmapFile(const void *file, int32_t size) {
#ifdef _WIN32
  (void)file;
  (void)size;
  LogError(kError, "UnmapFile unimplemented on Win32.");
#else
  munmap(const_cast<void *>(file), size);
#endif // _WIN32
}

#if defined(__ANDROID__)
static jobject GetSharedPreference(JNIEnv *env, jobject activity) {
  jclass activity_class = env->GetObjectClass(activity);
  jmethodID get_preferences = env->GetMethodID(
      activity_class, "getSharedPreferences",
      "(Ljava/lang/String;I)Landroid/content/SharedPreferences;");
  jstring file = env->NewStringUTF("preference");
  // The last value: Content.MODE_PRIVATE = 0.
  jobject preference =
      env->CallObjectMethod(activity, get_preferences, file, 0);
  env->DeleteLocalRef(activity_class);
  env->DeleteLocalRef(file);
  return preference;
}
#endif

bool LoadPreferences(const char *filename, std::string *dest) {
#if defined(__ANDROID__)
  jobject activity = AndroidGetActivity(true);
  if (!activity)
    return LoadFile(filename, dest);
  // Use Android preference API to store blob as a Java String.
  JNIEnv *env = AndroidGetJNIEnv();
  jobject preference = GetSharedPreference(env, activity);
  jclass preference_class = env->GetObjectClass(preference);

  // Retrieve blob as a String.
  jstring key = env->NewStringUTF("data");
  jstring default_value = env->NewStringUTF("");
  jmethodID get_string = env->GetMethodID(
      preference_class, "getString",
      "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
  jstring blob = static_cast<jstring>(
      env->CallObjectMethod(preference, get_string, key, default_value));

  // Retrieve value.
  const jchar *str = env->GetStringChars(blob, nullptr);
  jsize len = env->GetStringLength(blob);
  dest->assign(len, 0);

  memcpy(&(*dest)[0], reinterpret_cast<const char *>(str), len);
  LogInfo("Loaded %d bytes from the preference", len);
  bool ret = len > 0 ? true : false;

  env->ReleaseStringChars(blob, str);

  // Release objects references.
  env->DeleteLocalRef(blob);
  env->DeleteLocalRef(default_value);
  env->DeleteLocalRef(key);
  env->DeleteLocalRef(preference_class);
  env->DeleteLocalRef(preference);
  env->DeleteLocalRef(activity);

  return ret;
#else
  return LoadFile(filename, dest);
#endif
}

int32_t LoadPreference(const char *key, int32_t initial_value) {
#ifdef __ANDROID__
  // Use Android preference API to store an integer value.
  JNIEnv *env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
  jobject preference = GetSharedPreference(env, activity);
  jclass preference_class = env->GetObjectClass(preference);

  // Retrieve blob as a String.
  jstring pref_key = env->NewStringUTF(key);
  jmethodID get_int =
      env->GetMethodID(preference_class, "getInt", "(Ljava/lang/String;I)I");
  auto value = env->CallIntMethod(preference, get_int, pref_key, initial_value);

  // Release objects references.
  env->DeleteLocalRef(pref_key);
  env->DeleteLocalRef(preference_class);
  env->DeleteLocalRef(preference);
  env->DeleteLocalRef(activity);
  return value;
#else
  (void)key;
  return initial_value;
#endif
}

bool SavePreferences(const char *filename, const void *data, size_t size) {
#if defined(__ANDROID__)
  jobject activity = AndroidGetActivity(true);
  if (!activity)
    return SaveFile(filename, data, size);
  // Use Android preference API to store blob as a Java String.
  JNIEnv *env = AndroidGetJNIEnv();
  jobject preference = GetSharedPreference(env, activity);
  jclass preference_class = env->GetObjectClass(preference);

  // Retrieve an editor.
  jmethodID edit = env->GetMethodID(
      preference_class, "edit", "()Landroid/content/SharedPreferences$Editor;");
  jobject editor = env->CallObjectMethod(preference, edit);
  jclass editor_class = env->GetObjectClass(editor);

  // Put blob as a String.
  jstring key = env->NewStringUTF("data");
  jstring blob = env->NewString(static_cast<const jchar *>(data), size);
  jmethodID put_string =
      env->GetMethodID(editor_class, "putString",
                       "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/"
                       "SharedPreferences$Editor;");
  jobject result = env->CallObjectMethod(editor, put_string, key, blob);

  // Commit a change.
  jmethodID commit = env->GetMethodID(editor_class, "commit", "()Z");
  jboolean ret = env->CallBooleanMethod(editor, commit);
  LogInfo("Saved %d bytes to the preference, %d", size, ret);

  // Release objects references.
  env->DeleteLocalRef(result);
  env->DeleteLocalRef(blob);
  env->DeleteLocalRef(key);
  env->DeleteLocalRef(editor_class);
  env->DeleteLocalRef(editor);
  env->DeleteLocalRef(preference_class);
  env->DeleteLocalRef(preference);
  env->DeleteLocalRef(activity);

  return ret;
#else
  return SaveFile(filename, data, size);
#endif
}

bool SavePreference(const char *key, int32_t value) {
#ifdef __ANDROID__
  // Use Android preference API to store an integer value.
  JNIEnv *env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
  jobject preference = GetSharedPreference(env, activity);
  jclass preference_class = env->GetObjectClass(preference);

  // Retrieve an editor.
  jmethodID edit = env->GetMethodID(
      preference_class, "edit", "()Landroid/content/SharedPreferences$Editor;");
  jobject editor = env->CallObjectMethod(preference, edit);
  jclass editor_class = env->GetObjectClass(editor);

  // Put blob as a String.
  jstring pref_key = env->NewStringUTF(key);
  jmethodID put_int = env->GetMethodID(editor_class, "putInt",
                                       "(Ljava/lang/String;I)Landroid/content/"
                                       "SharedPreferences$Editor;");
  jobject result = env->CallObjectMethod(editor, put_int, pref_key, value);

  // Commit a change.
  jmethodID commit = env->GetMethodID(editor_class, "commit", "()Z");
  jboolean ret = env->CallBooleanMethod(editor, commit);

  // Release objects references.
  env->DeleteLocalRef(result);
  env->DeleteLocalRef(pref_key);
  env->DeleteLocalRef(editor_class);
  env->DeleteLocalRef(editor);
  env->DeleteLocalRef(preference_class);
  env->DeleteLocalRef(preference);
  env->DeleteLocalRef(activity);

  return ret;
#else
  (void)key;
  (void)value;
  return false;
#endif
}

bool HasSystemFeature(const char *feature_name) {
#if defined(__ANDROID__)
  jobject activity = AndroidGetActivity(true);
  if (!activity)
    return false;
  JNIEnv *env = AndroidGetJNIEnv();
  jclass fpl_class = env->GetObjectClass(activity);
  jmethodID has_system_feature =
      env->GetMethodID(fpl_class, "hasSystemFeature", "(Ljava/lang/String;)Z");
  jstring jfeature_name = env->NewStringUTF(feature_name);
  jboolean has_feature =
      env->CallBooleanMethod(activity, has_system_feature, jfeature_name);
  env->DeleteLocalRef(jfeature_name);
  env->DeleteLocalRef(fpl_class);
  env->DeleteLocalRef(activity);
  return has_feature;
#else
  (void)feature_name;
  return false;
#endif
}

bool TouchScreenDevice() {
  return HasSystemFeature("android.hardware.touchscreen");
}

static inline bool IsUpperCase(const char c) { return c == toupper(c); }

std::string CamelCaseToSnakeCase(const char *const camel) {
  // Replace capitals with underbar + lowercase.
  std::string snake;
  for (const char *c = camel; *c != '\0'; ++c) {
    if (IsUpperCase(*c)) {
      const bool is_start_or_end = c == camel || *(c + 1) == '\0';
      if (!is_start_or_end) {
        snake += '_';
      }
      snake += static_cast<char>(tolower(*c));
    } else {
      snake += *c;
    }
  }
  return snake;
}

std::string FileNameFromEnumName(const char *const enum_name,
                                 const char *const prefix,
                                 const char *const suffix) {
  // Skip over the initial 'k', if it exists.
  const bool starts_with_k = enum_name[0] == 'k' && IsUpperCase(enum_name[1]);
  const char *const camel_case_name = starts_with_k ? enum_name + 1 : enum_name;

  // Assemble file name.
  return std::string(prefix) + CamelCaseToSnakeCase(camel_case_name) +
         std::string(suffix);
}

#if defined(__ANDROID__)
int32_t AndroidGetApiLevel() {
  // Retrieve API level through JNI.
  JNIEnv *env = AndroidGetJNIEnv();
  jclass build_class = env->FindClass("android/os/Build$VERSION");
  jfieldID apilevel_id = env->GetStaticFieldID(build_class, "SDK_INT", "I");
  jint apilevel = env->GetStaticIntField(build_class, apilevel_id);

  // Clean up
  env->DeleteLocalRef(build_class);
  return apilevel;
}
#endif

#ifdef __ANDROID__
VsyncCallback g_vsync_callback = nullptr;

VsyncCallback RegisterVsyncCallback(VsyncCallback callback) {
  VsyncCallback old_callback = g_vsync_callback;
  g_vsync_callback = callback;
  return old_callback;
}

void CallVsyncCallback() {
  if (g_vsync_callback != nullptr) {
    g_vsync_callback();
  }
}

HighPerformanceParams high_performance_params;

// Sets the specific parameters for high performance mode on Android
void SetHighPerformanceParameters(const HighPerformanceParams &params) {
  high_performance_params = params;
}

// Returns the high performance mode parameters in a struct.
const HighPerformanceParams &GetHighPerformanceParameters() {
  return high_performance_params;
}
#endif  // __ANDROID__

static PerformanceMode performance_mode = kNormalPerformance;

// Sets the performance mode.
void SetPerformanceMode(PerformanceMode new_mode) {
  performance_mode = new_mode;
}

PerformanceMode GetPerformanceMode() { return performance_mode; }

#if defined(__ANDROID__)
bool AndroidCheckDeviceList(const char *device_list[], const int num_devices) {
  // Retrieve device name through JNI.
  JNIEnv *env = AndroidGetJNIEnv();
  if (!env)
    return true;
  jclass build_class = env->FindClass("android/os/Build");
  jfieldID model_id =
      env->GetStaticFieldID(build_class, "MODEL", "Ljava/lang/String;");
  jstring model_obj =
      static_cast<jstring>(env->GetStaticObjectField(build_class, model_id));
  const char *device_name = env->GetStringUTFChars(model_obj, 0);
  // Check if the device name is in the list.
  bool result = true;
  for (int i = 0; i < num_devices; ++i) {
    if (strcmp(device_list[i], device_name) == 0) {
      result = false;
      break;
    }
  }
  // Clean up
  env->ReleaseStringUTFChars(model_obj, device_name);
  env->DeleteLocalRef(model_obj);
  env->DeleteLocalRef(build_class);
  return result;
}
#endif

bool MipmapGeneration16bppSupported() {
#if defined(__ANDROID__)
  static const char *device_list[] = {"Galaxy Nexus", "Nexus S", "Nexus S 4G"};
  static bool supported = AndroidCheckDeviceList(
      device_list, sizeof(device_list) / sizeof(device_list[0]));
  return supported;
#else
  return true;
#endif
}

// Checks whether Head Mounted Displays are supported by the system.
bool SupportsHeadMountedDisplay() {
#if FPLBASE_ANDROID_VR
  JNIEnv *env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
  jclass fpl_class = env->GetObjectClass(activity);
  jmethodID supports_hmd =
      env->GetMethodID(fpl_class, "SupportsHeadMountedDisplay", "()Z");
  jboolean result = env->CallBooleanMethod(activity, supports_hmd);
  env->DeleteLocalRef(fpl_class);
  env->DeleteLocalRef(activity);
  return result;
#else
  return false;
#endif  // FPLBASE_ANDROID_VR
}

// Checks whether or not the activity is running on a Android-TV device.
bool IsTvDevice() {
#ifdef __ANDROID__
  JNIEnv *env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
  jclass fpl_class = env->GetObjectClass(activity);
  jmethodID is_tv_device = env->GetMethodID(fpl_class, "IsTvDevice", "()Z");
  jboolean result = env->CallBooleanMethod(activity, is_tv_device);
  env->DeleteLocalRef(fpl_class);
  env->DeleteLocalRef(activity);
  return result;
#else
  return false;
#endif  // __ANDROID
}

#if defined(__ANDROID__)
// Get the name of the current activity class.
std::string AndroidGetActivityName() {
  JNIEnv *env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
  jclass activity_class = env->GetObjectClass(activity);
  jmethodID get_class_method =
      env->GetMethodID(activity_class, "getClass", "()Ljava/lang/Class;");
  // Get the class instance.
  jclass activity_class_object =
      static_cast<jclass>(env->CallObjectMethod(activity, get_class_method));
  jclass activity_class_object_class =
      env->GetObjectClass(activity_class_object);
  jmethodID get_name_method = env->GetMethodID(
      activity_class_object_class, "getName", "()Ljava/lang/String;");
  jstring class_name_object = static_cast<jstring>(
      env->CallObjectMethod(activity_class_object, get_name_method));
  const char *class_name = env->GetStringUTFChars(class_name_object, JNI_FALSE);
  std::string activity_name(class_name);
  env->ReleaseStringUTFChars(class_name_object, class_name);
  env->DeleteLocalRef(class_name_object);
  env->DeleteLocalRef(activity_class_object_class);
  env->DeleteLocalRef(activity_class_object);
  env->DeleteLocalRef(activity_class);
  env->DeleteLocalRef(activity);
  return activity_name;
}

std::string AndroidGetViewIntentData() {
  std::string view_data;
  JNIEnv *env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
  jclass activity_class = env->GetObjectClass(activity);
  // Get the Intent used to start this activity using Activity.getIntent().
  jmethodID get_intent = env->GetMethodID(activity_class, "getIntent",
                                          "()Landroid/content/Intent;");
  jobject intent = env->CallObjectMethod(activity, get_intent);
  jclass intent_class = env->GetObjectClass(intent);
  // Get the action from the intent using Intent.getAction().
  jmethodID intent_get_action =
      env->GetMethodID(intent_class, "getAction", "()Ljava/lang/String;");
  jstring intent_action_string =
      static_cast<jstring>(env->CallObjectMethod(intent, intent_get_action));
  if (intent_action_string) {
    static const char *const kActionView = "android.intent.action.VIEW";
    const char *intent_action =
        env->GetStringUTFChars(intent_action_string, JNI_FALSE);
    // If the Intent action is Intent.ACTION_VIEW, get the data associated with
    // the Intent.
    if (strcmp(kActionView, intent_action) == 0) {
      jmethodID intent_get_data = env->GetMethodID(
          intent_class, "getDataString", "()Ljava/lang/String;");
      jstring intent_data_string =
          static_cast<jstring>(env->CallObjectMethod(intent, intent_get_data));
      if (intent_data_string) {
        const char *intent_data =
            env->GetStringUTFChars(intent_data_string, JNI_FALSE);
        view_data = intent_data;
        env->ReleaseStringUTFChars(intent_data_string, intent_data);
        env->DeleteLocalRef(intent_data_string);
      }
    }
    env->ReleaseStringUTFChars(intent_action_string, intent_action);
    env->DeleteLocalRef(intent_action_string);
  }
  env->DeleteLocalRef(intent_class);
  env->DeleteLocalRef(intent);
  env->DeleteLocalRef(activity_class);
  env->DeleteLocalRef(activity);
  return view_data;
}

void RelaunchApplication() {
  JNIEnv *env = AndroidGetJNIEnv();
  jobject activity = fplbase::AndroidGetActivity();
  jclass fpl_class = env->GetObjectClass(activity);

  jmethodID mid_relaunch = env->GetMethodID(fpl_class, "relaunch", "()V");
  env->CallVoidMethod(activity, mid_relaunch);

  env->DeleteLocalRef(fpl_class);
  env->DeleteLocalRef(activity);
}

// Sends a keypress event to the android system.  This will show up in android
// indistinguishable from a normal user key press
void SendKeypressEventToAndroid(int android_key_code) {
  jobject activity = AndroidGetActivity(true);
  if (!activity)
    return;
  JNIEnv *env = AndroidGetJNIEnv();
  jclass fpl_class = env->GetObjectClass(activity);
  jmethodID method_id =
      env->GetMethodID(fpl_class, "SendKeypressEventToAndroid", "(I)V");
  env->CallVoidMethod(activity, method_id, android_key_code);
  env->DeleteLocalRef(fpl_class);
  env->DeleteLocalRef(activity);
}

std::string DeviceModel() {
  JNIEnv *env = fplbase::AndroidGetJNIEnv();
  jclass build_class = env->FindClass("android/os/Build");
  jfieldID model_id =
      env->GetStaticFieldID(build_class, "MODEL", "Ljava/lang/String;");
  jstring model_object =
      static_cast<jstring>(env->GetStaticObjectField(build_class, model_id));
  const char *model_string = env->GetStringUTFChars(model_object, 0);
  std::string result = model_string;
  env->ReleaseStringUTFChars(model_object, model_string);
  env->DeleteLocalRef(model_object);
  env->DeleteLocalRef(build_class);
  return result;
}

#endif  // defined(__ANDROID__)

}  // namespace fplbase

#if !defined(FPLBASE_DISABLE_NEW_DELETE_OPERATORS)
// We use SIMD types in dynamically allocated objects and arrays.
MATHFU_DEFINE_GLOBAL_SIMD_AWARE_NEW_DELETE
#endif
