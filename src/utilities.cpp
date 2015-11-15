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
#include "fplbase/utilities.h"

#if defined(__ANDROID__)
#include <string>
#endif  // defined(__ANDROID__)

#ifdef FPL_BASE_BACKEND_STDLIB
#define _CRT_SECURE_NO_DEPRECATE
#include <fcntl.h>
#include <stdarg.h>
#include <cstdio>

#if defined(__ANDROID__)
#ifdef FPL_BASE_BACKEND_SDL
#include "SDL_thread.h"
#endif
#include <android/log.h>
namespace {
static AAssetManager *g_asset_manager = nullptr;
}
#endif
#endif

namespace fpl {

#ifdef FPL_BASE_BACKEND_SDL
static_assert(kApplication ==
                  static_cast<LogCategory>(SDL_LOG_CATEGORY_APPLICATION),
              "update kApplication");
static_assert(kError == static_cast<LogCategory>(SDL_LOG_CATEGORY_ERROR),
              "update kError");
static_assert(kSystem == static_cast<LogCategory>(SDL_LOG_CATEGORY_SYSTEM),
              "update kSystem");
static_assert(kAudio == static_cast<LogCategory>(SDL_LOG_CATEGORY_AUDIO),
              "update kAudio");
static_assert(kVideo == static_cast<LogCategory>(SDL_LOG_CATEGORY_VIDEO),
              "update kVideo");
static_assert(kRender == static_cast<LogCategory>(SDL_LOG_CATEGORY_RENDER),
              "update kRender");
static_assert(kInput == static_cast<LogCategory>(SDL_LOG_CATEGORY_INPUT),
              "update kInput");
static_assert(kCustom == static_cast<LogCategory>(SDL_LOG_CATEGORY_CUSTOM),
              "update kCustom");
#endif  // FPL_BASE_BACKEND_SDL

// Function called by LoadFile().
static LoadFileFunction g_load_file_function = LoadFileRaw;

LoadFileFunction SetLoadFileFunction(LoadFileFunction load_file_function) {
  LoadFileFunction previous_function = g_load_file_function;
  g_load_file_function = load_file_function ? load_file_function : LoadFileRaw;
  return previous_function;
}

bool LoadFile(const char *filename, std::string *dest) {
  assert(g_load_file_function);
  return g_load_file_function(filename, dest);
}

#ifdef FPL_BASE_BACKEND_SDL
bool LoadFileRaw(const char *filename, std::string *dest) {
  auto handle = SDL_RWFromFile(filename, "rb");
  if (!handle) {
    LogError(kError, "LoadFile fail on %s", filename);
    return false;
  }
  auto len = static_cast<size_t>(SDL_RWseek(handle, 0, RW_SEEK_END));
  SDL_RWseek(handle, 0, RW_SEEK_SET);
  dest->assign(len, 0);
  size_t rlen = static_cast<size_t>(SDL_RWread(handle, &(*dest)[0], 1, len));
  SDL_RWclose(handle);
  return len == rlen && len > 0;
}
#elif defined(FPL_BASE_BACKEND_STDLIB)
#if defined(__ANDROID__)
bool LoadFileRaw(const char *filename, std::string *dest) {
  if (!g_asset_manager) {
    LogError(kError,
             "Need to call SetAssetManager() once before calling LoadFile()");
    assert(false);
  }
  AAsset *asset =
      AAssetManager_open(g_asset_manager, filename, AASSET_MODE_STREAMING);
  if (!asset) {
    LogError(kError, "LoadFile fail on %s", filename);
    return false;
  }
  off_t len = AAsset_getLength(asset);
  dest->assign(len, 0);
  int rlen = AAsset_read(asset, &(*dest)[0], len);
  AAsset_close(asset);
  return len == rlen && len > 0;
}
#else
bool LoadFileRaw(const char *filename, std::string *dest) {
  FILE *fd = fopen(filename, "rb");
  if (fd == NULL) {
    LogError(kError, "LoadFile fail on %s", filename);
    return false;
  }
  if (fseek(fd, 0, SEEK_END)) {
    fclose(fd);
    return false;
  }
  size_t len = ftell(fd);
  fseek(fd, 0, SEEK_SET);
  dest->assign(len, 0);
  size_t rlen = fread(&(*dest)[0], 1, len, fd);
  fclose(fd);
  return len == rlen && len > 0;
}
#endif
#else
#error Please define a backend implementation for LoadFile.
#endif

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

#ifdef FPL_BASE_BACKEND_SDL
bool LoadPreferences(const char *filename, std::string *dest) {
#if defined(__ANDROID__)
  // Use Android preference API to store blob as a Java String.
  JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
  jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity());
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
#elif defined(FPL_BASE_BACKEND_STDLIB)
bool LoadPreferences(const char *filename, std::string *dest) {
  return LoadFile(filename, dest);
}
#endif

int32_t LoadPreference(const char *key, int32_t initial_value) {
#ifdef __ANDROID__
  // Use Android preference API to store an integer value.
  JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
  jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity());
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

bool LoadFileWithIncludesHelper(const char *filename, std::string *dest,
                                std::string *failedfilename,
                                std::set<std::string> *all_includes) {
  if (!LoadFile(filename, dest)) {
    *failedfilename = filename;
    return false;
  }
  all_includes->insert(filename);
  std::vector<std::string> includes;
  auto cursor = dest->c_str();
  static auto kIncludeStatement = "#include";
  auto include_len = strlen(kIncludeStatement);
  // Parse only lines that are include statements, terminating at the first
  // one that's not.
  for (;;) {
    auto start = cursor;
    cursor += strspn(cursor, " \t");  // Skip whitespace.
    auto empty = strspn(cursor, "\n\r");
    if (empty) {  // Skip empty lines.
      cursor += empty;
      continue;
    }
    auto comment = strspn(cursor, "/");
    if (comment >= 2) {  // Skip comments.
      cursor += comment;
      cursor += strcspn(cursor, "\n\r");  // Skip all except newline;
      cursor += strspn(cursor, "\n\r");   // Skip newline;
      continue;
    }
    if (strncmp(cursor, kIncludeStatement, include_len) == 0) {
      cursor += include_len;
      cursor += strspn(cursor, " \t");  // Skip whitespace.
      if (*cursor == '\"') {            // Must find quote.
        cursor++;
        auto len = strcspn(cursor, "\"\n\r");  // Filename part.
        if (cursor[len] == '\"') {             // Ending quote.
          includes.push_back(std::string(cursor, len));
          cursor += len + 1;
          cursor += strspn(cursor, "\n\r \t");  // Skip whitespace and newline.
          continue;
        }
      }
    }
    // This include statement didn't parse correctly, leave it untouched.
    cursor = start;
    break;
  }
  // Early out for files with no includes.
  if (!includes.size()) return true;
  // Remove the #includes from the final file.
  dest->erase(0, cursor - dest->c_str());
  // Now insert the includes.
  std::string include;
  size_t insertion_point = 0;
  for (auto it = includes.begin(); it != includes.end(); ++it) {
    if (all_includes->find(*it) == all_includes->end()) {
      if (!LoadFileWithIncludesHelper(it->c_str(), &include, failedfilename,
                                      all_includes)) {
        return false;
      }
      dest->insert(insertion_point, include);
      insertion_point += include.length();
      // Ensure there's a linefeed at eof.
      if (include.size() && include.back() != '\n') {
        dest->insert(insertion_point++, "\n");
      }
    }
  }
  return true;
}

bool LoadFileWithIncludes(const char *filename, std::string *dest,
                          std::string *failedfilename) {
  std::set<std::string> all_includes;
  return LoadFileWithIncludesHelper(filename, dest, failedfilename,
                                    &all_includes);
}

#if defined(FPL_BASE_BACKEND_SDL)
bool SaveFile(const char *filename, const void *data, size_t size) {
  auto handle = SDL_RWFromFile(filename, "wb");
  if (!handle) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SaveFile fail on %s", filename);
    return false;
  }
  size_t wlen = static_cast<size_t>(SDL_RWwrite(handle, data, 1, size));
  SDL_RWclose(handle);
  return (wlen == size);
}
#elif defined(FPL_BASE_BACKEND_STDLIB)
#if defined(__ANDROID__)
bool SaveFile(const char *filename, const void *data, size_t size) {
  (void)filename;
  (void)data;
  (void)size;
  LogError(kError, "SaveFile unimplemented on STDLIB on ANDROID.");
  return false;
}
#else
bool SaveFile(const char *filename, const void *data, size_t size) {
  FILE *fd = fopen(filename, "wb");
  if (fd < 0) {
    LogError(kError, "SaveFile fail on %s", filename);
    return false;
  }
  size_t wlen = fwrite(data, 1, size, fd);
  fclose(fd);
  return size == wlen && size > 0;
}
#endif
#else
#error Please define a backend implementation for SaveFile.
#endif

