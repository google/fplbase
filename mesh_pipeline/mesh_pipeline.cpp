// Copyright 2015 Google Inc. All rights reserved.
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

#include "mesh_pipeline.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <cfloat>
#include <fstream>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common_generated.h"
#include "fbx_common/fbx_common.h"
#include "flatbuffers/hash.h"
#include "fplbase/fpl_common.h"
#include "fplutil/file_utils.h"
#include "fplutil/string_utils.h"
#include "materials_generated.h"
#include "mathfu/constants.h"
#include "mathfu/glsl_mappings.h"
#include "mesh_generated.h"

namespace fplbase {

using fplutil::AxisSystem;
using fplutil::IndexOfName;
using fplutil::Logger;
using fplutil::LogLevel;
using fplutil::LogOptions;
using fplutil::kLogVerbose;
using fplutil::kLogInfo;
using fplutil::kLogImportant;
using fplutil::kLogWarning;
using fplutil::kLogError;
using mathfu::vec2;
using mathfu::vec3;
using mathfu::vec4;
using mathfu::mat3;
using mathfu::mat4;
using mathfu::quat;
using mathfu::vec2_packed;
using mathfu::vec3_packed;
using mathfu::vec4_packed;
using mathfu::kZeros2f;
using mathfu::kZeros3f;
using mathfu::kZeros4f;

static const char* const kImageExtensions[] = {"jpg", "jpeg", "png", "webp",
                                               "tga"};
static const FbxColor kDefaultColor(1.0, 1.0, 1.0, 1.0);

// Defines the order in which textures are assigned shader indices.
// Shader indices are assigned, starting from 0, as textures are found.
static const char* kTextureProperties[] = {
    FbxSurfaceMaterial::sDiffuse,
    FbxSurfaceMaterial::sEmissive,
    FbxSurfaceMaterial::sNormalMap,
    FbxSurfaceMaterial::sBump,
    FbxSurfaceMaterial::sDiffuseFactor,
    FbxSurfaceMaterial::sEmissiveFactor,
    FbxSurfaceMaterial::sAmbient,
    FbxSurfaceMaterial::sAmbientFactor,
    FbxSurfaceMaterial::sSpecular,
    FbxSurfaceMaterial::sSpecularFactor,
    FbxSurfaceMaterial::sShininess,
    FbxSurfaceMaterial::sTransparentColor,
    FbxSurfaceMaterial::sTransparencyFactor,
    FbxSurfaceMaterial::sReflection,
    FbxSurfaceMaterial::sReflectionFactor,
};

/// Return the direct index into `element`. If `element` is set up to be indexed
/// directly, the return value is just `index`. Otherwise, we dereference the
/// index array to get the direct index.
template <class T>
static int ElementDirectIndex(const FbxLayerElementTemplate<T>& element,
                              int index) {
  return element.GetReferenceMode() == FbxGeometryElement::eDirect
             ? index
             : element.GetIndexArray().GetAt(index);
}

/// Return element[index], accounting for the index array, if it is used.
template <class T>
static T Element(const FbxLayerElementTemplate<T>& element, int index) {
  const int direct_index = ElementDirectIndex(element, index);
  return element.GetDirectArray().GetAt(direct_index);
}

/// Return element[index], accounting for the index array, if it is used.
template <class T>
static T ElementFromIndices(const FbxLayerElementTemplate<T>* element,
                            int control_index, int vertex_counter) {
  if (!element) return T();
  const int index =
      element->GetMappingMode() == FbxGeometryElement::eByControlPoint
          ? control_index
          : vertex_counter;
  return Element(*element, index);
}

static inline vec4 Vec4FromFbx(const FbxColor& v) {
  return vec4(static_cast<float>(v.mRed), static_cast<float>(v.mGreen),
              static_cast<float>(v.mBlue), static_cast<float>(v.mAlpha));
}

static inline vec4 Vec4FromFbx(const FbxVector4& v) {
  const FbxDouble* d = v.mData;
  return vec4(static_cast<float>(d[0]), static_cast<float>(d[1]),
              static_cast<float>(d[2]), static_cast<float>(d[3]));
}

static inline vec3 Vec3FromFbx(const FbxVector4& v) {
  const FbxDouble* d = v.mData;
  return vec3(static_cast<float>(d[0]), static_cast<float>(d[1]),
              static_cast<float>(d[2]));
}

static inline vec2 Vec2FromFbx(const FbxVector2& v) {
  const FbxDouble* d = v.mData;
  return vec2(static_cast<float>(d[0]), static_cast<float>(d[1]));
}

// FBX UV format has the v-coordinate inverted from OpenGL.
static inline vec2 Vec2FromFbxUv(const FbxVector2& v) {
  const FbxDouble* d = v.mData;
  return vec2(static_cast<float>(d[0]), static_cast<float>(1.0 - d[1]));
}

static inline mat4 Mat4FromFbx(const FbxAMatrix& m) {
  const double* d = m;
  return mat4(static_cast<float>(d[0]), static_cast<float>(d[1]),
              static_cast<float>(d[2]), static_cast<float>(d[3]),
              static_cast<float>(d[4]), static_cast<float>(d[5]),
              static_cast<float>(d[6]), static_cast<float>(d[7]),
              static_cast<float>(d[8]), static_cast<float>(d[9]),
              static_cast<float>(d[10]), static_cast<float>(d[11]),
              static_cast<float>(d[12]), static_cast<float>(d[13]),
              static_cast<float>(d[14]), static_cast<float>(d[15]));
}

static inline Vec4 FlatBufferVec4(const vec4& v) {
  return Vec4(v.x, v.y, v.z, v.w);
}

static inline Vec3 FlatBufferVec3(const vec3& v) { return Vec3(v.x, v.y, v.z); }

static inline Vec2 FlatBufferVec2(const vec2& v) { return Vec2(v.x, v.y); }

static inline Vec4ub FlatBufferVec4ub(const vec4& v) {
  const vec4 scaled =
      static_cast<float>(std::numeric_limits<uint8_t>::max()) * v;
  return Vec4ub(static_cast<uint8_t>(scaled.x), static_cast<uint8_t>(scaled.y),
                static_cast<uint8_t>(scaled.z), static_cast<uint8_t>(scaled.w));
}

static inline Mat3x4 FlatBufferMat3x4(const mat4& matrix) {
  const mat4 m = matrix.Transpose();
  return Mat3x4(Vec4(m(0), m(1), m(2), m(3)), Vec4(m(4), m(5), m(6), m(7)),
                Vec4(m(8), m(9), m(10), m(11)));
}

static void LogVertexAttributes(VertexAttributeBitmask attributes,
                                const char* header, LogLevel level,
                                Logger* log) {
  log->Log(level, "%s", header);
  for (int i = 0; i < kVertexAttribute_Count; ++i) {
    const int i_bit = 1 << i;
    if (attributes & i_bit) {
      const bool prev_attribute_exists = (attributes & (i_bit - 1)) != 0;
      log->Log(level, "%s%s", prev_attribute_exists ? ", " : "",
               kVertexAttributeShortNames[i]);
    }
  }
  log->Log(level, "\n");
}

// Utility function to get the name of a mesh, or the name of the node that owns
// it if the mesh attribute is unnamed (which it commonly is).
static const char* GetMeshOrNodeName(const FbxMesh* mesh) {
  const char* const mesh_name = mesh->GetName();
  if (mesh_name && mesh_name[0]) return mesh_name;
  const FbxNode* const node = mesh->GetNode();
  return node ? node->GetName() : nullptr;
}

// Used for skinning, this maps a vertex to a weighted set of bones.
class SkinBinding {
 public:
  typedef uint16_t BoneIndex;
  typedef uint8_t PackedBoneIndex;
  typedef uint8_t PackedWeight;
  static const unsigned int kInfluenceMax = 4;
  static const BoneIndex kNoBoneIndex = 0xffff;
  static const BoneIndex kBoneIndexMax = 0xfffe;
  static const PackedBoneIndex kPackedBoneIndexMax = 0xff;
  static const PackedWeight kPackedWeightOne = 0xff;

  SkinBinding() { Clear(); }

  const BoneIndex* GetBoneIndices() const { return bone_indices_; }
  const float* GetBoneWeights() const { return bone_weights_; }

  void Clear() {
    for (unsigned int influence_index = 0; influence_index != kInfluenceMax;
         ++influence_index) {
      bone_indices_[influence_index] = kNoBoneIndex;
    }
    for (unsigned int influence_index = 0; influence_index != kInfluenceMax;
         ++influence_index) {
      bone_weights_[influence_index] = 0.0f;
    }
  }

  bool HasInfluences() const { return bone_indices_[0] != kNoBoneIndex; }

  unsigned int CountInfluences() const {
    for (unsigned int influence_index = 0; influence_index != kInfluenceMax;
         ++influence_index) {
      if (bone_indices_[influence_index] == kNoBoneIndex)
        return influence_index;
    }
    return kInfluenceMax;
  }

  void AppendInfluence(unsigned int bone_index, float bone_weight, Logger& log,
                       const FbxMesh* log_mesh, unsigned int log_vertex_index) {
    unsigned int influence_count = CountInfluences();

    // Discard the smallest influence if we reach capacity.
    if (influence_count == kInfluenceMax) {
      const unsigned int smallest_influence_index =
          FindSmallestInfluence(influence_count);
      const float smallest_bone_weight =
          bone_weights_[smallest_influence_index];
      if (smallest_bone_weight < bone_weight) {
        // Existing influence is the smallest.
        const BoneIndex smallest_bone_index =
            bone_indices_[smallest_influence_index];
        EraseInfluence(influence_count, smallest_influence_index);
        --influence_count;
        log.Log(kLogWarning,
                "Too many skin influences (max=%u) for mesh %s vertex %u."
                " Discarding the smallest influence (%f) to bone %u.\n",
                kInfluenceMax, GetMeshOrNodeName(log_mesh), log_vertex_index,
                smallest_bone_weight, smallest_bone_index);
      } else {
        // New influence is the smallest.
        log.Log(kLogWarning,
                "Too many skin influences (max=%u) for mesh %s vertex %u."
                " Discarding the smallest influence (%f) to bone %u.\n",
                kInfluenceMax, GetMeshOrNodeName(log_mesh), log_vertex_index,
                bone_weight, bone_index);
        return;
      }
    }

    // Append the influence.
    assert(bone_index <= kBoneIndexMax);
    bone_indices_[influence_count] = static_cast<BoneIndex>(bone_index);
    bone_weights_[influence_count] = bone_weight;
  }

  // Set the vertex to single-bone rigid binding.
  void BindRigid(BoneIndex bone_index) {
    Clear();
    bone_indices_[0] = bone_index;
    bone_weights_[0] = 1.0f;
  }

  // Normalize weights to sum to 1.0.
  void NormalizeBoneWeights() {
    unsigned int influence_count = 0;
    float bone_weight_sum = 0.0f;
    for (; influence_count != kInfluenceMax; ++influence_count) {
      if (bone_indices_[influence_count] == kNoBoneIndex) break;
      bone_weight_sum += bone_weights_[influence_count];
    }

    if (influence_count == 0) {
      // Vertex not weighted to any bone.  Set full weighting to the origin.
      bone_weights_[0] = 1.0f;
    } else if (bone_weight_sum == 0.0f) {
      // Weights sum to 0.  Probably shouldn't happen, but if it does just
      // evenly distribute weights.
      const float bone_weight = 1.0f / static_cast<float>(influence_count);
      for (unsigned int influence_index = 0; influence_index != kInfluenceMax;
           ++influence_index) {
        bone_weights_[influence_index] = bone_weight;
      }
    } else {
      // Scale weights so they sum to 1.0.
      const float scale = 1.0f / bone_weight_sum;
      for (unsigned int influence_index = 0; influence_index != kInfluenceMax;
           ++influence_index) {
        bone_weights_[influence_index] *= scale;
      }
    }
  }

