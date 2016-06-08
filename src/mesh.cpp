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

#include "precompiled.h"
#include "fplbase/mesh.h"
#include "fplbase/renderer.h"

using mathfu::mat4;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;
using mathfu::vec4i;

namespace fplbase {
namespace {

GLenum GetGlPrimitiveType(Mesh::Primitive primitive) {
  switch (primitive) {
    case Mesh::kLines:
      return GL_LINES;
    case Mesh::kPoints:
      return GL_POINTS;
    case Mesh::kTriangleStrip:
      return GL_TRIANGLE_STRIP;
    default:
      return GL_TRIANGLES;
  }
}

}  // namespace

void Mesh::SetAttributes(GLuint vbo, const Attribute *attributes, int stride,
                         const char *buffer) {
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  size_t offset = 0;
  for (;;) {
    switch (*attributes++) {
      case kPosition3f:
        GL_CALL(glEnableVertexAttribArray(kAttributePosition));
        GL_CALL(glVertexAttribPointer(kAttributePosition, 3, GL_FLOAT, false,
                                      stride, buffer + offset));
        offset += 3 * sizeof(float);
        break;
      case kNormal3f:
        GL_CALL(glEnableVertexAttribArray(kAttributeNormal));
        GL_CALL(glVertexAttribPointer(kAttributeNormal, 3, GL_FLOAT, false,
                                      stride, buffer + offset));
        offset += 3 * sizeof(float);
        break;
      case kTangent4f:
        GL_CALL(glEnableVertexAttribArray(kAttributeTangent));
        GL_CALL(glVertexAttribPointer(kAttributeTangent, 3, GL_FLOAT, false,
                                      stride, buffer + offset));
        offset += 4 * sizeof(float);
        break;
      case kTexCoord2f:
        GL_CALL(glEnableVertexAttribArray(kAttributeTexCoord));
        GL_CALL(glVertexAttribPointer(kAttributeTexCoord, 2, GL_FLOAT, false,
                                      stride, buffer + offset));
        offset += 2 * sizeof(float);
        break;
      case kColor4ub:
        GL_CALL(glEnableVertexAttribArray(kAttributeColor));
        GL_CALL(glVertexAttribPointer(kAttributeColor, 4, GL_UNSIGNED_BYTE,
                                      true, stride, buffer + offset));
        offset += 4;
        break;
      case kBoneIndices4ub:
        GL_CALL(glEnableVertexAttribArray(kAttributeBoneIndices));
        GL_CALL(glVertexAttribPointer(kAttributeBoneIndices, 4,
                                      GL_UNSIGNED_BYTE, false, stride,
                                      buffer + offset));
        offset += 4;
        break;
      case kBoneWeights4ub:
        GL_CALL(glEnableVertexAttribArray(kAttributeBoneWeights));
        GL_CALL(glVertexAttribPointer(kAttributeBoneWeights, 4,
                                      GL_UNSIGNED_BYTE, true, stride,
                                      buffer + offset));
        offset += 4;
        break;

      case kEND:
        return;
    }
  }
}

size_t Mesh::VertexSize(const Attribute *attributes, Attribute end) {
  size_t size = 0;
  for (;; attributes++) {
    if (*attributes == end) {
      return size;
    }
    // clang-format off
    switch (*attributes) {
      case kPosition3f:     size += 3 * sizeof(float); break;
      case kNormal3f:       size += 3 * sizeof(float); break;
      case kTangent4f:      size += 4 * sizeof(float); break;
      case kTexCoord2f:     size += 2 * sizeof(float); break;
      case kColor4ub:       size += 4;                 break;
      case kBoneIndices4ub: size += 4;                 break;
      case kBoneWeights4ub: size += 4;                 break;
      case kEND:            return size;
    }
    // clang-format on
  }
}

void Mesh::UnSetAttributes(const Attribute *attributes) {
  for (;;) {
    switch (*attributes++) {
      case kPosition3f:
        GL_CALL(glDisableVertexAttribArray(kAttributePosition));
        break;
      case kNormal3f:
        GL_CALL(glDisableVertexAttribArray(kAttributeNormal));
        break;
      case kTangent4f:
        GL_CALL(glDisableVertexAttribArray(kAttributeTangent));
        break;
      case kTexCoord2f:
        GL_CALL(glDisableVertexAttribArray(kAttributeTexCoord));
        break;
      case kColor4ub:
        GL_CALL(glDisableVertexAttribArray(kAttributeColor));
        break;
      case kBoneIndices4ub:
        GL_CALL(glDisableVertexAttribArray(kAttributeBoneIndices));
        break;
      case kBoneWeights4ub:
        GL_CALL(glDisableVertexAttribArray(kAttributeBoneWeights));
        break;
      case kEND:
        return;
    }
  }
}

Mesh::Mesh(const void *vertex_data, int count, int vertex_size,
           const Attribute *format, vec3 *max_position, vec3 *min_position)
    : vertex_size_(vertex_size),
      num_vertices_(static_cast<size_t>(count)),
      bone_transforms_(nullptr),
      bone_global_transforms_(nullptr) {
  set_format(format);
  GL_CALL(glGenBuffers(1, &vbo_));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vbo_));
  GL_CALL(glBufferData(GL_ARRAY_BUFFER, count * vertex_size, vertex_data,
                       GL_STATIC_DRAW));

  // Determine the min and max position
  if (max_position && min_position) {
    max_position_ = *max_position;
    min_position_ = *min_position;
  } else {
    auto data = static_cast<const float *>(vertex_data);
    const Attribute *attribute = format;
    data += VertexSize(attribute, kPosition3f) / sizeof(float);
    int step = vertex_size / sizeof(float);
    min_position_ = vec3(data);
    max_position_ = min_position_;
    for (int vertex = 1; vertex < count; vertex++) {
      data += step;
      min_position_ = vec3::Min(min_position_, vec3(data));
      max_position_ = vec3::Max(max_position_, vec3(data));
    }
  }
}

