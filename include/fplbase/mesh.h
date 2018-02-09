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

#ifndef FPL_MESH_H
#define FPL_MESH_H

#include <vector>

#include "fplbase/config.h"  // Must come first.

#include "fplbase/asset.h"
#include "fplbase/async_loader.h"
#include "fplbase/handles.h"
#include "fplbase/material.h"
#include "fplbase/render_state.h"
#include "fplbase/shader.h"
#include "mathfu/constants.h"

namespace fplbase {

/// @file
/// @addtogroup fplbase_mesh
/// @{

class Renderer;
struct MeshImpl;

/// @brief An array of these enums defines the format of vertex data.
enum Attribute {
  kEND = 0,  ///< @brief The array must always be terminated by one of these.
  kPosition3f,
  kNormal3f,
  kTangent4f,  ///< @brief xyz is the tangent vector; w is handedness.
  kTexCoord2f,
  kTexCoordAlt2f,  ///< @brief Second set of UVs for use with e.g. lightmaps.
  kColor4ub,
  kBoneIndices4ub,
  kBoneWeights4ub,
  kPosition2f,   ///< @brief 2D position.  Can't coexist with kPosition3f.
  /// @brief 2 unsigned shorts, normalized to [0,1].  Can't coexist with
  /// kTexCoord2f.
  kTexCoord2us,
  /// @brief A quaternion representation of normal/binormal/tangent.
  /// Order: (vector.xyz, scalar). The handededness is the sign of the scalar.
  kOrientation4f,
};

/// @class Mesh
/// @brief Abstraction for a set of indices, used for rendering.
///
/// A mesh instance contains a VBO and one or more IBO's.
class Mesh : public AsyncAsset {
  friend class Renderer;

 public:
  enum Primitive {
    kTriangles,
    kTriangleStrip,
    kTriangleFan,
    kLines,
    kPoints,
  };

  // Creates a Material object using the matdef::Material.  If the matdef is
  // null, attempts to load the Material from the specified filename instead.
  // If both are null, returns nullptr.
  typedef std::function<Material *(const char *filename,
                                   const matdef::Material *def)>
      MaterialCreateFn;

  /// @brief Initialize a Mesh from a file asynchronously.
  ///
  /// Asynchronously create mesh from a file if filename is valid.
  /// Otherwise, if filename is null, need to call LoadFromMemory to init
  /// manually.
  Mesh(const char *filename = nullptr,
       MaterialCreateFn material_create_fn = nullptr,
       Primitive primitive = kTriangles);

  /// @brief Initialize a Mesh by creating one VBO, and no IBO's.
  Mesh(const void *vertex_data, size_t count, size_t vertex_size,
       const Attribute *format, mathfu::vec3 *max_position = nullptr,
       mathfu::vec3 *min_position = nullptr, Primitive primitive = kTriangles);

  ~Mesh();

  /// @brief Initialize a Mesh by creating one VBO, and no IBO's.
  virtual void LoadFromMemory(const void *vertex_data, size_t count,
                              size_t vertex_size, const Attribute *format,
                              mathfu::vec3 *max_position = nullptr,
                              mathfu::vec3 *min_position = nullptr);

  /// @brief Loads and unpacks the Mesh from 'filename_' and 'data_'.
  virtual void Load();

  /// @brief Creates a mesh from 'data_'.
  virtual bool Finalize();

  /// @brief Whether this object loaded and finalized correctly. Call after
  /// Finalize has been called (by AssetManager::TryFinalize).
  bool IsValid();

  /// @brief Add an index buffer object to be part of this mesh
  ///
  /// Create one IBO to be part of this mesh. May be called more than once.
  ///
  /// @param indices The indices to be included in the IBO.
  /// @param count The number of indices.
  /// @param mat The material associated with the IBO.
  /// @param is_32_bit Specifies that the indices are 32bit. Default 16bit.
  /// @param primitive How the triangles are assembled from the indices.
  void AddIndices(const void *indices, int count, Material *mat,
                  bool is_32_bit = false);

