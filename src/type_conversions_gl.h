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

#ifndef FPLBASE_TYPE_CONVERSIONS_GL_H
#define FPLBASE_TYPE_CONVERSIONS_GL_H

#include "fplbase/glplatform.h"

#include "fplbase/internal/detailed_render_state.h"

namespace fplbase {

/// @brief Converts FPL render function value to equivalent GL enum value.
///
/// @param func The render function value to convert.
GLenum RenderFunctionToGlFunction(RenderFunction func);

/// @brief Converts FPL blend state factor to equivalent GL enum value.
///
/// @param factor The blend state factor to convert.
GLenum BlendStateFactorToGl(BlendState::BlendFactor factor);

/// @brief Converts FPL stencil operation value to equivalent GL enum value.
///
/// @param op The stencil operation to convert.
GLenum StencilOpToGlOp(StencilOperation::StencilOperations op);

/// @brief Converts FPL cull face value to equivalent GL enum value.
///
/// @param face The cull face value to convert.
GLenum CullFaceToGl(CullState::CullFace face);

}  // namespace fplbase

#endif  // FPLBASE_TYPE_CONVERSIONS_GL_H
