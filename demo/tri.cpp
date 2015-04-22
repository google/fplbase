#include "precompiled.h"
#include "fplbase/renderer.h"
#include "fplbase/input.h"
#include <cassert>

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
      float color = (1.0f - cos(input.Time())) / 2.0f;
      renderer.ClearFrameBuffer(fpl::vec4(color, 0.0f, color, 1.0f));
      shader->Set(renderer);
      glEnableVertexAttribArray(vPositionHandle);
      glVertexAttribPointer(vPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, verts);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }
};
// A vertex shader that passes untransformed position thru.
const char *Game::vertexShader = \
  "attribute vec4 vPosition;\n"
  "void main() {\n"
  "  gl_Position = vPosition;\n"
  "}\n";
// A fragment shader that outputs a green pixel.
const char *Game::fragmentShader = \
  "void main() {\n"
  "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
  "}\n";
// Triangle vertice coordinates.
const GLfloat Game::verts[] = \
  { 0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f };

int main() {
  Game game;
  game.Initialize();
  game.Run();
  game.ShutDown();
  return 0;
}