  /// @brief Set the bones used by an animated mesh.
  ///
  /// If mesh is animated set the transform from a bone's parent space into
  /// the bone's local space. Optionally record the bone names, too, for
  /// debugging.
  /// The shader only accesses a bone if at least one vertex is weighted to it.
  /// So, we don't have to pass every bone transform up to the shader. Instead,
  /// we compact the bone transforms by passing only those in
  /// shader_bone_indices.
  ///
  /// @param bone_transforms Array of bones to be used.
  /// @param bone_parents Array that contains, for each bone, the index of its
  ///        parent.
  /// @param bone_names Array containing the names of the bones.
  /// @param num_bones The number of bones in the given array.
  /// @param shader_bone_indices The indices of bones used by the shader.
  /// @param num_shader_bones The number of bones in the shader bones array.
  void SetBones(const mathfu::AffineTransform *bone_transforms,
                const uint8_t *bone_parents, const char **bone_names,
                size_t num_bones, const uint8_t *shader_bone_indices,
                size_t num_shader_bones);

  /// @brief Convert bone transforms for consumption by a skinning shader.
  ///
  /// Vertices are stored in object space, but we need to manipulate them
  /// in bone space, so the shader transform multiplies the inverse of the
  /// default bone transform. See default_bone_transform_inverses_ for details.
  ///
  /// @param bone_transforms Array of bone transforms, in object space.
  ///                        Length num_bones(). ith element represents the
  ///                        tranformation of the ith skeleton bone to its
  ///                        animated position.
  /// @param shader_transforms Output array of transforms, one for each bone
  ///                          that has vertices weighted to it. Bones without
  ///                          any weighted vertices are pruned.
  ///                          Length num_shader_bones().
  void GatherShaderTransforms(const mathfu::AffineTransform *bone_transforms,
                              mathfu::AffineTransform *shader_transforms) const;

  /// @brief Returns the number of index buffer objects in the mesh.
  ///
  /// @return Returns the number of index buffer objects in the mesh.
  size_t GetNumIndexBufferObjects() const;

  /// @brief Get the material associated with the IBO at the given index.
  ///
  /// @param i The index of the IBO.
  /// @return Returns the material of the corresponding IBO.
  Material *GetMaterial(int i) { return indices_[i].mat; }

  const Material *GetMaterial(int i) const { return indices_[i].mat; }

  /// @brief Define the vertex buffer format.
  ///
  /// `format` must have length <= kMaxAttributes, including `kEND`.
  ///
  /// @param format Array of attributes to set the format to, delimitted with
  ///        `kEND`.
  void set_format(const Attribute *format);

