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

#include "fplbase/flatbuffer_utils.h"
#include "fplbase/fpl_common.h"
#include "fplbase/internal/type_conversions_gl.h"
#include "fplbase/mesh.h"
#include "fplbase/utilities.h"

#include "mesh_generated.h"

using mathfu::mat4;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;
using mathfu::vec4i;

namespace fplbase {
namespace {

static_assert(
    kEND == static_cast<Attribute>(meshdef::Attribute_END) &&
        kPosition3f == static_cast<Attribute>(meshdef::Attribute_Position3f) &&
        kNormal3f == static_cast<Attribute>(meshdef::Attribute_Normal3f) &&
        kTangent4f == static_cast<Attribute>(meshdef::Attribute_Tangent4f) &&
        kTexCoord2f == static_cast<Attribute>(meshdef::Attribute_TexCoord2f) &&
        kTexCoordAlt2f ==
            static_cast<Attribute>(meshdef::Attribute_TexCoordAlt2f) &&
        kColor4ub == static_cast<Attribute>(meshdef::Attribute_Color4ub) &&
        kBoneIndices4ub ==
            static_cast<Attribute>(meshdef::Attribute_BoneIndices4ub) &&
        kBoneWeights4ub ==
            static_cast<Attribute>(meshdef::Attribute_BoneWeights4ub) &&
        kPosition2f == static_cast<Attribute>(meshdef::Attribute_Position2f) &&
        kTexCoord2us ==
            static_cast<Attribute>(meshdef::Attribute_TexCoord2us) &&
        kOrientation4f ==
            static_cast<Attribute>(meshdef::Attribute_Orientation4f),
    "Attribute enums in mesh.h and mesh.fbs must match.");

template <typename T>
void CopyAttribute(const T *attr, uint8_t *&buf) {
  auto dest = (T *)buf;
  *dest = *attr;
  buf += sizeof(T);
}

}  // namespace

Mesh::Mesh(const char *filename, MaterialCreateFn material_create_fn,
           Primitive primitive)
    : AsyncAsset(filename ? filename : ""),
      impl_(CreateMeshImpl()),
      primitive_(GetPrimitiveTypeFlags(primitive)),
      vertex_size_(0),
      num_vertices_(0),
      min_position_(mathfu::kZeros3f),
      max_position_(mathfu::kZeros3f),
      default_bone_transform_inverses_(nullptr),
      material_create_fn_(std::move(material_create_fn)) {}

Mesh::Mesh(const void *vertex_data, size_t count, size_t vertex_size,
           const Attribute *format, vec3 *max_position, vec3 *min_position,
           Primitive primitive)
    : impl_(CreateMeshImpl()),
      primitive_(GetPrimitiveTypeFlags(primitive)),
      vertex_size_(0),
      num_vertices_(0),
      min_position_(mathfu::kZeros3f),
      max_position_(mathfu::kZeros3f),
      default_bone_transform_inverses_(nullptr) {
  LoadFromMemory(vertex_data, count, vertex_size, format, max_position,
                 min_position);
}

Mesh::~Mesh() {
  Clear();
  DestroyMeshImpl(impl_);
}

bool Mesh::IsValidFormat(const Attribute *attributes) {
  bool seen[kMaxAttributes] = {false};
  int count = 0;
  for (;; attributes++) {
    size_t index = 0;
    // clang-format off
    switch (*attributes) {
      case kPosition3f:     index = kAttributePosition;      break;
      case kPosition2f:     index = kAttributePosition;      break;
      case kNormal3f:       index = kAttributeNormal;        break;
      case kTangent4f:      index = kAttributeTangent;       break;
      case kOrientation4f:  index = kAttributeOrientation;   break;
      case kTexCoord2f:     index = kAttributeTexCoord;      break;
      case kTexCoord2us:    index = kAttributeTexCoord;      break;
      case kTexCoordAlt2f:  index = kAttributeTexCoordAlt;   break;
      case kColor4ub:       index = kAttributeColor;         break;
      case kBoneIndices4ub: index = kAttributeBoneIndices;   break;
      case kBoneWeights4ub: index = kAttributeBoneWeights;   break;
      case kEND:            return seen[kAttributePosition];
    }
    // clang-format on
    assert(index < FPL_ARRAYSIZE(seen));
    if (seen[index] || count == kMaxAttributes) {
      break;
    }
    seen[index] = true;
    ++count;
  }
  return false;
}

size_t Mesh::AttributeOffset(const Attribute *attributes, Attribute end) {
  assert(IsValidFormat(attributes));

  size_t size = 0;
  for (;; attributes++) {
    if (*attributes == end) {
      return size;
    }
    // clang-format off
    switch (*attributes) {
      case kPosition3f:     size += 3 * sizeof(float);    break;
      case kPosition2f:     size += 2 * sizeof(float);    break;
      case kNormal3f:       size += 3 * sizeof(float);    break;
      case kTangent4f:      size += 4 * sizeof(float);    break;
      case kOrientation4f:  size += 4 * sizeof(float);    break;
      case kTexCoord2f:     size += 2 * sizeof(float);    break;
      case kTexCoord2us:    size += 2 * sizeof(uint16_t); break;
      case kTexCoordAlt2f:  size += 2 * sizeof(float);    break;
      case kColor4ub:       size += 4;                    break;
      case kBoneIndices4ub: size += 4;                    break;
      case kBoneWeights4ub: size += 4;                    break;
      case kEND:            return size;
    }
    // clang-format on
  }
}

size_t Mesh::VertexSize(const Attribute *attributes) {
  return AttributeOffset(attributes, kEND);
}

void Mesh::Load() {
  std::string *flatbuf = new std::string();
  if (LoadFile(filename_.c_str(), flatbuf)) {
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t *>(flatbuf->c_str()), flatbuf->length());
    assert(meshdef::VerifyMeshBuffer(verifier));
    data_ = reinterpret_cast<const uint8_t *>(flatbuf);
  } else {
    LogError(kError, "Couldn\'t load: %s", filename_.c_str());
    delete flatbuf;
  }
}

