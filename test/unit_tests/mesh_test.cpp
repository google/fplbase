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

#include "fplbase/mesh.h"
#include "gtest/gtest.h"

namespace fplbase {
namespace {

const Attribute kP[] = {kPosition3f, kEND};
const Attribute kPN[] = {kPosition3f, kNormal3f, kEND};
const Attribute kPT[] = {kPosition3f, kTangent4f, kEND};
const Attribute kPUv[] = {kPosition3f, kTexCoord2f, kEND};
const Attribute kPC[] = {kPosition3f, kColor4ub, kEND};
const Attribute kPIW[] = {kPosition3f, kBoneIndices4ub, kBoneWeights4ub, kEND};
const Attribute kPUvC[] = {kPosition3f, kTexCoord2f, kColor4ub, kEND};
const Attribute kPNTIW[] = {kPosition3f,     kNormal3f,       kTangent4f,
                            kBoneIndices4ub, kBoneWeights4ub, kEND};

}  // namespace

class MeshTests : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(MeshTests, IsValidFormat) {
  EXPECT_TRUE(Mesh::IsValidFormat(kP));
  EXPECT_TRUE(Mesh::IsValidFormat(kPN));
  EXPECT_TRUE(Mesh::IsValidFormat(kPT));
  EXPECT_TRUE(Mesh::IsValidFormat(kPUv));
  EXPECT_TRUE(Mesh::IsValidFormat(kPC));
  EXPECT_TRUE(Mesh::IsValidFormat(kPIW));
  EXPECT_TRUE(Mesh::IsValidFormat(kPNTIW));

  const Attribute kNoPosition[] = {kNormal3f, kEND};
  EXPECT_FALSE(Mesh::IsValidFormat(kNoPosition));

  const Attribute kBadPositions[] = {kPosition3f, kPosition2f, kEND};
  EXPECT_FALSE(Mesh::IsValidFormat(kBadPositions));

  const Attribute kBadUvs[] = {kTexCoord2f, kTexCoord2us, kEND};
  EXPECT_FALSE(Mesh::IsValidFormat(kBadUvs));

  // Simulate uninitialized memory by filling it with 0xff, which isn't kEND.
  Attribute unterminated[100];
  memset(unterminated, 0xff, sizeof unterminated);
  unterminated[0] = kPosition3f;
  EXPECT_FALSE(Mesh::IsValidFormat(unterminated));
}

// Check vertex size calculations.
TEST_F(MeshTests, VertexSize) {
  // kP = 3 floats = 12 bytes
  EXPECT_EQ(Mesh::VertexSize(kP), 12U);

  // kPN = 3 + 3 floats = 24 bytes
  EXPECT_EQ(Mesh::VertexSize(kPN), 24U);

  // kPT = 3 + 4 floats = 24 bytes
  EXPECT_EQ(Mesh::VertexSize(kPT), 28U);

  // kPUv = 3 + 2 floats = 20 bytes
  EXPECT_EQ(Mesh::VertexSize(kPUv), 20U);

  // kPC = 3 floats + 4 bytes = 16 bytes
  EXPECT_EQ(Mesh::VertexSize(kPC), 16U);

  // kPIW = 3 floats + (4 + 4) bytes = 20 bytes
  EXPECT_EQ(Mesh::VertexSize(kPIW), 20U);

  // kPUvC = (3 + 2) floats + 4 bytes = 24 bytes
  EXPECT_EQ(Mesh::VertexSize(kPUvC), 24U);

  // KPNTIW = (3 + 3 + 4) floats + (4 + 4) bytes = 48 bytes
  EXPECT_EQ(Mesh::VertexSize(kPNTIW), 48U);
}

TEST_F(MeshTests, AttributeOffset) {
  EXPECT_EQ(Mesh::AttributeOffset(kPUvC, kPosition3f), 0U);
  EXPECT_EQ(Mesh::AttributeOffset(kPUvC, kTexCoord2f), 12U);
  EXPECT_EQ(Mesh::AttributeOffset(kPUvC, kColor4ub), 20U);

  EXPECT_EQ(Mesh::AttributeOffset(kPNTIW, kPosition3f), 0U);
  EXPECT_EQ(Mesh::AttributeOffset(kPNTIW, kNormal3f), 12U);
  EXPECT_EQ(Mesh::AttributeOffset(kPNTIW, kTangent4f), 24U);
  EXPECT_EQ(Mesh::AttributeOffset(kPNTIW, kBoneIndices4ub), 40U);
  EXPECT_EQ(Mesh::AttributeOffset(kPNTIW, kBoneWeights4ub), 44U);
}

}  // namespace fplbase

extern "C" int FPL_main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
