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

#ifndef FPLBASE_GPU_DEBUG_H
#define FPLBASE_GPU_DEBUG_H

#include "fplbase/render_state.h"

namespace fplbase {

/// @brief: Validates that the current GPU state matches a given render state.
///
/// In debug this function will throw assets if the present state does not match
/// the expected render state. In release no checks will occur.
bool ValidateRenderState(const RenderState& render_state);

}  // namespace fplbase

#endif  // FPLBASE_GPU_DEBUG_H