bool Mesh::Finalize() {
  if (data_) {
    const std::string *flatbuf = reinterpret_cast<const std::string *>(data_);
    bool ok = InitFromMeshDef(flatbuf->c_str());
    delete flatbuf;
    data_ = nullptr;
    if (!ok) Clear();
  }
  CallFinalizeCallback();
  return IsValid();
}

void Mesh::ParseInterleavedVertexData(const void *meshdef_buffer,
                                      InterleavedVertexData *ivd) {
  auto meshdef = meshdef::GetMesh(meshdef_buffer);
  ivd->has_skinning =
      meshdef->bone_transforms() && meshdef->bone_transforms()->size() &&
      meshdef->bone_parents() && meshdef->bone_parents()->size() &&
      meshdef->shader_to_mesh_bones() &&
      meshdef->shader_to_mesh_bones()->size();
  // See if we're loading interleaved or non-interleaved data.
  if (meshdef->vertices() && meshdef->vertices()->size() &&
      meshdef->attributes() && meshdef->attributes()->size()) {
    // Interleaved.
    for (flatbuffers::uoffset_t i = 0; i < meshdef->attributes()->size(); i++) {
      ivd->format.push_back(
          static_cast<Attribute>(meshdef->attributes()->Get(i)));
    }
    ivd->vertex_size = Mesh::VertexSize(ivd->format.data());
    ivd->vertex_data = meshdef->vertices()->data();
    ivd->count = meshdef->vertices()->size() / ivd->vertex_size;
  } else {  // Non-interleaved.
    ivd->has_skinning = ivd->has_skinning && meshdef->skin_indices() &&
                        meshdef->skin_indices()->size() &&
                        meshdef->skin_weights() &&
                        meshdef->skin_weights()->size();
    auto has_normals = meshdef->normals() && meshdef->normals()->size();
    auto has_tangents = meshdef->tangents() && meshdef->tangents()->size();
    auto has_orientations = meshdef->orientations() &&
                            meshdef->orientations()->size();
    auto has_colors = meshdef->colors() && meshdef->colors()->size();
    auto has_texcoords = meshdef->texcoords() && meshdef->texcoords()->size();
    auto has_texcoords_alt =
        meshdef->texcoords_alt() && meshdef->texcoords_alt()->size();
    // Collect what attributes are available.
    ivd->format.push_back(kPosition3f);
    if (has_normals) ivd->format.push_back(kNormal3f);
    if (has_tangents) ivd->format.push_back(kTangent4f);
    if (has_orientations) ivd->format.push_back(kOrientation4f);
    if (has_colors) ivd->format.push_back(kColor4ub);
    if (has_texcoords) ivd->format.push_back(kTexCoord2f);
    if (has_texcoords_alt) ivd->format.push_back(kTexCoordAlt2f);
    if (ivd->has_skinning) {
      ivd->format.push_back(kBoneIndices4ub);
      ivd->format.push_back(kBoneWeights4ub);
    }
    ivd->format.push_back(kEND);
    ivd->vertex_size = Mesh::VertexSize(ivd->format.data());
    // Create an interleaved buffer. Would be cool to do this without
    // the additional copy, but that's not easy in OpenGL.
    // Could use multiple buffers instead, but likely less efficient.
    ivd->count = meshdef->positions()->size();
    ivd->owned_vertex_data.resize(ivd->vertex_size * ivd->count);
    auto p = ivd->owned_vertex_data.data();
    ivd->vertex_data = p;
    for (size_t i = 0; i < ivd->count; i++) {
      flatbuffers::uoffset_t index = static_cast<flatbuffers::uoffset_t>(i);
      assert(meshdef->positions());
      CopyAttribute(meshdef->positions()->Get(index), p);
      if (has_normals) CopyAttribute(meshdef->normals()->Get(index), p);
      if (has_tangents) CopyAttribute(meshdef->tangents()->Get(index), p);
      if (has_orientations) {
        CopyAttribute(meshdef->orientations()->Get(index), p);
      }
      if (has_colors) CopyAttribute(meshdef->colors()->Get(index), p);
      if (has_texcoords) CopyAttribute(meshdef->texcoords()->Get(index), p);
      if (has_texcoords_alt)
        CopyAttribute(meshdef->texcoords_alt()->Get(index), p);
      if (ivd->has_skinning) {
        CopyAttribute(meshdef->skin_indices()->Get(index), p);
        CopyAttribute(meshdef->skin_weights()->Get(index), p);
      }
    }
  }
}

