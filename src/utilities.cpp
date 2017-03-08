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
// clang-format on

#if defined(__ANDROID__)
#include <string>
#endif  // defined(__ANDROID__)

// Header files for mmap API.
#ifdef _WIN32
#else
// Platforms with POSIX
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#endif  // _WIN32

#if defined(FPLBASE_BACKEND_STDLIB)
#if !defined(_CRT_SECURE_NO_DEPRECATE)
#define _CRT_SECURE_NO_DEPRECATE
#endif
#include <fcntl.h>
#include <stdarg.h>
#include <cstdio>

#if defined(__ANDROID__)
#if defined(FPLBASE_BACKEND_SDL)
#include "SDL_thread.h"
#endif  // defined(FPLBASE_BACKEND_SDL)
#include <android/log.h>
namespace {
static AAssetManager *g_asset_manager = nullptr;
}
#endif  // defined(__ANDROID__)

// clang-format off
#if defined(__APPLE__)
#include "TargetConditionals.h"
#include <CoreFoundation/CoreFoundation.h>
#endif  // defined(__APPLE__)
// clang-format on

#endif  // defined(FPLBASE_BACKEND_STDLIB)

namespace fplbase {

#ifdef FPLBASE_BACKEND_SDL
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
#endif  // FPLBASE_BACKEND_SDL

// Function called by LoadFile().
static LoadFileFunction g_load_file_function = LoadFileRaw;

LoadFileFunction SetLoadFileFunction(LoadFileFunction load_file_function) {
  LoadFileFunction previous_function = g_load_file_function;
  if (load_file_function) {
    g_load_file_function = load_file_function;
  } else {
    g_load_file_function = LoadFileRaw;
  }
  return previous_function;
}

bool LoadFile(const char *filename, std::string *dest) {
  assert(g_load_file_function);
  return g_load_file_function(filename, dest);
}

#ifdef FPLBASE_BACKEND_SDL
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
#elif defined(FPLBASE_BACKEND_STDLIB)
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

#ifdef FPLBASE_BACKEND_SDL
bool LoadPreferences(const char *filename, std::string *dest) {
#if defined(__ANDROID__)
  // Use Android preference API to store blob as a Java String.
  JNIEnv *env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
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
#elif defined(FPLBASE_BACKEND_STDLIB)
bool LoadPreferences(const char *filename, std::string *dest) {
  return LoadFile(filename, dest);
}
#endif

#if defined(FPLBASE_BACKEND_SDL)
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
#endif

#if defined(FPLBASE_BACKEND_SDL)
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
#elif defined(FPLBASE_BACKEND_STDLIB)
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
  if (fd == NULL) {
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

#if defined(FPLBASE_BACKEND_SDL)
bool SavePreferences(const char *filename, const void *data, size_t size) {
#if defined(__ANDROID__)
  // Use Android preference API to store blob as a Java String.
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
#else
bool SavePreferences(const char *filename, const void *data, size_t size) {
  return SaveFile(filename, data, size);
}
#endif

#if defined(FPLBASE_BACKEND_SDL)
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
#endif

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
  std::string target_dir_str(target_dir);

#if defined(__APPLE__) && defined(FPLBASE_BACKEND_STDLIB)
  (void)binary_dir;
  // Get the root of the target directory from the Bundle instead of using the directory
  // specified by the client.
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
#elif !defined(__ANDROID__)
  {
    std::string current_dir = binary_dir;
    const std::string separator_str(1, flatbuffers::kPathSeparator);

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
      std::string target = current_dir + separator_str + target_dir_str;
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

#if defined(__ANDROID__) && defined(FPLBASE_BACKEND_SDL)
bool AndroidSystemFeature(const char *feature_name) {
  JNIEnv *env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
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

bool TouchScreenDevice() {
#if defined(__ANDROID__) && defined(FPLBASE_BACKEND_SDL)
  return AndroidSystemFeature("android.hardware.touchscreen");
#else
  return false;
#endif
}

#if defined(__ANDROID__) && defined(FPLBASE_BACKEND_SDL)
bool AndroidCheckDeviceList(const char *device_list[], const int num_devices) {
  // Retrieve device name through JNI.
  JNIEnv *env = AndroidGetJNIEnv();
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
#if defined(__ANDROID__) && defined(FPLBASE_BACKEND_SDL)
  static const char *device_list[] = {"Galaxy Nexus", "Nexus S", "Nexus S 4G"};
  static bool supported = AndroidCheckDeviceList(
      device_list, sizeof(device_list) / sizeof(device_list[0]));
  return supported;
#else
  return true;
#endif
}

int32_t GetSystemRamSize() {
#if defined(FPLBASE_BACKEND_SDL)
  return SDL_GetSystemRAM();
#else
  return 0;
#endif
}

#ifdef FPLBASE_BACKEND_SDL
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

#elif defined(FPLBASE_BACKEND_STDLIB)
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
#if defined(FPLBASE_BACKEND_SDL)
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
static JavaVM* g_jvm = NULL;
static jint g_jni_version = 0;
static pthread_key_t g_pthread_key;

static JavaVM* GetJVM() {
  return g_jvm;
}

static JNIEnv* AttachCurrentThread() {
  JNIEnv* env = NULL;
  JavaVM* jvm = GetJVM();
  if (!jvm)
    return NULL;

  // Avoid attaching threads provided by the JVM.
  if (jvm->GetEnv(reinterpret_cast<void**>(&env), g_jni_version) == JNI_OK)
    return env;

  if (!(env = static_cast<JNIEnv*>(pthread_getspecific(g_pthread_key)))) {
    jint ret = jvm->AttachCurrentThread(&env, NULL);
    if (ret != JNI_OK)
      return NULL;

    pthread_setspecific(g_pthread_key, env);
  }

  return env;
}

static void DetachCurrentThread() {
  JavaVM* jvm = GetJVM();
  if (jvm) {
    jvm->DetachCurrentThread();
  }
}

static void DetachCurrentThreadWrapper(void* value) {
  JNIEnv* env = reinterpret_cast<JNIEnv*>(value);
  if (env) {
    DetachCurrentThread();
    pthread_setspecific(g_pthread_key, NULL);
  }
}

void AndroidSetJavaVM(JavaVM* vm, jint jni_version) {
  g_jvm = vm;
  g_jni_version = jni_version;

  pthread_key_create(&g_pthread_key, DetachCurrentThreadWrapper);
}

JNIEnv* AndroidGetJNIEnv() {
  return AttachCurrentThread();
}
#endif  // defined(FPLBASE_BACKEND_SDL)
#endif

#if defined(__ANDROID__) && defined(FPLBASE_BACKEND_STDLIB)
void SetAAssetManager(AAssetManager *manager) { g_asset_manager = manager; }
#endif

#if defined(FPLBASE_BACKEND_SDL)
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
#elif defined(FPLBASE_BACKEND_STDLIB)
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

#ifdef FPLBASE_BACKEND_SDL
// Mutexes and ConditionVariables used for vsync synchonization:
SDL_mutex *frame_id_mutex;
SDL_mutex *vsync_cv_mutex;
SDL_cond *android_vsync_cv;
int vsync_frame_id = 0;
#endif  // FPLBASE_BACKEND_SDL

static void InitVsyncMutexes() {
#ifdef FPLBASE_BACKEND_SDL
  frame_id_mutex = SDL_CreateMutex();
  vsync_cv_mutex = SDL_CreateMutex();
  android_vsync_cv = SDL_CreateCond();
  vsync_frame_id = 0;
#endif  // FPLBASE_BACKEND_SDL
}

static void CleanupVsyncMutexes() {
#ifdef FPLBASE_BACKEND_SDL
  SDL_DestroyMutex(frame_id_mutex);
  SDL_DestroyMutex(vsync_cv_mutex);
  SDL_DestroyCond(android_vsync_cv);
#endif  // FPLBASE_BACKEND_SDL
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
#ifdef FPLBASE_BACKEND_SDL
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
#else  // FPLBASE_BACKEND_SDL
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
#ifdef FPLBASE_BACKEND_SDL
  SDL_LockMutex(frame_id_mutex);
  vsync_frame_id++;
  SDL_UnlockMutex(frame_id_mutex);
  SDL_CondBroadcast(android_vsync_cv);
#endif  // FPLBASE_BACKEND_SDL
}

int GetVsyncFrameId() {
#ifdef FPLBASE_BACKEND_SDL

  SDL_LockMutex(frame_id_mutex);
  int return_value = vsync_frame_id;
  SDL_UnlockMutex(frame_id_mutex);
  return return_value;
#else  // FPLBASE_BACKEND_SDL
  // TODO: Write STDLIB version
  return 0;
#endif
}

#endif  // __ANDROID__

#if defined(FPLBASE_BACKEND_SDL)
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
#endif

#if defined(FPLBASE_BACKEND_SDL)
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
#endif

#if defined(__ANDROID__) && defined(FPLBASE_BACKEND_SDL)
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
#endif  // defined(__ANDROID__) && defined(FPLBASE_BACKEND_SDL)

#if defined(__ANDROID__) && defined(FPLBASE_BACKEND_SDL)
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
#endif  // defined(__ANDROID__) && defined(FPLBASE_BACKEND_SDL)

#ifdef __ANDROID__
// Sends a keypress event to the android system.  This will show up in android
// indistinguishable from a normal user key press
void SendKeypressEventToAndroid(int android_key_code) {
#if defined(FPLBASE_BACKEND_SDL)
  JNIEnv *env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
  jclass fpl_class = env->GetObjectClass(activity);
  jmethodID method_id =
      env->GetMethodID(fpl_class, "SendKeypressEventToAndroid", "(I)V");
  env->CallVoidMethod(activity, method_id, android_key_code);
  env->DeleteLocalRef(fpl_class);
  env->DeleteLocalRef(activity);
#else
  (void)android_key_code;
#endif
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

#if defined(__ANDROID__) && defined(FPLBASE_BACKEND_SDL)
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
#endif  // defined(__ANDROID__) && defined(FPLBASE_BACKEND_SDL)

}  // namespace fplbase

#if !defined(FPLBASE_DISABLE_NEW_DELETE_OPERATORS)
// We use SIMD types in dynamically allocated objects and arrays.
MATHFU_DEFINE_GLOBAL_SIMD_AWARE_NEW_DELETE
#endif