  // Pack indices and weights to 8-bit components, remapping indices with
  // src_to_dst_index_map.
  void Pack(const BoneIndex* src_to_dst_index_map, size_t src_bone_count,
            Logger& log, const char* log_mesh_name,
            unsigned int log_vertex_index, Vec4ub* out_packed_indices,
            Vec4ub* out_packed_weights) const {
    PackedBoneIndex packed_indices[4] = {0, 0, 0, 0};
    PackedWeight packed_weights[4] = {0, 0, 0, 0};

    const float src_to_dst_scale = static_cast<float>(kPackedWeightOne);
    unsigned int dst_weight_remain = kPackedWeightOne;
    for (unsigned int influence_index = 0; influence_index != kInfluenceMax;
         ++influence_index) {
      const BoneIndex src_index = bone_indices_[influence_index];
      if (src_index == kNoBoneIndex) {
        break;
      }
      assert(src_index < src_bone_count);

      // This bone is referenced, so it shouldn't have been pruned.
      const BoneIndex dst_index = src_to_dst_index_map[src_index];
      assert(dst_index != kNoBoneIndex);

      if (dst_index > kPackedBoneIndexMax) {
        log.Log(kLogWarning,
                "Bone index %u exceeds %u."
                " Discarding skin weight for mesh %s vertex %u.\n",
                dst_index, kPackedBoneIndexMax, log_mesh_name,
                log_vertex_index);
        break;
      }

      // Pack weight, quantizing from float to byte.  The weight is rounded, and
      // we keep track of the total weight remaining so we can distribute
      // quantization error between weights at the end.
      const float src_weight = bone_weights_[influence_index];
      const float dst_weight = src_weight * src_to_dst_scale;
      const unsigned int dst_weight_rounded = std::min(
          static_cast<unsigned int>(dst_weight + 0.5f), dst_weight_remain);
      dst_weight_remain -= dst_weight_rounded;

      packed_indices[influence_index] = static_cast<PackedBoneIndex>(dst_index);
      packed_weights[influence_index] =
          static_cast<PackedWeight>(dst_weight_rounded);
    }

    // Distribute quantization error between weights, so they sum to 255.
    for (; dst_weight_remain; --dst_weight_remain) {
      // Choose the weight to which adding 1 minimizes error.
      unsigned int best_influence_index = 0;
      float diff_min = FLT_MAX;
      for (unsigned int influence_index = 0; influence_index != kInfluenceMax;
           ++influence_index) {
        if (bone_indices_[influence_index] == kNoBoneIndex) {
          break;
        }
        const float src_weight = bone_weights_[influence_index];
        const float dst_weight =
            static_cast<float>(packed_weights[influence_index] + 1);
        const float diff = dst_weight - src_weight * src_to_dst_scale;
        if (diff < diff_min) {
          best_influence_index = influence_index;
          diff_min = diff;
        }
      }
      packed_weights[best_influence_index] += 1;
    }

    *out_packed_indices = Vec4ub(packed_indices[0], packed_indices[1],
                                 packed_indices[2], packed_indices[3]);
    *out_packed_weights = Vec4ub(packed_weights[0], packed_weights[1],
                                 packed_weights[2], packed_weights[3]);
  }

 private:
  BoneIndex bone_indices_[kInfluenceMax];
  float bone_weights_[kInfluenceMax];

  // Find the smallest influence.  If there are multiple smallest influences,
  // this returns the one nearest the end of the array (i.e. most recently
  // added).
  unsigned int FindSmallestInfluence(unsigned int influence_count) const {
    assert(influence_count > 0);
    unsigned int smallest_influence_index = 0;
    for (unsigned int influence_index = 1; influence_index != influence_count;
         ++influence_index) {
      if (bone_weights_[influence_index] <=
          bone_weights_[smallest_influence_index]) {
        smallest_influence_index = influence_index;
      }
    }
    return smallest_influence_index;
  }

  // Erase an influence, preserving the order of the remaining influences.
  void EraseInfluence(unsigned int influence_count,
                      unsigned int influence_index) {
    assert(influence_index < influence_count);
    const unsigned int last_influence_index = influence_count - 1;
    for (; influence_index != last_influence_index; ++influence_index) {
      bone_indices_[influence_index] = bone_indices_[influence_index + 1];
      bone_weights_[influence_index] = bone_weights_[influence_index + 1];
    }
    bone_indices_[last_influence_index] = kNoBoneIndex;
    bone_weights_[last_influence_index] = 0.0f;
  }
};

class FlatTextures {
 public:
  size_t Count() const { return textures_.size(); }
  void Append(const std::string& texture) { textures_.push_back(texture); }

  // Access the ith texture.
  const std::string& operator[](size_t i) const {
    assert(0 <= i && i < Count());
    return textures_[i];
  }

  // Required for std::unordered_set.
  bool operator==(const FlatTextures& rhs) const {
    if (Count() != rhs.Count()) return false;
    for (size_t i = 0; i < Count(); ++i) {
      if (textures_[0] != rhs.textures_[0]) return false;
    }
    return true;
  }

 private:
  std::vector<std::string> textures_;
};

// Required for std::unordered_set. Only compare the primary texture.
class FlatTextureHash {
 public:
  size_t operator()(const FlatTextures& t) const {
    size_t hash = 0;
    for (size_t i = 0; i < t.Count(); ++i) {
      hash ^= std::hash<std::string>()(t[i]);
    }
    return hash;
  }
};

class FlatMesh {
 public:
  explicit FlatMesh(int max_verts, VertexAttributeBitmask vertex_attributes,
                    Logger& log)
      : points_(max_verts),
        cur_index_buf_(nullptr),
        mesh_vertex_attributes_(0),
        vertex_attributes_(vertex_attributes),
        log_(log) {
    points_.clear();
  }

  unsigned int AppendBone(const char* bone_name,
                          const mat4& default_bone_transform_inverse,
                          int parent_bone_index) {
    const unsigned int bone_index = static_cast<unsigned int>(bones_.size());
    bones_.push_back(
        Bone(bone_name, default_bone_transform_inverse, parent_bone_index));
    return bone_index;
  }

  void UpdateDefaultBoneTransformInverse(unsigned int bone_index,
                                         const mat4& transform) {
    if (bone_index < bones_.size()) {
      transform.Pack(bones_[bone_index].default_bone_transform_inverse);
    }
  }

  void SetSurface(const FlatTextures& textures) {
    // Grab existing surface for `texture_file_name`, or create a new one.
    IndexBuffer& index_buffer = surfaces_[textures];

    // Update the current index buffer to which we're logging control points.
    cur_index_buf_ = &index_buffer;

    // Log the surface switch.
    log_.Log(kLogVerbose, "Surface:");
    for (size_t i = 0; i < textures.Count(); ++i) {
      log_.Log(kLogVerbose, " %s", textures[i].c_str());
    }
    log_.Log(kLogVerbose, "\n");
  }

  void ReportSurfaceVertexAttributes(
      VertexAttributeBitmask surface_vertex_attributes) {
    // Warn when some surfaces have requested attributes but others do not.
    const VertexAttributeBitmask missing_attributes =
        vertex_attributes_ & mesh_vertex_attributes_ &
        ~surface_vertex_attributes;
    if (missing_attributes) {
      LogVertexAttributes(
          missing_attributes,
          "Surface missing vertex attributes that are in previous surfaces: ",
          kLogWarning, &log_);
    }

    // Remember which attributes exist so that we can output only those that
    // we recorded, if so requested.
    mesh_vertex_attributes_ |= surface_vertex_attributes;
  }

  // Populate a single surface with data from FBX arrays.
  void AppendPolyVert(const vec3& vertex, const vec3& normal,
                      const vec4& tangent, const vec4& orientation,
                      const vec4& color, const vec2& uv, const vec2& uv_alt,
                      const SkinBinding& skin_binding) {
    // The `unique_` map holds pointers into `points_`, so we cannot realloc
    // `points_`. Instead, we reserve enough memory for an upper bound on
    // its size. If this assert hits, NumVertsUpperBound() is incorrect.
    assert(points_.capacity() > points_.size());

    // TODO: Round values before creating.
    points_.push_back(Vertex(vertex_attributes_, vertex, normal, tangent,
                             orientation, color, uv, uv_alt, skin_binding));

    const VertexRef ref_to_insert(&points_.back(), points_.size() - 1);
    auto insertion = unique_.insert(ref_to_insert);

    // The `insert` call returns two values: iterator and success.
    const VertexRef& ref = *insertion.first;
    const bool new_control_point_created = insertion.second;

    // We recycled an existing point, so we can remove the one we added with
    // push_back().
    if (!new_control_point_created) {
      points_.pop_back();
    }

    // Append index of polygon point.
    cur_index_buf_->push_back(ref.index);

    // Log the data we just added.
    if (log_.level() <= kLogVerbose) {
      log_.Log(kLogVerbose, "Point: index %d", ref.index);
      if (new_control_point_created) {
        const VertexAttributeBitmask attributes =
            vertex_attributes_ & mesh_vertex_attributes_;
        if (attributes & kVertexAttributeBit_Position) {
          log_.Log(kLogVerbose, ", vertex (%.3f, %.3f, %.3f)", vertex.x,
                   vertex.y, vertex.z);
        }
        if (attributes & kVertexAttributeBit_Normal) {
          log_.Log(kLogVerbose, ", normal (%.3f, %.3f, %.3f)", normal.x,
                   normal.y, normal.z);
        }
        if (attributes & kVertexAttributeBit_Tangent) {
          log_.Log(kLogVerbose,
                   ", tangent (%.3f, %.3f, %.3f) binormal-handedness %.0f",
                   tangent.x, tangent.y, tangent.z, tangent.w);
        }
        if (attributes & kVertexAttributeBit_Orientation) {
          log_.Log(kLogVerbose,
                   ", orientation (%.3f, %.3f, %.3f, scalar %.3f)",
                   orientation.x, orientation.y, orientation.z, orientation.w);
        }
        if (attributes & kVertexAttributeBit_Uv) {
          log_.Log(kLogVerbose, ", uv (%.3f, %.3f)", uv.x, uv.y);
        }
        if (attributes & kVertexAttributeBit_UvAlt) {
          log_.Log(kLogVerbose, ", uv-alt (%.3f, %.3f)", uv_alt.x, uv_alt.y);
        }
        if (attributes & kVertexAttributeBit_Color) {
          log_.Log(kLogVerbose, ", color (%.3f, %.3f, %.3f, %.3f)", color.x,
                   color.y, color.z, color.w);
        }
        if (attributes & kVertexAttributeBit_Bone) {
          const BoneIndex* const bone_indices = skin_binding.GetBoneIndices();
          const float* const bone_weights = skin_binding.GetBoneWeights();
          log_.Log(kLogVerbose, ", skin (%u:%.3f, %u:%.3f, %u:%.3f, %u:%.3f)",
                   bone_indices[0], bone_weights[0], bone_indices[1],
                   bone_weights[1], bone_indices[2], bone_weights[2],
                   bone_indices[3], bone_weights[3]);
        }
      }
      log_.Log(kLogVerbose, "\n");
    }
  }