bool Mesh::InitFromMeshDef(const void *meshdef_buffer) {
  auto meshdef = meshdef::GetMesh(meshdef_buffer);
  // Ensure the data version matches the runtime version, or that it was not
  // tied to a specific version to begin with (e.g. it's legacy or it's
  // created from a json file instead of mesh_pipeline).
  if (meshdef->version() != meshdef::MeshVersion_Unspecified &&
      meshdef->version() != meshdef::MeshVersion_MostRecent) {
    LogError(kError, "Mesh file is stale: %s", filename_.c_str());
    return false;
  }

  // Load materials, return error if there is any material that is failed to
  // load.
  assert(material_create_fn_ != nullptr || meshdef->surfaces()->size() == 0);
  typedef std::pair<const meshdef::Surface *, Material *> SurfaceMaterialPair;
  std::vector<SurfaceMaterialPair> indices_data;
  for (size_t i = 0; i < meshdef->surfaces()->size(); i++) {
    flatbuffers::uoffset_t index = static_cast<flatbuffers::uoffset_t>(i);
    auto surface = meshdef->surfaces()->Get(index);
    auto mat = material_create_fn_(surface->material()->c_str(),
                                   surface->material_info());
    if (!mat) {
      LogError(kError, "Invalid material file: ", surface->material()->c_str());
      return false;
    }  // Error msg already set.
    indices_data.push_back(SurfaceMaterialPair(surface, mat));
  }

  // Load indices from surface and material.
  for (auto it = indices_data.begin(); it != indices_data.end(); it++) {
    auto surface = it->first;
    auto mat = it->second;
    AddIndices(surface->indices() ? surface->indices()->Data()
                                  : surface->indices32()->Data(),
               surface->indices() ? surface->indices()->Length()
                                  : surface->indices32()->Length(),
               mat, !surface->indices());
  }

  InterleavedVertexData ivd;
  ParseInterleavedVertexData(meshdef_buffer, &ivd);
  vec3 max = meshdef->max_position() ? LoadVec3(meshdef->max_position())
                                     : mathfu::kZeros3f;
  vec3 min = meshdef->min_position() ? LoadVec3(meshdef->min_position())
                                     : mathfu::kZeros3f;
  LoadFromMemory(ivd.vertex_data, ivd.count, ivd.vertex_size, ivd.format.data(),
                 meshdef->max_position() ? &max : nullptr,
                 meshdef->min_position() ? &min : nullptr);
  // Load the bone information.
  if (ivd.has_skinning) {
    const size_t num_bones = meshdef->bone_parents()->Length();
    assert(meshdef->bone_transforms()->Length() == num_bones);
    std::unique_ptr<mathfu::AffineTransform[]> bone_transforms(
        new mathfu::AffineTransform[num_bones]);
    std::vector<const char *> bone_names(num_bones);
    for (size_t i = 0; i < num_bones; ++i) {
      flatbuffers::uoffset_t index = static_cast<flatbuffers::uoffset_t>(i);
      bone_transforms[i] = LoadAffine(meshdef->bone_transforms()->Get(index));
      bone_names[i] = meshdef->bone_names()->Get(index)->c_str();
    }
    const uint8_t *bone_parents = meshdef->bone_parents()->data();
    SetBones(&bone_transforms[0], bone_parents, &bone_names[0], num_bones,
             meshdef->shader_to_mesh_bones()->Data(),
             meshdef->shader_to_mesh_bones()->Length());
  }

  return true;
}

