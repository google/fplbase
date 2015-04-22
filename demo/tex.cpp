#include "precompiled.h"
#include "fplbase/renderer.h"
#include "fplbase/input.h"
#include "fplbase/material_manager.h"
#include "fplbase/utilities.h"
#include <cassert>

struct Game {
  static const GLfloat verts[];
  static const GLfloat uvs[];

  fpl::Renderer renderer;
  fpl::InputSystem input;
  fpl::MaterialManager matManager;
  fpl::Shader *shader;
  GLuint position;
  GLuint uv;
  GLint scale;
  fpl::Texture* tex;
  float acc;

  Game() : matManager(renderer) {}

  void Initialize() {
    renderer.Initialize();
    input.Initialize();
    shader = matManager.LoadShader("tex");
    assert(shader);
    position = glGetAttribLocation(shader->GetProgram(), "in_position");
    uv = glGetAttribLocation(shader->GetProgram(), "in_uv");
    scale = shader->FindUniform("in_scale");

    tex = matManager.LoadTexture("tex.webp");
    assert(tex);
    matManager.StartLoadingTextures();
    fpl::LogInfo("start loading materials");
    while(!matManager.TryFinalize()) {
      fpl::LogInfo("loading %s ...", tex->filename().c_str());
    }
    fpl::LogInfo("done loading materials");
  }
  void ShutDown() {
    renderer.ShutDown();
  }
  void Run() {
    while(!input.exit_requested_) {
      input.AdvanceFrame(&renderer.window_size());
      renderer.AdvanceFrame(input.minimized_);
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
    renderer.model() = mathfu::mat4::FromRotationMatrix(rotz) * mathfu::mat4::FromScaleVector(zoom);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
};
// Quad vertices coordinates: 2 triangle strips.
const GLfloat Game::verts[] =  {
    -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f
};
// Quad texture coordinates.
const GLfloat Game::uvs[] = {
    -5.0f, -5.0f, -5.0f, 5.0f, 5.0f, -5.0f, 5.0f, 5.0f
};

int main() {
  Game game;
  game.Initialize();
  game.Run();
  game.ShutDown();
  return 0;
}