  // Output material and mesh flatbuffers for the gathered surfaces.
  bool OutputFlatBuffer(
      const std::string& mesh_name_unformated,
      const std::string& assets_base_dir_unformated,
      const std::string& assets_sub_dir_unformated,
      const std::string& texture_extension,
      const std::vector<matdef::TextureFormat>& texture_formats,
      matdef::BlendMode blend_mode, bool interleaved, bool force32,
      bool embed_materials) const {
    // Ensure directory names end with a slash.
    const std::string mesh_name = fplutil::BaseFileName(mesh_name_unformated);
    const std::string assets_base_dir =
        fplutil::FormatAsDirectoryName(assets_base_dir_unformated);
    const std::string assets_sub_dir =
        fplutil::FormatAsDirectoryName(assets_sub_dir_unformated);

    // Ensure output directory exists.
    const std::string assets_dir = assets_base_dir + assets_sub_dir;
    if (!fplutil::CreateDirectory(assets_dir.c_str())) {
      log_.Log(kLogError, "Could not create output directory %s\n",
               assets_dir.c_str());
      return false;
    }

    // Output bone hierarchy.
    LogBones();

    if (!embed_materials) {
      // Create material files that reference the textures.
      OutputMaterialFlatBuffers(mesh_name, assets_base_dir, assets_sub_dir,
                                texture_extension, texture_formats, blend_mode);
    }

    // Create final mesh file that references materials relative to
    // `assets_base_dir`.
    OutputMeshFlatBuffer(mesh_name, assets_base_dir, assets_sub_dir,
                         texture_extension, texture_formats, blend_mode,
                         interleaved, force32, embed_materials);

    // Log summary
    log_.Log(kLogImportant, "  %s (%d vertices, %d triangles)\n",
             (mesh_name + '.' + meshdef::MeshExtension()).c_str(),
             points_.size(), NumTriangles());
    return true;
  }

  int NumTriangles() const {
    size_t num_indices = 0;
    for (auto it = surfaces_.begin(); it != surfaces_.end(); ++it) {
      const IndexBuffer& index_buf = it->second;
      num_indices += index_buf.size();
    }
    return static_cast<int>(num_indices / 3);
  }

  static std::string RepeatCharacter(char c, int count) {
    std::string s;
    for (int i = 0; i < count; ++i) {
      s += c;
    }
    return s;
  }

  void LogBones() const {
    std::vector<BoneIndex> mesh_to_shader_bones;
    std::vector<BoneIndex> shader_to_mesh_bones;
    CalculateBoneIndexMaps(&mesh_to_shader_bones, &shader_to_mesh_bones);

    log_.Log(kLogInfo, "Mesh hierarchy (bone indices in brackets):\n");
    for (size_t j = 0; j < bones_.size(); ++j) {
      const Bone& b = bones_[j];
      std::string indent = RepeatCharacter(
          ' ', 2 * BoneDepth(static_cast<int>(j)));

      // Output bone name and index, indented to match the depth in the
      // hierarchy.
      const BoneIndex shader_bone = mesh_to_shader_bones[j];
      const bool has_verts = shader_bone != kInvalidBoneIdx;
      log_.Log(kLogInfo, "  %s[%d] %s", indent.c_str(), j, b.name.c_str());
      if (has_verts) {
        log_.Log(kLogInfo, " (shader bone %d)", shader_bone);
      }
      log_.Log(kLogInfo, "\n");

      // Output global-to-local matrix transform too.
      const mat4 t(b.default_bone_transform_inverse);
      for (int k = 0; k < 3; ++k) {
        log_.Log(kLogVerbose, "   %s  (%.3f, %.3f, %.3f, %.3f)\n",
                 indent.c_str(), t(k, 0), t(k, 1), t(k, 2), t(k, 3));
      }
    }
  }

  void CalculateMinMaxPosition(vec3* min_position, vec3* max_position) const {
    vec3 max(-FLT_MAX);
    vec3 min(FLT_MAX);

    // Loop through every vertex position.
    // Note that vertex positions are always in object space.
    for (size_t i = 0; i < points_.size(); ++i) {
      const vec3 position = vec3(points_[i].vertex);
      min = vec3::Min(min, position);
      max = vec3::Max(max, position);
    }

    *min_position = min;
    *max_position = max;
  }

 private:
  FPL_DISALLOW_COPY_AND_ASSIGN(FlatMesh);
  typedef SkinBinding::BoneIndex BoneIndex;
  typedef uint8_t BoneIndexCompact;
  typedef uint32_t VertIndex;
  typedef uint16_t VertIndexCompact;
  typedef std::vector<VertIndex> IndexBuffer;
  typedef std::vector<VertIndexCompact> IndexBufferCompact;

  // We use uint8_t for bone indices, and 0xFF marks invalid bones,
  // so the limit is 254.
  static const BoneIndex kMaxBoneIndex = SkinBinding::kPackedBoneIndexMax;
  static const BoneIndex kInvalidBoneIdx = SkinBinding::kNoBoneIndex;
  static const BoneIndexCompact kInvalidBoneIdxCompact = 0xFF;

  // We use uint16_t for vertex indices. It's possible for a large mesh to have
  // more vertices than that, so we output an error in that case.
  static const VertIndex kMaxVertexIndex = 0xFFFF;
  static const VertIndex kMaxNumPoints = kMaxVertexIndex + 1;

  struct Vertex {
    vec3_packed vertex;
    vec3_packed normal;
    vec4_packed tangent;  // 4th element is handedness: +1 or -1
    vec4_packed orientation;
    vec2_packed uv;
    vec2_packed uv_alt;
    Vec4ub color;  // Use byte-format to ensure correct hashing.
    SkinBinding skin_binding;
    Vertex() : color(0, 0, 0, 0) {
      // The Hash function operates on all the memory, so ensure everything is
      // zero'd out.
      memset(this, 0, sizeof(*this));
      this->skin_binding = SkinBinding();
    }
    // Only record the attributes that we're asked to record. Ignore the rest.
    Vertex(VertexAttributeBitmask attribs, const vec3& p, const vec3& n,
           const vec4& t, const vec4& q, const vec4& c,
           const vec2& u, const vec2& v, const SkinBinding& skin_binding)
        : vertex(attribs & kVertexAttributeBit_Position ? p : kZeros3f),
          normal(attribs & kVertexAttributeBit_Normal ? n : kZeros3f),
          tangent(attribs & kVertexAttributeBit_Tangent ? t : kZeros4f),
          orientation(attribs & kVertexAttributeBit_Orientation ? q : kZeros4f),
          uv(attribs & kVertexAttributeBit_Uv ? u : kZeros2f),
          uv_alt(attribs & kVertexAttributeBit_UvAlt ? v : kZeros2f),
          color(attribs & kVertexAttributeBit_Color ? FlatBufferVec4ub(c)
                                                    : Vec4ub(0, 0, 0, 0)) {
      if (attribs & kVertexAttributeBit_Bone) this->skin_binding = skin_binding;
    }
  };

  struct VertexRef {
    const Vertex* ref;
    VertIndex index;
    VertexRef(const Vertex* v, size_t index)
        : ref(v), index(static_cast<VertIndex>(index)) {
      assert(index <= std::numeric_limits<VertIndex>::max());
    }
  };

  struct VertexHash {
    uint64_t operator()(const VertexRef& c) const {
      return flatbuffers::HashFnv1a<uint64_t>(
          reinterpret_cast<const char*>(c.ref));
    }
  };

  struct VerticesEqual {
    bool operator()(const VertexRef& a, const VertexRef& b) const {
      return memcmp(a.ref, b.ref, sizeof(*a.ref)) == 0;
    }
  };

  struct Bone {
    std::string name;
    int parent_bone_index;
    vec4_packed default_bone_transform_inverse[4];
    Bone() : parent_bone_index(-1) {}
    Bone(const char* name, const mat4& default_bone_transform_inverse,
         int parent_bone_index)
        : name(name), parent_bone_index(parent_bone_index) {
      default_bone_transform_inverse.Pack(
          this->default_bone_transform_inverse);
    }
  };

  typedef std::unordered_map<FlatTextures, IndexBuffer, FlatTextureHash>
      SurfaceMap;
  typedef std::unordered_set<VertexRef, VertexHash, VerticesEqual> VertexSet;

  static bool HasTexture(const FlatTextures& textures) {
    return textures.Count() > 0;
  }

  static std::string TextureBaseFileName(const std::string& texture_file_name,
                                         const std::string& assets_sub_dir) {
    assert(texture_file_name != "");
    return assets_sub_dir + fplutil::BaseFileName(texture_file_name);
  }

  static std::string TextureFileName(const std::string& texture_file_name,
                                     const std::string& assets_sub_dir,
                                     const std::string& texture_extension) {
    const std::string extension =
        texture_extension.length() == 0
            ? fplutil::FileExtension(texture_file_name)
            : texture_extension;
    return TextureBaseFileName(texture_file_name, assets_sub_dir) + '.' +
           extension;
  }

  std::string MaterialFileName(const std::string& mesh_name, size_t surface_idx,
                               const std::string& assets_sub_dir) const {
    std::string name = TextureBaseFileName(mesh_name, assets_sub_dir);
    if (surfaces_.size() > 1) {
      std::stringstream ss;
      ss << "_" << surface_idx;
      name += ss.str();
    }
    name += std::string(".") + matdef::MaterialExtension();
    return name;
  }

  void OutputFlatBufferBuilder(const flatbuffers::FlatBufferBuilder& fbb,
                               const std::string& file_name) const {
    // Open the file.
    FILE* file = fopen(file_name.c_str(), "wb");
    if (file == nullptr) {
      log_.Log(kLogError, "Could not open %s for writing\n", file_name.c_str());
      return;
    }

    // Write the binary data to the file and close it.
    // TODO: Add option to write json file too.
    log_.Log(kLogVerbose, "Writing %s\n", file_name.c_str());
    fwrite(fbb.GetBufferPointer(), 1, fbb.GetSize(), file);
    fclose(file);
  }


  flatbuffers::Offset<matdef::Material> BuildMaterialFlatBuffer(
      flatbuffers::FlatBufferBuilder& fbb, const std::string& assets_sub_dir,
      const std::string& texture_extension,
      const std::vector<matdef::TextureFormat>& texture_formats,
      matdef::BlendMode blend_mode, const FlatTextures& textures) const {
    // Create FlatBuffer arrays of texture names and formats.
    std::vector<flatbuffers::Offset<flatbuffers::String>> textures_fb;
    std::vector<uint8_t> formats_fb;
    textures_fb.reserve(textures.Count());
    formats_fb.reserve(textures.Count());
    for (size_t i = 0; i < textures.Count(); ++i) {
      // Output texture file name to array of file names.
      const std::string texture_file_name =
          TextureFileName(textures[i], assets_sub_dir, texture_extension);
      textures_fb.push_back(fbb.CreateString(texture_file_name));

      // Append texture format (a uint8) to array of texture formats.
      const matdef::TextureFormat texture_format =
          i < texture_formats.size() ? texture_formats[i]
                                     : kDefaultTextureFormat;
      formats_fb.push_back(static_cast<uint8_t>(texture_format));

      // Log texture and format.
      log_.Log(kLogInfo, "%s %s", i == 0 ? "" : ",",
               fplutil::RemoveDirectoryFromName(texture_file_name).c_str());
      if (texture_format != kDefaultTextureFormat) {
        log_.Log(kLogInfo, "(%s)",
                 matdef::EnumNameTextureFormat(texture_format));
      }
    }
    log_.Log(kLogInfo, "\n");

    // Create final material FlatBuffer.
    auto textures_vector_fb = fbb.CreateVector(textures_fb);
    auto formats_vector_fb = fbb.CreateVector(formats_fb);
    auto material_fb = matdef::CreateMaterial(fbb, textures_vector_fb,
                                              blend_mode, formats_vector_fb);
    return material_fb;
  }