Mesh::~Mesh() {
  GL_CALL(glDeleteBuffers(1, &vbo_));
  for (auto it = indices_.begin(); it != indices_.end(); ++it) {
    GL_CALL(glDeleteBuffers(1, &it->ibo));
  }

  delete[] bone_transforms_;
  bone_transforms_ = nullptr;

  delete[] bone_global_transforms_;
  bone_global_transforms_ = nullptr;
}

void Mesh::set_format(const Attribute *format) {
  for (int i = 0;; ++i) {
    assert(i < kMaxAttributes);
    if (i >= kMaxAttributes) break;

    format_[i] = format[i];
    if (format[i] == kEND) break;
  }
}

void Mesh::AddIndices(const unsigned short *index_data, int count,
                      Material *mat) {
  indices_.push_back(Indices());
  auto &idxs = indices_.back();
  idxs.count = count;
  GL_CALL(glGenBuffers(1, &idxs.ibo));
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idxs.ibo));
  GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(short),
                       index_data, GL_STATIC_DRAW));
  idxs.mat = mat;
}

void Mesh::SetBones(const mathfu::AffineTransform *bone_transforms,
                    const uint8_t *bone_parents, const char **bone_names,
                    size_t num_bones, const uint8_t *shader_bone_indices,
                    size_t num_shader_bones) {
  delete[] bone_transforms_;
  bone_transforms_ = new mathfu::AffineTransform[num_bones];
  bone_parents_.resize(num_bones);
  shader_bone_indices_.resize(num_shader_bones);

  memcpy(&bone_transforms_[0], bone_transforms,
         num_bones * sizeof(bone_transforms_[0]));
  memcpy(&bone_parents_[0], bone_parents, num_bones * sizeof(bone_parents_[0]));
  memcpy(&shader_bone_indices_[0], shader_bone_indices,
         num_shader_bones * sizeof(shader_bone_indices_[0]));

  // Record the bone names if they're present. They're only for debugging,
  // so they're optional.
  if (bone_names != nullptr) {
    bone_names_.resize(num_bones);
    for (size_t i = 0; i < num_bones; ++i) {
      bone_names_[i] = bone_names[i];
    }
  }

  // Record the global version of the transforms, so we can still display
  // the mesh, even if it's not animated.
  static const uint8_t kInvalidBoneIdx = 0xFF;
  delete[] bone_global_transforms_;
  bone_global_transforms_ = new mathfu::AffineTransform[num_bones];
  for (size_t i = 0; i < num_bones; ++i) {
    const size_t parent_idx = bone_parents[i];
    if (parent_idx == kInvalidBoneIdx) {
      bone_global_transforms_[i] = bone_transforms[i];
    } else {
      assert(i > parent_idx);
      bone_global_transforms_[i] = mat4::ToAffineTransform(
          mat4::FromAffineTransform(bone_global_transforms_[parent_idx]) *
          mat4::FromAffineTransform(bone_transforms[i]));
    }
  }
}