#if defined(FPL_BASE_BACKEND_SDL)
bool SavePreferences(const char *filename, const void *data, size_t size) {
#if defined(__ANDROID__)
  // Use Android preference API to store blob as a Java String.
  JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
  jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity());
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
  env->CallObjectMethod(editor, put_string, key, blob);

  // Commit a change.
  jmethodID commit = env->GetMethodID(editor_class, "commit", "()Z");
  jboolean ret = env->CallBooleanMethod(editor, commit);
  LogInfo("Saved %d bytes to the preference, %d", size, ret);

  // Release objects references.
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
#else
bool SavePreferences(const char *filename, const void *data, size_t size) {
  return SaveFile(filename, data, size);
}
#endif

bool SavePreference(const char *key, int32_t value) {
#ifdef __ANDROID__
  // Use Android preference API to store an integer value.
  JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
  jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity());
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
  env->CallObjectMethod(editor, put_int, pref_key, value);

  // Commit a change.
  jmethodID commit = env->GetMethodID(editor_class, "commit", "()Z");
  jboolean ret = env->CallBooleanMethod(editor, commit);

  // Release objects references.
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
#endif
}

bool SaveFile(const char *filename, const std::string &src) {
  return SaveFile(filename, static_cast<const void *>(src.c_str()),
                  src.length());  // don't include the '\0'
}

