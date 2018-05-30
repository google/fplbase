// Copyright 2015 Google Inc. All rights reserved.
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

// This is a wrapper around main, as SDL needs to modify the main function
// directly.  Users of FPLBase should declare "FPL_main" which this will call.

#include "SDL_main.h"

#ifdef __ANDROID__
#include <assert.h>
#include <jni.h>
#include <android/log.h>

#if !defined(FPLBASE_JNI_ONLOAD_FUNCTIONS)
#error FPLBASE_JNI_ONLOAD_FUNCTIONS should at least be configured to \
       call SDL_JNI_OnLoad.  For example: \
       -D'FPLBASE_JNI_ONLOAD_FUNCTIONS(X)=X(SDL_JNI_OnLoad)'
#endif  // !defined(FPLBASE_JNI_ONLOAD_FUNCTIONS)

// Used by FPLBASE_JNI_ONLOAD_FUNCTIONS to expand into a list of JNI_OnLoad
// style function declarations.
#define FPLBASE_JNI_ONLOAD_FUNCTION_DECLARATIONS(X) \
  extern "C" jint X(JavaVM* vm, void* reserved);
// Used by FPLBASE_JNI_ONLOAD_FUNCTIONS to expand into a comma separated list.
#define FPLBASE_JNI_ONLOAD_FUNCTION_TABLE(X) X,
// Used by FPLBASE_JNI_ONLOAD_FUNCTIONS to expand into a comma separated list
// of strings.
#define FPLBASE_JNI_ONLOAD_FUNCTION_STRINGS(X) #X,

#endif  // __ANDROID__

extern "C" int FPL_main(int argc, char* argv[]);

// main() is redefined as SDL_main() (see SDL_main.h) on some platforms.
int main(int argc, char* argv[]) { return FPL_main(argc, argv); }

// NOTE: The following code is included in this module to prevent the linker
// from stripping the JNI_OnLoad entry point from the application.  Since
// main() / SDL_main() will always be referenced this object should always be
// linked.
#if defined(__ANDROID__)
typedef jint (*JniOnLoadFunction)(JavaVM* vm, void* reserved);

FPLBASE_JNI_ONLOAD_FUNCTIONS(FPLBASE_JNI_ONLOAD_FUNCTION_DECLARATIONS)

#define FPLBASE_JNI_LOG 1
#if FPLBASE_JNI_LOG
#define FPLBASE_JNI_ONLOAD_LOG(...) \
  __android_log_print(ANDROID_LOG_INFO, __FUNCTION__, __VA_ARGS__)
#endif  // FPLBASE_JNI_LOG

// Hook the JNI entry point and call functions specified by the
// FPLBASE_JNI_ONLOAD_FUNCTIONS macro (defined in jni/Android.mk).
// To override the default list define FPLBASE_JNI_ON_LOAD_FUNCTIONS.
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
  static JniOnLoadFunction jni_on_load_function_table[] = {
      FPLBASE_JNI_ONLOAD_FUNCTIONS(FPLBASE_JNI_ONLOAD_FUNCTION_TABLE)};
#if FPLBASE_JNI_LOG
  static const char* jni_on_load_function_name[] = {
      FPLBASE_JNI_ONLOAD_FUNCTIONS(FPLBASE_JNI_ONLOAD_FUNCTION_STRINGS)};
#endif  // FPLBASE_JNI_LOG
  jint expected_jni_version = JNI_VERSION_1_4;
  for (int i = 0; i < sizeof(jni_on_load_function_table) /
                          sizeof(jni_on_load_function_table[0]);
       ++i) {
    FPLBASE_JNI_ONLOAD_LOG("Running %s()", jni_on_load_function_name[i]);
    jint jni_version = jni_on_load_function_table[i](vm, reserved);
    (void)jni_version;
    assert(jni_version == expected_jni_version);
  }
  return expected_jni_version;
}
#endif  // defined(__ANDROID__)
