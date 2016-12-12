// Copyright 2016 Google Inc. All rights reserved.
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

#include "common_generated.h"
#include "fplbase/flatbuffer_utils.h"
#include "fplbase/preprocessor.h"
#include "gtest/gtest.h"
#include "mathfu/glsl_mappings.h"

class UtilsTests : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}
};

// Check conversion from FlatBuffers's Vec2 to mathfu's vec2.
TEST_F(UtilsTests, LoadVec2) {
  const fplbase::Vec2 flat(1.0f, 2.0f);
  const mathfu::vec2 v = fplbase::LoadVec2(&flat);
  EXPECT_EQ(flat.x(), v.x);
  EXPECT_EQ(flat.y(), v.y);
}

// Check conversion from FlatBuffers's Vec3 to mathfu's vec3.
TEST_F(UtilsTests, LoadVec3) {
  const fplbase::Vec3 flat(1.0f, 2.0f, 3.0f);
  const mathfu::vec3 v = fplbase::LoadVec3(&flat);
  EXPECT_EQ(flat.x(), v.x);
  EXPECT_EQ(flat.y(), v.y);
  EXPECT_EQ(flat.z(), v.z);
}

// Check conversion from FlatBuffers's Vec4 to mathfu's vec4.
TEST_F(UtilsTests, LoadVec4) {
  const fplbase::Vec4 flat(1.0f, 2.0f, 3.0f, 4.0f);
  const mathfu::vec4 v = fplbase::LoadVec4(&flat);
  EXPECT_EQ(flat.x(), v.x);
  EXPECT_EQ(flat.y(), v.y);
  EXPECT_EQ(flat.z(), v.z);
  EXPECT_EQ(flat.w(), v.w);
}

// Check conversion from FlatBuffers's Vec2i to mathfu's vec2i.
TEST_F(UtilsTests, LoadVec2i) {
  const fplbase::Vec2i flat(1, 2);
  const mathfu::vec2i v = fplbase::LoadVec2i(&flat);
  EXPECT_EQ(flat.x(), v.x);
  EXPECT_EQ(flat.y(), v.y);
}

// Check conversion from FlatBuffers's Vec3i to mathfu's vec3i.
TEST_F(UtilsTests, LoadVec3i) {
  const fplbase::Vec3i flat(1, 2, 3);
  const mathfu::vec3i v = fplbase::LoadVec3i(&flat);
  EXPECT_EQ(flat.x(), v.x);
  EXPECT_EQ(flat.y(), v.y);
  EXPECT_EQ(flat.z(), v.z);
}

// Check conversion from FlatBuffers's Vec4i to mathfu's vec4i.
TEST_F(UtilsTests, LoadVec4i) {
  const fplbase::Vec4i flat(1, 2, 3, 4);
  const mathfu::vec4i v = fplbase::LoadVec4i(&flat);
  EXPECT_EQ(flat.x(), v.x);
  EXPECT_EQ(flat.y(), v.y);
  EXPECT_EQ(flat.z(), v.z);
  EXPECT_EQ(flat.w(), v.w);
}

// Check conversion from FlatBuffers's ColorRGBA to mathfu's vec4.
TEST_F(UtilsTests, LoadColorRGBA) {
  const fplbase::ColorRGBA flat(0.1f, 0.2f, 0.3f, 0.4f);
  const mathfu::vec4 v = fplbase::LoadColorRGBA(&flat);
  EXPECT_EQ(flat.r(), v.x);
  EXPECT_EQ(flat.g(), v.y);
  EXPECT_EQ(flat.b(), v.z);
  EXPECT_EQ(flat.a(), v.w);
}

// Check conversion from mathfu's vec4 to FlatBuffers's ColorRGBA.
TEST_F(UtilsTests, Vec4ToColorRGBA) {
  const mathfu::vec4 v(0.1f, 0.2f, 0.3f, 0.4f);
  const fplbase::ColorRGBA flat = fplbase::Vec4ToColorRGBA(v);
  EXPECT_EQ(flat.r(), v.x);
  EXPECT_EQ(flat.g(), v.y);
  EXPECT_EQ(flat.b(), v.z);
  EXPECT_EQ(flat.a(), v.w);
}

// Check conversion from FlatBuffers's Mat3x4 to mathfu's AffineTransform.
TEST_F(UtilsTests, LoadAffine) {
  const fplbase::Mat3x4 flat(fplbase::Vec4(1.0f, 2.0f, 3.0f, 4.0f),
                             fplbase::Vec4(5.0f, 6.0f, 7.0f, 8.0f),
                             fplbase::Vec4(9.0f, 10.0f, 11.0f, 12.0f));
  const mathfu::AffineTransform m = fplbase::LoadAffine(&flat);
  EXPECT_EQ(flat.c0().x(), m.GetColumn(0).x);
  EXPECT_EQ(flat.c0().y(), m.GetColumn(0).y);
  EXPECT_EQ(flat.c0().z(), m.GetColumn(0).z);
  EXPECT_EQ(flat.c0().w(), m.GetColumn(0).w);
  EXPECT_EQ(flat.c1().x(), m.GetColumn(1).x);
  EXPECT_EQ(flat.c1().y(), m.GetColumn(1).y);
  EXPECT_EQ(flat.c1().z(), m.GetColumn(1).z);
  EXPECT_EQ(flat.c1().w(), m.GetColumn(1).w);
  EXPECT_EQ(flat.c2().x(), m.GetColumn(2).x);
  EXPECT_EQ(flat.c2().y(), m.GetColumn(2).y);
  EXPECT_EQ(flat.c2().z(), m.GetColumn(2).z);
  EXPECT_EQ(flat.c2().w(), m.GetColumn(2).w);
}

TEST_F(UtilsTests, LoadAxis) {
  EXPECT_TRUE(fplbase::LoadAxis(fplbase::Axis_X) == mathfu::kAxisX3f);
  EXPECT_TRUE(fplbase::LoadAxis(fplbase::Axis_Y) == mathfu::kAxisY3f);
  EXPECT_TRUE(fplbase::LoadAxis(fplbase::Axis_Z) == mathfu::kAxisZ3f);
}

extern "C" int FPL_main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
