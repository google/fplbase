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

#include "fplbase/material.h"

namespace fpl {

class Renderer;

// An array of these enums defines the format of vertex data.
enum Attribute {
  kEND = 0,  // The array must always be terminated by one of these.
  kPosition3f,
  kNormal3f,
  kTangent4f,
  kTexCoord2f,
  kColor4ub,
  kBoneIndices4ub,
  kBoneWeights4ub
};

// A mesh instance contains a VBO and one or more IBO's.
class Mesh {
 public:
  enum Primitive { kTriangles, kLines };

  // Initialize a Mesh by creating one VBO, and no IBO's.
  Mesh(const void *vertex_data, int count, int vertex_size,
       const Attribute *format, vec3 *max_position = nullptr,
       vec3 *min_position = nullptr);
  ~Mesh();

  // Create one IBO to be part of this mesh. May be called more than once.
  void AddIndices(const unsigned short *indices, int count, Material *mat);

  // If mesh is animated set the transform from a bone's parent space into
  // the bone's local space. Optionally record the bone names, too, for
  // debugging.
  // The shader only accesses a bone if at least one vertex is weighted to it.
  // So, we don't have to pass every bone transform up to the shader. Instead,
  // we compact the bone transforms by passing only those in
  // shader_bone_indices.
  void SetBones(const mathfu::mat4 *bone_transforms,
                const uint8_t *bone_parents, const char **bone_names,
                size_t num_bones, const uint8_t *shader_bone_indices,
                size_t num_shader_bones);

  // Compact bone transforms to eliminate the bones that have no verts
  // weighted to them.
  // `bone_transforms` is an input array of length num_bones().
  // `shader_transforms` is an output array of length num_shder_bones().
  void GatherShaderTransforms(const mat4 *bone_transforms,
                              mat4 *shader_transforms) const;

  // Render itself. Uniforms must have been set before calling this.
  // Use a value >1 for instances to get instanced rendering (this needs
  // OpenGL ES 3.0 to work).
  void Render(Renderer &renderer, bool ignore_material = false,
              size_t instances = 1);

  // Get the material associated with the Nth IBO.
  Material *GetMaterial(int i) { return indices_[i].mat; }

  // Define the vertex buffer format.
  // `format` is an array delimitted with `kEND`.
  // `format` must have length <= kMaxAttributes, including `kEND`.
  void set_format(const Attribute *format);

  // Renders primatives using vertex and index data directly in local memory.
  // This is a convenient alternative to creating a Mesh instance for small
  // amounts of data, or dynamic data.
  static void RenderArray(Primitive primitive, int index_count,
                          const Attribute *format, int vertex_size,
                          const char *vertices, const unsigned short *indices);

  // Convenience method for rendering a Quad. bottom_left and top_right must
  // have their X coordinate be different, but either Y or Z can be the same.
  static void RenderAAQuadAlongX(const vec3 &bottom_left, const vec3 &top_right,
                                 const vec2 &tex_bottom_left = vec2(0, 0),
                                 const vec2 &tex_top_right = vec2(1, 1));

  // Convenience method for rendering a Quad with nine patch settings.
  // In the patch_info, the user can define nine patch settings
  // as vec4(x0, y0, x1, y1) where
  // (x0,y0): top-left corner of stretchable area in UV coordinate.
  // (x1,y1): bottom-right corner of stretchable area in UV coordinate.
  static void RenderAAQuadAlongXNinePatch(const vec3 &bottom_left,
                                          const vec3 &top_right,
                                          const vec2i &texture_size,
                                          const vec4 &patch_info);