  /// @brief Compute normals and tangents given position and texcoords.
  ///
  /// The template type should be a struct with at least the following fields:
  /// mathfu::vec3_packed pos;
  /// mathfu::vec2_packed tc;
  /// mathfu::vec3_packed norm;
  /// mathfu::vec4_packed tangent;
  ///
  /// @param vertices The vertices to computes the information for.
  /// @param indices The indices that make up the mesh.
  /// @param numverts The number of vertices in the vertex array.
  /// @param numindices The number of indices in the index array.
  template <typename T>
  static void ComputeNormalsTangents(T *vertices, const unsigned short *indices,
                                     int numverts, int numindices) {
    std::unique_ptr<mathfu::vec3[]> binormals(new mathfu::vec3[numverts]);

    // set all normals to 0, as we'll accumulate
    for (int i = 0; i < numverts; i++) {
      vertices[i].norm = mathfu::kZeros3f;
      vertices[i].tangent = mathfu::kZeros4f;
      binormals[i] = mathfu::kZeros3f;
    }
    // Go through each triangle and calculate tangent space for it, then
    // contribute results to adjacent triangles.
    // For a description of the math see e.g.:
    // http://www.terathon.com/code/tangent.html
    for (int i = 0; i < numindices; i += 3) {
      auto &v0 = vertices[indices[i + 0]];
      auto &v1 = vertices[indices[i + 1]];
      auto &v2 = vertices[indices[i + 2]];
      // The cross product of two vectors along the triangle surface from the
      // first vertex gives us this triangle's normal.
      auto q1 = mathfu::vec3(v1.pos) - mathfu::vec3(v0.pos);
      auto q2 = mathfu::vec3(v2.pos) - mathfu::vec3(v0.pos);
      auto norm = normalize(cross(q1, q2));
      // Contribute the triangle normal into all 3 verts:
      v0.norm = mathfu::vec3(v0.norm) + norm;
      v1.norm = mathfu::vec3(v1.norm) + norm;
      v2.norm = mathfu::vec3(v2.norm) + norm;
      // Similarly create uv space vectors:
      auto uv1 = mathfu::vec2(v1.tc) - mathfu::vec2(v0.tc);
      auto uv2 = mathfu::vec2(v2.tc) - mathfu::vec2(v0.tc);
      float m = 1 / (uv1.x * uv2.y - uv2.x * uv1.y);
      auto tangent = mathfu::vec4((uv2.y * q1 - uv1.y * q2) * m, 0);
      auto binorm = (uv1.x * q2 - uv2.x * q1) * m;
      v0.tangent = mathfu::vec4(v0.tangent) + tangent;
      v1.tangent = mathfu::vec4(v1.tangent) + tangent;
      v2.tangent = mathfu::vec4(v2.tangent) + tangent;
      binormals[indices[i + 0]] = binorm;
      binormals[indices[i + 1]] = binorm;
      binormals[indices[i + 2]] = binorm;
    }
    // Normalize per vertex tangent space constributions, and pack tangent /
    // binormal into a 4 component tangent.
    for (int i = 0; i < numverts; i++) {
      auto norm = mathfu::vec3(vertices[i].norm);
      auto tangent = mathfu::vec4(vertices[i].tangent);
      // Renormalize all 3 axes:
      norm = normalize(norm);
      tangent = mathfu::vec4(normalize(tangent.xyz()), 0);
      binormals[i] = normalize(binormals[i]);
      tangent = mathfu::vec4(
          // Gram-Schmidt orthogonalize xyz components:
          normalize(tangent.xyz() - norm * dot(norm, tangent.xyz())),
          // The w component is the handedness, set as difference between the
          // binormal we computed from the texture coordinates and that from the
          // cross-product:
          dot(cross(norm, tangent.xyz()), binormals[i]));
      vertices[i].norm = norm;
      vertices[i].tangent = tangent;
    }
  }

  enum {
    kAttributePosition,
    kAttributeNormal,
    kAttributeTangent,
    kAttributeOrientation,
    kAttributeTexCoord,
    kAttributeTexCoordAlt,
    kAttributeColor,
    kAttributeBoneIndices,
    kAttributeBoneWeights,
  };

  /// @brief Compute the byte size for a vertex from given attributes.
  ///
  /// @param attributes The array of attributes describing the vertex,
  /// terminated with kEND.
  /// @return Returns the byte size based on the given attributes.
  static size_t VertexSize(const Attribute *attributes);

  /// @brief Compute the byte offset of an attribute within a given format.
  ///
  /// @param vertex_attributes The array of attributes describing the vertex,
  /// terminated with kEND.
  /// @param attribute The attribute to calculate the offset of.
  /// @return Returns the byte offset of the given attribute.
  static size_t AttributeOffset(const Attribute *vertex_attributes,
                                Attribute attribute);

  /// @brief Checks the vertex format for correctness.
  ///
  /// @param attributes The array of attributes describing the vertex,
  /// terminated with kEND.
  /// @return Returns whether the format is valid.
  static bool IsValidFormat(const Attribute *attributes);

  /// @brief Get the minimum position of an AABB about the mesh.
  ///
  /// @return Returns the minimum position of the mesh.
  const mathfu::vec3 &min_position() const { return min_position_; }
  /// @brief Get the maximum position of an AABB about the mesh.
  ///
  /// @return Returns the maximum position of the mesh.
  const mathfu::vec3 &max_position() const { return max_position_; }
  /// @brief The defines parents of each bone.
  ///
  /// @return Returns an array of indices of each bone's parent.
  const uint8_t *bone_parents() const { return bone_parents_.data(); }
  /// @brief Array of names for each bone.
  ///
  /// @return Returns the array of names for each bone, of length num_bones().
  const std::string *bone_names() const { return bone_names_.data(); }
  /// @brief The array of default bone transform inverses.
  ///
  /// @return Returns the array of default bone transform inverses.
  const mathfu::AffineTransform *default_bone_transform_inverses() const {
    return default_bone_transform_inverses_;
  }
  /// @brief The number of bones in the mesh.
  ///
  /// @return Returns the number of bones.
  size_t num_bones() const { return bone_parents_.size(); }
  /// @brief The indices of bones used by the shader.
  ///
  /// @return Returns an array of indices of bones used by the shader.
  const uint8_t *shader_bone_indices() const {
    return &shader_bone_indices_[0];
  }
  /// @brief The number of bones used by the shader.
  ///
  /// @return Returns the number of bones used by the shader.
  size_t num_shader_bones() const { return shader_bone_indices_.size(); }

