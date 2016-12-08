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
#include "fplbase/material.h"
#include "fplbase/render_state.h"
#include "fplbase/shader.h"
#include "mathfu/constants.h"

namespace fplbase {

/// @file
/// @addtogroup fplbase_mesh
/// @{

class Renderer;

/// @brief An array of these enums defines the format of vertex data.
enum Attribute {
  kEND = 0,  ///< @brief The array must always be terminated by one of these.
  kPosition3f,
  kNormal3f,
  kTangent4f,
  kTexCoord2f,
  kTexCoordAlt2f,  ///< @brief Second set of UVs for use with e.g. lightmaps.
  kColor4ub,
  kBoneIndices4ub,
  kBoneWeights4ub,
};

/// @class Mesh
/// @brief Abstraction for a set of indices, used for rendering.
///
/// A mesh instance contains a VBO and one or more IBO's.
class Mesh : public AsyncAsset {
 public:
  enum Primitive {
    kTriangles,
    kTriangleStrip,
    kLines,
    kPoints,
  };

  typedef std::function<Material *(const char *filename)> MaterialLoaderFn;

  /// @brief Initialize a Mesh from a file asynchronously.
  ///
  /// Asynchronously create mesh from a file if filename is valid.
  /// Otherwise, if filename is null, need to call LoadFromMemory to init
  /// manually.
  Mesh(const char *filename = nullptr,
       MaterialLoaderFn material_loader_fn = nullptr);

  /// @brief Initialize a Mesh by creating one VBO, and no IBO's.
  Mesh(const void *vertex_data, size_t count, size_t vertex_size,
       const Attribute *format, mathfu::vec3 *max_position = nullptr,
       mathfu::vec3 *min_position = nullptr);

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
  bool IsValid() { return vbo_ != 0; }

  /// @brief Add an index buffer object to be part of this mesh
  ///
  /// Create one IBO to be part of this mesh. May be called more than once.
  ///
  /// @param indices The indices to be included in the IBO.
  /// @param count The number of indices.
  /// @param mat The material associated with the IBO.
  /// @param is_32_bit Specifies that the indices are 32bit. Default 16bit.
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

  /// @brief Render the mesh.
  ///
  /// Call to have the mesh render itself. Uniforms must have been set before
  /// calling this. For instanced rendering, pass in a value >1 (needs OpenGL
  /// ES 3.0 to work).
  ///
  /// @param renderer The renderer object to be used.
  /// @param ignore_material Whether to ignore the meshes defined material.
  /// @param instances The number of instances to be rendered.
  void Render(Renderer &renderer, bool ignore_material = false,
              size_t instances = 1);

  /// @brief Render the mesh, itself, into stereoscopic viewports.
  /// @param renderer The renderer object to be used.
  /// @param shader The shader object to be used.
  /// @param viewport An array with two elements (left and right parameters) for
  /// the viewport.
  /// @param mvp An array with two elements (left and right parameters) for the
  /// Model View Projection (MVP) matrix.
  /// @param camera_position An array with two elements (left and right
  /// parameters) for camera position.
  /// @param ignore_material Whether to ignore the meshes defined material.
  /// @param instances The number of instances to be rendered.
  void RenderStereo(Renderer &renderer, const Shader *shader,
                    const Viewport *viewport, const mathfu::mat4 *mvp,
                    const mathfu::vec3 *camera_position,
                    bool ignore_material = false, size_t instances = 1);

  /// @brief Get the material associated with the IBO at the given index.
  ///
  /// @param i The index of the IBO.
  /// @return Returns the material of the corresponding IBO.
  Material *GetMaterial(int i) { return indices_[i].mat; }

  /// @brief Define the vertex buffer format.
  ///
  /// `format` must have length <= kMaxAttributes, including `kEND`.
  ///
  /// @param format Array of attributes to set the format to, delimitted with
  ///        `kEND`.
  void set_format(const Attribute *format);

  /// @brief Renders the given vertex and index data directly.
  ///
  /// Renders primitives using vertex and index data directly in local memory.
  /// This is a convenient alternative to creating a Mesh instance for small
  /// amounts of data, or dynamic data.
  ///
  /// @param primitive The type of primitive to render the data as.
  /// @param index_count The total number of indices.
  /// @param format The vertex buffer format, following the same rules as
  ///        described in set_format().
  /// @param vertex_size The size of an individual vertex.
  /// @param vertices The array of vertices.
  /// @param indices The array of indices into the vertex array.
  static void RenderArray(Primitive primitive, int index_count,
                          const Attribute *format, int vertex_size,
                          const void *vertices, const unsigned short *indices);

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
  static void RenderArray(Primitive primitive, int vertex_count,
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
  static void RenderAAQuadAlongX(const mathfu::vec3 &bottom_left,
                                 const mathfu::vec3 &top_right,
                                 const mathfu::vec2 &tex_bottom_left =
                                     mathfu::vec2(0, 0),
                                 const mathfu::vec2 &tex_top_right =
                                     mathfu::vec2(1, 1));

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
  static void RenderAAQuadAlongXNinePatch(const mathfu::vec3 &bottom_left,
                                          const mathfu::vec3 &top_right,
                                          const mathfu::vec2i &texture_size,
                                          const mathfu::vec4 &patch_info);

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
    kAttributeTexCoord,
    kAttributeTexCoordAlt,
    kAttributeColor,
    kAttributeBoneIndices,
    kAttributeBoneWeights,
  };

  /// @brief Compute the byte size for a vertex from given attributes.
  ///
  /// @param attributes The array of attributes describing the vertex.
  /// @param end The attribute to treat as the end of the array.
  /// @return Returns the byte size based on the given attributes.
  static size_t VertexSize(const Attribute *attributes, Attribute end = kEND);

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
  void ParseInterleavedVertexData(const void *meshdef_buffer,
                                  InterleavedVertexData *ivd);

  MATHFU_DEFINE_CLASS_SIMD_AWARE_NEW_DELETE

 private:
  // Disallow copies because of pointers bone_transforms_ and
  // bone_global_transforms_. Feel free to implement copy or move operators
  // if required.
  Mesh(const Mesh &);
  Mesh &operator=(const Mesh &);

  void Clear();

  // Init mesh from MeshDef FlatBuffer.
  bool InitFromMeshDef(const void *meshdef_buffer);

  // This typedef is compatible with its OpenGL equivalent, but doesn't require
  // this header to depend on OpenGL.
  typedef unsigned int BufferHandle;

  static const int kMaxAttributes = 9;

  static void SetAttributes(BufferHandle vbo, const Attribute *attributes,
                            int vertex_size, const char *buffer);
  static void UnSetAttributes(const Attribute *attributes);

  void BindAttributes();
  void UnbindAttributes();

  void DrawElement(Renderer &renderer, int32_t count, int32_t instances,
                   uint32_t index_type);

  struct Indices {
    int count;
    BufferHandle ibo;
    Material *mat;
    uint32_t index_type;
  };
  std::vector<Indices> indices_;
  size_t vertex_size_;
  size_t num_vertices_;
  Attribute format_[kMaxAttributes];
  BufferHandle vbo_;
  BufferHandle vao_;
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

  // Function to load material by filename.
  MaterialLoaderFn material_loader_fn_;
};

/// @}
}  // namespace fplbase

#endif  // FPL_MESH_H