  // Compute normals and tangents given position and texcoords.
  // The template type should be a struct with at least the following fields:
  // mathfu::vec3_packed pos;
  // mathfu::vec2_packed tc;
  // mathfu::vec3_packed norm;
  // mathfu::vec4_packed tangent;
  template <typename T>
  static void ComputeNormalsTangents(T *vertices, const unsigned short *indices,
                                     int numverts, int numindices) {
    std::unique_ptr<vec3[]> binormals(new vec3[numverts]);

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
      auto q1 = vec3(v1.pos) - vec3(v0.pos);
      auto q2 = vec3(v2.pos) - vec3(v0.pos);
      auto norm = normalize(cross(q1, q2));
      // Contribute the triangle normal into all 3 verts:
      v0.norm = vec3(v0.norm) + norm;
      v1.norm = vec3(v1.norm) + norm;
      v2.norm = vec3(v2.norm) + norm;
      // Similarly create uv space vectors:
      auto uv1 = vec2(v1.tc) - vec2(v0.tc);
      auto uv2 = vec2(v2.tc) - vec2(v0.tc);
      float m = 1 / (uv1.x() * uv2.y() - uv2.x() * uv1.y());
      auto tangent = vec4((uv2.y() * q1 - uv1.y() * q2) * m, 0);
      auto binorm = (uv1.x() * q2 - uv2.x() * q1) * m;
      v0.tangent = vec4(v0.tangent) + tangent;
      v1.tangent = vec4(v1.tangent) + tangent;
      v2.tangent = vec4(v2.tangent) + tangent;
      binormals[indices[i + 0]] = binorm;
      binormals[indices[i + 1]] = binorm;
      binormals[indices[i + 2]] = binorm;
    }
    // Normalize per vertex tangent space constributions, and pack tangent /
    // binormal into a 4 component tangent.
    for (int i = 0; i < numverts; i++) {
      auto norm = vec3(vertices[i].norm);
      auto tangent = vec4(vertices[i].tangent);
      // Renormalize all 3 axes:
      norm = normalize(norm);
      tangent = vec4(normalize(tangent.xyz()), 0);
      binormals[i] = normalize(binormals[i]);
      tangent = vec4(
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
    kAttributeColor,
    kAttributeBoneIndices,
    kAttributeBoneWeights,
  };

  // Compute the byte size for a vertex from given attributes.
  static size_t VertexSize(const Attribute *attributes, Attribute end = kEND);

  const mathfu::vec3 &min_position() const { return min_position_; }
  const mathfu::vec3 &max_position() const { return max_position_; }
  const mathfu::mat4 *bone_transforms() const { return bone_transforms_; }
  const mathfu::mat4 *bone_global_transforms() const {
    return bone_global_transforms_;
  }
  const uint8_t *bone_parents() const { return &bone_parents_[0]; }
  size_t num_bones() const { return bone_parents_.size(); }
  const uint8_t *shader_bone_indices() const {
    return &shader_bone_indices_[0];
  }
  size_t num_shader_bones() const { return shader_bone_indices_.size(); }

 private:
  // Disallow copies because of pointers bone_transforms_ and
  // bone_global_transforms_. Feel free to implement copy or move operators
  // if required.
  Mesh(const Mesh&);
  Mesh& operator=(const Mesh&);

  // This typedef is compatible with its OpenGL equivalent, but doesn't require
  // this header to depend on OpenGL.
  typedef unsigned int BufferHandle;

  static const int kMaxAttributes = 8;

  static void SetAttributes(BufferHandle vbo, const Attribute *attributes,
                            int vertex_size, const char *buffer);
  static void UnSetAttributes(const Attribute *attributes);
  struct Indices {
    int count;
    BufferHandle ibo;
    Material *mat;
  };
  std::vector<Indices> indices_;
  size_t vertex_size_;
  Attribute format_[kMaxAttributes];
  BufferHandle vbo_;
  mathfu::vec3 min_position_;
  mathfu::vec3 max_position_;

  // Bone arrays are of length NumBones().
  // Note that vector<mat4> is not possible on Visual Studio 2010 because
  // it doesn't support vectors of aligned types.
  mathfu::mat4* bone_transforms_;
  mathfu::mat4* bone_global_transforms_;
  std::vector<uint8_t> bone_parents_;
  std::vector<std::string> bone_names_;
  std::vector<uint8_t> shader_bone_indices_;
};

}  // namespace fpl

#endif  // FPL_MESH_H