#if defined(_WIN32)
inline char *getcwd(char *buffer, int maxlen) {
  return _getcwd(buffer, maxlen);
}

inline int chdir(const char *dirname) { return _chdir(dirname); }
#endif  // defined(_WIN32)

// Search up the directory tree from binary_dir for target_dir, changing the
// working directory to the target_dir and returning true if it's found,
// false otherwise.
bool ChangeToUpstreamDir(const char *const binary_dir,
                         const char *const target_dir) {
#if !defined(__ANDROID__)
  {
    std::string current_dir = binary_dir;

    // Search up the tree from the directory containing the binary searching
    // for target_dir.
    for (;;) {
      size_t separator = current_dir.find_last_of(flatbuffers::kPathSeparator);
      if (separator == std::string::npos) break;
      current_dir = current_dir.substr(0, separator);
      int success = chdir(current_dir.c_str());
      if (success) break;
      char real_path[256];
      current_dir = getcwd(real_path, sizeof(real_path));
      std::string target = current_dir +
                           std::string(1, flatbuffers::kPathSeparator) +
                           std::string(target_dir);
      success = chdir(target.c_str());
      if (success == 0) return true;
    }
    return false;
  }
#else
  (void)binary_dir;
  (void)target_dir;
  return true;
#endif  //  !defined(__ANDROID__)
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

#if defined(__ANDROID__) && defined(FPL_BASE_BACKEND_SDL)
bool AndroidSystemFeature(const char *feature_name) {
  JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
  jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity());
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
}
#endif

