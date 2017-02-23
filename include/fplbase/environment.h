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

#ifndef FPLBASE_ENVIRONMENT_H
#define FPLBASE_ENVIRONMENT_H

#include <string>

#include "fplbase/config.h"  // Must come first.

#include "mathfu/glsl_mappings.h"

namespace fplbase {

// OpenGL ES feature level we are able to obtain.
enum FeatureLevel {
  kFeatureLevel20,  // 2.0: Our fallback.
  kFeatureLevel30,  // 3.0: We request this by default.
};

enum WindowMode {
  // Doesn't use all of the screen, typically not available on mobile. If used
  // on a device that has no windows (mobile), will do the same as
  // kWindowModeFullscreenNative.
  kWindowModeWindowedNative,
  // Doesn't use all of the screen, typically not available on mobile. If used
  // on a device that has no windows (mobile), it will do the same thing as
  // kWindowModeFullscreenScaled.
  kWindowModeWindowedScaled,
  // Uses all of the display at the native resolution of the device. Any size
  // supplied is ignored.
  kWindowModeFullscreenNative,
  // Uses all of the display, tries to scale from supplied size as best as
  // possible.
  kWindowModeFullscreenScaled,
};

// Any backend stores its data in an object inherited from this.
struct EnvironmentHandles {
  virtual ~EnvironmentHandles() {}
};

// An environment is responsible for managing the window context and rendering
// context (e.g. OpenGL context), if any.
class Environment {
 public:
  Environment()
    : feature_level_(kFeatureLevel20),
      window_size_(mathfu::vec2i(800, 600))  // Overwritten elsewhere.
  {}

  // The following functions are implemented differently for each rendering
  // backend.
  bool Initialize(const mathfu::vec2i &window_size, const char *window_title,
                  WindowMode window_mode = kWindowModeWindowedScaled);
  void ShutDown();
  void AdvanceFrame(bool minimized);

  // This is typically called by backends when they detect a size change.
  // Should typically be called in between frames to keep rendering consistent.
  void SetWindowSize(const mathfu::vec2i &window_size) {
    window_size_ = window_size;
  }

  mathfu::vec2i GetViewportSize() const;

  FeatureLevel feature_level() const { return feature_level_; }

  const mathfu::vec2i &window_size() const { return window_size_; }
  mathfu::vec2i &window_size() { return window_size_; }

  const std::string &last_error() const { return last_error_; }

 private:
  FeatureLevel feature_level_;
  mathfu::vec2i window_size_;
  std::string last_error_;
  std::unique_ptr<EnvironmentHandles> handles_;
};

#define LOOKUP_GL_FUNCTION(type, name, required, lookup_fn)        \
  union {                                                          \
    void *data;                                                    \
    type function;                                                 \
  } data_function_union_##name;                                    \
  data_function_union_##name.data = (void *)lookup_fn(#name);      \
  if (required && !data_function_union_##name.data) {              \
    last_error_ = "could not retrieve GL function pointer " #name; \
    return false;                                                  \
  }                                                                \
  name = data_function_union_##name.function;


}  // namespace fplbase

#endif  // FPLBASE_ENVIRONMENT_H
