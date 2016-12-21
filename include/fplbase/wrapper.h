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

#ifndef FPLBASE_WRAPPER_H
#define FPLBASE_WRAPPER_H

#include "fplbase/config.h"  // Must come first.

namespace fplbase {

#if defined(FPLBASE_OPENGL)

/// @brief These typedefs are compatible with OpenGL equivalents, but don't
/// require this header to depend on OpenGL.
typedef unsigned int TextureHandle;
typedef unsigned int TextureTarget;
typedef unsigned int ShaderHandle;
typedef int UniformHandle;
typedef unsigned int BufferHandle;

#endif  // defined(FPLBASE_OPENGL)

}  // namespace fplbase

#endif  // FPLBASE_WRAPPER_H
