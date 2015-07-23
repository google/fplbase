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
// - fpl::AssetManager to load textures and shaders.
// - fpl::Renderer to setup rendering and transform models.
// - fpl::InputSystem to query for exit events and elapsed time.
struct Game {
  static const GLfloat verts[];
  static const GLfloat uvs[];

  fpl::Renderer renderer;
  fpl::InputSystem input;
  fpl::AssetManager assetManager;
  fpl::Shader* shader;
  GLuint position;
  GLuint uv;
  GLint scale;
  fpl::Texture* tex;
  float acc;

  Game() : assetManager(renderer) {}

  void Initialize() {
    bool result = fpl::ChangeToUpstreamDir("./", "assets");
    assert(result);
    renderer.Initialize();
    input.Initialize();
    shader = assetManager.LoadShader("tex");
    assert(shader);
    position = glGetAttribLocation(shader->GetProgram(), "in_position");
    uv = glGetAttribLocation(shader->GetProgram(), "in_uv");
    scale = shader->FindUniform("in_scale");

    tex = assetManager.LoadTexture("tex.webp");
    assert(tex);
    assetManager.StartLoadingTextures();
    fpl::LogInfo("start loading materials");
    while (!assetManager.TryFinalize()) {
      fpl::LogInfo("loading %s ...", tex->filename().c_str());
    }
    fpl::LogInfo("done loading materials");
  }
  void ShutDown() { renderer.ShutDown(); }
  void Run() {
    while (!input.exit_requested()) {
      input.AdvanceFrame(&renderer.window_size());
      renderer.AdvanceFrame(input.minimized(), input.Time());
      Render();
    }
  }
  void Render() {
    renderer.ClearFrameBuffer(fpl::vec4(0.0, 0.0f, 0.0, 1.0f));
    shader->Set(renderer);
    glEnableVertexAttribArray(position);
    glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(uv);
    glVertexAttribPointer(uv, 2, GL_FLOAT, GL_FALSE, 0, uvs);
    auto time = input.Time();
    auto c = cos(time);
    auto s = sin(time);
    auto rotz = mathfu::mat3::RotationZ(s * 2);
    auto zoom = mathfu::vec3(3.0f, 3.0f, 1.0f) + mathfu::vec3(c, c, 1.0f);
    renderer.model() = mathfu::mat4::FromRotationMatrix(rotz) *
                       mathfu::mat4::FromScaleVector(zoom);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
};
// Quad vertices coordinates: 2 triangle strips.
const GLfloat Game::verts[] = {-1.0f, 1.0f, -1.0f, -1.0f,
                               1.0f,  1.0f, 1.0f,  -1.0f};
// Quad texture coordinates.
const GLfloat Game::uvs[] = {-5.0f, -5.0f, -5.0f, 5.0f,
                             5.0f,  -5.0f, 5.0f,  5.0f};

int main() {
  Game game;
  game.Initialize();
  game.Run();
  game.ShutDown();
  return 0;
}
