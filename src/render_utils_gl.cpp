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

#include "precompiled.h"

#include "fplbase/internal/type_conversions_gl.h"
#include "fplbase/render_utils.h"

using mathfu::mat4;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;
using mathfu::vec4i;

namespace fplbase {

static void DrawElements(Mesh::Primitive primitive, int index_count,
                         const Attribute *format, int vertex_size,
                         const void *vertices, const void *indices,
                         GLenum gl_index_type) {
  SetAttributes(0 /* vbo */, format, vertex_size,
                reinterpret_cast<const char *>(vertices));
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
  auto gl_primitive = GetPrimitiveTypeFlags(primitive);
  GL_CALL(glDrawElements(gl_primitive, index_count, gl_index_type, indices));
  UnSetAttributes(format);
}

void RenderArray(Mesh::Primitive primitive, int index_count,
                 const Attribute *format, int vertex_size, const void *vertices,
                 const unsigned short *indices) {
  DrawElements(primitive, index_count, format, vertex_size, vertices, indices,
               GL_UNSIGNED_SHORT);
}

void RenderArray(Mesh::Primitive primitive, int index_count,
                 const Attribute *format, int vertex_size, const void *vertices,
                 const unsigned int *indices) {
  DrawElements(primitive, index_count, format, vertex_size, vertices, indices,
               GL_UNSIGNED_INT);
}

void RenderArray(Mesh::Primitive primitive, int vertex_count,
                 const Attribute *format, int vertex_size,
                 const void *vertices) {
  SetAttributes(0 /* vbo */, format, vertex_size,
                reinterpret_cast<const char *>(vertices));
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
  auto gl_primitive = GetPrimitiveTypeFlags(primitive);
  GL_CALL(glDrawArrays(gl_primitive, 0, vertex_count));
  UnSetAttributes(format);
}

void RenderAAQuadAlongX(const vec3 &bottom_left, const vec3 &top_right,
                        const vec2 &tex_bottom_left,
                        const vec2 &tex_top_right) {
  static const Attribute format[] = {kPosition3f, kTexCoord2f, kEND};
  static const unsigned short indices[] = {0, 1, 2, 1, 3, 2};
  static const int kNumIndices = 6;
  static const int kVertexSize = sizeof(float) * 5;

  // clang-format off
  // vertex format is [x, y, z] [u, v]:
  const float vertices[] = {
      bottom_left.x,     bottom_left.y,     bottom_left.z,
      tex_bottom_left.x, tex_bottom_left.y,
      bottom_left.x,     top_right.y,       top_right.z,
      tex_bottom_left.x, tex_top_right.y,
      top_right.x,       bottom_left.y,     bottom_left.z,
      tex_top_right.x,   tex_bottom_left.y,
      top_right.x,       top_right.y,       top_right.z,
      tex_top_right.x,   tex_top_right.y};
  // clang-format on
  RenderArray(Mesh::kTriangles, kNumIndices, format, kVertexSize,
              reinterpret_cast<const char *>(vertices), indices);
}

void RenderAAQuadAlongXNinePatch(const vec3 &bottom_left, const vec3 &top_right,
                                 const vec2i &texture_size,
                                 const vec4 &patch_info) {
  static const Attribute format[] = {kPosition3f, kTexCoord2f, kEND};
  static const unsigned short indices[] = {
      0, 2, 1,  1,  2, 3,  2, 4,  3,  3,  4,  5,  4,  6,  5,  5,  6,  7,
      1, 3, 8,  8,  3, 9,  3, 5,  9,  9,  5,  10, 5,  7,  10, 10, 7,  11,
      8, 9, 12, 12, 9, 13, 9, 10, 13, 13, 10, 14, 10, 11, 14, 14, 11, 15,
  };
  vec2 max = vec2::Max(bottom_left.xy(), top_right.xy());
  vec2 min = vec2::Min(bottom_left.xy(), top_right.xy());
  vec2 p0 = vec2(texture_size) * patch_info.xy() + min;
  vec2 p1 = max - vec2(texture_size) * (mathfu::kOnes2f - patch_info.zw());

  // Check if the 9 patch edges are not overwrapping.
  // In that case, adjust 9 patch geometry locations not to overwrap.
  if (p0.x > p1.x) {
    p0.x = p1.x = (min.x + max.x) / 2;
  }
  if (p0.y > p1.y) {
    p0.y = p1.y = (min.y + max.y) / 2;
  }

  // vertex format is [x, y, z] [u, v]:
  float z = bottom_left.z;
  // clang-format off
  const float vertices[] = {
      min.x, min.y, z, 0.0f,           0.0f,
      p0.x,  min.y, z, patch_info.x, 0.0f,
      min.x, p0.y,  z, 0.0f,           patch_info.y,
      p0.x,  p0.y,  z, patch_info.x, patch_info.y,
      min.x, p1.y,  z, 0.0,            patch_info.w,
      p0.x,  p1.y,  z, patch_info.x, patch_info.w,
      min.x, max.y, z, 0.0,            1.0,
      p0.x,  max.y, z, patch_info.x, 1.0,
      p1.x,  min.y, z, patch_info.z, 0.0f,
      p1.x,  p0.y,  z, patch_info.z, patch_info.y,
      p1.x,  p1.y,  z, patch_info.z, patch_info.w,
      p1.x,  max.y, z, patch_info.z, 1.0f,
      max.x, min.y, z, 1.0f,           0.0f,
      max.x, p0.y,  z, 1.0f,           patch_info.y,
      max.x, p1.y,  z, 1.0f,           patch_info.w,
      max.x, max.y, z, 1.0f,           1.0f,
  };
  // clang-format on
  RenderArray(Mesh::kTriangles, 6 * 9, format, sizeof(float) * 5,
              reinterpret_cast<const char *>(vertices), indices);
}

void SetAttributes(GLuint vbo, const Attribute *attributes, int stride,
                   const char *buffer) {
  assert(Mesh::IsValidFormat(attributes));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  size_t offset = 0;
  for (;;) {
    switch (*attributes++) {
      case kPosition3f:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributePosition));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributePosition, 3, GL_FLOAT,
                                      false, stride, buffer + offset));
        offset += 3 * sizeof(float);
        break;
      case kPosition2f:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributePosition));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributePosition, 2, GL_FLOAT,
                                      false, stride, buffer + offset));
        offset += 2 * sizeof(float);
        break;
      case kNormal3f:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeNormal));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeNormal, 3, GL_FLOAT,
                                      false, stride, buffer + offset));
        offset += 3 * sizeof(float);
        break;
      case kTangent4f:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeTangent));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeTangent, 4, GL_FLOAT,
                                      false, stride, buffer + offset));
        offset += 4 * sizeof(float);
        break;
      case kOrientation4f:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeOrientation));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeOrientation, 4, GL_FLOAT,
                                      false, stride, buffer + offset));
        offset += 4 * sizeof(float);
        break;
      case kTexCoord2f:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeTexCoord));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeTexCoord, 2, GL_FLOAT,
                                      false, stride, buffer + offset));
        offset += 2 * sizeof(float);
        break;
      case kTexCoord2us:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeTexCoord));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeTexCoord, 2,
                                      GL_UNSIGNED_SHORT,
                                      /* normalized = */ true, stride,
                                      buffer + offset));
        offset += 2 * sizeof(uint16_t);
        break;
      case kTexCoordAlt2f:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeTexCoordAlt));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeTexCoordAlt, 2, GL_FLOAT,
                                      false, stride, buffer + offset));
        offset += 2 * sizeof(float);
        break;
      case kColor4ub:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeColor));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeColor, 4,
                                      GL_UNSIGNED_BYTE, true, stride,
                                      buffer + offset));
        offset += 4;
        break;
      case kBoneIndices4ub:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeBoneIndices));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeBoneIndices, 4,
                                      GL_UNSIGNED_BYTE, false, stride,
                                      buffer + offset));
        offset += 4;
        break;
      case kBoneWeights4ub:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeBoneWeights));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeBoneWeights, 4,
                                      GL_UNSIGNED_BYTE, true, stride,
                                      buffer + offset));
        offset += 4;
        break;

      case kEND:
        return;
    }
  }
}

void UnSetAttributes(const Attribute *attributes) {
  for (;;) {
    switch (*attributes++) {
      case kPosition3f:
      case kPosition2f:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributePosition));
        break;
      case kNormal3f:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeNormal));
        break;
      case kTangent4f:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeTangent));
        break;
      case kOrientation4f:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeOrientation));
        break;
      case kTexCoord2f:
      case kTexCoord2us:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeTexCoord));
        break;
      case kTexCoordAlt2f:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeTexCoordAlt));
        break;
      case kColor4ub:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeColor));
        break;
      case kBoneIndices4ub:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeBoneIndices));
        break;
      case kBoneWeights4ub:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeBoneWeights));
        break;
      case kEND:
        GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
        return;
    }
  }
}

}  // namespace fplbase
