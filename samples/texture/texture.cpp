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

#include "precompiled.h"
#include "fplbase/asset_manager.h"
#include "fplbase/renderer.h"
#include "fplbase/input.h"
#include "fplbase/utilities.h"
#include <cassert>

// Game is a sample that displays a textured quad.
//
// It demonstrates usage of:
// - fpl::asset_manager to load textures and shaders.
// - fpl::Renderer to setup rendering and transform models.
// - fpl::InputSystem to query for exit events and elapsed time.

int main(int, char** argv) {
  fpl::InputSystem input;
  input.Initialize();

  fpl::Renderer renderer;
  renderer.Initialize();

  fpl::AssetManager asset_manager(renderer);

  bool result = fpl::ChangeToUpstreamDir(argv[0], "assets");
  assert(result);

  auto shader = asset_manager.LoadShader("tex");
  assert(shader);

  auto tex = asset_manager.LoadTexture("tex.webp");
  assert(tex);

  asset_manager.StartLoadingTextures();
  while (!asset_manager.TryFinalize()) {
    // Can display loading screen here.
  }

  while (!input.exit_requested()) {
    input.AdvanceFrame(&renderer.window_size());
    renderer.AdvanceFrame(input.minimized(), input.Time());

    renderer.ClearFrameBuffer(fpl::vec4(0.0, 0.0f, 0.0, 1.0f));

    shader->Set(renderer);
    tex->Set(0);

    auto time = input.Time();
    auto c = cos(time);
    auto s = sin(time);
    auto rotz = mathfu::mat3::RotationZ(s * 2);
    auto zoom = mathfu::vec3(3.0f, 3.0f, 1.0f) + mathfu::vec3(c, c, 1.0f);
    renderer.model_view_projection() = mathfu::mat4::FromRotationMatrix(rotz) *
                                       mathfu::mat4::FromScaleVector(zoom);

    fpl::Mesh::RenderAAQuadAlongX(mathfu::vec3(-1, -1, 0),
                                  mathfu::vec3( 1,  1, 0),
                                  mathfu::vec2(0, 0),
                                  mathfu::vec2(10, 10));
  }

  renderer.ShutDown();
  return 0;
}
