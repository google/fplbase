// Copyright 2017 Google Inc. All rights reserved.
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

#ifndef FPLBASE_RENDERER_IOS_H
#define FPLBASE_RENDERER_IOS_H

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

namespace fplbase {

/// @file
/// @addtogroup fplbase_renderer
/// @{

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR || defined(DOXYGEN)
/// @brief Gets the major GLES client version of the current EGL context.
/// @return Returns the major GLES client version of the current EGL context.
int IosGetContextClientVersion();
#endif  // TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR || defined(DOXYGEN)

/// @}
}  // namespace fplbase

#endif  // FPLBASE_RENDERER_IOS_H
