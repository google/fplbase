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

#ifndef FPLBASE_DEBUG_MARKERS_H
#define FPLBASE_DEBUG_MARKERS_H

/// @file fplbase/debug_markers.h
/// @brief Functions for inserting GL debug markers.
///
/// To enable, \#define FPLBASE_ENABLE_DEBUG_MARKERS

#include "fplbase/config.h"  // Must come first.
#include "fplbase/glplatform.h"

#if defined(FPLBASE_GLES) || defined(PLATFORM_OSX)
#define FPLBASE_DEBUG_MARKER_NOLENGTH 0
#else
#define FPLBASE_DEBUG_MARKER_NOLENGTH -1
#endif

inline void PushDebugMarker(const char *marker,
                            int length = FPLBASE_DEBUG_MARKER_NOLENGTH) {
  (void)marker;
  (void)length;
#ifdef FPLBASE_ENABLE_DEBUG_MARKERS
#if defined(FPLBASE_GLES) || defined(PLATFORM_OSX)
// TODO(jsanmiya): Get glPushGroupMarker working for all iOS builds
//   if (glPushGroupMarker) {
//     GL_CALL(glPushGroupMarker(length, marker));
//   }
#else
//   if (glPushDebugGroup) {
//     GL_CALL(glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 1, length, marker));
//   }
#endif  // FPLBASE_GLES || PLATFORM_OSX
#endif  // FPLBASE_ENABLE_DEBUG_MARKERS
}

inline void PushDebugMarker(const std::string& marker) {
  (void)marker;
#ifdef FPLBASE_ENABLE_DEBUG_MARKERS
  PushDebugMarker(marker.c_str(), marker.length());
#endif
}

inline void PopDebugMarker() {
#ifdef FPLBASE_ENABLE_DEBUG_MARKERS
#if defined(FPLBASE_GLES) || defined(PLATFORM_OSX)
// TODO(jsanmiya): Get glPushGroupMarker working for all iOS builds
//   if (glPopGroupMarker) {
//     GL_CALL(glPopGroupMarker());
//   }
#else
//   if (glPopDebugGroup) {
//     GL_CALL(glPopDebugGroup());
//   }
#endif  // FPLBASE_GLES || PLATFORM_OSX
#endif  // FPLBASE_ENABLE_DEBUG_MARKERS
}

#endif
