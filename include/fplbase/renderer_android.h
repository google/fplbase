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

#ifndef FPLBASE_RENDERER_ANDROID_H
#define FPLBASE_RENDERER_ANDROID_H

#include "fplbase/config.h"  // Must come first.

namespace fplbase {

/// @file
/// @addtogroup fplbase_renderer
/// @{

#ifdef __ANDROID__
/// @brief Create the Android window surface.
void AndroidPreCreateWindow();

/// @brief Scalar settings can occasionally fail on fail on some Android
/// devices. In failure cases, the caller can check for success by
/// verifying `AndroidGetScalerResolution()` returns the expected resolution.
/// If `AndroidGetScalarResolution()` returns a diffrent resolution than what
/// was set in `AndroidSetScaleResolution()`, the application can assume
/// failure and attempt to restart the app to try configuration again.
/// @param[in] resolution A const `mathfu::vec2i` reference to the scaler
/// resolution to set for the Android device.
void AndroidSetScalerResolution(const mathfu::vec2i& resolution);

/// @brief Get the Android scaler resolution.
/// @return Returns the Android scaler resolution as a const `mathfu::vec2i`
/// reference.
const mathfu::vec2i& AndroidGetScalerResolution();

/// @brief Gets the GLES client version of the current EGL context.
/// @return Returns the GLES client version of the current EGL context.
int AndroidGetContextClientVersion();

/// @brief Gets all the GL3 function pointers.  Using this rather than
/// GLES3/gl3.h directly means we can still compile on older SDKs and run on
/// older devices too.
void AndroidInitGl3Functions();
#endif

/// @}
}  // namespace fplbase

#endif  // FPLBASE_RENDERER_ANDROID_H