void Mesh::DrawElement(Renderer &renderer, int32_t count, int32_t instances) {
  if (instances == 1) {
    GL_CALL(glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, 0));
  } else {
    (void)renderer;
    assert(renderer.feature_level() == Renderer::kFeatureLevel30);
    GL_CALL(glDrawElementsInstanced(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, 0,
                                    instances));
  }
}

void Mesh::Render(Renderer &renderer, bool ignore_material, size_t instances) {
  SetAttributes(vbo_, format_, static_cast<int>(vertex_size_), nullptr);
  for (auto it = indices_.begin(); it != indices_.end(); ++it) {
    if (!ignore_material) it->mat->Set(renderer);
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, it->ibo));
    DrawElement(renderer, it->count, static_cast<int32_t>(instances));
  }
  UnSetAttributes(format_);
}

void Mesh::RenderStereo(Renderer &renderer, const Shader *shader,
                        const vec4i *viewport, const mat4 *mvp,
                        const vec3 *camera_position, bool ignore_material,
                        size_t instances) {
  SetAttributes(vbo_, format_, static_cast<int>(vertex_size_), nullptr);
  for (auto it = indices_.begin(); it != indices_.end(); ++it) {
    if (!ignore_material) it->mat->Set(renderer);
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, it->ibo));

    for (auto i = 0; i < 2; ++i) {
      renderer.set_camera_pos(camera_position[i]);
      renderer.set_model_view_projection(mvp[i]);
      shader->Set(renderer);

      auto vp = viewport[i];
      glViewport(vp.x(), vp.y(), vp.z(), vp.w());
      DrawElement(renderer, it->count, static_cast<int32_t>(instances));
    }
  }
  UnSetAttributes(format_);
}

void Mesh::RenderArray(Primitive primitive, int index_count,
                       const Attribute *format, int vertex_size,
                       const void *vertices, const unsigned short *indices) {
  SetAttributes(0, format, vertex_size,
                reinterpret_cast<const char *>(vertices));
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
  auto gl_primitive = GetGlPrimitiveType(primitive);
  GL_CALL(
      glDrawElements(gl_primitive, index_count, GL_UNSIGNED_SHORT, indices));
  UnSetAttributes(format);
}

void Mesh::RenderArray(Primitive primitive, int vertex_count,
                       const Attribute *format, int vertex_size,
                       const void *vertices) {
  SetAttributes(0, format, vertex_size,
                reinterpret_cast<const char *>(vertices));
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
  auto gl_primitive = GetGlPrimitiveType(primitive);
  GL_CALL(glDrawArrays(gl_primitive, 0, vertex_count));
  UnSetAttributes(format);
}

void Mesh::RenderAAQuadAlongX(const vec3 &bottom_left, const vec3 &top_right,
                              const vec2 &tex_bottom_left,
                              const vec2 &tex_top_right) {
  static const Attribute format[] = {kPosition3f, kTexCoord2f, kEND};
  static const unsigned short indices[] = {0, 1, 2, 1, 3, 2};

  // clang-format off
  // vertex format is [x, y, z] [u, v]:
  const float vertices[] = {
      bottom_left.x(),     bottom_left.y(),     bottom_left.z(),
      tex_bottom_left.x(), tex_bottom_left.y(),
      bottom_left.x(),     top_right.y(),       top_right.z(),
      tex_bottom_left.x(), tex_top_right.y(),
      top_right.x(),       bottom_left.y(),     bottom_left.z(),
      tex_top_right.x(),   tex_bottom_left.y(),
      top_right.x(),       top_right.y(),       top_right.z(),
      tex_top_right.x(),   tex_top_right.y()};
  // clang-format on
  Mesh::RenderArray(kTriangles, 6, format, sizeof(float) * 5,
                    reinterpret_cast<const char *>(vertices), indices);
}