#if defined(__ANDROID__) && defined(FPL_BASE_BACKEND_SDL)
int32_t AndroidGetAPILevel() {
  // Retrieve API level through JNI.
  JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
  jclass build_class = env->FindClass("android/os/Build$VERSION");
  jfieldID apilevel_id = env->GetStaticFieldID(build_class, "SDK_INT", "I");
  jint apilevel = env->GetStaticIntField(build_class, apilevel_id);

  // Clean up
  env->DeleteLocalRef(build_class);
  return apilevel;
}
#endif

bool TouchScreenDevice() {
#if defined(__ANDROID__) && defined(FPL_BASE_BACKEND_SDL)
  return AndroidSystemFeature("android.hardware.touchscreen");
#else
  return false;
#endif
}

#if defined(__ANDROID__) && defined(FPL_BASE_BACKEND_SDL)
bool AndroidCheckDeviceList(const char *device_list[], const int num_devices) {
  // Retrieve device name through JNI.
  JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
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
#if defined(__ANDROID__) && defined(FPL_BASE_BACKEND_SDL)
  static const char *device_list[] = {"Galaxy Nexus", "Nexus S", "Nexus S 4G"};
  static bool supported = AndroidCheckDeviceList(
      device_list, sizeof(device_list) / sizeof(device_list[0]));
  return supported;
#else
  return true;
#endif
}

int32_t GetSystemRamSize() {
#if defined(FPL_BASE_BACKEND_SDL)
  return SDL_GetSystemRAM();
#else
  return 0;
#endif
}

#ifdef FPL_BASE_BACKEND_SDL
void LogInfo(LogCategory category, const char *fmt, va_list args) {
  SDL_LogMessageV(category, SDL_LOG_PRIORITY_INFO, fmt, args);
}

void LogError(LogCategory category, const char *fmt, va_list args) {
  SDL_LogMessageV(category, SDL_LOG_PRIORITY_ERROR, fmt, args);
}

void LogInfo(const char *fmt, va_list args) {
  LogInfo(kApplication, fmt, args);
}

void LogError(const char *fmt, va_list args) {
  LogError(kApplication, fmt, args);
}

#elif defined(FPL_BASE_BACKEND_STDLIB)
#if defined(__ANDROID__)
void LogInfo(LogCategory category, const char *fmt, va_list args) {
  (void)category;
  LogInfo(fmt, args);
}

void LogError(LogCategory category, const char *fmt, va_list args) {
  (void)category;
  LogError(fmt, args);
}

void LogInfo(const char *fmt, va_list args) {
  __android_log_vprint(ANDROID_LOG_VERBOSE, "fplbase", fmt, args);
}

void LogError(const char *fmt, va_list args) {
  __android_log_vprint(ANDROID_LOG_ERROR, "fplbase", fmt, args);
}

#else
void LogInfo(LogCategory category, const char *fmt, va_list args) {
  (void)category;
  LogInfo(fmt, args);
}

void LogError(LogCategory category, const char *fmt, va_list args) {
  (void)category;
  LogError(fmt, args);
}

void LogInfo(const char *fmt, va_list args) {
  vprintf(fmt, args);
  printf("\n");
}

void LogError(const char *fmt, va_list args) {
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
}
#endif
#else
#error Please define a backend implementation for LogXXX.
#endif
void LogInfo(LogCategory category, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  LogInfo(category, fmt, args);
  va_end(args);
}

void LogError(LogCategory category, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  LogError(category, fmt, args);
  va_end(args);
}

void LogInfo(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  LogInfo(fmt, args);
  va_end(args);
}

void LogError(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  LogError(fmt, args);
  va_end(args);
}

#if defined(__ANDROID__)
#if defined(FPL_BASE_BACKEND_SDL)
// This function always returns a pointer to a jobject, but we are returning a
// void* for the same reason SDL does - to avoid having to include the jni
// libraries in this library.  Anything calling this will probably want to
// static cast the return value into a jobject*.
jobject AndroidGetActivity() {
  return reinterpret_cast<jobject>(SDL_AndroidGetActivity());
}

