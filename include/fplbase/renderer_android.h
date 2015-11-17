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

#ifdef __ANDROID__
void AndroidPreCreateWindow();
// Occasionally the scaler setting API would fail on some Android devices.
// In such case, the caller may check if the API was success by checking
// if AndroidGetScalerResolution() returns expected resolution.
// The caller may handle that as an error situation (e.g. re-launching an app
// etc.).
void AndroidSetScalerResolution(const mathfu::vec2i& resolution);
const mathfu::vec2i& AndroidGetScalerResolution();
#endif

}  // namespace fplbase

#endif  // FPLBASE_RENDERER_ANDROID_H