void Mesh::RenderAAQuadAlongXNinePatch(const vec3 &bottom_left,
                                       const vec3 &top_right,
                                       const vec2i &texture_size,
                                       const vec4 &patch_info) {
  static const Attribute format[] = {kPosition3f, kTexCoord2f, kEND};
  static const unsigned short indices[] = {
      0, 2, 1,  1,  2, 3,  2, 4,  3,  3,  4,  5,  4,  6,  5,  5,  6,  7,
      1, 3, 8,  8,  3, 9,  3, 5,  9,  9,  5,  10, 5,  7,  10, 10, 7,  11,
      8, 9, 12, 12, 9, 13, 9, 10, 13, 13, 10, 14, 10, 11, 14, 14, 11, 15,
  };
  auto max = vec2::Max(bottom_left.xy(), top_right.xy());
  auto min = vec2::Min(bottom_left.xy(), top_right.xy());
  auto p0 = vec2(texture_size) * patch_info.xy() + min;
  auto p1 = max - vec2(texture_size) * (mathfu::kOnes2f - patch_info.zw());

  // Check if the 9 patch edges are not overwrapping.
  // In that case, adjust 9 patch geometry locations not to overwrap.
  if (p0.x() > p1.x()) {
    p0.x() = p1.x() = (min.x() + max.x()) / 2;
  }
  if (p0.y() > p1.y()) {
    p0.y() = p1.y() = (min.y() + max.y()) / 2;
  }

  // vertex format is [x, y, z] [u, v]:
  float z = bottom_left.z();
  // clang-format off
  const float vertices[] = {
      min.x(), min.y(), z, 0.0f,           0.0f,
      p0.x(),  min.y(), z, patch_info.x(), 0.0f,
      min.x(), p0.y(),  z, 0.0f,           patch_info.y(),
      p0.x(),  p0.y(),  z, patch_info.x(), patch_info.y(),
      min.x(), p1.y(),  z, 0.0,            patch_info.w(),
      p0.x(),  p1.y(),  z, patch_info.x(), patch_info.w(),
      min.x(), max.y(), z, 0.0,            1.0,
      p0.x(),  max.y(), z, patch_info.x(), 1.0,
      p1.x(),  min.y(), z, patch_info.z(), 0.0f,
      p1.x(),  p0.y(),  z, patch_info.z(), patch_info.y(),
      p1.x(),  p1.y(),  z, patch_info.z(), patch_info.w(),
      p1.x(),  max.y(), z, patch_info.z(), 1.0f,
      max.x(), min.y(), z, 1.0f,           0.0f,
      max.x(), p0.y(),  z, 1.0f,           patch_info.y(),
      max.x(), p1.y(),  z, 1.0f,           patch_info.w(),
      max.x(), max.y(), z, 1.0f,           1.0f,
  };
  // clang-format on
  Mesh::RenderArray(kTriangles, 6 * 9, format, sizeof(float) * 5,
                    reinterpret_cast<const char *>(vertices), indices);
}

void Mesh::GatherShaderTransforms(
    const mathfu::AffineTransform *bone_transforms,
    mathfu::AffineTransform *shader_transforms) const {
  for (size_t i = 0; i < shader_bone_indices_.size(); ++i) {
    shader_transforms[i] = bone_transforms[shader_bone_indices_[i]];
  }
}

size_t Mesh::CalculateTotalNumberOfIndices() const {
  int total = 0;
  for (size_t i = 0; i < indices_.size(); ++i) {
    total += indices_[i].count;
  }
  return static_cast<size_t>(total);
}

}  // namespace fplbase
