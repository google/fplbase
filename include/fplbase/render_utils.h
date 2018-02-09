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

#ifndef FPL_RENDER_UTILS_H
#define FPL_RENDER_UTILS_H

#include "fplbase/handles.h"
#include "fplbase/mesh.h"

namespace fplbase {

/// @brief Renders the given vertex data directly.
///
/// Renders primitives using vertex data directly in local memory. This is a
/// convenient alternative to creating a Mesh instance for small amounts of
/// data, or dynamic data.
///
/// @param primitive The type of primitive to render the data as.
/// @param vertex_count The total number of vertices.
/// @param format The vertex buffer format, following the same rules as
///        described in set_format().
/// @param vertex_size The size of an individual vertex.
/// @param vertices The array of vertices.
/// @param indices The list of vertex indices to use to construct primitives.
void RenderArray(Mesh::Primitive primitive, int index_count,
                 const Attribute *format, int vertex_size, const void *vertices,
                 const uint16_t *indices);

/// @brief Renders the given vertex data directly.
///
/// Renders primitives using vertex data directly in local memory. This is a
/// convenient alternative to creating a Mesh instance for small amounts of
/// data, or dynamic data. This uint32 version is only guaranteed to work with
/// GL ES 3.0+ or any version of desktop GL.
///
/// @param primitive The type of primitive to render the data as.
/// @param vertex_count The total number of vertices.
/// @param format The vertex buffer format, following the same rules as
///        described in set_format().
/// @param vertex_size The size of an individual vertex.
/// @param vertices The array of vertices.
/// @param indices The list of vertex indices to use to construct primitives.
void RenderArray(Mesh::Primitive primitive, int index_count,
                 const Attribute *format, int vertex_size, const void *vertices,
                 const uint32_t *indices);

/// @brief Renders the given vertex data directly.
///
/// Renders primitives using vertex data directly in local memory. This is a
/// convenient alternative to creating a Mesh instance for small amounts of
/// data, or dynamic data.
///
/// @param primitive The type of primitive to render the data as.
/// @param vertex_count The total number of vertices.
/// @param format The vertex buffer format, following the same rules as
///        described in set_format().
/// @param vertex_size The size of an individual vertex.
/// @param vertices The array of vertices.
void RenderArray(Mesh::Primitive primitive, int vertex_count,
                 const Attribute *format, int vertex_size,
                 const void *vertices);

/// @brief Convenience method for rendering a Quad.
///
/// bottom_left and top_right must have their X coordinate be different, but
/// either Y or Z can be the same.
///
/// @param bottom_left The bottom left coordinate of the Quad.
/// @param top_right The bottom left coordinate of the Quad.
/// @param tex_bottom_left The texture coordinates at the bottom left.
/// @param tex_top_right The texture coordinates at the top right.
void RenderAAQuadAlongX(const mathfu::vec3 &bottom_left,
                        const mathfu::vec3 &top_right,
                        const mathfu::vec2 &tex_bottom_left,
                        const mathfu::vec2 &tex_top_right);

/// @brief Convenience method for rendering a Quad with nine patch settings.
///
/// In the patch_info, the user can define nine patch settings
/// as vec4(x0, y0, x1, y1) where
/// (x0,y0): top-left corner of stretchable area in UV coordinate.
/// (x1,y1): bottom-right corner of stretchable area in UV coordinate.
///
/// @param bottom_left The bottom left coordinate of the Quad.
/// @param top_right The top right coordinate of the Quad.
/// @param texture_size The size of the texture used by the patches.
/// @param patch_info Defines how the patches are set up.
void RenderAAQuadAlongXNinePatch(const mathfu::vec3 &bottom_left,
                                 const mathfu::vec3 &top_right,
                                 const mathfu::vec2i &texture_size,
                                 const mathfu::vec4 &patch_info);

/// @brief Convenience method for setting vertex attributes.
///
/// Sets the vertex attributes to prepare for rendering or initializing a VAO.
///
/// @param vbo The vertex buffer object to set.
/// @param attributes The array of vertex attributes to set.
/// @param stride The byte offset between consecutive generic vertex attributes.
/// @param buffer Pointer to data the buffer if is in memory, or 0 if in GPU.
void SetAttributes(unsigned int vbo, const Attribute *attributes, int stride,
                   const char *buffer);

/// @brief Convenience method for resetting vertex attributes.
///
/// Disables active vertex attributes.
///
/// @param attributes The array of vertex attributes to unset.
void UnSetAttributes(const Attribute *attributes);

}  // namespace fplbase

#endif  // FPL_RENDER_UTILS_H