  VertIndex GetMaxIndex(const IndexBuffer& indices) const {
    return indices.empty() ? 0
                           : *std::max_element(indices.begin(), indices.end());
  }

  flatbuffers::Offset<meshdef::Mesh> BuildMeshFlatBuffer(
      flatbuffers::FlatBufferBuilder& fbb, const std::string& mesh_name,
      const std::string& assets_sub_dir, const std::string& texture_extension,
      const std::vector<matdef::TextureFormat>& texture_formats,
      matdef::BlendMode blend_mode, bool interleaved, bool force32,
      bool embed_materials) const {
    const VertexAttributeBitmask attributes =
        vertex_attributes_ == kVertexAttributeBit_AllAttributesInSourceFile
            ? mesh_vertex_attributes_
            : vertex_attributes_;
    LogVertexAttributes(attributes, "  Vertex attributes: ", kLogInfo, &log_);

    // Bone count is limited since we index with an 8-bit value.
    const bool bone_overflow = bones_.size() > kMaxBoneIndex;
    if (bone_overflow && (attributes & kVertexAttributeBit_Bone)) {
      log_.Log(kLogError,
               "Bone count %d exeeds maximum %d. "
               "Verts weighted to bones beyond %d will instead be weighted"
               " to bone 0.\n",
               bones_.size(), kMaxBoneIndex, kMaxBoneIndex);
    }

    // Get the mapping from mesh bones (i.e. all bones in the model)
    // to shader bones (i.e. bones that have verts weighted to them).
    std::vector<BoneIndex> mesh_to_shader_bones;
    std::vector<BoneIndex> shader_to_mesh_bones;
    CalculateBoneIndexMaps(&mesh_to_shader_bones, &shader_to_mesh_bones);

    // Output the surfaces.
    std::vector<flatbuffers::Offset<meshdef::Surface>> surfaces_fb;
    surfaces_fb.reserve(surfaces_.size());
    size_t surface_idx = 0;
    IndexBufferCompact index_buf_compact;
    for (auto it = surfaces_.begin(); it != surfaces_.end(); ++it) {
      const FlatTextures& textures = it->first;
      const IndexBuffer& index_buf = it->second;
      const std::string material_file_name =
          HasTexture(textures)
              ? MaterialFileName(mesh_name, surface_idx, assets_sub_dir)
              : std::string("");
      auto material_fb = fbb.CreateString(material_file_name);
      log_.Log(kLogInfo, "  Surface %d (%s) has %d triangles\n", surface_idx,
               material_file_name.length() == 0 ? "unnamed"
                                                : material_file_name.c_str(),
               index_buf.size() / 3);
      flatbuffers::Offset<flatbuffers::Vector<VertIndexCompact>> indices_fb = 0;
      flatbuffers::Offset<flatbuffers::Vector<VertIndex>> indices32_fb = 0;
      if (!force32 && GetMaxIndex(index_buf) <= kMaxVertexIndex) {
        CopyIndexBuf(index_buf, &index_buf_compact);
        indices_fb = fbb.CreateVector(index_buf_compact);
      } else {
        indices32_fb = fbb.CreateVector(index_buf);
      }

      flatbuffers::Offset<matdef::Material> material_data_fb = 0;
      if (embed_materials && HasTexture(textures)) {
        log_.Log(kLogInfo, "  %s:", material_file_name.c_str());
        material_data_fb =
            BuildMaterialFlatBuffer(fbb, assets_sub_dir, texture_extension,
                                    texture_formats, blend_mode, textures);
      }

      auto surface_fb = meshdef::CreateSurface(fbb, indices_fb, material_fb,
                                               indices32_fb, material_data_fb);
      surfaces_fb.push_back(surface_fb);
      surface_idx++;
    }
    auto surface_vector_fb = fbb.CreateVector(surfaces_fb);

    // Output the mesh.

    // Output the bone transforms, for skinning, and the bone names,
    // for debugging.
    std::vector<flatbuffers::Offset<flatbuffers::String>> bone_names;
    std::vector<Mat3x4> bone_transforms;
    std::vector<BoneIndexCompact> bone_parents;
    bone_names.reserve(bones_.size());
    bone_transforms.reserve(bones_.size());
    bone_parents.reserve(bones_.size());
    for (size_t i = 0; i < bones_.size(); ++i) {
      const Bone& bone = bones_[i];
      bone_names.push_back(fbb.CreateString(bone.name));
      bone_transforms.push_back(
          FlatBufferMat3x4(mat4(bone.default_bone_transform_inverse)));
      bone_parents.push_back(
          TruncateBoneIndex(BoneParent(static_cast<int>(i))));
    }

    // Compact the shader to mesh bone map.
    std::vector<BoneIndexCompact> shader_to_mesh_bones_compact;
    shader_to_mesh_bones_compact.reserve(shader_to_mesh_bones.size());
    for (size_t i = 0; i < shader_to_mesh_bones.size(); ++i) {
      shader_to_mesh_bones_compact.push_back(
          TruncateBoneIndex(shader_to_mesh_bones[i]));
    }

    // Get the overal min/max values, in object space.
    vec3 min_position;
    vec3 max_position;
    CalculateMinMaxPosition(&min_position, &max_position);

    const size_t num_points = points_.size();
    auto max_fb = FlatBufferVec3(max_position);
    auto min_fb = FlatBufferVec3(min_position);
    auto bone_names_fb = fbb.CreateVector(bone_names);
    auto bone_transforms_fb = fbb.CreateVectorOfStructs(bone_transforms);
    auto bone_parents_fb = fbb.CreateVector(bone_parents);
    auto shader_to_mesh_bones_fb =
        fbb.CreateVector(shader_to_mesh_bones_compact);

    if (interleaved) {
      std::vector<uint8_t> format;
      size_t vert_size = 0;
      if (attributes & kVertexAttributeBit_Position) {
        format.push_back(meshdef::Attribute_Position3f);
        vert_size += sizeof(vec3_packed);
      }
      if (attributes & kVertexAttributeBit_Normal) {
        format.push_back(meshdef::Attribute_Normal3f);
        vert_size += sizeof(vec3_packed);
      }
      if (attributes & kVertexAttributeBit_Tangent) {
        format.push_back(meshdef::Attribute_Tangent4f);
        vert_size += sizeof(vec4_packed);
      }
      if (attributes & kVertexAttributeBit_Orientation) {
        format.push_back(meshdef::Attribute_Orientation4f);
        vert_size += sizeof(vec4_packed);
      }
      if (attributes & kVertexAttributeBit_Uv) {
        format.push_back(meshdef::Attribute_TexCoord2f);
        vert_size += sizeof(vec2_packed);
      }
      if (attributes & kVertexAttributeBit_UvAlt) {
        format.push_back(meshdef::Attribute_TexCoordAlt2f);
        vert_size += sizeof(vec2_packed);
      }
      if (attributes & kVertexAttributeBit_Color) {
        format.push_back(meshdef::Attribute_Color4ub);
        vert_size += sizeof(Vec4ub);
      }
      if (attributes & kVertexAttributeBit_Bone) {
        format.push_back(meshdef::Attribute_BoneIndices4ub);
        format.push_back(meshdef::Attribute_BoneWeights4ub);
        vert_size += sizeof(Vec4ub) + sizeof(Vec4ub);
      }
      format.push_back(meshdef::Attribute_END);
      std::vector<uint8_t> iattrs;
      iattrs.reserve(num_points * vert_size);
      // TODO(wvo): this is only valid on little-endian.
      for (size_t i = 0; i < num_points; ++i) {
        const Vertex& p = points_[i];
        if (attributes & kVertexAttributeBit_Position) {
          auto attr = reinterpret_cast<const uint8_t *>(&p.vertex);
          iattrs.insert(iattrs.end(), attr, attr + sizeof(vec3_packed));
        }
        if (attributes & kVertexAttributeBit_Normal) {
          auto attr = reinterpret_cast<const uint8_t *>(&p.normal);
          iattrs.insert(iattrs.end(), attr, attr + sizeof(vec3_packed));
        }
        if (attributes & kVertexAttributeBit_Tangent) {
          auto attr = reinterpret_cast<const uint8_t *>(&p.tangent);
          iattrs.insert(iattrs.end(), attr, attr + sizeof(vec4_packed));
        }
        if (attributes & kVertexAttributeBit_Orientation) {
          auto attr = reinterpret_cast<const uint8_t *>(&p.orientation);
          iattrs.insert(iattrs.end(), attr, attr + sizeof(vec4_packed));
        }
        if (attributes & kVertexAttributeBit_Uv) {
          auto attr = reinterpret_cast<const uint8_t *>(&p.uv);
          iattrs.insert(iattrs.end(), attr, attr + sizeof(vec2_packed));
        }
        if (attributes & kVertexAttributeBit_UvAlt) {
          auto attr = reinterpret_cast<const uint8_t *>(&p.uv_alt);
          iattrs.insert(iattrs.end(), attr, attr + sizeof(vec2_packed));
        }
        if (attributes & kVertexAttributeBit_Color) {
          auto attr = reinterpret_cast<const uint8_t *>(&p.color);
          iattrs.insert(iattrs.end(), attr, attr + sizeof(Vec4ub));
        }
        if (attributes & kVertexAttributeBit_Bone) {
          Vec4ub bone, weights;
          p.skin_binding.Pack(mesh_to_shader_bones.data(),
                              mesh_to_shader_bones.size(), log_,
                              mesh_name.c_str(), static_cast<unsigned int>(i),
                              &bone, &weights);
          auto attr = reinterpret_cast<const uint8_t *>(&bone);
          iattrs.insert(iattrs.end(), attr, attr + sizeof(Vec4ub));
          attr = reinterpret_cast<const uint8_t *>(&weights);
          iattrs.insert(iattrs.end(), attr, attr + sizeof(Vec4ub));
        }
      }
      assert(vert_size * num_points == iattrs.size());
      auto formatvec = fbb.CreateVector(format);
      auto attrvec = fbb.CreateVector(iattrs);
      return meshdef::CreateMesh(
          fbb, surface_vector_fb, 0, 0, 0, 0,
          0, 0, 0, &max_fb, &min_fb,
          bone_names_fb, bone_transforms_fb, bone_parents_fb,
          shader_to_mesh_bones_fb, 0, meshdef::MeshVersion_MostRecent,
          formatvec, attrvec);
    } else {
      // First convert to structure-of-array format.
      std::vector<Vec3> vertices;
      std::vector<Vec3> normals;
      std::vector<Vec4> tangents;
      std::vector<Vec4> orientations;
      std::vector<Vec4ub> colors;
      std::vector<Vec2> uvs;
      std::vector<Vec2> uvs_alt;
      std::vector<Vec4ub> skin_indices;
      std::vector<Vec4ub> skin_weights;
      vertices.reserve(num_points);
      normals.reserve(num_points);
      tangents.reserve(num_points);
      orientations.reserve(num_points);
      colors.reserve(num_points);
      uvs.reserve(num_points);
      uvs_alt.reserve(num_points);
      skin_indices.reserve(num_points);
      skin_weights.reserve(num_points);
      for (size_t i = 0; i < num_points; ++i) {
        const Vertex& p = points_[i];
        vertices.push_back(FlatBufferVec3(vec3(p.vertex)));
        normals.push_back(FlatBufferVec3(vec3(p.normal)));
        tangents.push_back(FlatBufferVec4(vec4(p.tangent)));
        orientations.push_back(FlatBufferVec4(vec4(p.orientation)));
        colors.push_back(p.color);
        uvs.push_back(FlatBufferVec2(vec2(p.uv)));
        uvs_alt.push_back(FlatBufferVec2(vec2(p.uv_alt)));

        Vec4ub bone, weights;
        p.skin_binding.Pack(mesh_to_shader_bones.data(),
                            mesh_to_shader_bones.size(), log_,
                            mesh_name.c_str(), static_cast<unsigned int>(i),
                            &bone, &weights);
        skin_indices.push_back(bone);
        skin_weights.push_back(weights);
      }
      // Then create a FlatBuffer vector for each array that we want to export.
      auto vertices_fb = (attributes & kVertexAttributeBit_Position)
                             ? fbb.CreateVectorOfStructs(vertices)
                             : 0;
      auto normals_fb = (attributes & kVertexAttributeBit_Normal)
                            ? fbb.CreateVectorOfStructs(normals)
                            : 0;
      auto tangents_fb = (attributes & kVertexAttributeBit_Tangent)
                             ? fbb.CreateVectorOfStructs(tangents)
                             : 0;
      auto orientations_fb = (attributes & kVertexAttributeBit_Orientation)
                                 ? fbb.CreateVectorOfStructs(orientations)
                                 : 0;
      auto colors_fb = (attributes & kVertexAttributeBit_Color)
                           ? fbb.CreateVectorOfStructs(colors)
                           : 0;
      auto uvs_fb = (attributes & kVertexAttributeBit_Uv)
                        ? fbb.CreateVectorOfStructs(uvs)
                        : 0;
      auto uvs_alt_fb = (attributes & kVertexAttributeBit_UvAlt)
                            ? fbb.CreateVectorOfStructs(uvs_alt)
                            : 0;
      auto skin_indices_fb = (attributes & kVertexAttributeBit_Bone)
                                 ? fbb.CreateVectorOfStructs(skin_indices)
                                 : 0;
      auto skin_weights_fb = (attributes & kVertexAttributeBit_Bone)
                                 ? fbb.CreateVectorOfStructs(skin_weights)
                                 : 0;
      return meshdef::CreateMesh(
          fbb, surface_vector_fb, vertices_fb, normals_fb, tangents_fb,
          colors_fb, uvs_fb, skin_indices_fb, skin_weights_fb, &max_fb, &min_fb,
          bone_names_fb, bone_transforms_fb, bone_parents_fb,
          shader_to_mesh_bones_fb, uvs_alt_fb, meshdef::MeshVersion_MostRecent,
          /* attributes = */ 0, /* vertices = */ 0, orientations_fb);
    }
  }

