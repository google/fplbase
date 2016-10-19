// Copyright 2016 Google Inc. All rights reserved.
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

#include "precompiled.h"  // NOLINT

#include "fplbase/environment.h"

#ifdef __ANDROID__
#include "fplbase/renderer_android.h"
#endif


using mathfu::mat4;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;

namespace fplbase {

struct SDLHandles : EnvironmentHandles {
  SDLHandles(SDL_Window *window, SDL_GLContext context)
    : window_(window), context_(context) {}
  ~SDLHandles() {}

  SDL_Window *window_;
  SDL_GLContext context_;
};

bool Environment::Initialize(const vec2i &window_size,
                             const char *window_title) {
  // Basic SDL initialization, does not actually initialize a Window or OpenGL,
  // typically should not fail.
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO)) {
    last_error_ = std::string("SDL_Init fail: ") + SDL_GetError();
    return false;
  }

  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);

#ifdef __ANDROID__
  // Setup HW scaler in Android
  AndroidSetScalerResolution(window_size);
  AndroidPreCreateWindow();
#endif

  // Always double buffer.
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  // Set back buffer format to 565
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);

  // Create the window:
  auto window = SDL_CreateWindow(
      window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      window_size.x(), window_size.y(), SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN |
#ifdef PLATFORM_MOBILE
                                            SDL_WINDOW_BORDERLESS);
#else
                                            SDL_WINDOW_RESIZABLE);
#endif
  if (!window) {
    last_error_ = std::string("SDL_CreateWindow fail: ") + SDL_GetError();
    return false;
  }

  // Get the size we actually got, which typically is native res for
  // any fullscreen display:
  SDL_GetWindowSize(window, &window_size_.x(), &window_size_.y());

  // Create the OpenGL context:
  // Try to get OpenGL ES 3 on mobile.
  // On desktop, we assume we can get function pointers for all ES 3 equivalent
  // functions.
  feature_level_ = kFeatureLevel30;
#ifdef PLATFORM_MOBILE
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
#ifdef __APPLE__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#endif
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                      SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif
  auto context = SDL_GL_CreateContext(window);
#ifdef PLATFORM_MOBILE
  if (context_) {
#ifdef __ANDROID__
    AndroidInitGl3Functions();
#endif
  } else {
    // Failed to get ES 3.0 context, let's try 2.0.
    feature_level_ = kFeatureLevel20;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    context_ = SDL_GL_CreateContext(window);
  }
#endif
  if (!context) {
    last_error_ = std::string("SDL_GL_CreateContext fail: ") + SDL_GetError();
    return false;
  }


  handles_.reset(new SDLHandles(window, context));

#ifdef PLATFORM_MOBILE
  LogInfo("FPLBase: got OpenGL ES context level %s",
          feature_level_ == kFeatureLevel20 ? "2.0" : "3.0");
#endif

// Enable Vsync on desktop
#ifndef PLATFORM_MOBILE
  SDL_GL_SetSwapInterval(1);
#endif

#if !defined(PLATFORM_MOBILE) && !defined(__APPLE__)
#define GLEXT(type, name, required) \
  LOOKUP_GL_FUNCTION(type, name, required, SDL_GL_GetProcAddress)
  GLBASEEXTS GLEXTS
#undef GLEXT
#endif

#ifdef PLATFORM_MOBILE
#define GLEXT(type, name, required) \
  LOOKUP_GL_FUNCTION(type, name, required, SDL_GL_GetProcAddress)
  GLESEXTS
#undef GLEXT
#endif  // PLATFORM_MOBILE

  return true;
}

void Environment::ShutDown() {
  auto handles = static_cast<SDLHandles *>(handles_.get());
  if (handles) {
    SDL_GL_DeleteContext(handles->context_);
    SDL_DestroyWindow(handles->window_);
    handles = nullptr;
  }
  SDL_Quit();
}

void Environment::AdvanceFrame(bool minimized) {
  auto handles = static_cast<SDLHandles *>(handles_.get());
  if (!handles) return;

  if (minimized) {
    // Save some cpu / battery:
    SDL_Delay(10);
  } else {
    SDL_GL_SwapWindow(handles->window_);
  }
  // Get window size again, just in case it has changed.
  SDL_GetWindowSize(handles->window_, &window_size_.x(), &window_size_.y());
}

vec2i Environment::GetViewportSize() const {
#if defined(__ANDROID__)
  // Check HW scaler setting and change a viewport size if they are set.
  vec2i scaled_size = fplbase::AndroidGetScalerResolution();
  if (scaled_size.x() && scaled_size.y()) return scaled_size;
#endif
  return window_size();
}

}
