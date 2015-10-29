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

// Suppress warnings in external header.
#pragma warning(push)            // for Visual Studio
#pragma warning(disable : 4068)  // "unknown pragma" -- for Visual Studio
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <fbxsdk.h>
#pragma GCC diagnostic pop
#pragma warning(pop)

#include <assert.h>
#include <cfloat>
#include <fstream>
#include <functional>
#include <stdarg.h>
#include <stdio.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common_generated.h"
#include "fplbase/fpl_common.h"
#include "fplutil/file_utils.h"
#include "fplutil/string_utils.h"
#include "materials_generated.h"
#include "mathfu/glsl_mappings.h"
#include "mesh_generated.h"

namespace fpl {

using mathfu::vec2;
using mathfu::vec3;
using mathfu::vec4;
using mathfu::mat4;
using mathfu::vec2_packed;
using mathfu::vec3_packed;
using mathfu::vec4_packed;

typedef uint8_t BoneIndex;

enum AxisSystem {
  kInvalidAxisSystem = -2,
  kUnspecifiedAxisSystem = -1,

  kXUp_PositiveYFront_PositiveZLeft = 0,
  kXUp_PositiveYFront_NegativeZLeft,
  kXUp_NegativeYFront_PositiveZLeft,
  kXUp_NegativeYFront_NegativeZLeft,
  kXUp_PositiveZFront_PositiveYLeft,
  kXUp_PositiveZFront_NegativeYLeft,
  kXUp_NegativeZFront_PositiveYLeft,
  kXUp_NegativeZFront_NegativeYLeft,
  kLastXUpAxisSystem,

  kYUp_PositiveXFront_PositiveZLeft = kLastXUpAxisSystem,
  kYUp_PositiveXFront_NegativeZLeft,
  kYUp_NegativeXFront_PositiveZLeft,
  kYUp_NegativeXFront_NegativeZLeft,
  kYUp_PositiveZFront_PositiveXLeft,
  kYUp_PositiveZFront_NegativeXLeft,
  kYUp_NegativeZFront_PositiveXLeft,
  kYUp_NegativeZFront_NegativeXLeft,
  kLastYUpAxisSystem,

  kZUp_PositiveXFront_PositiveYLeft = kLastYUpAxisSystem,
  kZUp_PositiveXFront_NegativeYLeft,
  kZUp_NegativeXFront_PositiveYLeft,
  kZUp_NegativeXFront_NegativeYLeft,
  kZUp_PositiveYFront_PositiveXLeft,
  kZUp_PositiveYFront_NegativeXLeft,
  kZUp_NegativeYFront_PositiveXLeft,
  kZUp_NegativeYFront_NegativeXLeft,
  kLastZUpAxisSystem,

  kNumAxisSystems = kLastZUpAxisSystem
};

static const char* const kImageExtensions[] =
    { "jpg", "jpeg", "png", "webp", "tga" };
static const FbxColor kDefaultColor(1.0, 1.0, 1.0, 1.0);
static const BoneIndex kInvalidBoneIdx = 0xFF;
static const matdef::TextureFormat kDefaultTextureFormat =
    matdef::TextureFormat_AUTO;

// We use uint8_t for bone indices, and 0xFF marks invalid bones,
// so the limit is 254.
static const BoneIndex kMaxBoneIndex = 0xFE;

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

static const char* kAxisSystemNames[] = {
  "x+y+z", // kXUp_PositiveYFront_PositiveZLeft
  "x+y-z", // kXUp_PositiveYFront_NegativeZLeft
  "x-y+z", // kXUp_NegativeYFront_PositiveZLeft
  "x-y-z", // kXUp_NegativeYFront_NegativeZLeft
  "x+z+y", // kXUp_PositiveZFront_PositiveYLeft
  "x+z-y", // kXUp_PositiveZFront_NegativeYLeft
  "x-z+y", // kXUp_NegativeZFront_PositiveYLeft
  "x-z-y", // kXUp_NegativeZFront_NegativeYLeft
  "y+x+z", // kYUp_PositiveXFront_PositiveZLeft
  "y+x-z", // kYUp_PositiveXFront_NegativeZLeft
  "y-x+z", // kYUp_NegativeXFront_PositiveZLeft
  "y-x-z", // kYUp_NegativeXFront_NegativeZLeft
  "y+z+x", // kYUp_PositiveZFront_PositiveXLeft
  "y+z-x", // kYUp_PositiveZFront_NegativeXLeft
  "y-z+x", // kYUp_NegativeZFront_PositiveXLeft
  "y-z-x", // kYUp_NegativeZFront_NegativeXLeft
  "z+x+y", // kZUp_PositiveXFront_PositiveYLeft
  "z+x-y", // kZUp_PositiveXFront_NegativeYLeft
  "z-x+y", // kZUp_NegativeXFront_PositiveYLeft
  "z-x-y", // kZUp_NegativeXFront_NegativeYLeft
  "z+y+x", // kZUp_PositiveYFront_PositiveXLeft
  "z+y-x", // kZUp_PositiveYFront_NegativeXLeft
  "z-y+x", // kZUp_NegativeYFront_PositiveXLeft
  "z-y-x", // kZUp_NegativeYFront_NegativeXLeft
  nullptr
};
static_assert(FPL_ARRAYSIZE(kAxisSystemNames) == kNumAxisSystems + 1, "");

// Each log message is given a level of importance.
// We only output messages that have level >= our current logging level.
enum LogLevel {
  kLogVerbose,
  kLogInfo,
  kLogImportant,
  kLogWarning,
  kLogError,
  kNumLogLevels
};

// Prefix log messages at this level with this message.
static const char* kLogPrefix[] = {
  "",           // kLogVerbose
  "",           // kLogInfo
  "",           // kLogImportant
  "Warning: ",  // kLogWarning
  "Error: "     // kLogError
};
static_assert(FPL_ARRAYSIZE(kLogPrefix) == kNumLogLevels,
              "kLogPrefix length is incorrect");

/// @class Logger
/// @brief Output log messages if they are above an adjustable threshold.
class Logger {
 public:
  Logger() : level_(kLogImportant) {}

  void set_level(LogLevel level) { level_ = level; }
  LogLevel level() const { return level_; }

  /// Output a printf-style message if our current logging level is
  /// >= `level`.
  void Log(LogLevel level, const char* format, ...) const {
    if (level < level_) return;

    // Prefix message with log level, if required.
    const char* prefix = kLogPrefix[level];
    if (prefix[0] != '\0') {
      printf("%s", prefix);
    }

    // Redirect output to stdout.
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
  }

 private:
  LogLevel level_;
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
static T ElementFromIndices(const FbxLayerElementTemplate<T>& element,
                            int control_index, int vertex_counter) {
  const int index =
      element.GetMappingMode() == FbxGeometryElement::eByControlPoint ?
      control_index : vertex_counter;
  return Element(element, index);
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
  return Vec4(v.x(), v.y(), v.z(), v.w());
}

static inline Vec3 FlatBufferVec3(const vec3& v) {
  return Vec3(v.x(), v.y(), v.z());
}

static inline Vec2 FlatBufferVec2(const vec2& v) {
  return Vec2(v.x(), v.y());
}

static inline Vec4ub FlatBufferVec4ub(const vec4& v) {
  const vec4 scaled = 255.0f * v;
  return Vec4ub(
      static_cast<uint8_t>(scaled.x()), static_cast<uint8_t>(scaled.y()),
      static_cast<uint8_t>(scaled.z()), static_cast<uint8_t>(scaled.w()));
}

static inline Mat3x4 FlatBufferMat3x4(const mat4& matrix) {
  const mat4 m = matrix.Transpose();
  return Mat3x4(Vec4(m(0), m(1), m(2), m(3)), Vec4(m(4), m(5), m(6), m(7)),
                Vec4(m(8), m(9), m(10), m(11)));
}

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
  explicit FlatMesh(int max_verts, Logger& log)
      : points_(max_verts),
        cur_index_buf_(nullptr),
        export_vertex_color_(false),
        max_position_(-FLT_MAX),
        min_position_(FLT_MAX),
        log_(log) {
    points_.clear();
  }

  void AppendBone(const char* bone_name, const mat4& relative_transform,
                  int depth) {
    // Bone count is limited by data size.
    if (bones_.size() >= kMaxBoneIndex) {
      log_.Log(kLogError, "%d bone limit exceeded.\n", kMaxBoneIndex);
      return;
    }

    // Until this function is called again, all appended vertices will
    // reference this bone.
    bones_.push_back(
        Bone(bone_name, relative_transform, depth, points_.size()));
  }