  void OutputMeshFlatBuffer(
      const std::string& mesh_name, const std::string& assets_base_dir,
      const std::string& assets_sub_dir, const std::string& texture_extension,
      const std::vector<matdef::TextureFormat>& texture_formats,
      matdef::BlendMode blend_mode, bool interleaved, bool force32,
      bool embed_materials) const {
    const std::string rel_mesh_file_name =
        assets_sub_dir + mesh_name + "." + meshdef::MeshExtension();
    const std::string full_mesh_file_name =
        assets_base_dir + rel_mesh_file_name;

    log_.Log(kLogInfo, "Mesh:\n");

    flatbuffers::FlatBufferBuilder fbb;
    auto mesh_fb = BuildMeshFlatBuffer(
        fbb, mesh_name, assets_sub_dir, texture_extension, texture_formats,
        blend_mode, interleaved, force32, embed_materials);

    meshdef::FinishMeshBuffer(fbb, mesh_fb);

    // Write the buffer to a file.
    OutputFlatBufferBuilder(fbb, full_mesh_file_name);
  }

  void OutputMaterialFlatBuffers(
      const std::string& mesh_name, const std::string& assets_base_dir,
      const std::string& assets_sub_dir, const std::string& texture_extension,
      const std::vector<matdef::TextureFormat>& texture_formats,
      matdef::BlendMode blend_mode) const {
    log_.Log(kLogInfo, "Materials:\n");

    size_t surface_idx = 0;
    for (auto it = surfaces_.begin(); it != surfaces_.end(); ++it) {
      const FlatTextures& textures = it->first;
      if (!HasTexture(textures)) {
        ++surface_idx;
        continue;
      }

      const std::string material_file_name =
          MaterialFileName(mesh_name, surface_idx, assets_sub_dir);
      log_.Log(kLogInfo, "  %s:", material_file_name.c_str());

      flatbuffers::FlatBufferBuilder fbb;
      auto material_fb =
          BuildMaterialFlatBuffer(fbb, assets_sub_dir, texture_extension,
                                  texture_formats, blend_mode, textures);
      matdef::FinishMaterialBuffer(fbb, material_fb);

      const std::string full_material_file_name =
          assets_base_dir + material_file_name;
      OutputFlatBufferBuilder(fbb, full_material_file_name);

      surface_idx++;
    }

    // Log blend mode, if blend mode is being used.
    if (blend_mode != matdef::BlendMode_OFF) {
      log_.Log(kLogInfo, "  blend mode: %s\n",
               matdef::EnumNameBlendMode(blend_mode));
    }
  }

  int BoneParent(int i) const { return bones_[i].parent_bone_index; }

  unsigned int BoneDepth(int i) const {
    unsigned int depth = 0;
    for (;;) {
      i = bones_[i].parent_bone_index;
      if (i < 0) break;
      ++depth;
    }
    return depth;
  }

  mat4 BoneGlobalTransform(int i) const {
    mat4 m(bones_[i].default_bone_transform_inverse);
    for (;;) {
      i = BoneParent(i);
      if (i < 0) break;

      // Update with parent transform.
      m = mat4(bones_[i].default_bone_transform_inverse) * m;
    }
    return m;
  }

  // Inspect vertices to determine which bones are referenced.
  void GetUsedBoneFlags(std::vector<bool>* out_used_bone_flags) const {
    std::vector<bool> used_bone_flags(bones_.size());
    const Vertex* vertex = points_.data();
    const Vertex* const vertex_end = vertex + points_.size();
    for (; vertex != vertex_end; ++vertex) {
      const BoneIndex* bone_index_it = vertex->skin_binding.GetBoneIndices();
      const BoneIndex* const bone_index_end =
          bone_index_it + SkinBinding::kInfluenceMax;
      for (; bone_index_it != bone_index_end; ++bone_index_it) {
        const BoneIndex bone_index = *bone_index_it;
        if (bone_index == SkinBinding::kNoBoneIndex) {
          break;
        }
        used_bone_flags[bone_index] = true;
      }
    }
    out_used_bone_flags->swap(used_bone_flags);
  }

  void CalculateBoneIndexMaps(
      std::vector<BoneIndex>* mesh_to_shader_bones,
      std::vector<BoneIndex>* shader_to_mesh_bones) const {
    mesh_to_shader_bones->clear();
    shader_to_mesh_bones->clear();

    std::vector<bool> used_bone_flags;
    GetUsedBoneFlags(&used_bone_flags);

    // Only bones that have vertices weighted to them are uploaded to the
    // shader.
    BoneIndex shader_bone = 0;
    mesh_to_shader_bones->reserve(bones_.size());
    for (BoneIndex mesh_bone = 0; mesh_bone < bones_.size(); ++mesh_bone) {
      if (used_bone_flags[mesh_bone]) {
        mesh_to_shader_bones->push_back(shader_bone);
        shader_to_mesh_bones->push_back(mesh_bone);
        shader_bone++;
      } else {
        mesh_to_shader_bones->push_back(kInvalidBoneIdx);
      }
    }
  }

  // Copy 32bit indices into 16bit index buffer.
  static void CopyIndexBuf(const IndexBuffer& index_buf,
                           IndexBufferCompact* index_buf16) {
    // Indices are output in groups of three, since we only output triangles.
    assert(index_buf.size() % 3 == 0);

    index_buf16->clear();
    index_buf16->reserve(index_buf.size());

    // Copy triangles.
    for (size_t i = 0; i < index_buf.size(); i += 3) {
      index_buf16->push_back(static_cast<VertIndexCompact>(index_buf[i]));
      index_buf16->push_back(static_cast<VertIndexCompact>(index_buf[i + 1]));
      index_buf16->push_back(static_cast<VertIndexCompact>(index_buf[i + 2]));
    }
  }

  // Bones >8-bits are unindexable, so weight them to the root bone 0.
  // This will look funny but it's the best we can do.
  static BoneIndexCompact TruncateBoneIndex(int bone_idx) {
    return bone_idx == kInvalidBoneIdx
               ? kInvalidBoneIdxCompact
               : bone_idx > kMaxBoneIndex
                     ? 0
                     : static_cast<BoneIndexCompact>(bone_idx);
  }

  SurfaceMap surfaces_;
  VertexSet unique_;
  std::vector<Vertex> points_;
  IndexBuffer* cur_index_buf_;
  VertexAttributeBitmask mesh_vertex_attributes_;
  std::vector<Bone> bones_;
  VertexAttributeBitmask vertex_attributes_;

  // Information and warnings.
  Logger& log_;
};

// This instance is required since push_back takes its reference.
// static
const FlatMesh::BoneIndex FlatMesh::kInvalidBoneIdx;

/// @class FbxMeshParser
/// @brief Load FBX files and save their geometry in our FlatBuffer format.
class FbxMeshParser {
 public:
  FbxMeshParser(Logger& log, const FbxAMatrix& bake_transform)
      : manager_(nullptr),
        scene_(nullptr),
        log_(log),
        bake_transform_(bake_transform) {
    // The FbxManager is the gateway to the FBX API.
    manager_ = FbxManager::Create();
    if (manager_ == nullptr) {
      log_.Log(kLogError, "Unable to create FBX manager.\n");
      return;
    }

    // Initialize with standard IO settings.
    FbxIOSettings* ios = FbxIOSettings::Create(manager_, IOSROOT);
    manager_->SetIOSettings(ios);

    // Create an FBX scene. This object holds most objects imported/exported
    // from/to files.
    scene_ = FbxScene::Create(manager_, "My Scene");
    if (scene_ == nullptr) {
      log_.Log(kLogError, "Unable to create FBX scene.\n");
      return;
    }
  }

  ~FbxMeshParser() {
    // Delete the FBX Manager and all objects that it created.
    if (manager_ != nullptr) manager_->Destroy();
  }

  bool Valid() const { return manager_ != nullptr && scene_ != nullptr; }

