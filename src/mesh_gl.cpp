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

#include <utility>

#include "fplbase/environment.h"
#include "fplbase/flatbuffer_utils.h"
#include "fplbase/internal/type_conversions_gl.h"
#include "fplbase/mesh.h"
#include "fplbase/render_utils.h"
#include "fplbase/renderer.h"
#include "fplbase/utilities.h"
#include "mesh_impl_gl.h"

#include "mesh_generated.h"

using mathfu::mat4;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;
using mathfu::vec4i;

namespace fplbase {

// Even though these functions are identical in each implementation, the
// definition of MeshImpl is different, so these functions cannot be in
// mesh_common.cpp.
MeshImpl *Mesh::CreateMeshImpl() { return new MeshImpl; }
void Mesh::DestroyMeshImpl(MeshImpl *impl) { delete impl; }

bool Mesh::IsValid() { return ValidBufferHandle(impl_->vbo); }

void Mesh::ClearPlatformDependent() {
  if (ValidBufferHandle(impl_->vbo)) {
    auto vbo = GlBufferHandle(impl_->vbo);
    GL_CALL(glDeleteBuffers(1, &vbo));
    impl_->vbo = InvalidBufferHandle();
  }
  if (ValidBufferHandle(impl_->vao)) {
    auto vao = GlBufferHandle(impl_->vao);
    GL_CALL(glDeleteVertexArrays(1, &vao));
    impl_->vao = InvalidBufferHandle();
  }
  for (auto it = indices_.begin(); it != indices_.end(); ++it) {
    auto ibo = GlBufferHandle(it->ibo);
    GL_CALL(glDeleteBuffers(1, &ibo));
  }
}

void Mesh::LoadFromMemory(const void *vertex_data, size_t count,
                          size_t vertex_size, const Attribute *format,
                          vec3 *max_position, vec3 *min_position) {
  assert(count > 0);
  vertex_size_ = vertex_size;
  num_vertices_ = count;
  default_bone_transform_inverses_ = nullptr;

  set_format(format);
  GLuint vbo = 0;
  GL_CALL(glGenBuffers(1, &vbo));
  impl_->vbo = BufferHandleFromGl(vbo);
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  GL_CALL(glBufferData(GL_ARRAY_BUFFER, count * vertex_size, vertex_data,
                       GL_STATIC_DRAW));

  if (RendererBase::Get()->feature_level() >= kFeatureLevel30) {
    GLuint vao = 0;
    GL_CALL(glGenVertexArrays(1, &vao));
    impl_->vao = BufferHandleFromGl(vao);
    GL_CALL(glBindVertexArray(vao));
    SetAttributes(vbo, format_, static_cast<int>(vertex_size_), nullptr);
    GL_CALL(glBindVertexArray(0));
  }

  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

  // Determine the min and max position
  if (max_position && min_position) {
    max_position_ = *max_position;
    min_position_ = *min_position;
  } else {
    auto data = static_cast<const float *>(vertex_data);
    const Attribute *attribute = format;
    data += AttributeOffset(attribute, kPosition3f) / sizeof(float);
    const size_t step = vertex_size / sizeof(float);
    min_position_ = vec3(data);
    max_position_ = min_position_;
    for (size_t vertex = 1; vertex < count; vertex++) {
      data += step;
      min_position_ = vec3::Min(min_position_, vec3(data));
      max_position_ = vec3::Max(max_position_, vec3(data));
    }
  }
}

void Mesh::AddIndices(const void *index_data, int count, Material *mat,
                      bool is_32_bit) {
  indices_.push_back(Indices());
  auto &idxs = indices_.back();
  idxs.count = count;
  GLuint ibo = 0;
  GL_CALL(glGenBuffers(1, &ibo));
  idxs.ibo = BufferHandleFromGl(ibo);
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo));
  GL_CALL(
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                   count * (is_32_bit ? sizeof(uint32_t) : sizeof(uint16_t)),
                   index_data, GL_STATIC_DRAW));
  idxs.index_type = (is_32_bit ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT);
  idxs.mat = mat;
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

}  // namespace fplbase
