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

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#include "fplbase/internal/renderer_ios.h"
#endif

using mathfu::mat4;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;

namespace fplbase {

// When building without SDL we assume the window and rendering context have
// already been created prior to calling initialize.
bool Environment::Initialize(const vec2i& /*window_size*/,
                             const char* /*window_title*/,
                             WindowMode /*window_mode*/) {
#if defined(_WIN32) && !defined(FPLBASE_GLES)
#define GLEXT(type, name, required) \
  LOOKUP_GL_FUNCTION(type, name, required, wglGetProcAddress)
  GLBASEEXTS GLEXTS
#undef GLEXT
#endif  // defined(_WIN32)

#ifndef PLATFORM_MOBILE
#ifdef GL_MAJOR_VERSION
  GLint version = 0;
  glGetIntegerv(GL_MAJOR_VERSION, &version);
  if (glGetError() == 0) {
    if (version >= 3) {
      feature_level_ = kFeatureLevel30;
    }
  }
#endif  // defined(GL_MAJOR_VERSION)
#endif  // !defined(PLATFORM_MOBILE)

#ifdef __ANDROID__
  const int version = AndroidGetContextClientVersion();
  if (version >= 3) {
    feature_level_ = kFeatureLevel30;
    AndroidInitGl3Functions();
  }

#ifdef FPLBASE_GLES
#define GLEXT(type, name, required) \
  LOOKUP_GL_FUNCTION(type, name, required, eglGetProcAddress)
  GLESEXTS
#undef GLEXT
#endif  // FPLBASE_GLES
#endif  // defined(__ANDROID__)

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
  const int version = IosGetContextClientVersion();
  assert(version >= 2);
  if (version >= 3) {
    feature_level_ = kFeatureLevel30;
  }
#endif  // TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
  return true;
}

void Environment::ShutDown() {
}

void Environment::AdvanceFrame(bool minimized) {
  (void)minimized;
}

vec2i Environment::GetViewportSize() const {
  return window_size();
}

}  // namespace fplbase