  bool Load(const char* file_name, AxisSystem axis_system,
            float distance_unit_scale, bool recenter,
            VertexAttributeBitmask vertex_attributes) {
    if (!Valid()) return false;

    log_.Log(
        kLogInfo,
        "---- mesh_pipeline: %s ------------------------------------------\n",
        fplutil::BaseFileName(file_name).c_str());

    // Create the importer and initialize with the file.
    FbxImporter* importer = FbxImporter::Create(manager_, "");
    const bool init_success =
        importer->Initialize(file_name, -1, manager_->GetIOSettings());
    const FbxStatus init_status = importer->GetStatus();

    // Check the SDK and pipeline versions.
    int sdk_major = 0, sdk_minor = 0, sdk_revision = 0;
    int file_major = 0, file_minor = 0, file_revision = 0;
    FbxManager::GetFileFormatVersion(sdk_major, sdk_minor, sdk_revision);
    importer->GetFileVersion(file_major, file_minor, file_revision);

    // Report version information.
    log_.Log(kLogVerbose, "File version %d.%d.%d, SDK version %d.%d.%d\n",
             file_major, file_minor, file_revision, sdk_major, sdk_minor,
             sdk_revision);

    // Exit on load error.
    if (!init_success) {
      log_.Log(kLogError, "init, %s\n\n", init_status.GetErrorString());
      return false;
    }

    // Import the scene.
    const bool import_success = importer->Import(scene_);
    const FbxStatus import_status = importer->GetStatus();

    // Clean-up temporaries.
    importer->Destroy();

    // Exit if the import failed.
    if (!import_success) {
      log_.Log(kLogError, "import, %s\n\n", import_status.GetErrorString());
      return false;
    }

    // Remember the source file name so we can search for textures nearby.
    mesh_file_name_ = std::string(file_name);

    // Ensure the correct distance unit and axis system are being used.
    fplutil::ConvertFbxScale(distance_unit_scale, scene_, &log_);
    fplutil::ConvertFbxAxes(axis_system, scene_, &log_);

    // Bring the geo into our format.
    ConvertGeometry(recenter, vertex_attributes);

    // Log nodes after we've processed them.
    log_.Log(kLogVerbose, "Converted scene nodes\n");
    fplutil::LogFbxScene(scene_, 0, kLogVerbose, &log_);
    return true;
  }

  // Return an upper bound on the number of vertices in the scene.
  int NumVertsUpperBound() const {
    // The scene's been triangulated, so there are three verts per poly.
    // Many of those verts may be duplicates, but we're only looking for an
    // upper bound.
    return 3 * NumPolysRecursive(scene_->GetRootNode());
  }

  // Map FBX nodes to bone indices, used to create bone index references.
  typedef std::unordered_map<const FbxNode*, unsigned int> NodeToBoneMap;

  static int AddBoneForNode(NodeToBoneMap* node_to_bone_map, FbxNode* node,
                            int parent_bone_index, FlatMesh* out) {
    // The node is a bone if it was marked as one by MarkBoneNodesRecursive.
    const auto found_it = node_to_bone_map->find(node);
    if (found_it == node_to_bone_map->end()) {
      return -1;
    }

    // Add the bone entry.
    const FbxAMatrix global_transform = node->EvaluateGlobalTransform();
    const FbxAMatrix default_bone_transform_inverse =
        global_transform.Inverse();
    const char* const name = node->GetName();
    const unsigned int bone_index = out->AppendBone(
        name, Mat4FromFbx(default_bone_transform_inverse), parent_bone_index);
    found_it->second = bone_index;
    return bone_index;
  }

  bool MarkBoneNodesRecursive(NodeToBoneMap* node_to_bone_map,
                              FbxNode* node) const {
    // We need a bone for this node if it has a skeleton attribute or a mesh.
    bool need_bone = (node->GetSkeleton() || node->GetMesh());

    // We also need a bone for this node if it has any such child bones.
    const int child_count = node->GetChildCount();
    for (int child_index = 0; child_index != child_count; ++child_index) {
      FbxNode* const child_node = node->GetChild(child_index);
      if (MarkBoneNodesRecursive(node_to_bone_map, child_node)) {
        need_bone = true;
      }
    }

    // Flag the node as a bone.
    if (need_bone) {
      node_to_bone_map->insert(NodeToBoneMap::value_type(node, -1));
    }
    return need_bone;
  }

  void GatherBonesRecursive(NodeToBoneMap* node_to_bone_map, FbxNode* node,
                            int parent_bone_index, FlatMesh* out) const {
    const int bone_index =
        AddBoneForNode(node_to_bone_map, node, parent_bone_index, out);
    if (bone_index >= 0) {
      const int child_count = node->GetChildCount();
      for (int child_index = 0; child_index != child_count; ++child_index) {
        FbxNode* const child_node = node->GetChild(child_index);
        GatherBonesRecursive(node_to_bone_map, child_node, bone_index, out);
      }
    }
  }

  // Gather converted geometry into our `FlatMesh` class.
  void GatherFlatMesh(bool gather_textures, FlatMesh* out) const {
    FbxNode* const root_node = scene_->GetRootNode();
    const int child_count = root_node->GetChildCount();
    NodeToBoneMap node_to_bone_map;

    // First pass: determine which nodes are to be treated as bones.
    // We skip the root node so it's not included in the bone hierarchy.
    for (int child_index = 0; child_index != child_count; ++child_index) {
      FbxNode* const child_node = root_node->GetChild(child_index);
      MarkBoneNodesRecursive(&node_to_bone_map, child_node);
    }

    // Second pass: add bones.
    // We skip the root node so it's not included in the bone hierarchy.
    for (int child_index = 0; child_index != child_count; ++child_index) {
      FbxNode* const child_node = root_node->GetChild(child_index);
      GatherBonesRecursive(&node_to_bone_map, child_node, -1, out);
    }

    // Final pass: Traverse the scene and output one surface per mesh.
    GatherFlatMeshRecursive(gather_textures, &node_to_bone_map, root_node,
                            root_node, out);
  }

 private:
  FPL_DISALLOW_COPY_AND_ASSIGN(FbxMeshParser);

  vec4 CalculateOrientation(const vec3& normal, const vec4& tangent) const {
    const vec3 n = normal.Normalized();
    const vec3 t = tangent.xyz().Normalized();
    const vec3 b = vec3::CrossProduct(n, t).Normalized();
    const mat3 m(t.x, t.y, t.z, b.x, b.y, b.z, n.x, n.y, n.z);
    quat q = quat::FromMatrix(m).Normalized();
    // Align the sign bit of the orientation scalar to our handedness.
    if (signbit(tangent.w) != signbit(q.scalar())) {
      q = quat(-q.scalar(), -q.vector());
    }
    return vec4(q.vector(), q.scalar());
  }

  void ConvertGeometry(bool recenter,
                       VertexAttributeBitmask vertex_attributes) {
    FbxGeometryConverter geo_converter(manager_);

    // Ensure origin is in the center of geometry.
    if (recenter) {
      const bool recentered =
          geo_converter.RecenterSceneToWorldCenter(scene_, 0.0);
      if (recentered) {
        log_.Log(kLogInfo, "Recentering\n");
      } else {
        log_.Log(kLogInfo, "Already centered so ignoring recenter request\n");
      }
    }

    // Ensure each mesh has only one texture, and only triangles.
    geo_converter.SplitMeshesPerMaterial(scene_, true);
    geo_converter.Triangulate(scene_, true);

    // Traverse all meshes in the scene, generating normals and tangents.
    ConvertGeometryRecursive(scene_->GetRootNode(), vertex_attributes);
  }

  void ConvertGeometryRecursive(FbxNode* node,
                                VertexAttributeBitmask vertex_attributes) {
    if (node == nullptr) return;

    // We're only interested in meshes, for the moment.
    for (int i = 0; i < node->GetNodeAttributeCount(); ++i) {
      FbxNodeAttribute* attr = node->GetNodeAttributeByIndex(i);
      if (attr == nullptr ||
          attr->GetAttributeType() != FbxNodeAttribute::eMesh)
        continue;
      FbxMesh* mesh = static_cast<FbxMesh*>(attr);

      // Generate normals. Leaves existing normal data if it already exists.
      if (vertex_attributes != kVertexAttributeBit_AllAttributesInSourceFile &&
          (vertex_attributes &
           (kVertexAttributeBit_Normal | kVertexAttributeBit_Orientation))) {
        const bool normals_generated = mesh->GenerateNormals();
        if (normals_generated) {
          log_.Log(kLogInfo, "Generating normals for mesh %s\n",
                   mesh->GetName());
        } else {
          log_.Log(kLogWarning, "Could not generate normals for mesh %s\n",
                   mesh->GetName());
        }
      }

      // Generate tangents. Leaves existing tangent data if it already exists.
      if (mesh->GetElementUVCount() > 0 &&
          vertex_attributes != kVertexAttributeBit_AllAttributesInSourceFile &&
          (vertex_attributes &
           (kVertexAttributeBit_Tangent | kVertexAttributeBit_Orientation))) {
        const bool tangents_generated = mesh->GenerateTangentsData(0);
        if (tangents_generated) {
          log_.Log(kLogInfo, "Generating tangents for mesh %s\n",
                   mesh->GetName());
        } else {
          log_.Log(kLogWarning, "Could not generate tangents for mesh %s\n",
                   mesh->GetName());
        }
      }
    }

    // Recursively traverse each node in the scene
    for (int i = 0; i < node->GetChildCount(); i++) {
      ConvertGeometryRecursive(node->GetChild(i), vertex_attributes);
    }
  }

  // Return the total number of polygons under `node`.
  int NumPolysRecursive(FbxNode* node) const {
    if (node == nullptr) return 0;

    // Sum the number of polygons across all meshes on this node.
    int num_polys = 0;
    for (int i = 0; i < node->GetNodeAttributeCount(); ++i) {
      const FbxNodeAttribute* attr = node->GetNodeAttributeByIndex(i);
      if (attr == nullptr ||
          attr->GetAttributeType() != FbxNodeAttribute::eMesh)
        continue;
      const FbxMesh* mesh = static_cast<const FbxMesh*>(attr);
      num_polys += mesh->GetPolygonCount();
    }

    // Recursively traverse each node in the scene
    for (int i = 0; i < node->GetChildCount(); i++) {
      num_polys += NumPolysRecursive(node->GetChild(i));
    }
    return num_polys;
  }

  // Get the UVs for a mesh.
  const FbxGeometryElementUV* UvElements(
      const FbxMesh* mesh, const FbxGeometryElementUV** uv_alt_element) const {
    const int uv_count = mesh->GetElementUVCount();
    const FbxGeometryElementUV* uv_element = nullptr;
    *uv_alt_element = nullptr;

    // Use the first UV set as the primary UV set.
    if (uv_count > 0) {
      uv_element = mesh->GetElementUV(0);
      log_.Log(kLogVerbose, "Using UV map %s for mesh %s.\n",
               uv_element->GetName(), mesh->GetName());
    }

    // Use the second UV set if it exists.
    if (uv_count > 1) {
      *uv_alt_element = mesh->GetElementUV(1);
      log_.Log(kLogVerbose, "Using alternate UV map %s for mesh %s.\n",
               (*uv_alt_element)->GetName(), mesh->GetName());
    }

    // Warn when more UV sets exist.
    if (uv_count > 2 && log_.level() <= kLogWarning) {
      FbxStringList uv_set_names;
      mesh->GetUVSetNames(uv_set_names);
      log_.Log(kLogWarning,
               "Multiple UVs for mesh %s. Using %s and %s. Ignoring %s.\n",
               mesh->GetName(), uv_set_names.GetStringAt(0),
               uv_set_names.GetStringAt(1), uv_set_names.GetStringAt(2));
    }

    return uv_element;
  }