  void SetSurface(const FlatTextures& textures) {
    // Grab existing surface for `texture_file_name`, or create a new one.
    IndexBuffer& index_buffer = surfaces_[textures];

    // Update the current index buffer to which we're logging control points.
    cur_index_buf_ = &index_buffer;

    // Log the surface switch.
    log_.Log(kLogInfo, "Surface:");
    for (size_t i = 0; i < textures.Count(); ++i) {
      log_.Log(kLogInfo, " %s", textures[i].c_str());
    }
    log_.Log(kLogInfo, "\n");
  }

  void SetExportVertexColor(bool should_export) {
    if (points_.size() > 0 && export_vertex_color_ != should_export) {
      log_.Log(kLogWarning,
               "Some meshes have vertex colors and others do not."
               " Meshes that do not will be assigned white vertices.\n");
    }
    export_vertex_color_ = export_vertex_color_ || should_export;
  }

  // Populate a single surface with data from FBX arrays.
  void AppendPolyVert(const vec3& vertex, const vec3& normal,
                      const vec4& tangent, const vec4& color, const vec2& uv) {
    // The `unique_` map holds pointers into `points_`, so we cannot realloc
    // `points_`. Instead, we reserve enough memory for an upper bound on
    // its size. If this assert hits, NumVertsUpperBound() is incorrect.
    assert(points_.capacity() > points_.size());

    // TODO: Round values before creating.
    points_.push_back(Vertex(vertex, normal, tangent, color, uv,
                             static_cast<uint8_t>(bones_.size() - 1)));

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

    // Update the min and max positions
    min_position_ = vec3::Min(min_position_, vertex);
    max_position_ = vec3::Max(max_position_, vertex);

    // Log the data we just added.
    log_.Log(kLogVerbose, "Point: index %d", ref.index);
    if (new_control_point_created) {
      log_.Log(kLogVerbose,
               ", vertex (%.3f, %.3f, %.3f)"
               ", normal (%.3f, %.3f, %.3f)"
               ", tangent (%.3f, %.3f, %.3f)"
               ", binormal-handedness %.0f"
               ", uv (%.3f, %.3f)",
               vertex.x(), vertex.y(), vertex.z(), normal.x(), normal.y(),
               normal.z(), tangent.x(), tangent.y(), tangent.z(), tangent.w(),
               uv.x(), uv.y());
      if (export_vertex_color_) {
        log_.Log(kLogVerbose, ", color (%.3f, %.3f, %.3f, %.3f)", color.x(),
                 color.y(), color.z(), color.w());
      }
    }
    log_.Log(kLogVerbose, "\n");
  }

  // Output material and mesh flatbuffers for the gathered surfaces.
  bool OutputFlatBuffer(
      const std::string& mesh_name_unformated,
      const std::string& assets_base_dir_unformated,
      const std::string& assets_sub_dir_unformated,
      const std::string& texture_extension,
      const std::vector<matdef::TextureFormat>& texture_formats,
      matdef::BlendMode blend_mode,
      bool skin) const {
    // Ensure directory names end with a slash.
    const std::string mesh_name = BaseFileName(mesh_name_unformated);
    const std::string assets_base_dir =
        FormatAsDirectoryName(assets_base_dir_unformated);
    const std::string assets_sub_dir =
        FormatAsDirectoryName(assets_sub_dir_unformated);

    // Ensure output directory exists.
    const std::string assets_dir = assets_base_dir + assets_sub_dir;
    if (!CreateDirectory(assets_dir.c_str())) {
      log_.Log(kLogError, "Could not create output directory %s\n",
               assets_dir.c_str());
      return false;
    }

    // Output bone hierarchy.
    LogBones();

    // Create material files that reference the textures.
    OutputMaterialFlatBuffers(mesh_name, assets_base_dir, assets_sub_dir,
                              texture_extension, texture_formats, blend_mode);

    // Create final mesh file that references materials relative to
    // `assets_base_dir`.
    OutputMeshFlatBuffer(mesh_name, assets_base_dir, assets_sub_dir, skin);
    return true;
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

    log_.Log(kLogImportant, "Mesh hierarchy (bone indices in brackets):\n");
    for (size_t j = 0; j < bones_.size(); ++j) {
      const Bone& b = bones_[j];
      std::string indent = RepeatCharacter(' ', 2 * b.depth);

      // Output bone name and index, indented to match the depth in the
      // hierarchy.
      const BoneIndex shader_bone = mesh_to_shader_bones[j];
      const bool has_verts = shader_bone != kInvalidBoneIdx;
      log_.Log(kLogImportant, "  %s[%d] %s", indent.c_str(), j, b.name.c_str());
      if (has_verts) {
        log_.Log(kLogImportant, " (shader bone %d)", shader_bone);
      }
      log_.Log(kLogImportant, "\n");

      // Output local matrix transform too.
      const mat4& t = b.relative_transform;
      for (size_t k = 0; k < 3; ++k) {
        log_.Log(kLogInfo, "   %s  (%.3f, %.3f, %.3f, %.3f)\n", indent.c_str(),
                 t(k, 0), t(k, 1), t(k, 2), t(k, 3));
      }

      // And the first point, in global space.
      // This should be the same in both hierarchical and flat outputs.
      if (has_verts) {
        const mat4 glob = BoneGlobalTransform(static_cast<int>(j));
        const Vertex& first_vertex = points_[b.first_vertex_index];
        const vec3 first_point = glob * vec3(first_vertex.vertex);
        log_.Log(kLogInfo, "   %s  first-point (%.3f, %.3f, %.3f)\n",
                 indent.c_str(), first_point.x(), first_point.y(),
                 first_point.z());
      }
    }
  }

 private:
  FPL_DISALLOW_COPY_AND_ASSIGN(FlatMesh);
  typedef uint16_t IndexBufIndex;
  typedef std::vector<IndexBufIndex> IndexBuffer;

  struct Vertex {
    vec3_packed vertex;
    vec3_packed normal;
    vec4_packed tangent; // 4th element is handedness: +1 or -1
    vec2_packed uv;
    Vec4ub color;
    BoneIndex bone;
    Vertex() : color(0, 0, 0, 0) {
      // The Hash function operates on all the memory, so ensure everything is
      // zero'd out.
      memset(this, 0, sizeof(*this));
    }
    Vertex(const vec3& v, const vec3& n, const vec4& t, const vec4& c,
           const vec2& u, BoneIndex bone)
        : vertex(v),
          normal(n),
          tangent(t),
          uv(u),
          color(FlatBufferVec4ub(c)),
          bone(bone) {}
  };

  struct VertexRef {
    const Vertex* ref;
    IndexBufIndex index;
    VertexRef(const Vertex* v, size_t index)
        : ref(v), index(static_cast<IndexBufIndex>(index)) {
      assert(index <= std::numeric_limits<IndexBufIndex>::max());
    }
  };

  struct VertexHash {
    size_t operator()(const VertexRef& c) const {
      const size_t* p = reinterpret_cast<const size_t*>(c.ref);
      size_t hash = 0;
      for (size_t i = 0; i < sizeof(*c.ref) / sizeof(size_t); ++i) {
        hash ^= Rotate(p[i], i);
      }
      return hash;
    }

    static size_t Rotate(size_t x, size_t bytes) {
      return (x << (8 * bytes)) | (x >> (8 * (sizeof(size_t) - bytes)));
    }
  };

  struct VerticesEqual {
    bool operator()(const VertexRef& a, const VertexRef& b) const {
      return memcmp(a.ref, b.ref, sizeof(*a.ref)) == 0;
    }
  };

  struct Bone {
    std::string name;
    int depth;
    size_t first_vertex_index;
    mat4 relative_transform;
    Bone() : depth(0), first_vertex_index(0) {}
    Bone(const char* name, const mat4& relative_transform, int depth,
         int first_vertex_index)
        : name(name),
          depth(depth),
          first_vertex_index(first_vertex_index),
          relative_transform(relative_transform) {}
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
    return assets_sub_dir + BaseFileName(texture_file_name);
  }

  static std::string TextureFileName(const std::string& texture_file_name,
                                     const std::string& assets_sub_dir,
                                     const std::string& texture_extension) {
    const std::string extension = texture_extension.length() == 0 ?
        FileExtension(texture_file_name) : texture_extension;
    return TextureBaseFileName(texture_file_name, assets_sub_dir) + '.'
           + extension;
  }

