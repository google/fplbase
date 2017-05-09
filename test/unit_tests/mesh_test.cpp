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

class MeshTests : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}
};

// Check vertex size calculations.
TEST_F(MeshTests, VertexSize) {
  const Attribute kP[] = {kPosition3f, kEND};
  EXPECT_EQ(Mesh::VertexSize(kP), 12U);

  const Attribute kPN[] = {kPosition3f, kNormal3f, kEND};
  EXPECT_EQ(Mesh::VertexSize(kPN), 24U);

  const Attribute kPT[] = {kPosition3f, kTangent4f, kEND};
  EXPECT_EQ(Mesh::VertexSize(kPT), 28U);

  const Attribute kPUv[] = {kPosition3f, kTexCoord2f, kEND};
  EXPECT_EQ(Mesh::VertexSize(kPUv), 20U);

  const Attribute kPC[] = {kPosition3f, kColor4ub, kEND};
  EXPECT_EQ(Mesh::VertexSize(kPC), 16U);

  const Attribute kPIW[] = {kPosition3f, kBoneIndices4ub, kBoneWeights4ub,
                            kEND};
  EXPECT_EQ(Mesh::VertexSize(kPIW), 20U);

  const Attribute kPUvC[] = {kPosition3f, kTexCoord2f, kColor4ub, kEND};
  EXPECT_EQ(Mesh::VertexSize(kPUvC), 24U);
  EXPECT_EQ(Mesh::VertexSize(kPUvC, kPosition3f), 0U);
  EXPECT_EQ(Mesh::VertexSize(kPUvC, kTexCoord2f), 12U);
  EXPECT_EQ(Mesh::VertexSize(kPUvC, kColor4ub), 20U);

  const Attribute kPNTIW[] = {kPosition3f,     kNormal3f,       kTangent4f,
                              kBoneIndices4ub, kBoneWeights4ub, kEND};
  EXPECT_EQ(Mesh::VertexSize(kPNTIW), 48U);
  EXPECT_EQ(Mesh::VertexSize(kPNTIW, kPosition3f), 0U);
  EXPECT_EQ(Mesh::VertexSize(kPNTIW, kNormal3f), 12U);
  EXPECT_EQ(Mesh::VertexSize(kPNTIW, kTangent4f), 24U);
  EXPECT_EQ(Mesh::VertexSize(kPNTIW, kBoneIndices4ub), 40U);
  EXPECT_EQ(Mesh::VertexSize(kPNTIW, kBoneWeights4ub), 44U);
}

}  // namespace fplbase

extern "C" int FPL_main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