  bool SolidColor(FbxNode* node, const FbxMesh* mesh, FbxColor* color) const {
    FbxLayerElementArrayTemplate<int>* material_indices;
    const bool valid_indices = mesh->GetMaterialIndices(&material_indices);
    if (!valid_indices) return false;

    for (int j = 0; j < material_indices->GetCount(); ++j) {
      // Check every material attached to this mesh.
      const int material_index = (*material_indices)[j];
      const FbxSurfaceMaterial* material = node->GetMaterial(material_index);
      if (material == nullptr) continue;

      // Textures are properties of the material. Check if the diffuse
      // color has been set.
      const FbxProperty diffuse_property =
          material->FindProperty(FbxSurfaceMaterial::sDiffuse);
      const FbxProperty diffuse_factor_property =
          material->FindProperty(FbxSurfaceMaterial::sDiffuseFactor);
      if (!diffuse_property.IsValid() || !diffuse_factor_property.IsValid())
        continue;

      // Final diffuse color is the factor times the base color.
      const double factor = diffuse_factor_property.Get<FbxDouble>();
      const FbxColor base = diffuse_property.Get<FbxColor>();
      color->Set(factor * base.mRed, factor * base.mGreen, factor * base.mBlue,
                 base.mAlpha);
      return true;
    }
    return false;
  }

  // Get the texture for a mesh node.
  const FbxFileTexture* TextureFromNode(FbxNode* node, const FbxMesh* mesh,
                                        const char* texture_property) const {
    FbxLayerElementArrayTemplate<int>* material_indices;
    const bool valid_indices = mesh->GetMaterialIndices(&material_indices);
    if (!valid_indices) return nullptr;

    // Gather the unique materials attached to this mesh.
    std::unordered_set<int> unique_material_indices;
    for (int j = 0; j < material_indices->GetCount(); ++j) {
      const int material_index = (*material_indices)[j];
      unique_material_indices.insert(material_index);
    }

    for (auto it = unique_material_indices.begin();
         it != unique_material_indices.end(); ++it) {
      const FbxSurfaceMaterial* material = node->GetMaterial(*it);
      if (material == nullptr) continue;

      // Textures are properties of the material.
      const FbxProperty property = material->FindProperty(texture_property);
      const int texture_count = property.GetSrcObjectCount<FbxFileTexture>();
      if (texture_count == 0) continue;

      // Grab the first texture.
      const FbxFileTexture* texture =
          FbxCast<FbxFileTexture>(property.GetSrcObject<FbxFileTexture>(0));

      // Warn if there are extra unused textures.
      if (texture_count > 1 && log_.level() <= kLogWarning) {
        const FbxFileTexture* texture1 =
            FbxCast<FbxFileTexture>(property.GetSrcObject<FbxFileTexture>(1));
        log_.Log(kLogWarning,
                 "Material %s has multiple textures. Using %s. Ignoring %s.\n",
                 material->GetName(), texture->GetFileName(),
                 texture1->GetFileName());
      }

      // Log the texture we found and return.
      if (texture != nullptr) return texture;
    }

    return nullptr;
  }

  bool TextureFileExists(const std::string& file_name) const {
    return FileExists(file_name, fplutil::kCaseSensitive);
  }

  // Try variations of the texture name until we find one on disk.
  std::string FindSourceTextureFileName(const std::string& source_mesh_name,
                                        const std::string& texture_name) const {
    std::set<std::string> attempted_textures;

    // If the texture name is relative, check for it relative to the
    // source mesh's directory.
    const std::string source_dir = fplutil::DirectoryName(source_mesh_name);
    if (!fplutil::AbsoluteFileName(texture_name)) {
      std::string texture_rel_name = source_dir + texture_name;
      if (TextureFileExists(texture_rel_name)) return texture_rel_name;
      attempted_textures.insert(std::move(texture_rel_name));
    }

    // If the texture exists in the same directory as the source mesh, use it.
    const std::string texture_no_dir =
        fplutil::RemoveDirectoryFromName(texture_name);
    std::string texture_in_source_dir = source_dir + texture_no_dir;
    if (TextureFileExists(texture_in_source_dir)) return texture_in_source_dir;
    attempted_textures.insert(std::move(texture_in_source_dir));

    // Check to see if there's a texture with the same base name as the mesh.
    const std::string source_name = fplutil::BaseFileName(source_mesh_name);
    const std::string texture_extension = fplutil::FileExtension(texture_name);
    std::string source_texture =
        source_dir + source_name + "." + texture_extension;
    if (TextureFileExists(source_texture)) return source_texture;
    attempted_textures.insert(std::move(source_texture));

    // Gather potential base names for the texture (i.e. name without directory
    // or extension).
    const std::string base_name = fplutil::BaseFileName(texture_no_dir);
    const std::string base_names[] = {base_name, fplutil::SnakeCase(base_name),
                                      fplutil::CamelCase(base_name),
                                      source_name};

    // For each potential base name, loop through known image file extensions.
    // The image may have been converted to a new format.
    for (size_t i = 0; i < FPL_ARRAYSIZE(base_names); ++i) {
      for (size_t j = 0; j < FPL_ARRAYSIZE(kImageExtensions); ++j) {
        std::string potential_name =
            source_dir + base_names[i] + "." + kImageExtensions[j];
        if (TextureFileExists(potential_name)) return potential_name;
        attempted_textures.insert(std::move(potential_name));
      }
    }

    // As a last resort, use the texture name as supplied. We don't want to
    // do this, normally, since the name can be an absolute path on the drive,
    // or relative to the directory we're currently running from.
    if (TextureFileExists(texture_name)) return texture_name;
    attempted_textures.insert(texture_name.c_str());

    // Texture can't be found. Only log warning once, to avoid spamming.
    static std::unordered_set<std::string> missing_textures;
    if (missing_textures.find(texture_name) == missing_textures.end()) {
      log_.Log(kLogWarning, "Can't find texture `%s`. Tried these variants:\n",
               texture_name.c_str());
      for (auto it = attempted_textures.begin(); it != attempted_textures.end();
           ++it) {
        log_.Log(kLogWarning, "  %s\n", it->c_str());
      }
      log_.Log(kLogWarning, "\n");
      missing_textures.insert(texture_name.c_str());
    }
    return "";
  }

  std::string TextureFileName(FbxNode* node, const FbxMesh* mesh,
                              const char* texture_property) const {
    // Grab the texture attached to this node.
    const FbxFileTexture* texture =
        TextureFromNode(node, mesh, texture_property);
    if (texture == nullptr) return "";

    // Look for a texture on disk that matches the texture referenced by
    // the FBX.
    const std::string texture_file_name = FindSourceTextureFileName(
        mesh_file_name_, std::string(texture->GetFileName()));
    return texture_file_name;
  }

  FlatTextures GatherTextures(FbxNode* node, const FbxMesh* mesh) const {
    FlatTextures textures;

    // FBX nodes can have many different kinds of textures.
    // We search for each kind of texture in the order specified by
    // kTextureProperties. When we find a texture, we assign it the next
    // shader index.
    for (size_t i = 0; i < FPL_ARRAYSIZE(kTextureProperties); ++i) {
      // Find the filename for the texture type given by `texture_property`.
      const char* texture_property = kTextureProperties[i];
      std::string texture = TextureFileName(node, mesh, texture_property);
      if (texture == "") continue;

      // Append texture to our list of textures.
      log_.Log(kLogVerbose, " Mapping %s texture `%s` to shader texture %d\n",
               texture_property,
               fplutil::RemoveDirectoryFromName(texture).c_str(),
               textures.Count());
      textures.Append(texture);
    }

    return textures;
  }

  // Factor node's global_transform into two transforms:
  //   point_transform <== apply in pipeline
  //   default_bone_transform_inverse <== apply at runtime
  void Transforms(FbxNode* node, FbxNode* parent_node,
                  FbxAMatrix* default_bone_transform_inverse,
                  FbxAMatrix* point_transform) const {
    // geometric_transform is applied to each point, but is not inherited
    // by children.
    const FbxVector4 geometric_translation =
        node->GetGeometricTranslation(FbxNode::eSourcePivot);
    const FbxVector4 geometric_rotation =
        node->GetGeometricRotation(FbxNode::eSourcePivot);
    const FbxVector4 geometric_scaling =
        node->GetGeometricScaling(FbxNode::eSourcePivot);
    const FbxAMatrix geometric_transform(geometric_translation,
                                         geometric_rotation, geometric_scaling);

    const FbxAMatrix global_transform =
        bake_transform_ * node->EvaluateGlobalTransform();
    const FbxAMatrix parent_global_transform =
        parent_node->EvaluateGlobalTransform();

    // We want the root node to be the identity. Everything in object space
    // is relative to the root.
    *default_bone_transform_inverse = global_transform.Inverse();
    *point_transform = global_transform * geometric_transform;
  }

  // For each mesh in the tree of nodes under `node`, add a surface to `out`.
  void GatherFlatMeshRecursive(bool gather_textures,
                               const NodeToBoneMap* node_to_bone_map,
                               FbxNode* node, FbxNode* parent_node,
                               FlatMesh* out) const {
    // We're only interested in mesh nodes. If a node and all nodes under it
    // have no meshes, we early out.
    if (node == nullptr || !fplutil::NodeHasMesh(node)) return;
    log_.Log(kLogVerbose, "Node: %s\n", node->GetName());

    // The root node cannot have a transform applied to it, so we do not
    // export it as a bone.
    int bone_index = -1;
    if (node != scene_->GetRootNode()) {
      // Get the transform to this node from its parent.
      FbxAMatrix default_bone_transform_inverse;
      FbxAMatrix point_transform;
      Transforms(node, parent_node, &default_bone_transform_inverse,
                 &point_transform);

      // Find the bone for this node.  It must have one, because we checked that
      // it contained a mesh above.
      const auto found_it = node_to_bone_map->find(node);
      assert(found_it != node_to_bone_map->end());
      bone_index = found_it->second;

      // Gather mesh data for this bone.
      // Note that there may be more than one mesh attached to a node.
      for (int i = 0; i < node->GetNodeAttributeCount(); ++i) {
        const FbxNodeAttribute* attr = node->GetNodeAttributeByIndex(i);
        if (attr == nullptr ||
            attr->GetAttributeType() != FbxNodeAttribute::eMesh)
          continue;
        const FbxMesh* mesh = static_cast<const FbxMesh*>(attr);

        // Gather the textures attached to this mesh.
        FlatTextures textures;
        if (gather_textures) {
          textures = GatherTextures(node, mesh);
        }
        out->SetSurface(textures);

        // If no textures for this mesh, try to get a solid color from the
        // material.
        FbxColor solid_color;
        const bool has_solid_color =
            textures.Count() == 0 && SolidColor(node, mesh, &solid_color);

        // Without a base texture or color, the model will look rather plane.
        if (textures.Count() == 0 && !has_solid_color) {
          log_.Log(kLogWarning, "No texture or solid color found for node %s\n",
                   node->GetName());
        }

        // Gather the verticies and indices.
        GatherFlatSurface(
            mesh, static_cast<SkinBinding::BoneIndex>(bone_index),
            node_to_bone_map, point_transform, has_solid_color, solid_color,
            out);
      }
    }

    // Recursively traverse each node in the scene
    for (int i = 0; i < node->GetChildCount(); i++) {
      GatherFlatMeshRecursive(gather_textures, node_to_bone_map,
                              node->GetChild(i), node, out);
    }
  }

