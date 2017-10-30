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

#include <EGL/egl.h>

#include "fplbase/renderer.h"
#include "fplbase/utilities.h"
#include "precompiled.h"

#if defined(FPLBASE_BACKEND_SDL)
// Include SDL internal headers and external refs
#define TARGET_OS_IPHONE \
  1  // This one is not to turn on 'SDL_DYNAMIC_API' defitnition
extern "C" {
#include "src/core/android/SDL_android.h"
#include "src/video/SDL_egl_c.h"
#include "src/video/SDL_sysvideo.h"
}
// We don't need this anymore
#undef TARGET_OS_IPHONE
#endif  // defined(FPLBASE_BACKEND_SDL)

using mathfu::mat4;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;
using mathfu::vec4i;

namespace fplbase {

#ifdef FPLBASE_BACKEND_SDL
// Quick hack for HW scaler setting
static vec2i g_android_scaler_resolution;

void AndroidSetScalerResolution(const vec2i& resolution) {
  // Check against the real size of the device
  JNIEnv* env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
  jclass fpl_class = env->GetObjectClass(activity);
  jmethodID get_size = env->GetMethodID(fpl_class, "GetDisplaySize", "()[I");
  jintArray size = (jintArray)env->CallObjectMethod(activity, get_size);
  jint* size_ints = env->GetIntArrayElements(size, NULL);

  int width = std::min(size_ints[0], resolution.x);
  int height = std::min(size_ints[1], resolution.y);
  g_android_scaler_resolution = vec2i(width, height);

#if FPLBASE_ANDROID_VR
  // Update the underlying activity with the scaled resolution
  // TODO(wvo): Create FPLVRActivity that derives from FPLActivity and
  // implements this function and those in renderer_hmd.h (b/29940321).
  jmethodID set_resolution =
      env->GetMethodID(fpl_class, "SetHeadMountedDisplayResolution", "(II)V");
  env->CallVoidMethod(activity, set_resolution, width, height);
#endif  // FPLBASE_ANDROID_VR

  env->ReleaseIntArrayElements(size, size_ints, JNI_ABORT);
  env->DeleteLocalRef(size);
  env->DeleteLocalRef(fpl_class);
  env->DeleteLocalRef(activity);
}

const vec2i& AndroidGetScalerResolution() {
  return g_android_scaler_resolution;
}

EGLAPI EGLSurface EGLAPIENTRY
HookEglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                           EGLNativeWindowType win, const EGLint* attrib_list) {
  // Apply scaler setting
  ANativeWindow* window = Android_JNI_GetNativeWindow();
  ANativeWindow_setBuffersGeometry(window, g_android_scaler_resolution.x,
                                   g_android_scaler_resolution.y, 0);

  auto surface = eglCreateWindowSurface(dpy, config, win, attrib_list);
  // Check surface size if the HW scaler setting was successful.
  int32_t width;
  int32_t height;
  eglQuerySurface(dpy, surface, EGL_WIDTH, &width);
  eglQuerySurface(dpy, surface, EGL_HEIGHT, &height);
  if (width != g_android_scaler_resolution.x ||
      height != g_android_scaler_resolution.y) {
    LogError("Failed to initialize HW scaler.");
    // Reset scaler resolution.
    g_android_scaler_resolution.x = width;
    g_android_scaler_resolution.y = height;
  }
  return surface;
}

void AndroidPreCreateWindow() {
  // Apply scaler setting prior creating surface
  if (g_android_scaler_resolution.x && g_android_scaler_resolution.y) {
    // Initialize OpenGL function pointers inside SDL
    if (SDL_GL_LoadLibrary(NULL) < 0) {
      LogError(kError, "couldn't initialize OpenGL library: %s\n",
               SDL_GetError());
    }

    // Hook eglCreateWindowSurface call
    SDL_VideoDevice* device = SDL_GetVideoDevice();
    if (device) {
      device->egl_data->eglCreateWindowSurface = HookEglCreateWindowSurface;
    }
  }
}
#endif  // FPLBASE_BACKEND_SDL

int AndroidGetContextClientVersion() {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLContext context = eglGetCurrentContext();
  EGLint value = 0;
  eglQueryContext(display, context, EGL_CONTEXT_CLIENT_VERSION, &value);
  return value;
}

void AndroidInitGl3Functions() {
#if __ANDROID_API__ < 18
    gl3stubInit();
#endif
}

}  // namespace fplbase
