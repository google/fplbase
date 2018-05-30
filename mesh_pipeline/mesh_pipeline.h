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

#ifndef FPLBASE_MESH_PIPELINE_H_
#define FPLBASE_MESH_PIPELINE_H_

#include <string>
#include "fbx_common/fbx_common.h"
#include "flatbuffers/flatbuffers.h"
#include "fplbase/fpl_common.h"
#include "materials_generated.h"

namespace fplbase {

enum VertexAttribute {
  kVertexAttribute_Position,
  kVertexAttribute_Normal,
  kVertexAttribute_Tangent,
  kVertexAttribute_Orientation,
  kVertexAttribute_Uv,
  kVertexAttribute_UvAlt,
  kVertexAttribute_Color,
  kVertexAttribute_Bone,
  kVertexAttribute_Count  // must come at end
};

// Bitwise OR of the kVertexAttributeBits.
typedef uint32_t VertexAttributeBitmask;
static const VertexAttributeBitmask kVertexAttributeBit_Position =
    1 << kVertexAttribute_Position;
static const VertexAttributeBitmask kVertexAttributeBit_Normal =
    1 << kVertexAttribute_Normal;
static const VertexAttributeBitmask kVertexAttributeBit_Tangent =
    1 << kVertexAttribute_Tangent;
static const VertexAttributeBitmask kVertexAttributeBit_Orientation =
    1 << kVertexAttribute_Orientation;
static const VertexAttributeBitmask kVertexAttributeBit_Uv =
    1 << kVertexAttribute_Uv;
static const VertexAttributeBitmask kVertexAttributeBit_UvAlt =
    1 << kVertexAttribute_UvAlt;
static const VertexAttributeBitmask kVertexAttributeBit_Color =
    1 << kVertexAttribute_Color;
static const VertexAttributeBitmask kVertexAttributeBit_Bone =
    1 << kVertexAttribute_Bone;
static const VertexAttributeBitmask
    kVertexAttributeBit_AllAttributesInSourceFile =
        static_cast<VertexAttributeBitmask>(-1) ^
        kVertexAttributeBit_Orientation;

static const char* kVertexAttributeShortNames[] = {
    "p - positions",    "n - normals",      "t - tangents",
    "q - orientations", "u - UVs",          "v - alternate UVs",
    "c - colors",       "b - bone indices", nullptr,
};
static_assert(
    FPL_ARRAYSIZE(kVertexAttributeShortNames) - 1 == kVertexAttribute_Count,
    "kVertexAttributeShortNames is not in sync with VertexAttribute.");

static const matdef::TextureFormat kDefaultTextureFormat =
    matdef::TextureFormat_AUTO;

struct MeshPipelineArgs {
  MeshPipelineArgs();

  std::string fbx_file;        /// FBX input file to convert.
  std::string asset_base_dir;  /// Directory from which all assets are loaded.
  std::string asset_rel_dir;   /// Directory (relative to base) to output files.
  std::string texture_extension;  /// Extension of textures in material file.
  std::vector<matdef::TextureFormat> texture_formats;
  matdef::BlendMode blend_mode;
  fplutil::AxisSystem axis_system;
  float distance_unit_scale;
  bool recenter;         /// Translate geometry to origin.
  bool interleaved;      /// Write vertex attributes interleaved.
  bool force32;          /// Force 32bit indices.
  bool embed_materials;  /// Embed material definitions in fplmesh file.
  VertexAttributeBitmask vertex_attributes;  /// Vertex attributes to output.
  fplutil::LogLevel log_level;  /// Amount of logging to dump during conversion.
  bool gather_textures;         /// Gather textures and generate .fplmat files.
  FbxAMatrix bake_transform;    /// Transform baked into vertices.
};

int RunMeshPipeline(const MeshPipelineArgs& args, fplutil::Logger& log);

}  // namespace fplbase

#endif  // FPLBASE_MESH_PIPELINE_H_