  std::string MaterialFileName(const std::string& mesh_name, size_t surface_idx,
                               const std::string& assets_sub_dir) const {
    std::string name = TextureBaseFileName(mesh_name, assets_sub_dir);
    if (surfaces_.size() > 1) {
      name += std::string("_") + std::to_string(surface_idx);
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

  void OutputMaterialFlatBuffers(
      const std::string& mesh_name, const std::string& assets_base_dir,
      const std::string& assets_sub_dir, const std::string& texture_extension,
      const std::vector<matdef::TextureFormat>& texture_formats,
      matdef::BlendMode blend_mode) const {
    log_.Log(kLogImportant, "Materials:\n");

    size_t surface_idx = 0;
    for (auto it = surfaces_.begin(); it != surfaces_.end(); ++it) {
      const FlatTextures& textures = it->first;
      if (!HasTexture(textures)) continue;

      const std::string material_file_name =
          MaterialFileName(mesh_name, surface_idx, assets_sub_dir);
      log_.Log(kLogImportant, "  %s:", material_file_name.c_str());

      // Create FlatBuffer arrays of texture names and formats.
      flatbuffers::FlatBufferBuilder fbb;
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
        log_.Log(kLogImportant, "%s %s", i == 0 ? "" : ",",
                 RemoveDirectoryFromName(texture_file_name).c_str());
        if (texture_format != kDefaultTextureFormat) {
          log_.Log(kLogImportant, "(%s)",
                   matdef::EnumNameTextureFormat(texture_format));
        }
      }
      log_.Log(kLogImportant, "\n");

      // Create final material FlatBuffer.
      auto textures_vector_fb = fbb.CreateVector(textures_fb);
      auto formats_vector_fb = fbb.CreateVector(formats_fb);
      auto material_fb = matdef::CreateMaterial(
          fbb, textures_vector_fb, blend_mode, formats_vector_fb);
      matdef::FinishMaterialBuffer(fbb, material_fb);

      const std::string full_material_file_name =
          assets_base_dir + material_file_name;
      OutputFlatBufferBuilder(fbb, full_material_file_name);

      surface_idx++;
    }

    // Log blend mode, if blend mode is being used.
    if (blend_mode != matdef::BlendMode_OFF) {
      log_.Log(kLogImportant, "  blend mode: %s\n",
               matdef::EnumNameBlendMode(blend_mode));
    }
  }

  void OutputMeshFlatBuffer(const std::string& mesh_name,
                            const std::string& assets_base_dir,
                            const std::string& assets_sub_dir,
                            bool skin) const {
    flatbuffers::FlatBufferBuilder fbb;

    const std::string rel_mesh_file_name =
        assets_sub_dir + mesh_name + "." + meshdef::MeshExtension();
    const std::string full_mesh_file_name =
        assets_base_dir + rel_mesh_file_name;
    log_.Log(kLogImportant, "Mesh:\n  %s has %d verts\n",
             rel_mesh_file_name.c_str(), points_.size());

    // Get the mapping from mesh bones (i.e. all bones in the model)
    // to shader bones (i.e. bones that have verts weighted to them).
    std::vector<BoneIndex> mesh_to_shader_bones;
    std::vector<BoneIndex> shader_to_mesh_bones;
    CalculateBoneIndexMaps(&mesh_to_shader_bones, &shader_to_mesh_bones);

    // Output the surfaces.
    std::vector<flatbuffers::Offset<meshdef::Surface>> surfaces_fb;
    surfaces_fb.reserve(surfaces_.size());
    size_t surface_idx = 0;
    for (auto it = surfaces_.begin(); it != surfaces_.end(); ++it) {
      const FlatTextures& textures = it->first;
      const IndexBuffer& index_buf = it->second;
      const std::string material_file_name =
          HasTexture(textures)
              ? MaterialFileName(mesh_name, surface_idx, assets_sub_dir)
              : std::string("");
      auto material_fb = fbb.CreateString(material_file_name);
      auto indices_fb = fbb.CreateVector(index_buf);
      auto surface_fb = meshdef::CreateSurface(fbb, indices_fb, material_fb);
      surfaces_fb.push_back(surface_fb);

      log_.Log(kLogImportant, "  Surface %d (%s) has %d triangles\n",
               surface_idx, material_file_name.c_str(), index_buf.size() / 3);
      surface_idx++;
    }
    auto surface_vector_fb = fbb.CreateVector(surfaces_fb);

    // Output the mesh.
    // First convert to structure-of-array format.
    std::vector<Vec3> vertices;
    std::vector<Vec3> normals;
    std::vector<Vec4> tangents;
    std::vector<Vec4ub> colors;
    std::vector<Vec2> uvs;
    std::vector<Vec4ub> skin_indices;
    std::vector<Vec4ub> skin_weights;
    vertices.reserve(points_.size());
    normals.reserve(points_.size());
    tangents.reserve(points_.size());
    colors.reserve(points_.size());
    uvs.reserve(points_.size());
    skin_indices.reserve(points_.size());
    skin_weights.reserve(points_.size());
    for (auto it = points_.begin(); it != points_.end(); ++it) {
      const Vertex& p = *it;
      const BoneIndex shader_bone_idx = mesh_to_shader_bones[p.bone];
      vertices.push_back(FlatBufferVec3(vec3(p.vertex)));
      normals.push_back(FlatBufferVec3(vec3(p.normal)));
      tangents.push_back(FlatBufferVec4(vec4(p.tangent)));
      colors.push_back(p.color);
      uvs.push_back(FlatBufferVec2(vec2(p.uv)));
      skin_indices.push_back(Vec4ub(shader_bone_idx, 0, 0, 0));
      skin_weights.push_back(Vec4ub(1, 0, 0, 0));
    }

    // Output the bone transforms, for skinning, and the bone names,
    // for debugging.
    std::vector<flatbuffers::Offset<flatbuffers::String>> bone_names;
    std::vector<Mat3x4> bone_transforms;
    std::vector<BoneIndex> bone_parents;
    bone_names.reserve(bones_.size());
    bone_transforms.reserve(bones_.size());
    bone_parents.reserve(bones_.size());
    for (size_t i = 0; i < bones_.size(); ++i) {
      const Bone& bone = bones_[i];
      bone_names.push_back(fbb.CreateString(bone.name));
      bone_transforms.push_back(FlatBufferMat3x4(bone.relative_transform));
      bone_parents.push_back(static_cast<BoneIndex>(BoneParent(i)));
    }

    // Then create a FlatBuffer vector for each array.
    auto vertices_fb = fbb.CreateVectorOfStructs(vertices);
    auto normals_fb = fbb.CreateVectorOfStructs(normals);
    auto tangents_fb = fbb.CreateVectorOfStructs(tangents);
    auto colors_fb =
        export_vertex_color_ ? fbb.CreateVectorOfStructs(colors) : 0;
    auto uvs_fb = fbb.CreateVectorOfStructs(uvs);
    auto skin_indices_fb = fbb.CreateVectorOfStructs(skin_indices);
    auto skin_weights_fb = fbb.CreateVectorOfStructs(skin_weights);
    auto max_fb = FlatBufferVec3(max_position_);
    auto min_fb = FlatBufferVec3(min_position_);
    auto bone_names_fb = fbb.CreateVector(bone_names);
    auto bone_transforms_fb = fbb.CreateVectorOfStructs(bone_transforms);
    auto bone_parents_fb = fbb.CreateVector(bone_parents);
    auto shader_to_mesh_bones_fb = fbb.CreateVector(shader_to_mesh_bones);
    auto mesh_fb = meshdef::CreateMesh(
        fbb, surface_vector_fb, vertices_fb, normals_fb, tangents_fb, colors_fb,
        uvs_fb, skin ? skin_indices_fb : 0, skin ? skin_weights_fb : 0, &max_fb,
        &min_fb, bone_names_fb, bone_transforms_fb, bone_parents_fb,
        shader_to_mesh_bones_fb);
    meshdef::FinishMeshBuffer(fbb, mesh_fb);

    log_.Log(kLogImportant, "Skin: %s\n", skin ? "yes" : "no");

    // Write the buffer to a file.
    OutputFlatBufferBuilder(fbb, full_mesh_file_name);
  }

  int BoneParent(int i) const {
    // Return invalid index if we're at a root.
    const int depth = bones_[i].depth;
    if (depth <= 0) return -1;

    // Advance to the parent of 'i'.
    // `bones_` is listed in depth-first order, so we look for the previous
    // non-sibling.
    while (bones_[i].depth >= depth) {
      i--;
      assert(i >= 0);
    }
    assert(bones_[i].depth == depth - 1);
    return i;
  }

  mat4 BoneGlobalTransform(int i) const {
    mat4 m = bones_[i].relative_transform;
    for (;;) {
      i = BoneParent(i);
      if (i < 0) break;

      // Update with parent transform.
      m = bones_[i].relative_transform * m;
    }
    return m;
  }

  bool BoneHasVertices(size_t bone_idx) const {
    const size_t end_vertex_index =
        bone_idx + 1 == bones_.size() ? points_.size()
                                      : bones_[bone_idx + 1].first_vertex_index;
    return end_vertex_index - bones_[bone_idx].first_vertex_index > 0;
  }

  void CalculateBoneIndexMaps(
      std::vector<BoneIndex>* mesh_to_shader_bones,
      std::vector<BoneIndex>* shader_to_mesh_bones) const {
    mesh_to_shader_bones->clear();
    shader_to_mesh_bones->clear();

    // Only bones that have vertices weighte to them are uploaded to the
    // shader.
    BoneIndex shader_bone = 0;
    mesh_to_shader_bones->reserve(bones_.size());
    for (BoneIndex mesh_bone = 0; mesh_bone < bones_.size(); ++mesh_bone) {
      if (BoneHasVertices(mesh_bone)) {
        mesh_to_shader_bones->push_back(shader_bone);
        shader_to_mesh_bones->push_back(mesh_bone);
        shader_bone++;
      } else {
        mesh_to_shader_bones->push_back(kInvalidBoneIdx);
      }
    }
  }

  SurfaceMap surfaces_;
  VertexSet unique_;
  std::vector<Vertex> points_;
  IndexBuffer* cur_index_buf_;
  bool export_vertex_color_;
  vec3 max_position_;
  vec3 min_position_;
  std::vector<Bone> bones_;

  // Information and warnings.
  Logger& log_;
};

/// @class FbxMeshParser
/// @brief Load FBX files and save their geometry in our FlatBuffer format.
class FbxMeshParser {
 public:
  explicit FbxMeshParser(Logger& log)
      : manager_(nullptr), scene_(nullptr), hierarchy_(false), log_(log) {
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

  bool Load(const char* file_name, AxisSystem axis_system, bool recenter,
            bool hierarchy) {
    if (!Valid()) return false;

    log_.Log(kLogImportant,
        "\n---- mesh_pipeline: %s ------------------------------------------\n",
        BaseFileName(file_name).c_str());

    // Create the importer and initialize with the file.
    FbxImporter* importer = FbxImporter::Create(manager_, "");
    const bool init_status =
        importer->Initialize(file_name, -1, manager_->GetIOSettings());

    // Check the SDK and pipeline versions.
    int sdk_major = 0, sdk_minor = 0, sdk_revision = 0;
    int file_major = 0, file_minor = 0, file_revision = 0;
    FbxManager::GetFileFormatVersion(sdk_major, sdk_minor, sdk_revision);
    importer->GetFileVersion(file_major, file_minor, file_revision);

    // Report version information.
    log_.Log(kLogInfo,
             "Loading %s (version %d.%d.%d) with SDK version %d.%d.%d\n",
             RemoveDirectoryFromName(file_name).c_str(), file_major, file_minor,
             file_revision, sdk_major, sdk_minor, sdk_revision);

    // Exit on load error.
    if (!init_status) {
      FbxString error = importer->GetStatus().GetErrorString();
      log_.Log(kLogError, "%s\n\n", error.Buffer());
      return false;
    }
    if (!importer->IsFBX()) {
      log_.Log(kLogError, "Not an FBX file\n\n");
      return false;
    }

    // Import the scene.
    const bool import_status = importer->Import(scene_);

    // Clean-up temporaries.
    importer->Destroy();

    // Exit if the import failed.
    if (!import_status) return false;

    // Remember if we're recording hierarchy information for these meshes,
    // or flattening it.
    hierarchy_ = hierarchy;

    // Remember the source file name so we can search for textures nearby.
    mesh_file_name_ = std::string(file_name);

    // Log nodes before we do anything to them.
    LogNodes("Original scene nodes\n");

    // Ensure the correct axis system is being used.
    ConvertAxes(axis_system);

    // Bring the geo into our format.
    ConvertGeometry(recenter);

    // Log nodes after we've processed them.
    LogNodes("Converted scene nodes\n");
    return true;
  }

  // Return an upper bound on the number of vertices in the scene.
  int NumVertsUpperBound() const {
    // The scene's been triangulated, so there are three verts per poly.
    // Many of those verts may be duplicates, but we're only looking for an
    // upper bound.
    return 3 * NumPolysRecursive(scene_->GetRootNode());
  }

  // Gather converted geometry into our `FlatMesh` class.
  void GatherFlatMesh(FlatMesh* out) const {
    // Traverse the scene and output one surface per mesh.
    FbxNode* root_node = scene_->GetRootNode();
    GatherFlatMeshRecursive(-1, root_node, root_node, out);
  }

 private:
  FPL_DISALLOW_COPY_AND_ASSIGN(FbxMeshParser);

  // For debugging. Output details of the the node transforms.
  void LogNodes(const char* header) const {
    log_.Log(kLogVerbose, "--------------------\n%s\n", header);
    LogNodesRecursive(scene_->GetRootNode(), FbxNode::eSourcePivot);
  }

  void LogNodesRecursive(const FbxNode* node,
                         FbxNode::EPivotSet pivot_set) const {
    if (log_.level() > kLogVerbose) return;

    log_.Log(kLogVerbose, "-----\nNode: %s\n", node->GetName());

    const FbxVector4 post_rotation = node->GetPostRotation(pivot_set);
    const FbxVector4 pre_rotation = node->GetPreRotation(pivot_set);
    const FbxVector4 rotation_offset = node->GetRotationOffset(pivot_set);
    const FbxVector4 scaling_offset = node->GetScalingOffset(pivot_set);
    const FbxVector4 rotation_pivot = node->GetRotationPivot(pivot_set);
    const FbxVector4 scaling_pivot = node->GetScalingPivot(pivot_set);
    const FbxVector4 geometric_translation =
        node->GetGeometricTranslation(pivot_set);
    const FbxVector4 geometric_rotation = node->GetGeometricRotation(pivot_set);
    const FbxVector4 geometric_scaling = node->GetGeometricScaling(pivot_set);
    const FbxDouble3 lcl_translation = node->LclTranslation.Get();
    const FbxDouble3 lcl_rotation = node->LclRotation.Get();
    const FbxDouble3 lcl_scaling = node->LclScaling.Get();

    log_.Log(kLogVerbose, "post_rotation = (%0.3f, %0.3f, %0.3f)\n",
             post_rotation[0], post_rotation[1], post_rotation[2]);
    log_.Log(kLogVerbose, "pre_rotation = (%0.3f, %0.3f, %0.3f)\n",
             pre_rotation[0], pre_rotation[1], pre_rotation[2]);
    log_.Log(kLogVerbose, "rotation_offset = (%0.3f, %0.3f, %0.3f)\n",
             rotation_offset[0], rotation_offset[1], rotation_offset[2]);
    log_.Log(kLogVerbose, "scaling_offset = (%0.3f, %0.3f, %0.3f)\n",
             scaling_offset[0], scaling_offset[1], scaling_offset[2]);
    log_.Log(kLogVerbose, "rotation_pivot = (%0.3f, %0.3f, %0.3f)\n",
             rotation_pivot[0], rotation_pivot[1], rotation_pivot[2]);
    log_.Log(kLogVerbose, "scaling_pivot = (%0.3f, %0.3f, %0.3f)\n",
             scaling_pivot[0], scaling_pivot[1], scaling_pivot[2]);
    log_.Log(kLogVerbose, "geometric_translation = (%0.3f, %0.3f, %0.3f)\n",
             geometric_translation[0], geometric_translation[1],
             geometric_translation[2]);
    log_.Log(kLogVerbose, "geometric_rotation = (%0.3f, %0.3f, %0.3f)\n",
             geometric_rotation[0], geometric_rotation[1],
             geometric_rotation[2]);
    log_.Log(kLogVerbose, "geometric_scaling = (%0.3f, %0.3f, %0.3f)\n",
             geometric_scaling[0], geometric_scaling[1], geometric_scaling[2]);
    log_.Log(kLogVerbose, "lcl_translation = (%0.3f, %0.3f, %0.3f)\n",
             lcl_translation[0], lcl_translation[1], lcl_translation[2]);
    log_.Log(kLogVerbose, "lcl_rotation = (%0.3f, %0.3f, %0.3f)\n",
             lcl_rotation[0], lcl_rotation[1], lcl_rotation[2]);
    log_.Log(kLogVerbose, "lcl_scaling = (%0.3f, %0.3f, %0.3f)\n",
             lcl_scaling[0], lcl_scaling[1], lcl_scaling[2]);

    for (int i = 0; i < node->GetChildCount(); i++) {
      LogNodesRecursive(node->GetChild(i), pivot_set);
    }
  }

  void ConvertAxes(AxisSystem axis_system) {
    if (axis_system < 0) return;

    const FbxAxisSystem import_axes =
        scene_->GetGlobalSettings().GetAxisSystem();
    const FbxAxisSystem export_axes = ConvertAxisSystemToFbx(axis_system);
    if (import_axes == export_axes) {
      log_.Log(kLogInfo, "Scene's axes are already %s.\n",
               kAxisSystemNames[ConvertAxisSystemFromFbx(export_axes)]);
      return;
    }

    log_.Log(kLogImportant,
             "Converting scene's axes (%s) to requested axes (%s).\n",
             kAxisSystemNames[ConvertAxisSystemFromFbx(import_axes)],
             kAxisSystemNames[ConvertAxisSystemFromFbx(export_axes)]);
    export_axes.ConvertScene(scene_);
  }

  void ConvertGeometry(bool recenter) {
    FbxGeometryConverter geo_converter(manager_);

    // Ensure origin is in the center of geometry.
    if (recenter) {
      const bool recentered =
          geo_converter.RecenterSceneToWorldCenter(scene_, 0.0);
      if (recentered) {
        log_.Log(kLogInfo, "Recentering\n");
      } else {
        log_.Log(kLogImportant,
                 "Already centered so ignoring recenter request\n");
      }
    }

    // Ensure each mesh has only one texture, and only triangles.
    geo_converter.SplitMeshesPerMaterial(scene_, true);
    geo_converter.Triangulate(scene_, true);

    // Traverse all meshes in the scene, generating normals and tangents.
    ConvertGeometryRecursive(scene_->GetRootNode());

    // For each mesh, set the geometry origin as the pivot.
    if (hierarchy_) {
      scene_->GetRootNode()->SetRotationPivotAsCenterRecursive();
    }
  }

  void ConvertGeometryRecursive(FbxNode* node) {
    if (node == nullptr) return;

    // When exporting the hierarchy, we center about the rotation pivot,
    // and then ignore the rotation and scaling pivots. In this manner its
    // easy to apply animated rotation and scaling values.
    if (hierarchy_) {
      const FbxVector4 rotation_pivot =
          node->GetRotationPivot(FbxNode::eSourcePivot);
      const FbxVector4 scaling_pivot =
          node->GetScalingPivot(FbxNode::eSourcePivot);
      if (rotation_pivot != scaling_pivot) {
        log_.Log(kLogWarning,
                 "Node %s has different rotation (%0.3f, %0.3f, %0.3f) and "
                 "scale (%0.3f, %0.3f, %0.3f) pivots. Scaling will not be "
                 "properly centered.\n",
                 node->GetName(), rotation_pivot[0], rotation_pivot[1],
                 rotation_pivot[2], scaling_pivot[0], scaling_pivot[1],
                 scaling_pivot[2]);
      }
    }

    // We're only interested in meshes, for the moment.
    for (int i = 0; i < node->GetNodeAttributeCount(); ++i) {
      FbxNodeAttribute* attr = node->GetNodeAttributeByIndex(i);
      if (attr == nullptr ||
          attr->GetAttributeType() != FbxNodeAttribute::eMesh) continue;
      FbxMesh* mesh = static_cast<FbxMesh*>(attr);

      // Generate normals. Leaves existing normal data if it already exists.
      const bool normals_generated = mesh->GenerateNormals();
      if (!normals_generated) {
        log_.Log(kLogWarning, "Could not generate normals for mesh %s\n",
                 mesh->GetName());
      }

      // Generate tangents. Leaves existing tangent data if it already exists.
      if (mesh->GetElementUVCount() > 0) {
        const bool tangents_generated = mesh->GenerateTangentsData(0);
        if (!tangents_generated) {
          log_.Log(kLogWarning, "Could not generate tangents for mesh %s\n",
                   mesh->GetName());
        }
      }
    }

    // Recursively traverse each node in the scene
    for (int i = 0; i < node->GetChildCount(); i++) {
      ConvertGeometryRecursive(node->GetChild(i));
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
  const FbxGeometryElementUV* UvElement(const FbxMesh* mesh) const {
    // Grab texture coordinates.
    const int uv_count = mesh->GetElementUVCount();
    if (uv_count <= 0) {
      log_.Log(kLogWarning, "No UVs for mesh %s\n", mesh->GetName());
      return nullptr;
    }

    // Always use the first UV set.
    const FbxGeometryElementUV* uv_element = mesh->GetElementUV(0);

    // Warn if multiple UV sets exist.
    if (uv_count > 1 && log_.level() >= kLogWarning) {
      FbxStringList uv_set_names;
      mesh->GetUVSetNames(uv_set_names);
      log_.Log(kLogWarning, "Multiple UVs for mesh %s. Using %s. Ignoring %s\n",
               mesh->GetName(), uv_set_names.GetStringAt(0),
               uv_set_names.GetStringAt(1));
    } else {
      log_.Log(kLogVerbose, "Using UV map %s for mesh %s.\n",
               uv_element->GetName(), mesh->GetName());
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
      color->Set(factor * base.mRed, factor * base.mGreen,
                 factor * base.mBlue, base.mAlpha);
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

    for (int j = 0; j < material_indices->GetCount(); ++j) {
      // Check every material attached to this mesh.
      const int material_index = (*material_indices)[j];
      const FbxSurfaceMaterial* material = node->GetMaterial(material_index);
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
    return FileExists(file_name, kCaseSensitive);
  }

  // Try variations of the texture name until we find one on disk.
  std::string FindSourceTextureFileName(
      const std::string& source_mesh_name,
      const std::string& texture_name) const {
    std::string attempted_textures;

    // If the texture name is relative, check for it relative to the
    // source mesh's directory.
    const std::string source_dir = DirectoryName(source_mesh_name);
    if (!AbsoluteFileName(texture_name)) {
      const std::string texture_rel_name = source_dir + texture_name;
      if (TextureFileExists(texture_rel_name)) return texture_rel_name;
      attempted_textures += texture_rel_name + '\n';
    }

    // If the texture exists in the same directory as the source mesh, use it.
    const std::string texture_no_dir = RemoveDirectoryFromName(texture_name);
    const std::string texture_in_source_dir = source_dir + texture_no_dir;
    if (TextureFileExists(texture_in_source_dir)) return texture_in_source_dir;
    attempted_textures += texture_in_source_dir + '\n';

    // Check to see if there's a texture with the same base name as the mesh.
    const std::string source_name = BaseFileName(source_mesh_name);
    const std::string texture_extension = FileExtension(texture_name);
    const std::string source_texture =
        source_dir + source_name + "." + texture_extension;
    if (TextureFileExists(source_texture)) return source_texture;
    attempted_textures += source_texture + '\n';

    // Gather potential base names for the texture (i.e. name without directory
    // or extension).
    const std::string base_name = BaseFileName(texture_no_dir);
    const std::string base_names[] = {
      base_name,
      SnakeCase(base_name),
      CamelCase(base_name),
      source_name
    };

    // For each potential base name, loop through known image file extensions.
    // The image may have been converted to a new format.
    for (size_t i = 0; i < FPL_ARRAYSIZE(base_names); ++i) {
      for (size_t j = 0; j < FPL_ARRAYSIZE(kImageExtensions); ++j) {
        const std::string potential_name = source_dir + base_names[i] + "." +
                                           kImageExtensions[j];
        if (TextureFileExists(potential_name)) return potential_name;
        attempted_textures += potential_name + '\n';
      }
    }

    // As a last resort, use the texture name as supplied. We don't want to
    // do this, normally, since the name can be an absolute path on the drive,
    // or relative to the directory we're currently running from.
    if (TextureFileExists(texture_name)) return texture_name;
    attempted_textures += texture_name + '\n';

    // Texture can't be found.
    log_.Log(kLogWarning, "Can't find texture `%s`. Tried these variants:\n%s\n",
             texture_name.c_str(), attempted_textures.c_str());
    return "";
  }

  std::string TextureFileName(FbxNode* node, const FbxMesh* mesh,
                              const char* texture_property) const {
    // Grab the texture attached to this node.
    const FbxFileTexture* texture = TextureFromNode(node, mesh, texture_property);
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
      log_.Log(kLogInfo, " Mapping %s texture `%s` to shader texture %d\n",
               texture_property, RemoveDirectoryFromName(texture).c_str(),
               textures.Count());
      textures.Append(texture);
    }

    return textures;
  }

  // Factor node's global_transform into two transforms:
  //   point_transform <== apply in pipeline
  //   relative_transform <== apply at runtime
  void Transforms(FbxNode* node, FbxNode* parent_node,
                  FbxAMatrix* relative_transform,
                  FbxAMatrix* point_transform) const {
    const FbxAMatrix global_transform = node->EvaluateGlobalTransform();

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

    if (hierarchy_) {
      // The FBX tranform format is defined as below (see
      // http://help.autodesk.com/view/FBX/2016/ENU/?guid=__files_GUID_10CDD63C_79C1_4F2D_BB28_AD2BE65A02ED_htm):
      //
      // WorldTransform = ParentWorldTransform * T * Roff * Rp * Rpre * R *
      //                  Rpost_inv * Rp_inv * Soff * Sp * S * Sp_inv
      //
      // In animation data, we animate T, R, and S (translation, rotation,
      // scaling).
      // However, we must make some assumptions so that T, R, and S they can be
      // applied in order:
      //
      // Rp, Sp, Soff are translations that must be 0
      // Rpre and Rpost are rotations that must be 0
      //
      // Then, since Roff and T are translations and therefore commutitive, we
      // have:
      //
      // WorldTransform = ParentWorldTransform * Roff * T * R * S
      //
      // Note: We still use EvaluateGlobalTransform() instead of calculating it
      //       ourselves because there could be intermediate nodes that are
      //       not animated. These intermediate nodes do not have the same
      //       restrictions.
      const FbxVector4 post_rotation =
          node->GetPostRotation(FbxNode::eSourcePivot);
      const FbxVector4 pre_rotation =
          node->GetPreRotation(FbxNode::eSourcePivot);
      const FbxVector4 scaling_offset =
          node->GetScalingOffset(FbxNode::eSourcePivot);
      const FbxVector4 rotation_pivot =
          node->GetRotationPivot(FbxNode::eSourcePivot);
      const FbxVector4 scaling_pivot =
          node->GetScalingPivot(FbxNode::eSourcePivot);
      const FbxVector4 zero(0.0, 0.0, 0.0, 0.0);
      if (post_rotation != zero) {
        log_.Log(kLogWarning,
                 "post_rotation (%.3f, %.3f, %.3f) is non-zero. Animations "
                 "will be incorrect.\n",
                 post_rotation[0], post_rotation[1], post_rotation[2]);
      }
      if (pre_rotation != zero && node->GetSkeleton() == nullptr) {
        log_.Log(kLogWarning,
                 "pre_rotation (%.3f, %.3f, %.3f) is non-zero. Only joints are "
                 "allowed to have non-zero pre_rotations. Animations will "
                 "be incorrect.\n",
                 pre_rotation[0], pre_rotation[1], pre_rotation[2]);
      }
      if (scaling_offset != zero) {
        log_.Log(kLogWarning,
                 "scaling_offset (%.3f, %.3f, %.3f) is non-zero. Animations "
                 "will be incorrect.\n",
                 scaling_offset[0], scaling_offset[1], scaling_offset[2]);
      }
      if (rotation_pivot != zero) {
        log_.Log(kLogWarning,
                 "rotation_pivot (%.3f, %.3f, %.3f) is non-zero. Animations "
                 "will be incorrect.\n",
                 rotation_pivot[0], rotation_pivot[1], rotation_pivot[2]);
      }
      if (scaling_pivot != zero) {
        log_.Log(kLogWarning,
                 "scaling_pivot (%.3f, %.3f, %.3f) is non-zero. Animations "
                 "will be incorrect.\n",
                 scaling_pivot[0], scaling_pivot[1], scaling_pivot[2]);
      }

      *relative_transform =
          parent_node->EvaluateGlobalTransform().Inverse() * global_transform;
      *point_transform = geometric_transform;

    } else {
      // When meshes are output in global space, there are no parents (all is
      // flat, so the relative transform is the identity). The points are
      // brought into global space.
      relative_transform->SetIdentity();
      *point_transform = global_transform * geometric_transform;
    }
  }

  // For each mesh in the tree of nodes under `node`, add a surface to `out`.
  void GatherFlatMeshRecursive(int depth, FbxNode* node, FbxNode* parent_node,
                               FlatMesh* out) const {
    // We're only interested in mesh nodes. If a node and all nodes under it
    // have no meshes, we early out.
    if (node == nullptr || !NodeHasMesh(node)) return;
    log_.Log(kLogInfo, "Node: %s\n", node->GetName());

    // The root node cannot have a transform applied to it, so we do not
    // export it as a bone.
    if (node != scene_->GetRootNode()) {
      // Get the transform to this node from its parent.
      FbxAMatrix relative_transform;
      FbxAMatrix point_transform;
      Transforms(node, parent_node, &relative_transform, &point_transform);

      // Create a "bone" for this node. The mesh_pipeline doesn't support
      // skeleton definitions yet, but we use the mesh hierarchy to represent
      // the animations.
      out->AppendBone(node->GetName(), Mat4FromFbx(relative_transform), depth);

      // Gather mesh data for this bone.
      // Note that there may be more than one mesh attached to a node.
      for (int i = 0; i < node->GetNodeAttributeCount(); ++i) {
        const FbxNodeAttribute* attr = node->GetNodeAttributeByIndex(i);
        if (attr == nullptr ||
            attr->GetAttributeType() != FbxNodeAttribute::eMesh)
          continue;
        const FbxMesh* mesh = static_cast<const FbxMesh*>(attr);

        // Gather the textures attached to this mesh.
        std::string normal_map_file_name;
        const FlatTextures textures = GatherTextures(node, mesh);
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
        GatherFlatSurface(mesh, point_transform, has_solid_color, solid_color,
                          out);
      }
    }

    // Recursively traverse each node in the scene
    for (int i = 0; i < node->GetChildCount(); i++) {
      GatherFlatMeshRecursive(hierarchy_ ? depth + 1 : depth, node->GetChild(i),
                              node, out);
    }
  }

  void GatherFlatSurface(const FbxMesh* mesh, const FbxAMatrix& point_transform,
                         bool has_solid_color, const FbxColor& solid_color,
                         FlatMesh* out) const {
    const FbxAMatrix& t = point_transform;
    log_.Log(kLogVerbose,
        "    transform: {%.3f %.3f %.3f %.3f}\n"
        "               {%.3f %.3f %.3f %.3f}\n"
        "               {%.3f %.3f %.3f %.3f}\n"
        "               {%.3f %.3f %.3f %.3f}\n",
        t[0][0], t[0][1], t[0][2], t[0][3],
        t[1][0], t[1][1], t[1][2], t[1][3],
        t[2][0], t[2][1], t[2][2], t[2][3],
        t[3][0], t[3][1], t[3][2], t[3][3]);

    // Affine matrix only supports multiplication by a point, not a vector.
    // That is, there is no way to ignore the translation (as is required
    // for normals and tangents). So, we create a copy of `transform` that
    // has no translation.
    // http://forums.autodesk.com/t5/fbx-sdk/matrix-vector-multiplication/td-p/4245079
    FbxAMatrix vector_transform = point_transform;
    vector_transform.SetT(FbxVector4(0.0, 0.0, 0.0, 0.0));

    // Get references to various vertex elements.
    const FbxVector4* vertices = mesh->GetControlPoints();
    const FbxGeometryElementUV* uv_element = UvElement(mesh);
    const FbxGeometryElementNormal* normal_element = mesh->GetElementNormal();
    const FbxGeometryElementTangent* tangent_element = mesh->GetElementTangent();
    const FbxGeometryElementVertexColor* color_element =
        mesh->GetElementVertexColor();
    assert(uv_element != nullptr && normal_element != nullptr &&
           tangent_element != nullptr);
    out->SetExportVertexColor(color_element != nullptr || has_solid_color);
    log_.Log(kLogVerbose,
             color_element != nullptr ? "Mesh has vertex colors\n" :
             has_solid_color ? "Mesh material has a solid color\n" :
             "Mesh does not have vertex colors\n");

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
            ElementFromIndices(*normal_element, control_index, vertex_counter);
        const FbxVector4 tangent_fbx =
            ElementFromIndices(*tangent_element, control_index, vertex_counter);
        const FbxColor color_fbx =
            color_element != nullptr
                ? ElementFromIndices(*color_element, control_index,
                                     vertex_counter)
                : has_solid_color ? solid_color : kDefaultColor;
        const FbxVector2 uv_fbx =
            ElementFromIndices(*uv_element, control_index, vertex_counter);

        // Output this poly-vert.
        // Note that the v-axis is flipped between FBX UVs and FlatBuffer UVs.
        const vec3 vertex = Vec3FromFbx(point_transform.MultT(vertex_fbx));
        const vec3 normal =
            Vec3FromFbx(vector_transform.MultT(normal_fbx)).Normalized();
        const vec4 tangent(
            Vec3FromFbx(vector_transform.MultT(tangent_fbx)).Normalized(),
            static_cast<float>(tangent_fbx[3]));
        const vec4 color = Vec4FromFbx(color_fbx);
        const vec2 uv =
            Vec2FromFbx(FbxVector2(uv_fbx.mData[0], 1.0 - uv_fbx.mData[1]));
        out->AppendPolyVert(vertex, normal, tangent, color, uv);

        // Control points are listed in order of poly + vertex.
        vertex_counter++;
      }
    }
  };

  static AxisSystem ConvertAxisSystemFromFbx(const FbxAxisSystem& axis) {
    int up_sign = 0;
    int front_sign = 0;
    const FbxAxisSystem::EUpVector up = axis.GetUpVector(up_sign);
    const FbxAxisSystem::EFrontVector front = axis.GetFrontVector(front_sign);
    const FbxAxisSystem::ECoordSystem coord = axis.GetCoorSystem();
    assert(up_sign > 0);

    const int up_idx = up - FbxAxisSystem::EUpVector::eXAxis;
    const int front_idx = front - FbxAxisSystem::EFrontVector::eParityEven;
    const int front_sign_idx = front_sign > 0 ? 0 : 1;
    const int coord_idx = coord - FbxAxisSystem::eRightHanded;
    return static_cast<AxisSystem>(
        8 * up_idx + 4 * front_idx + 2 * front_sign_idx + coord_idx);
  }

  static FbxAxisSystem ConvertAxisSystemToFbx(AxisSystem system) {
    const int up_idx = system / 8 + FbxAxisSystem::EUpVector::eXAxis;
    const int front_sign = system % 4 < 2 ? 1 : -1;
    const int front_idx = (system % 8) / 4 + FbxAxisSystem::EFrontVector::eParityEven;
    const int coord_idx = system % 2;

    const auto up = static_cast<FbxAxisSystem::EUpVector>(up_idx);
    const auto front = static_cast<FbxAxisSystem::EFrontVector>(front_sign * front_idx);
    const auto coord = static_cast<FbxAxisSystem::ECoordSystem>(coord_idx);
    return FbxAxisSystem(up, front, coord);
  }

  // Return true if `node` or any of its children has a mesh.
  static bool NodeHasMesh(FbxNode* node) {
    if (node->GetMesh() != nullptr) return true;

    // Recursively traverse each child node.
    for (int i = 0; i < node->GetChildCount(); i++) {
      if (NodeHasMesh(node->GetChild(i))) return true;
    }
    return false;
  }

  // Entry point to the FBX SDK.
  FbxManager* manager_;

  // Hold the FBX file data.
  FbxScene* scene_;

  // Name of source mesh file. Used to search for textures, when the textures
  // are not found in their referenced location.
  std::string mesh_file_name_;

  // If true, output mesh hiearchically, with each child node relative to the
  // transform of its parents. If false, output mesh flat, with all points in
  // global space.
  bool hierarchy_;

  // Information and warnings.
  Logger& log_;
};

struct MeshPipelineArgs {
  MeshPipelineArgs()
    : blend_mode(static_cast<matdef::BlendMode>(-1)),
      axis_system(kUnspecifiedAxisSystem),
      recenter(false),
      hierarchy(false),
      log_level(kLogWarning) {}

  std::string fbx_file;        /// FBX input file to convert.
  std::string asset_base_dir;  /// Directory from which all assets are loaded.
  std::string asset_rel_dir;   /// Directory (relative to base) to output files.
  std::string texture_extension;/// Extension of textures in material file.
  std::vector<matdef::TextureFormat> texture_formats;
  matdef::BlendMode blend_mode;
  AxisSystem axis_system;
  bool recenter;               /// Translate geometry to origin.
  bool hierarchy;              /// Mesh vertices output relative to local pivot.
  LogLevel log_level;          /// Amount of logging to dump during conversion.
};

// Returns index of `s` in `array_of_strings`, or -1 if `s` not found.
// `array_of_strings` is a null-terminated array of char* strings, in the style
// generated by FlatBuffers.
static int IndexOfString(const char* s, const char** array_of_strings) {
  int i = 0;
  for (const char** a = array_of_strings; *a != nullptr; ++a) {
    if (strcmp(*a, s) == 0) return i;
    ++i;
  }
  return -1;
}

static AxisSystem ParseAxisSystem(const char* s) {
  return static_cast<AxisSystem>(IndexOfString(s, kAxisSystemNames));
}

static matdef::TextureFormat ParseTextureFormat(const char* s) {
  return static_cast<matdef::TextureFormat>(
        IndexOfString(s, matdef::EnumNamesTextureFormat()));
}

static matdef::BlendMode ParseBlendMode(const char* s) {
  return static_cast<matdef::BlendMode>(
        IndexOfString(s, matdef::EnumNamesBlendMode()));
}

static bool ParseTextureFormats(
    const std::string& arg, Logger& log,
    std::vector<matdef::TextureFormat>* texture_formats) {
  // No texture formats specified is valid. Always use `AUTO`.
  if (arg.size() == 0) return true;

  // Loop through the comma-delimited string of texture formats.
  size_t format_start = 0;
  for (;;) {
    // Get substring with the name of one texture format.
    const size_t comma = arg.find_first_of(',', format_start);
    const std::string s = arg.substr(format_start, comma);

    // Parse the format. If it is invalid, log an error and exit.
    const matdef::TextureFormat format = ParseTextureFormat(s.c_str());
    if (format < 0) {
      log.Log(kLogError, "Invalid texture format `%s`\n", s.c_str());
      return false;
    }
    texture_formats->push_back(format);

    // Break if on last texture format. Otherwise, advance to next argument.
    if (comma == std::string::npos) return true;
    format_start = comma + 1;
  }
}

static bool TextureFormatHasAlpha(matdef::TextureFormat format) {
 return format == matdef::TextureFormat_F_8888;
}

static matdef::BlendMode DefaultBlendMode(
    const std::vector<matdef::TextureFormat>& texture_formats) {
  return texture_formats.size() > 0 && TextureFormatHasAlpha(texture_formats[0]) ?
      matdef::BlendMode_ALPHA : matdef::BlendMode_OFF;
}

static void LogOptions(const char* indent, const char** array_of_options,
                       Logger& log) {
  for (const char** option = array_of_options; *option != nullptr; ++option) {
    log.Log(kLogImportant, "%s%s\n", indent, *option);
  }
}

static bool ParseMeshPipelineArgs(int argc, char** argv, Logger& log,
                                  MeshPipelineArgs* args) {
  bool valid_args = true;

  // Last parameter is used as file name.
  if (argc > 1) {
    args->fbx_file = std::string(argv[argc - 1]);
  }

  // Ensure file name is valid.
  const bool valid_fbx_file =
      args->fbx_file.length() > 0 && args->fbx_file[0] != '-';
  if (!valid_fbx_file) {
    valid_args = false;
  }

  // Parse switches.
  for (int i = 1; i < argc - 1; ++i) {
    const std::string arg = argv[i];

    // -v switch
    if (arg == "-v" || arg == "--verbose") {
      args->log_level = kLogVerbose;

      // -d switch
    } else if (arg == "-d" || arg == "--details") {
      args->log_level = kLogImportant;

      // -i switch
    } else if (arg == "-i" || arg == "--info") {
      args->log_level = kLogInfo;

      // -b switch
    } else if (arg == "-b" || arg == "--base-dir") {
      if (i + 1 < argc - 1) {
        args->asset_base_dir = std::string(argv[i + 1]);
        i++;
      } else {
        valid_args = false;
      }

      // -r switch
    } else if (arg == "-r" || arg == "--relative-dir") {
      if (i + 1 < argc - 1) {
        args->asset_rel_dir = std::string(argv[i + 1]);
        i++;
      } else {
        valid_args = false;
      }

    } else if (arg == "-e" || arg == "--texture-extension") {
      if (i + 1 < argc - 1) {
        args->texture_extension = std::string(argv[i + 1]);
        i++;
      } else {
        valid_args = false;
      }

      // -h switch
    } else if (arg == "-h" || arg == "--hierarchy") {
      args->hierarchy = true;

      // -c switch
    } else if (arg == "-c" || arg == "--center") {
      args->recenter = true;

      // -f switch
    } else if (arg == "-f" || arg == "--texture-formats") {
      if (i + 1 < argc - 1) {
        valid_args = ParseTextureFormats(
            std::string(argv[i + 1]), log, &args->texture_formats);
        if (!valid_args) {
          log.Log(kLogError, "Unknown texture format: %s\n\n", argv[i + 1]);
        }
        i++;
      } else {
        valid_args = false;
      }

      // -m switch
    } else if (arg == "-m" || arg == "--blend-mode") {
      if (i + 1 < argc - 1) {
        args->blend_mode = ParseBlendMode(argv[i + 1]);
        valid_args = args->blend_mode >= 0;
        if (!valid_args) {
          log.Log(kLogError, "Unknown blend mode: %s\n\n", argv[i + 1]);
        }
        i++;
      } else {
        valid_args = false;
      }

    } else if (arg == "-a" || arg == "--axes") {
      if (i + 1 < argc - 1) {
        args->axis_system = ParseAxisSystem(argv[i + 1]);
        valid_args = args->axis_system >= 0;
        if (!valid_args) {
          log.Log(kLogError, "Unknown coordinate system: %s\n\n", argv[i + 1]);
        }
        i++;
      } else {
        valid_args = false;
      }

      // ignore empty arguments
    } else if (arg == "") {
      // Invalid switch.
    } else {
      log.Log(kLogError, "Unknown parameter: %s\n", arg.c_str());
      valid_args = false;
    }

    if (!valid_args) break;
  }

  // If blend mode not explicitly specified, calculate it from the texture
  // formats.
  if (args->blend_mode < 0) {
    args->blend_mode = DefaultBlendMode(args->texture_formats);
  }

  // Print usage.
  if (!valid_args) {
    static const char kOptionIndent[] = "                           ";
    log.Log(
        kLogImportant,
        "Usage: mesh_pipeline [-b ASSET_BASE_DIR] [-r ASSET_REL_DIR]\n"
        "                     [-e TEXTURE_EXTENSION] [-f TEXTURE_FORMATS]\n"
        "                     [-m BLEND_MODE] [-a AXES] [-h] [-c] [-v|-d|-i]\n"
        "                     FBX_FILE\n"
        "\n"
        "Pipeline to convert FBX mesh data into FlatBuffer mesh data.\n"
        "We output a .fplmesh file and (potentially several) .fplmat files,\n"
        "one for each material. The files have the same base name as\n"
        "FBX_FILE, with a number appended to the .fplmat files if required.\n"
        "The .fplmesh file references the .fplmat files.\n"
        "The .fplmat files reference the textures.\n"
        "\n"
        "Options:\n"
        "  -b, --base-dir ASSET_BASE_DIR\n"
        "  -r, --relative-dir ASSET_REL_DIR\n"
        "                The .fplmesh file and the .fplmat files are output\n"
        "                to the ASSET_BASE_DIR/ASSET_REL_DIR directory.\n"
        "                ASSET_BASE_DIR is the working directory of your app,\n"
        "                from which all files are loaded. The .fplmesh file\n"
        "                references the .fplmat file relative to\n"
        "                ASSET_BASE_DIR, that is, by prefixing ASSET_REL_DIR.\n"
        "                If ASSET_BASE_DIR is unspecified, use current\n"
        "                directory. If ASSET_REL_DIR is unspecified, output\n"
        "                and reference files from ASSET_BASE_DIR.\n"
        "  -e, --texture-extension TEXTURE_EXTENSION\n"
        "                material files use this extension for texture files.\n"
        "                Useful if your textures are externally converted\n"
        "                to a different file format.\n"
        "                If unspecified, uses original file extension.\n"
        "  -f, --texture-formats TEXTURE_FORMATS\n"
        "                comma-separated list of formats for each output\n"
        "                texture. For example, if a mesh has two textures\n"
        "                then `AUTO,F_888` will ensure the second texture's\n"
        "                material has 8-bits of RGB precision.\n"
        "                Default is %s.\n"
        "                Valid possibilities:\n",
        matdef::EnumNameTextureFormat(kDefaultTextureFormat));
    LogOptions(kOptionIndent, matdef::EnumNamesTextureFormat(), log);

    log.Log(
        kLogImportant,
        "  -m, --blend-mode BLEND_MODE\n"
        "                rendering blend mode for the generated materials.\n"
        "                If texture format has an alpha channel, defaults to\n"
        "                ALPHA. Otherwise, defaults to OFF.\n"
        "                Valid possibilities:\n");
    LogOptions(kOptionIndent, matdef::EnumNamesBlendMode(), log);

    log.Log(
        kLogImportant,
        "  -a, --axes AXES\n"
        "                coordinate system of exported file, in format\n"
        "                    (up-axis)(front-axis)(left-axis) \n"
        "                where,\n"
        "                    'up' = [x|y|z]\n"
        "                    'front' = [+x|-x|+y|-y|+z|-z], is the axis\n"
        "                      pointing out of the front of the mesh.\n"
        "                      For example, the vector pointing out of a\n"
        "                      character's belly button.\n"
        "                    'left' = [+x|-x|+y|-y|+z|-z], is the axis\n"
        "                      pointing out the left of the mesh.\n"
        "                      For example, the vector from the character's\n"
        "                      neck to his left shoulder.\n"
        "                For example, 'z+y+x' is z-axis up, positive y-axis\n"
        "                out of a character's belly button, positive x-axis\n"
        "                out of a character's left side.\n"
        "                If unspecified, use file's coordinate system.\n"
        "  -h, --hierarchy\n"
        "                output vertices relative to local pivot of each\n"
        "                sub-mesh. The transforms from parent pivot to local\n"
        "                pivot are also output.\n"
        "                This option is necessary for animated meshes.\n"
        "  -c, --center  ensure world origin is inside geometry bounding box\n"
        "                by adding a translation if required.\n"
        "  -v, --verbose output all informative messages\n"
        "  -d, --details output important informative messages\n"
        "  -i, --info    output more than details, less than verbose\n");
  }

  return valid_args;
}

}  // namespace fpl

int main(int argc, char** argv) {
  using namespace fpl;
  Logger log;

  // Parse the command line arguments.
  MeshPipelineArgs args;
  if (!ParseMeshPipelineArgs(argc, argv, log, &args)) return 1;

  // Update the amount of information we're dumping.
  log.set_level(args.log_level);

  // Load the FBX file.
  FbxMeshParser pipe(log);
  const bool load_status = pipe.Load(args.fbx_file.c_str(), args.axis_system,
                                     args.recenter, args.hierarchy);
  if (!load_status) return 1;

  // Gather data into a format conducive to our FlatBuffer format.
  const int max_verts = pipe.NumVertsUpperBound();
  FlatMesh mesh(max_verts, log);
  pipe.GatherFlatMesh(&mesh);

  // Output gathered data to a binary FlatBuffer.
  const bool output_status = mesh.OutputFlatBuffer(
      args.fbx_file, args.asset_base_dir, args.asset_rel_dir,
      args.texture_extension, args.texture_formats, args.blend_mode,
      args.hierarchy);
  if (!output_status) return 1;

  // Success.
  return 0;
}
