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

#ifndef FPLBASE_FLATBUFFER_UTILS_H
#define FPLBASE_FLATBUFFER_UTILS_H

#include "fplbase/config.h" // Must come first.

#include "common_generated.h"
#include "mathfu/constants.h"
#include "mathfu/glsl_mappings.h"

namespace fpl {

inline const mathfu::vec2 LoadVec2(const Vec2* v) {
  // Note: eschew the constructor that loads contiguous floats. It's faster
  // than the x, y constructor we use here, but doesn't account for the
  // endian swap that might occur in Vec3::x().
  return mathfu::vec2(v->x(), v->y());
}

inline const mathfu::vec3 LoadVec3(const Vec3* v) {
  return mathfu::vec3(v->x(), v->y(), v->z());
}

inline const mathfu::vec4 LoadVec4(const Vec4* v) {
  return mathfu::vec4(v->x(), v->y(), v->z(), v->w());
}

inline const mathfu::vec2i LoadVec2i(const Vec2i* v) {
  return mathfu::vec2i(v->x(), v->y());
}

inline const mathfu::vec3i LoadVec3i(const Vec3i* v) {
  return mathfu::vec3i(v->x(), v->y(), v->z());
}

inline const mathfu::vec4i LoadVec4i(const Vec4i* v) {
  return mathfu::vec4i(v->x(), v->y(), v->z(), v->w());
}

inline const mathfu::vec3 LoadAxis(Axis axis) {
  return axis == Axis_X ? mathfu::kAxisX3f : axis == Axis_Y ? mathfu::kAxisY3f
                                                            : mathfu::kAxisZ3f;
}

inline const mathfu::vec4 LoadColorRGBA(const ColorRGBA* c) {
  return mathfu::vec4(c->r(), c->g(), c->b(), c->a());
}

// Affine transform can be serialized as a 3x4 matrix (three rows of four
// elements). The fourth row is (0,0,0,1). We load the rows into the columns,
// and then transpose it to get a standard 4x4 matrix.
inline mathfu::mat4 LoadAffineMat4(const Mat3x4* m) {
  const mathfu::vec4 c0 = LoadVec4(&m->c0());
  const mathfu::vec4 c1 = LoadVec4(&m->c1());
  const mathfu::vec4 c2 = LoadVec4(&m->c2());
  return mathfu::mat4(c0, c1, c2, mathfu::kAxisW4f).Transpose();
}

}  // namespace fpl

#endif  // FPLBASE_FLATBUFFER_UTILS_H