void Mesh::set_format(const Attribute *format) {
  assert(IsValidFormat(format));

  for (int i = 0; i < kMaxAttributes; ++i) {
    format_[i] = format[i];
    if (format[i] == kEND) break;
  }
}

void Mesh::SetBones(const mathfu::AffineTransform *bone_transforms,
                    const uint8_t *bone_parents, const char **bone_names,
                    size_t num_bones, const uint8_t *shader_bone_indices,
                    size_t num_shader_bones) {
  delete[] default_bone_transform_inverses_;
  default_bone_transform_inverses_ = new mathfu::AffineTransform[num_bones];
  bone_parents_.resize(num_bones);
  shader_bone_indices_.resize(num_shader_bones);

  memcpy(&default_bone_transform_inverses_[0], bone_transforms,
         num_bones * sizeof(default_bone_transform_inverses_[0]));
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
}

void Mesh::GatherShaderTransforms(
    const mathfu::AffineTransform *bone_transforms,
    mathfu::AffineTransform *shader_transforms) const {
  for (size_t i = 0; i < shader_bone_indices_.size(); ++i) {
    const int bone_idx = shader_bone_indices_[i];
    shader_transforms[i] = mat4::ToAffineTransform(
        mat4::FromAffineTransform(bone_transforms[bone_idx]) *
        mat4::FromAffineTransform(default_bone_transform_inverses_[bone_idx]));
  }
}

size_t Mesh::CalculateTotalNumberOfIndices() const {
  int total = 0;
  for (size_t i = 0; i < indices_.size(); ++i) {
    total += indices_[i].count;
  }
  return static_cast<size_t>(total);
}

void Mesh::Clear() {
  ClearPlatformDependent();

  indices_.clear();

  delete[] default_bone_transform_inverses_;
  default_bone_transform_inverses_ = nullptr;
  bone_parents_.clear();
  bone_names_.clear();
  shader_bone_indices_.clear();

  if (data_ != nullptr) {
    delete reinterpret_cast<const std::string *>(data_);
    data_ = nullptr;
  }
}

size_t Mesh::GetNumIndexBufferObjects() const {
  return indices_.size();
}

}  // namespace fplbase
