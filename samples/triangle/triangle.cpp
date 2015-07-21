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
#include "fplbase/renderer.h"
#include "fplbase/input.h"
#include <cassert>

// Game is a sample that displays a colored triangle.
//
// It demonstrates usage of:
// - fpl::Renderer to load shaders from strings and setup rendering.
// - fpl::InputSystem to query for exit events and elapsed time.
class Game {
  static const char *vertexShader;
  static const char *fragmentShader;
  static const GLfloat verts[];

  fpl::Renderer renderer;
  fpl::InputSystem input;
  fpl::Shader *shader;
  GLuint vPositionHandle;

 public:
  void Initialize() {
    renderer.Initialize();
    input.Initialize();
    shader = renderer.CompileAndLinkShader(vertexShader, fragmentShader);
    assert(shader);
    vPositionHandle = glGetAttribLocation(shader->GetProgram(), "vPosition");
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
    float color = (1.0f - cos(input.Time())) / 2.0f;
    renderer.ClearFrameBuffer(fpl::vec4(color, 0.0f, color, 1.0f));
    shader->Set(renderer);
    glEnableVertexAttribArray(vPositionHandle);
    glVertexAttribPointer(vPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }
};
// A vertex shader that passes untransformed position thru.
const char *Game::vertexShader =
    "attribute vec4 vPosition;\n"
    "void main() {\n"
    "  gl_Position = vPosition;\n"
    "}\n";
// A fragment shader that outputs a green pixel.
const char *Game::fragmentShader =
    "void main() {\n"
    "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
    "}\n";
// Triangle vertice coordinates.
const GLfloat Game::verts[] = {0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f};

int main() {
  Game game;
  game.Initialize();
  game.Run();
  game.ShutDown();
  return 0;
}