  void GatherSkinBindings(const FbxMesh* mesh,
                          SkinBinding::BoneIndex transform_bone_index,
                          const NodeToBoneMap* node_to_bone_map,
                          std::vector<SkinBinding>* out_skin_bindings,
                          FlatMesh* out) const {
    const unsigned int point_count = mesh->GetControlPointsCount();
    std::vector<SkinBinding> skin_bindings(point_count);

    // Each cluster stores a mapping from a bone to all the vertices it
    // influences.  This generates an inverse mapping from each point to all
    // the bones influencing it.
    const int skin_count = mesh->GetDeformerCount(FbxDeformer::eSkin);
    for (int skin_index = 0; skin_index != skin_count; ++skin_index) {
      const FbxSkin* const skin = static_cast<const FbxSkin*>(
          mesh->GetDeformer(skin_index, FbxDeformer::eSkin));
      const int cluster_count = skin->GetClusterCount();
      for (int cluster_index = 0; cluster_index != cluster_count;
           ++cluster_index) {
        const FbxCluster* const cluster = skin->GetCluster(cluster_index);
        const FbxNode* const link_node = cluster->GetLink();

        // Get the bone index from the node pointer.
        const NodeToBoneMap::const_iterator link_it =
            node_to_bone_map->find(link_node);
        assert(link_it != node_to_bone_map->end());
        const int bone_index = link_it->second;

        // Use the LinkMatrix as the inverse default transform for this bone.
        FbxAMatrix matrix;
        cluster->GetTransformLinkMatrix(matrix);
        out->UpdateDefaultBoneTransformInverse(bone_index,
                                               Mat4FromFbx(matrix).Inverse());

        // We currently only support normalized weights.  Both eNormalize and
        // eTotalOne can be treated as normalized, because we renormalize
        // weights after extraction.
        const FbxCluster::ELinkMode link_mode = cluster->GetLinkMode();
        if (link_mode != FbxCluster::eNormalize &&
            link_mode != FbxCluster::eTotalOne) {
          log_.Log(kLogWarning,
                   "Mesh %s skin %d(%s) cluster %d(%s) has"
                   " unsupported LinkMode %d (only eNormalize(%d) and"
                   " eTotalOne(%d) are supported).\n",
                   GetMeshOrNodeName(mesh), skin_index, skin->GetName(),
                   cluster_index, cluster->GetName(),
                   static_cast<int>(link_mode),
                   static_cast<int>(FbxCluster::eNormalize),
                   static_cast<int>(FbxCluster::eTotalOne));
        }

        // Assign bone weights to all cluster influences.
        const int influence_count = cluster->GetControlPointIndicesCount();
        const int* const point_indices = cluster->GetControlPointIndices();
        const double* const weights = cluster->GetControlPointWeights();
        for (int influence_index = 0; influence_index != influence_count;
             ++influence_index) {
          const int point_index = point_indices[influence_index];
          assert(static_cast<unsigned int>(point_index) < point_count);
          const float weight = static_cast<float>(weights[influence_index]);
          skin_bindings[point_index].AppendInfluence(bone_index, weight, log_,
                                                     mesh, point_index);
        }
      }
    }

    // Normalize weights.
    for (unsigned int point_index = 0; point_index != point_count;
         ++point_index) {
      SkinBinding* const skin_binding = &skin_bindings[point_index];
      if (!skin_binding->HasInfluences()) {
        // Any non-skinned vertices not bound to a deformer are implicitly bound
        // to their parent transform.
        skin_binding->BindRigid(transform_bone_index);
      } else {
        skin_binding->NormalizeBoneWeights();
      }
    }

    out_skin_bindings->swap(skin_bindings);
  }

  void GatherFlatSurface(const FbxMesh* mesh,
                         SkinBinding::BoneIndex transform_bone_index,
                         const NodeToBoneMap* node_to_bone_map,
                         const FbxAMatrix& point_transform,
                         bool has_solid_color, const FbxColor& solid_color,
                         FlatMesh* out) const {
    const FbxAMatrix& t = point_transform;
    log_.Log(kLogVerbose,
             "    transform: {%.3f %.3f %.3f %.3f}\n"
             "               {%.3f %.3f %.3f %.3f}\n"
             "               {%.3f %.3f %.3f %.3f}\n"
             "               {%.3f %.3f %.3f %.3f}\n",
             t[0][0], t[0][1], t[0][2], t[0][3], t[1][0], t[1][1], t[1][2],
             t[1][3], t[2][0], t[2][1], t[2][2], t[2][3], t[3][0], t[3][1],
             t[3][2], t[3][3]);

    // Affine matrix only supports multiplication by a point, not a vector.
    // That is, there is no way to ignore the translation (as is required
    // for normals and tangents). So, we create a copy of `transform` that
    // has no translation.
    // http://forums.autodesk.com/t5/fbx-sdk/matrix-vector-multiplication/td-p/4245079
    FbxAMatrix vector_transform = point_transform;
    vector_transform.SetT(FbxVector4(0.0, 0.0, 0.0, 0.0));

    std::vector<SkinBinding> skin_bindings;
    GatherSkinBindings(mesh, transform_bone_index, node_to_bone_map,
                       &skin_bindings, out);

    // Get references to various vertex elements.
    const FbxVector4* vertices = mesh->GetControlPoints();
    const FbxGeometryElementNormal* normal_element = mesh->GetElementNormal();
    const FbxGeometryElementTangent* tangent_element =
        mesh->GetElementTangent();
    const FbxGeometryElementVertexColor* color_element =
        mesh->GetElementVertexColor();
    const FbxGeometryElementUV* uv_alt_element = nullptr;
    const FbxGeometryElementUV* uv_element = UvElements(mesh, &uv_alt_element);

    // Record which vertex attributes exist for this surface.
    // We reported the bone name and parents in AppendBone().
    const VertexAttributeBitmask surface_vertex_attributes =
        kVertexAttributeBit_Bone |
        (vertices ? kVertexAttributeBit_Position : 0) |
        (normal_element ? kVertexAttributeBit_Normal : 0) |
        (tangent_element ? kVertexAttributeBit_Tangent : 0) |
        (color_element != nullptr || has_solid_color ? kVertexAttributeBit_Color
                                                     : 0) |
        (uv_element ? kVertexAttributeBit_Uv : 0) |
        (uv_alt_element ? kVertexAttributeBit_UvAlt : 0);
    out->ReportSurfaceVertexAttributes(surface_vertex_attributes);
    log_.Log(kLogVerbose, color_element != nullptr
                              ? "Mesh has vertex colors\n"
                              : has_solid_color
                                    ? "Mesh material has a solid color\n"
                                    : "Mesh does not have vertex colors\n");

    // Loop through every poly in the mesh.
    int vertex_counter = 0;
    const int num_polys = mesh->GetPolygonCount();
    for (int poly_index = 0; poly_index < num_polys; ++poly_index) {
      // Ensure polygon is a triangle. This should be true since we call
      // Triangulate() when we load the scene.
      const int num_verts = mesh->GetPolygonSize(poly_index);
      if (num_verts != 3) {
        log_.Log(kLogWarning, "mesh %s poly %d has %d verts instead of 3\n",
                 mesh->GetName(), poly_index, num_verts);
        continue;
      }

      // Loop through all three verts.
      for (int vert_index = 0; vert_index < num_verts; ++vert_index) {
        // Get the control index for this poly, vert combination.
        const int control_index =
            mesh->GetPolygonVertex(poly_index, vert_index);

        // Depending on the FBX format, normals and UVs are indexed either
        // by control point or by polygon-vertex.
        const FbxVector4 vertex_fbx = vertices[control_index];
        const FbxVector4 normal_fbx =
            ElementFromIndices(normal_element, control_index, vertex_counter);
        const FbxVector4 tangent_fbx =
            ElementFromIndices(tangent_element, control_index, vertex_counter);
        const FbxColor color_fbx =
            color_element != nullptr
                ? ElementFromIndices(color_element, control_index,
                                     vertex_counter)
                : has_solid_color ? solid_color : kDefaultColor;
        const FbxVector2 uv_fbx =
            ElementFromIndices(uv_element, control_index, vertex_counter);
        const FbxVector2 uv_alt_fbx =
            ElementFromIndices(uv_alt_element, control_index, vertex_counter);

        // Output this poly-vert.
        // Note that the v-axis is flipped between FBX UVs and FlatBuffer UVs.
        const vec3 vertex = Vec3FromFbx(point_transform.MultT(vertex_fbx));
        const vec3 normal =
            Vec3FromFbx(vector_transform.MultT(normal_fbx)).Normalized();
        const vec4 tangent(
            Vec3FromFbx(vector_transform.MultT(tangent_fbx)).Normalized(),
            static_cast<float>(tangent_fbx[3]));
        const vec4 orientation = CalculateOrientation(normal, tangent);
        const vec4 color = Vec4FromFbx(color_fbx);
        const vec2 uv = Vec2FromFbxUv(uv_fbx);
        const vec2 uv_alt = Vec2FromFbxUv(uv_alt_fbx);
        const SkinBinding& skin_binding = skin_bindings[control_index];
        out->AppendPolyVert(vertex, normal, tangent, orientation, color, uv,
                            uv_alt, skin_binding);

        // Control points are listed in order of poly + vertex.
        vertex_counter++;
      }
    }
  }

  // Entry point to the FBX SDK.
  FbxManager* manager_;

  // Hold the FBX file data.
  FbxScene* scene_;

  // Name of source mesh file. Used to search for textures, when the textures
  // are not found in their referenced location.
  std::string mesh_file_name_;

  // Information and warnings.
  Logger& log_;

  // Transform baked into vertices.
  FbxAMatrix bake_transform_;
};

MeshPipelineArgs::MeshPipelineArgs()
    : blend_mode(static_cast<matdef::BlendMode>(-1)),
      axis_system(fplutil::kUnspecifiedAxisSystem),
      distance_unit_scale(-1.0f),
      recenter(false),
      interleaved(true),
      force32(false),
      embed_materials(false),
      vertex_attributes(kVertexAttributeBit_AllAttributesInSourceFile),
      log_level(kLogWarning),
      gather_textures(true) {
  bake_transform.SetIdentity();
}

int RunMeshPipeline(const MeshPipelineArgs& args, fplutil::Logger& log) {
  // Update the amount of information we're dumping.
  log.set_level(args.log_level);

  // Currently orientations can only be generated from normal-tangents, so it
  // doesn't make sense to export both. If this changes at some point, then be
  // sure to also update kVertexAttributeBit_AllAttributesInSourceFile.
  if ((args.vertex_attributes & kVertexAttributeBit_Orientation) &&
      (args.vertex_attributes &
       (kVertexAttributeBit_Normal | kVertexAttributeBit_Tangent))) {
    log.Log(kLogError, "Can't output normal-tangent and orientation.\n");
    return 1;
  }

  // Load the FBX file.
  fplbase::FbxMeshParser pipe(log, args.bake_transform);
  const bool load_status = pipe.Load(args.fbx_file.c_str(), args.axis_system,
                                     args.distance_unit_scale, args.recenter,
                                     args.vertex_attributes);
  if (!load_status) return 1;

  // Gather data into a format conducive to our FlatBuffer format.
  const int max_verts = pipe.NumVertsUpperBound();
  fplbase::FlatMesh mesh(max_verts, args.vertex_attributes, log);
  pipe.GatherFlatMesh(args.gather_textures, &mesh);

  // Output gathered data to a binary FlatBuffer.
  const bool output_status = mesh.OutputFlatBuffer(
      args.fbx_file, args.asset_base_dir, args.asset_rel_dir,
      args.texture_extension, args.texture_formats, args.blend_mode,
      args.interleaved, args.force32, args.embed_materials);
  if (!output_status) return 1;

  // Success.
  return 0;
}

}  // namespace fplbase
