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

#include <cassert>
#include <cmath>
#include "fplbase/asset_manager.h"
#include "fplbase/input.h"
#include "fplbase/render_utils.h"
#include "fplbase/renderer.h"
#include "fplbase/utilities.h"

// Game is a sample that displays a textured quad.
//
// It demonstrates usage of:
// - asset_manager to load textures and shaders.
// - Renderer to setup rendering and transform models.
// - InputSystem to query for exit events and elapsed time.

extern "C" int FPL_main(int /*argc*/, char* argv[]) {
  fplbase::InputSystem input;
  input.Initialize();

  fplbase::Renderer renderer;
  renderer.Initialize(mathfu::vec2i(800, 600), "Simple asset loading test");

  fplbase::AssetManager asset_manager(renderer);

  bool result = fplbase::ChangeToUpstreamDir(argv[0], "assets");
  (void)result;
  assert(result);

  auto shader = asset_manager.LoadShader("tex");
  assert(shader);

  // This will load a .webp instead if file not available or no hardware
  // support for this texture compression format.
  auto tex = asset_manager.LoadTexture("tex.ktx");  // ETC2
  assert(tex);

  asset_manager.StartLoadingTextures();
  while (!asset_manager.TryFinalize()) {
    // Can display loading screen here.
  }

  while (!(input.exit_requested() ||
           input.GetButton(fplbase::FPLK_AC_BACK).went_down())) {
    renderer.AdvanceFrame(input.minimized(), input.Time());
    input.AdvanceFrame(&renderer.window_size());

    renderer.ClearFrameBuffer(mathfu::vec4(0.0, 0.0f, 0.0, 1.0f));

    auto time = static_cast<float>(input.Time());
    auto c = std::cos(time);
    auto s = std::sin(time);
    auto rotz = mathfu::mat3::RotationZ(s * 2);
    auto zoom = mathfu::vec3(3.0f, 3.0f, 1.0f) + mathfu::vec3(c, c, 1.0f);
    auto aspect =
        static_cast<float>(renderer.window_size().y) / renderer.window_size().x;
    renderer.set_model_view_projection(
        mathfu::mat4::Ortho(-1.0, 1.0, -aspect, aspect, -1.0, 1.0) *
        mathfu::mat4::FromRotationMatrix(rotz) *
        mathfu::mat4::FromScaleVector(zoom));

    renderer.SetShader(shader);
    tex->Set(0);

    fplbase::RenderAAQuadAlongX(mathfu::vec3(-1, -1, 0), mathfu::vec3(1, 1, 0),
                                mathfu::vec2(0, 0), mathfu::vec2(10, 10));
  }

  asset_manager.ClearAllAssets();
  renderer.ShutDown();
  return 0;
}
