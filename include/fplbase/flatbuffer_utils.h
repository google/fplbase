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

#include "fplbase/config.h"  // Must come first.

#include "common_generated.h"
#include "mathfu/constants.h"
#include "mathfu/glsl_mappings.h"

/// @file fplbase/flatbuffer_utils.h
/// @brief Contains helper functions for loading the structs in common.fbs from
///        Flatbuffer files.

/// @brief Namespace for FPLBase library.
namespace fplbase {

/// @addtogroup fplbase_flatbuffer_utils
/// @{

// If defined, treat Flatbuffer types as byte-for-byte equivalents of mathfu
// types. This speeds up load operations significantly.
// Flatbuffers stores data in little endian, so if the platform is also
// little endian, we're byte-for-byte equivalent.
#if FLATBUFFERS_LITTLEENDIAN
#define FPLBASE_FLATBUFFER_AND_MATHFU_PACKED_TYPES_EQUIVALENT 1
#endif

/// @brief Converts a Vec2.
///
/// @param v The Flatbuffer Vec2 to convert.
/// @return Returns a converted mathfu vec2.
inline mathfu::vec2 LoadVec2(const Vec2* v) {
#if FPLBASE_FLATBUFFER_AND_MATHFU_PACKED_TYPES_EQUIVALENT
  return mathfu::vec2::FromType(*v);
#else
  // Eschew the constructor that loads contiguous floats because we need to
  // swap endian for each float. Very few systems are big-endian, so this
  // code path is more for completeness than practicality. If a big-endian
  // system does become popular, we should implement endian-swap on load
  // in a mathfu vec2 function.
  return mathfu::vec2(v->x(), v->y());
#endif
}

/// @brief Converts a Vec3.
///
/// @param v The Flatbuffer Vec3 to convert.
/// @return Returns a converted mathfu vec3.
inline mathfu::vec3 LoadVec3(const Vec3* v) {
#if FPLBASE_FLATBUFFER_AND_MATHFU_PACKED_TYPES_EQUIVALENT
  // Even if mathfu::vec3 is padded to be 16-bytes instead of 12-bytes,
  // FromType() only reads 12-bytes (because we pun to VectorPacked<>).
  // We do not read off the end of `v`.
  return mathfu::vec3::FromType(*v);
#else
  return mathfu::vec3(v->x(), v->y(), v->z());
#endif
}

/// @brief Converts a Vec4.
///
/// @param v The Flatbuffer Vec4 to convert.
/// @return Returns a converted mathfu vec4.
inline mathfu::vec4 LoadVec4(const Vec4* v) {
#if FPLBASE_FLATBUFFER_AND_MATHFU_PACKED_TYPES_EQUIVALENT
  return mathfu::vec4::FromType(*v);
#else
  return mathfu::vec4(v->x(), v->y(), v->z(), v->w());
#endif
}

/// @brief Converts a Vec2i.
///
/// @param v The Flatbuffer Vec2i to convert.
/// @return Returns a converted mathfu vec2i.
inline mathfu::vec2i LoadVec2i(const Vec2i* v) {
#if FPLBASE_FLATBUFFER_AND_MATHFU_PACKED_TYPES_EQUIVALENT
  return mathfu::vec2i::FromType(*v);
#else
  return mathfu::vec2i(v->x(), v->y());
#endif
}

/// @brief Converts a Vec3i.
///
/// @param v The Flatbuffer Vec3i to convert.
/// @return Returns a converted mathfu vec3i.
inline mathfu::vec3i LoadVec3i(const Vec3i* v) {
#if FPLBASE_FLATBUFFER_AND_MATHFU_PACKED_TYPES_EQUIVALENT
  return mathfu::vec3i::FromType(*v);
#else
  return mathfu::vec3i(v->x(), v->y(), v->z());
#endif
}

/// @brief Converts a Vec4i.
///
/// @param v The Flatbuffer Vec4i to convert.
/// @return Returns a converted mathfu vec4i.
inline mathfu::vec4i LoadVec4i(const Vec4i* v) {
#if FPLBASE_FLATBUFFER_AND_MATHFU_PACKED_TYPES_EQUIVALENT
  return mathfu::vec4i::FromType(*v);
#else
  return mathfu::vec4i(v->x(), v->y(), v->z(), v->w());
#endif
}

/// @brief Converts a Axis to the corresponding vec3.
///
/// @param axis The Flatbuffer Axis to convert.
/// @return Returns the corresponding unit length mathfu vec3.
inline mathfu::vec3 LoadAxis(Axis axis) {
  return axis == Axis_X ? mathfu::kAxisX3f : axis == Axis_Y ? mathfu::kAxisY3f
                                                            : mathfu::kAxisZ3f;
}

/// @brief Converts a ColorRGBA to a vec4.
///
/// @param c The Flatbuffer ColorRGBA to convert.
/// @return Returns a converted mathfu vec4.
inline mathfu::vec4 LoadColorRGBA(const ColorRGBA* c) {
#if FPLBASE_FLATBUFFER_AND_MATHFU_PACKED_TYPES_EQUIVALENT
  return mathfu::vec4::FromType(*c);
#else
  return mathfu::vec4(c->r(), c->g(), c->b(), c->a());
#endif
}

/// @brief Converts a vec4 to a ColorRGBA.
///
/// @param v The mathfu vec4 to convert.
/// @return Returns a converted ColorRGBA.
inline ColorRGBA Vec4ToColorRGBA(const mathfu::vec4& v) {
#if FPLBASE_FLATBUFFER_AND_MATHFU_PACKED_TYPES_EQUIVALENT
  return mathfu::vec4::ToType<ColorRGBA>(v);
#else
  return ColorRGBA(v.x(), v.y(), v.z(), v.w());
#endif
}

/// @brief Converts a Mat3x4 to a mat4.
///
/// Affine transform can be serialized as a 3x4 matrix (three rows of four
/// elements). The fourth row is (0,0,0,1). We load the rows into the columns
/// of a 4x4 matrix, transpose it, and return it as a `mathfu::AffineTransform`.
///
/// @param m The Flatbuffer Mat3x4 to convert.
/// @return Returns a converted mathfu mat4.
inline mathfu::AffineTransform LoadAffine(const Mat3x4* m) {
#if FPLBASE_FLATBUFFER_AND_MATHFU_PACKED_TYPES_EQUIVALENT
  return mathfu::AffineTransform::FromType(*m);
#else
  const mathfu::vec4 c0 = LoadVec4(&m->c0());
  const mathfu::vec4 c1 = LoadVec4(&m->c1());
  const mathfu::vec4 c2 = LoadVec4(&m->c2());
  return mathfu::mat4::ToAffineTransform(
      mathfu::mat4(c0, c1, c2, mathfu::kAxisW4f).Transpose());
#endif
}

/// @}
}  // namespace fplbase

#endif  // FPLBASE_FLATBUFFER_UTILS_H