  /// @brief The number of vertices in the VBO.
  ///
  /// @return Returns the number of vertices in the VBO.
  size_t num_vertices() const { return num_vertices_; }

  /// @brief The total number of indices in all IBOs.
  ///
  /// @return Returns the total number of indices across all IBOs.
  size_t CalculateTotalNumberOfIndices() const;

  /// @brief Holder for data that can be turned into a mesh.
  struct InterleavedVertexData {
    const void *vertex_data;
    std::vector<uint8_t> owned_vertex_data;
    size_t count;
    size_t vertex_size;
    std::vector<Attribute> format;
    bool has_skinning;

    InterleavedVertexData()
        : vertex_data(nullptr), count(0), vertex_size(0), has_skinning(false) {}
  };

  /// @brief: Load vertex data from a FlatBuffer into CPU memory first.
  static void ParseInterleavedVertexData(const void *meshdef_buffer,
                                         InterleavedVertexData *ivd);

  // Init mesh from MeshDef FlatBuffer.
  bool InitFromMeshDef(const void *meshdef_buffer);

  MATHFU_DEFINE_CLASS_SIMD_AWARE_NEW_DELETE

 private:
  // Disallow copies because of pointers bone_transforms_ and
  // bone_global_transforms_. Feel free to implement copy or move operators
  // if required.
  Mesh(const Mesh &);
  Mesh &operator=(const Mesh &);

  // Free all resources in the platform-independent data (i.e. everything
  // outside of the impl_ class). Implemented in mesh_common.cc.
  void Clear();

  // Free all resources in the platform-dependent data (i.e. everything in the
  // impl_ class). Implemented in platform-dependent code.
  void ClearPlatformDependent();

  // Backend-specific create and destroy calls. These just call new and delete
  // on the platform-specific MeshImpl structs.
  static MeshImpl *CreateMeshImpl();
  static void DestroyMeshImpl(MeshImpl *impl);

  static const int kMaxAttributes = 10;

  struct Indices {
    Indices()
        : count(0),
          ibo(InvalidBufferHandle()),
          mat(nullptr),
          index_type(0),
          indexBufferMem(InvalidDeviceMemoryHandle()) {}
    int count;
    BufferHandle ibo;
    Material *mat;
    uint32_t index_type;
    DeviceMemoryHandle indexBufferMem;
  };

  MeshImpl *impl_;
  std::vector<Indices> indices_;
  uint32_t primitive_;
  size_t vertex_size_;
  size_t num_vertices_;
  Attribute format_[kMaxAttributes];
  mathfu::vec3 min_position_;
  mathfu::vec3 max_position_;

  // The default bone positions, in object space, inverted. Length NumBones().
  // Used when skinning.
  //
  // The vertex transform is,
  //
  //    mvp * bone_transforms_[i] * default_bone_transform_inverses_[i]
  //
  // where bone_transforms_[i] is the placement of bone i relative to
  // the root of the object. So, when the bone is in its default position,
  //    bone_transforms_[i] * default_bone_transform_inverses_[i] = Identity
  // which makes sense.
  //
  // How to think of this: default_bone_transform_inverses_[i] maps the
  // vertex from object space into bone space. That is, it gives the
  // coordinates of the vertex relative to bone i. Then bone_transforms_[i]
  // maps the bone back into object space, in its animated location.
  //
  // Note that vector<AffineTransform> is not possible on Visual Studio 2010
  // because it doesn't support vectors of aligned types, so we use a raw
  // pointer instead.
  mathfu::AffineTransform *default_bone_transform_inverses_;
  std::vector<uint8_t> bone_parents_;
  std::vector<std::string> bone_names_;
  std::vector<uint8_t> shader_bone_indices_;

  // Function to create material.
  MaterialCreateFn material_create_fn_;
};

/// @}
}  // namespace fplbase

#endif  // FPL_MESH_H