// This function always returns a pointer to a JNIEnv, but we are returning a
// void* for the same reason SDL does - to avoid having to include the jni
// libraries in this library.  Anything calling this will probably want to
// static cast the return value into a JNIEnv*.
JNIEnv *AndroidGetJNIEnv() {
  return reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
}
#else
// TODO: Implement JNI methods that do not depend upon SDL.
#endif  // defined(FPL_BASE_BACKEND_SDL)
#endif

#if defined(__ANDROID__) && defined(FPL_BASE_BACKEND_STDLIB)
void SetAAssetManager(AAssetManager *manager) { g_asset_manager = manager; }
#endif

#if defined(FPL_BASE_BACKEND_SDL)
bool GetStoragePath(const char *app_name, std::string *path_string) {
#if defined(__ANDROID__)
  auto path = SDL_AndroidGetInternalStoragePath();
#else
  auto path = SDL_GetPrefPath("FPLBase", app_name);
#endif
  if (path == nullptr) {
    return false;
  }
  *path_string = path;
  return true;
}
#elif defined(FPL_BASE_BACKEND_STDLIB)
bool GetStoragePath(const char *app_name, std::string *path_string) {
  (void)app_name;
  *path_string = "/";
  return true;
}
#else
#error Please define a backend implementation for SaveFile.
#endif

#ifdef __ANDROID__
VsyncCallback g_vsync_callback = nullptr;

VsyncCallback RegisterVsyncCallback(VsyncCallback callback) {
  VsyncCallback old_callback = g_vsync_callback;
  g_vsync_callback = callback;
  return old_callback;
}

#ifdef FPL_BASE_BACKEND_SDL
// Mutexes and ConditionVariables used for vsync synchonization:
SDL_mutex *frame_id_mutex;
SDL_mutex *vsync_cv_mutex;
SDL_cond *android_vsync_cv;
int vsync_frame_id = 0;
#endif  // FPL_BASE_BACKEND_SDL

static void InitVsyncMutexes() {
#ifdef FPL_BASE_BACKEND_SDL
  frame_id_mutex = SDL_CreateMutex();
  vsync_cv_mutex = SDL_CreateMutex();
  android_vsync_cv = SDL_CreateCond();
  vsync_frame_id = 0;
#endif  // FPL_BASE_BACKEND_SDL
}

static void CleanupVsyncMutexes() {
#ifdef FPL_BASE_BACKEND_SDL
  SDL_DestroyMutex(frame_id_mutex);
  SDL_DestroyMutex(vsync_cv_mutex);
  SDL_DestroyCond(android_vsync_cv);
#endif  // FPL_BASE_BACKEND_SDL
}

// Initialize the Vsync mutexes.  Called by android lifecycle events.
extern "C" JNIEXPORT void JNICALL
Java_com_google_fpl_fplbase_FPLActivity_nativeInitVsync(JNIEnv *env,
                                                        jobject thiz,
                                                        jobject activity) {
  (void)env;
  (void)thiz;
  (void)activity;
  InitVsyncMutexes();
}

// Clean up the Vsync mutexes.  Called by android lifecycle events.
extern "C" JNIEXPORT void JNICALL
Java_com_google_fpl_fplbase_FPLActivity_nativeCleanupVsync(JNIEnv *env,
                                                           jobject thiz,
                                                           jobject activity) {
  (void)env;
  (void)thiz;
  (void)activity;
  CleanupVsyncMutexes();
}

// Blocks until the next time a VSync event occurs.
void WaitForVsync() {
#ifdef FPL_BASE_BACKEND_SDL
  SDL_LockMutex(frame_id_mutex);
  int starting_id = vsync_frame_id;
  SDL_UnlockMutex(frame_id_mutex);

  SDL_LockMutex(vsync_cv_mutex);
  // CondWait will normally only wake up when we receive an actual vsync
  // event, but it's not *guaranteed* not to wake up other times as well.
  // It is generally good practice to verify that your wake condition has
  // been satisfied before actually moving on to do work.
  while (starting_id == vsync_frame_id) {
    SDL_CondWait(android_vsync_cv, vsync_cv_mutex);
  }
  SDL_UnlockMutex(vsync_cv_mutex);
#else  // FPL_BASE_BACKEND_SDL
// TODO: Write STDLIB version
#endif
}

