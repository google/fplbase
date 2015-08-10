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

#ifdef FPL_BASE_BACKEND_STDLIB
#define _CRT_SECURE_NO_DEPRECATE
#include <cstdio>
#include <fcntl.h>
#include <stdarg.h>
#if defined(__ANDROID__)
#include <android/log.h>
namespace {
static AAssetManager* g_asset_manager = nullptr;
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

#ifdef FPL_BASE_BACKEND_SDL
bool LoadFile(const char *filename, std::string *dest) {
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
bool LoadFile(const char* filename, std::string* dest) {
  if (!g_asset_manager) {
    LogError(kError,
             "Need to call SetAssetManager() once before calling LoadFile()");
    assert(false);
  }
  AAsset* asset =
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
bool LoadFile(const char *filename, std::string *dest) {
  FILE* fd = fopen(filename, "rb");
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
    if (comment >= 2) { // Skip comments.
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
  FILE* fd = fopen(filename, "wb");
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

bool SaveFile(const char *filename, const std::string &src) {
  return SaveFile(filename, static_cast<const void*>(src.c_str()),
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
  for (const char* c = camel; *c != '\0'; ++c) {
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
  const char* const camel_case_name = starts_with_k ? enum_name + 1 : enum_name;

  // Assemble file name.
  return std::string(prefix) + CamelCaseToSnakeCase(camel_case_name) +
         std::string(suffix);
}

#if defined(__ANDROID__) && defined(FPL_BASE_BACKEND_SDL)
bool AndroidSystemFeature(const char *feature_name) {
  JNIEnv* env = reinterpret_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
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
  JNIEnv* env = reinterpret_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
  jclass build_class = env->FindClass("android/os/Build");
  jfieldID model_id =
      env->GetStaticFieldID(build_class, "MODEL", "Ljava/lang/String;");
  jstring model_obj =
      static_cast<jstring>(env->GetStaticObjectField(build_class, model_id));
  const char* device_name = env->GetStringUTFChars(model_obj, 0);

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
  const char *device_list[] = {"Galaxy Nexus"};
  return AndroidCheckDeviceList(device_list,
                                sizeof(device_list) / sizeof(device_list[0]));
#else
  return true;
#endif
}

#ifdef FPL_BASE_BACKEND_SDL
void LogInfo(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt,
                  args);
  va_end(args);
}

void LogError(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, fmt,
                  args);
  va_end(args);
}

void LogInfo(LogCategory category, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  SDL_LogMessageV(category, SDL_LOG_PRIORITY_INFO, fmt, args);
  va_end(args);
}

void LogError(LogCategory category, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  SDL_LogMessageV(category, SDL_LOG_PRIORITY_ERROR, fmt, args);
  va_end(args);
}
#elif defined(FPL_BASE_BACKEND_STDLIB)
#if defined(__ANDROID__)
void LogInfo(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  __android_log_print(ANDROID_LOG_VERBOSE, "fplbase", fmt, args);
  va_end(args);
}

void LogError(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  __android_log_print(ANDROID_LOG_ERROR, "fplbase", fmt, args);
  va_end(args);
}

void LogInfo(LogCategory category, const char *fmt, ...) {
  (void)category;
  va_list args;
  va_start(args, fmt);
  LogInfo(fmt, args);
  va_end(args);
}

void LogError(LogCategory category, const char *fmt, ...) {
  (void)category;
  va_list args;
  va_start(args, fmt);
  LogError(fmt, args);
  va_end(args);
}
#else
void LogInfo(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  printf(fmt, args);
  va_end(args);
}

void LogError(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, fmt, args);
  va_end(args);
}

void LogInfo(LogCategory category, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  LogInfo(fmt, args);
  va_end(args);
}

void LogError(LogCategory category, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  LogError(fmt, args);
  va_end(args);
}
#endif
#else
#error Please define a backend implementation for LogXXX.
#endif

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
JNIEnv* AndroidGetJNIEnv() {
  return reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
}
#else
// TODO: Implement JNI methods that do not depend upon SDL.
#endif  // defined(FPL_BASE_BACKEND_SDL)
#endif

#if defined(__ANDROID__) && defined(FPL_BASE_BACKEND_STDLIB)
void SetAAssetManager(AAssetManager *manager) { g_asset_manager = manager; }
#endif

#ifdef FPL_BASE_BACKEND_SDL
WorldTime GetTicks() { return SDL_GetTicks(); }

void Delay(WorldTime time) { SDL_Delay(time); }
#elif defined(FPL_BASE_BACKEND_STDLIB)
WorldTime GetTicks() { return 0; }

void Delay(WorldTime time) {}
#else
#error Please define a backend implementation of a monotonic clock.
#endif

}  // namespace fpl
