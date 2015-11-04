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
//

#include "fplbase/renderer.h"
#include "fplbase/input.h"
#include "fplbase/asset_manager.h"
#include "fplbase/utilities.h"
#include "mathfu/matrix.h"
#include "mathfu/matrix_4x4.h"
#include <cassert>

// Mesh is a sample that draws from mesh of a red hot shrimp on a white box.
//
// It demonstrates the usage of:
// - fpl::asset_manager to load mesh as asset.
// - fpl::Renderer to setup rendering and transform models.
// - fpl::InputSystem to query for exit events and elapsed time.

extern "C" int FPL_main(int /*argc*/, char **argv) {
  fpl::Renderer renderer;
  renderer.Initialize(mathfu::vec2i(800, 600), "Simple mesh test");

  fpl::InputSystem input;
  input.Initialize();

  bool result = fpl::ChangeToUpstreamDir(argv[0], "assets");
  assert(result);

  fpl::AssetManager assetMgr(renderer);
  fpl::Shader *shader = assetMgr.LoadShader("mesh");
  assert(shader);

  fpl::Mesh *mesh = assetMgr.LoadMesh("meshes/sushi_shrimp.fplmesh");
  assert(mesh);

  assetMgr.StartLoadingTextures();
  while (!assetMgr.TryFinalize()) {
  }

  while (!(input.exit_requested() ||
           input.GetButton(fpl::FPLK_AC_BACK).went_down())) {
    input.AdvanceFrame(&renderer.window_size());
    renderer.AdvanceFrame(input.minimized(), input.Time());
    renderer.ClearFrameBuffer(fpl::vec4(0.0, 0.0f, 0.0, 1.0f));

    // generate animation matrix
    auto time = input.Time();
    auto c = cos(time);
    auto s = sin(time);
    auto rotz = mathfu::mat3::RotationZ(s * 3);
    auto rotx = mathfu::mat3::RotationX(s * 5);
    auto zoom = mathfu::vec3(10.0f, 10.0f, 10.0f) + mathfu::vec3(c, c, 1.0f);
    auto aspect = static_cast<float>(renderer.window_size().y()) /
                  renderer.window_size().x();
    auto mvp = mathfu::mat4::Ortho(-1.0, 1.0, -aspect, aspect, -1.0, 1.0) *
               mathfu::mat4::FromRotationMatrix(rotz) *
               mathfu::mat4::FromRotationMatrix(rotx) *
               mathfu::mat4::FromScaleVector(zoom);
    renderer.set_model_view_projection(mvp);
    shader->Set(renderer);
    mesh->Render(renderer);
  }

  renderer.ShutDown();
  return 0;
}