// Receive native vsync updates from the choreographer, and use them to
// signal starting a frame update and render.
// Note that this callback is signaled from another thread, and so
// needs to be thread-safe.
extern "C" JNIEXPORT void JNICALL
Java_com_google_fpl_fplbase_FPLActivity_nativeOnVsync(JNIEnv *env, jobject thiz,
                                                      jobject activity) {
  (void)env;
  (void)thiz;
  (void)activity;
  if (g_vsync_callback != nullptr) {
    g_vsync_callback();
  }
#ifdef FPL_BASE_BACKEND_SDL
  SDL_LockMutex(frame_id_mutex);
  vsync_frame_id++;
  SDL_UnlockMutex(frame_id_mutex);
  SDL_CondBroadcast(android_vsync_cv);
#endif  // FPL_BASE_BACKEND_SDL
}

int GetVsyncFrameId() {
#ifdef FPL_BASE_BACKEND_SDL

  SDL_LockMutex(frame_id_mutex);
  int return_value = vsync_frame_id;
  SDL_UnlockMutex(frame_id_mutex);
  return return_value;
#else  // FPL_BASE_BACKEND_SDL
  // TODO: Write STDLIB version
  return 0;
#endif
}

#endif  // __ANDROID__

// Checks whether Head Mounted Displays are supported by the system.
bool SupportsHeadMountedDisplay() {
#ifdef __ANDROID__
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
#endif  // __ANDROID
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
      (jclass)env->CallObjectMethod(activity, get_class_method);
  jclass activity_class_object_class =
      env->GetObjectClass(activity_class_object);
  jmethodID get_name_method = env->GetMethodID(
      activity_class_object_class, "getName", "()Ljava/lang/String;");
  jstring class_name_object =
      (jstring)env->CallObjectMethod(activity_class_object, get_name_method);
  char *class_name =
      (char *)env->GetStringUTFChars(class_name_object, JNI_FALSE);
  std::string activity_name(class_name);
  env->ReleaseStringUTFChars(class_name_object, class_name);
  env->DeleteLocalRef(class_name_object);
  env->DeleteLocalRef(activity_class_object_class);
  env->DeleteLocalRef(activity_class_object);
  env->DeleteLocalRef(activity_class);
  env->DeleteLocalRef(activity);
  return activity_name;
}
#endif  // defined(__ANDROID__)

#ifdef __ANDROID__
// Sends a keypress event to the android system.  This will show up in android
// indistinguishable from a normal user key press
void SendKeypressEventToAndroid(int android_key_code) {
  JNIEnv *env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
  jclass fpl_class = env->GetObjectClass(activity);
  jmethodID method_id =
      env->GetMethodID(fpl_class, "SendKeypressEventToAndroid", "(I)V");
  env->CallVoidMethod(activity, method_id, android_key_code);
  env->DeleteLocalRef(fpl_class);
  env->DeleteLocalRef(activity);
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

void RelaunchApplication() {
  JNIEnv *env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
  jclass fpl_class = env->GetObjectClass(activity);

  jmethodID mid_relaunch = env->GetMethodID(fpl_class, "relaunch", "()V");
  env->CallVoidMethod(activity, mid_relaunch);

  env->DeleteLocalRef(fpl_class);
  env->DeleteLocalRef(activity);
}
#endif  // __ANDROID

static PerformanceMode performance_mode = kNormalPerformance;

// Sets the performance mode.
void SetPerformanceMode(PerformanceMode new_mode) {
  performance_mode = new_mode;
}

PerformanceMode GetPerformanceMode() { return performance_mode; }

}  // namespace fpl
