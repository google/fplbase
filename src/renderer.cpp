// Copyright 2014 Google Inc. All rights reserved.
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

#include "precompiled.h"  // NOLINT

#include "fplbase/render_target.h"
#include "fplbase/renderer.h"
#include "fplbase/texture.h"
#include "fplbase/utilities.h"

using mathfu::mat4;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;

namespace fplbase {

Renderer *Renderer::the_renderer_ = nullptr;

#define LOOKUP_GL_FUNCTION(type, name, lookup_fn)                  \
  union {                                                          \
    void *data;                                                    \
    type function;                                                 \
  } data_function_union_##name;                                    \
  data_function_union_##name.data = lookup_fn(#name);              \
  if (!data_function_union_##name.data) {                          \
    last_error_ = "could not retrieve GL function pointer " #name; \
    return false;                                                  \
  }                                                                \
  name = data_function_union_##name.function;

Renderer::Renderer()
    : time_(0),
      window_size_(vec2i(800, 600)),  // Overwritten elsewhere.
      default_render_context_(nullptr),
#ifdef FPL_BASE_RENDERER_BACKEND_SDL
      window_(nullptr),
      context_(nullptr),
#endif  // FPL_BASE_RENDERER_BACKEND_SDL
      feature_level_(kFeatureLevel20),
      supports_texture_format_(-1),
      supports_texture_npot_(false),
      force_shader_(nullptr),
      force_blend_mode_(kBlendModeCount),
      max_vertex_uniform_components_(0),
      version_(&Version()) {
  assert(!the_renderer_);
  the_renderer_ = this;
}

#ifndef FPL_BASE_RENDERER_BACKEND_SDL

Renderer::~Renderer() {
  the_renderer_ = nullptr;
  delete default_render_context_;
}

// When building without SDL we assume the window and rendering context have
// already been created prior to calling initialize.
bool Renderer::Initialize(const vec2i & /*window_size*/,
                          const char * /*window_size*/) {
#if defined(_WIN32)
#define GLEXT(type, name) LOOKUP_GL_FUNCTION(type, name, wglGetProcAddress)
  GLBASEEXTS GLEXTS
#undef GLEXT
#endif  // defined(_WIN32)

#ifdef __ANDROID__
      const int version = AndroidGetContextClientVersion();
  if (version >= 3) {
    feature_level_ = kFeatureLevel30;
    AndroidInitGl3Functions();
  }
#endif  // defined(__ANDROID__)

  return InitializeRenderingState();
}

void Renderer::SetWindowSize(const vec2i &window_size) {
  window_size_ = window_size;
}

#else  // !FPL_BASE_RENDERER_BACKEND_SDL

Renderer::~Renderer() {
  the_renderer_ = nullptr;
  delete default_render_context_;
  ShutDown();
}

bool Renderer::Initialize(const vec2i &window_size, const char *window_title) {
  // Basic SDL initialization, does not actually initialize a Window or OpenGL,
  // typically should not fail.
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO)) {
    last_error_ = std::string("SDL_Init fail: ") + SDL_GetError();
    return false;
  }

  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);

#ifdef __ANDROID__
  // Setup HW scaler in Android
  AndroidSetScalerResolution(window_size);
  AndroidPreCreateWindow();
#endif

  // Always double buffer.
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  // Set back buffer format to 565
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);

  // Create the window:
  window_ = SDL_CreateWindow(
      window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      window_size.x(), window_size.y(), SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN |
#ifdef PLATFORM_MOBILE
                                            SDL_WINDOW_BORDERLESS);
#else
                                            SDL_WINDOW_RESIZABLE);
#endif
  if (!window_) {
    last_error_ = std::string("SDL_CreateWindow fail: ") + SDL_GetError();
    return false;
  }

  // Get the size we actually got, which typically is native res for
  // any fullscreen display:
  SDL_GetWindowSize(static_cast<SDL_Window *>(window_), &window_size_.x(),
                    &window_size_.y());

  // Create the OpenGL context:
  // Try to get OpenGL ES 3 on mobile.
  // On desktop, we assume we can get function pointers for all ES 3 equivalent
  // functions.
  feature_level_ = kFeatureLevel30;
#ifdef PLATFORM_MOBILE
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
#ifdef __APPLE__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#endif
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                      SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif
  context_ = SDL_GL_CreateContext(static_cast<SDL_Window *>(window_));
#ifdef PLATFORM_MOBILE
  if (context_) {
#ifdef __ANDROID__
    AndroidInitGl3Functions();
#endif
  } else {
    // Failed to get ES 3.0 context, let's try 2.0.
    feature_level_ = kFeatureLevel20;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    context_ = SDL_GL_CreateContext(static_cast<SDL_Window *>(window_));
  }
#endif
  if (!context_) {
    last_error_ = std::string("SDL_GL_CreateContext fail: ") + SDL_GetError();
    return false;
  }

#ifdef PLATFORM_MOBILE
  LogInfo("FPLBase: got OpenGL ES context level %s",
          feature_level_ == kFeatureLevel20 ? "2.0" : "3.0");
#endif

// Enable Vsync on desktop
#ifndef PLATFORM_MOBILE
  SDL_GL_SetSwapInterval(1);
#endif

#if !defined(PLATFORM_MOBILE) && !defined(__APPLE__)
#define GLEXT(type, name) LOOKUP_GL_FUNCTION(type, name, SDL_GL_GetProcAddress)
  GLBASEEXTS GLEXTS
#undef GLEXT
#endif

  // Non-SDL-specific initialization continues here:
  return InitializeRenderingState();
}

void Renderer::BeginRendering(RenderContext *) {}

void Renderer::EndRendering(RenderContext *) {}

void Renderer::AdvanceFrame(bool minimized, double time) {
  time_ = time;
  if (minimized) {
    // Save some cpu / battery:
    SDL_Delay(10);
  } else {
    SDL_GL_SwapWindow(static_cast<SDL_Window *>(window_));
  }
  // Get window size again, just in case it has changed.
  SDL_GetWindowSize(static_cast<SDL_Window *>(window_), &window_size_.x(),
                    &window_size_.y());

  auto viewport_size = GetViewportSize();
  GL_CALL(glViewport(0, 0, viewport_size.x(), viewport_size.y()));
  DepthTest(true);
}

void Renderer::ShutDown() {
  if (context_) {
    SDL_GL_DeleteContext(context_);
    context_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(static_cast<SDL_Window *>(window_));
    window_ = nullptr;
  }
  SDL_Quit();
}

#endif  // !FPL_BASE_RENDERER_BACKEND_SDL

bool Renderer::SupportsTextureFormat(TextureFormat texture_format) const {
  return (supports_texture_format_ & (1LL << texture_format)) != 0;
}

bool Renderer::SupportsTextureNpot() const { return supports_texture_npot_; }

bool Renderer::InitializeRenderingState() {
  default_render_context_ = new RenderContext();

  auto exts = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));

  auto HasGLExt = [&exts](const char *ext) -> bool {
    // TODO(b/28761934): Consider supporting GL3.0 version.
    if (exts == nullptr) {
      return false;
    }
    auto pos = strstr(exts, ext);
    return pos && pos[strlen(ext)] <= ' ';  // Make sure it matched all.
  };

  // Check for ASTC: Available in devices supporting AEP.
  if (!HasGLExt("GL_KHR_texture_compression_astc_ldr")) {
    supports_texture_format_ &= ~(1 << kFormatASTC);
  }
  // Check for Non Power of 2 (NPOT) extension.
  if (HasGLExt("GL_ARB_texture_non_power_of_two") ||
      HasGLExt("GL_OES_texture_npot")) {
    supports_texture_npot_ = true;
  }

// Check for ETC2:
#ifdef PLATFORM_MOBILE
  if (feature_level_ < kFeatureLevel30) {
#else
  if (!HasGLExt("GL_ARB_ES3_compatibility")) {
#endif
    supports_texture_format_ &= ~((1 << kFormatPKM) | (1 << kFormatKTX));
  }

#ifndef PLATFORM_MOBILE
  if (!HasGLExt("GL_ARB_vertex_buffer_object") ||
      !HasGLExt("GL_ARB_multitexture") || !HasGLExt("GL_ARB_vertex_program") ||
      !HasGLExt("GL_ARB_fragment_program")) {
    last_error_ = "missing GL extensions";
    return false;
  }
#endif

  GL_CALL(glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS,
                        &max_vertex_uniform_components_));
#if defined(GL_MAX_VERTEX_UNIFORM_VECTORS)
  if (max_vertex_uniform_components_ == 0) {
    // If missing the number of uniform components, use the number of vectors.
    GL_CALL(glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS,
                          &max_vertex_uniform_components_));
    max_vertex_uniform_components_ *= 4;
  }
#endif  // defined(GL_MAX_VERTEX_UNIFORM_VECTORS)

  return true;
}

void Renderer::ClearFrameBuffer(const vec4 &color, RenderContext *) {
  GL_CALL(glClearColor(color.x(), color.y(), color.z(), color.w()));
  GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

void Renderer::ClearDepthBuffer(RenderContext *) {
  GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));
}

GLuint Renderer::CompileShader(bool is_vertex_shader, GLuint program,
                               const GLchar *source) {
  if (!is_vertex_shader && override_pixel_shader_.length())
    source = override_pixel_shader_.c_str();
  GLenum stage = is_vertex_shader ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
  std::string platform_source =
#ifdef PLATFORM_MOBILE
      "#ifdef GL_ES\nprecision highp float;\n#endif\n";
#else
      "#version 120\n#define lowp\n#define mediump\n#define highp\n";
#endif
  assert(max_vertex_uniform_components_);
  platform_source += "#define MAX_VERTEX_UNIFORM_COMPONENTS ";
  platform_source += flatbuffers::NumToString(max_vertex_uniform_components_);
  platform_source += "\n";
  platform_source += source;
  const char *platform_source_ptr = platform_source.c_str();
  auto shader_obj = glCreateShader(stage);
  GL_CALL(glShaderSource(shader_obj, 1, &platform_source_ptr, nullptr));
  GL_CALL(glCompileShader(shader_obj));
  GLint success;
  GL_CALL(glGetShaderiv(shader_obj, GL_COMPILE_STATUS, &success));
  if (success) {
    GL_CALL(glAttachShader(program, shader_obj));
    return shader_obj;
  } else {
    GLint length = 0;
    GL_CALL(glGetShaderiv(shader_obj, GL_INFO_LOG_LENGTH, &length));
    last_error_.assign(length, '\0');
    GL_CALL(glGetShaderInfoLog(shader_obj, length, &length, &last_error_[0]));
    GL_CALL(glDeleteShader(shader_obj));
    return 0;
  }
}

Shader *Renderer::CompileAndLinkShaderHelper(const char *vs_source,
                                             const char *ps_source,
                                             Shader *shader) {
  auto program = glCreateProgram();
  auto vs = CompileShader(true, program, vs_source);
  if (vs) {
    auto ps = CompileShader(false, program, ps_source);
    if (ps) {
      GL_CALL(glBindAttribLocation(program, Mesh::kAttributePosition,
                                   "aPosition"));
      GL_CALL(glBindAttribLocation(program, Mesh::kAttributeNormal,
                                   "aNormal"));
      GL_CALL(glBindAttribLocation(program, Mesh::kAttributeTangent,
                                   "aTangent"));
      GL_CALL(glBindAttribLocation(program, Mesh::kAttributeTexCoord,
                                   "aTexCoord"));
      GL_CALL(glBindAttribLocation(program, Mesh::kAttributeTexCoordAlt,
                                   "aTexCoordAlt"));
      GL_CALL(glBindAttribLocation(program, Mesh::kAttributeColor,
                                   "aColor"));
      GL_CALL(glBindAttribLocation(program, Mesh::kAttributeBoneIndices,
                                   "aBoneIndices"));
      GL_CALL(glBindAttribLocation(program, Mesh::kAttributeBoneWeights,
                                   "aBoneWeights"));
      GL_CALL(glLinkProgram(program));
      GLint status;
      GL_CALL(glGetProgramiv(program, GL_LINK_STATUS, &status));
      if (status == GL_TRUE) {
        if (shader == nullptr) {
          // Load a new shader.
          shader = new Shader(program, vs, ps);
        } else {
          // Destruct old shader and create recompiled shader in its place.
          shader->~Shader();
          shader = new (shader) Shader(program, vs, ps);
        }
        GL_CALL(glUseProgram(program));
        shader->InitializeUniforms();
        return shader;
      }
      GLint length = 0;
      GL_CALL(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length));
      last_error_.assign(length, '\0');
      GL_CALL(glGetProgramInfoLog(program, length, &length, &last_error_[0]));
      GL_CALL(glDeleteShader(ps));
    }
    GL_CALL(glDeleteShader(vs));
  }
  GL_CALL(glDeleteProgram(program));
  return nullptr;
}

Shader *Renderer::CompileAndLinkShader(const char *vs_source,
                                       const char *ps_source) {
  return CompileAndLinkShaderHelper(vs_source, ps_source, nullptr);
}

void Renderer::RecompileShader(const char *vs_source, const char *ps_source,
                               Shader *shader) {
  shader = CompileAndLinkShaderHelper(vs_source, ps_source, shader);
}

void Renderer::DepthTest(bool on, RenderContext *render_context) {
  if (on) {
    GL_CALL(glEnable(GL_DEPTH_TEST));
    render_context->depth_test = true;
  } else {
    GL_CALL(glDisable(GL_DEPTH_TEST));
    render_context->depth_test = false;
  }
}

void Renderer::SetBlendMode(BlendMode blend_mode,
                            RenderContext *render_context) {
  SetBlendMode(blend_mode, 0.5f, render_context);
}

void Renderer::SetBlendMode(BlendMode blend_mode, float amount,
                            RenderContext *render_context) {
  (void)amount;
  if (blend_mode == render_context->blend_mode_) return;

  if (force_blend_mode_ != kBlendModeCount) blend_mode = force_blend_mode_;

  // Disable current blend mode.
  switch (render_context->blend_mode_) {
    case kBlendModeOff:
      break;
    case kBlendModeTest:
#ifndef PLATFORM_MOBILE  // Alpha test not supported in ES 2.
      GL_CALL(glDisable(GL_ALPHA_TEST));
      break;
#endif
    case kBlendModeAlpha:
    case kBlendModeAdd:
    case kBlendModeAddAlpha:
    case kBlendModeMultiply:
    case kBlendModePreMultipliedAlpha:
      GL_CALL(glDisable(GL_BLEND));
      break;
    default:
      assert(false);  // Not yet implemented
      break;
  }

  // Enable new blend mode.
  switch (blend_mode) {
    case kBlendModeOff:
      break;
    case kBlendModeTest:
#ifndef PLATFORM_MOBILE
      GL_CALL(glEnable(GL_ALPHA_TEST));
      GL_CALL(glAlphaFunc(GL_GREATER, amount));
      break;
#endif
    case kBlendModeAlpha:
      GL_CALL(glEnable(GL_BLEND));
      GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
      break;
    case kBlendModeAdd:
      GL_CALL(glEnable(GL_BLEND));
      GL_CALL(glBlendFunc(GL_ONE, GL_ONE));
      break;
    case kBlendModeAddAlpha:
      GL_CALL(glEnable(GL_BLEND));
      GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE));
      break;
    case kBlendModeMultiply:
      GL_CALL(glEnable(GL_BLEND));
      GL_CALL(glBlendFunc(GL_DST_COLOR, GL_ZERO));
      break;
    case kBlendModePreMultipliedAlpha:
      GL_CALL(glEnable(GL_BLEND));
      GL_CALL(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
      break;
    default:
      assert(false);  // Not yet implemented.
      break;
  }

  // Remember new mode as the current mode.
  render_context->blend_mode_ = blend_mode;
}

void Renderer::SetCulling(CullingMode mode, RenderContext *render_context) {
  if (mode == kCullingModeNone) {
    GL_CALL(glDisable(GL_CULL_FACE));
  } else {
    GL_CALL(glEnable(GL_CULL_FACE));
    switch (mode) {
      case kCullingModeBack:
        GL_CALL(glCullFace(GL_BACK));
        break;
      case kCullingModeFront:
        GL_CALL(glCullFace(GL_FRONT));
        break;
      case kCullingModeFrontAndBack:
        GL_CALL(glCullFace(GL_FRONT_AND_BACK));
        break;
      default:
        // Unknown culling mode.
        assert(false);
    }
  }
  render_context->cull_mode_ = mode;
}

vec2i Renderer::GetViewportSize() {
#if defined(__ANDROID__) && defined(FPL_BASE_RENDERER_BACKEND_SDL)
  // Check HW scaler setting and change a viewport size if they are set.
  vec2i scaled_size = fplbase::AndroidGetScalerResolution();
  vec2i viewport_size =
      scaled_size.x() && scaled_size.y() ? scaled_size : window_size_;
  return viewport_size;
#else
  return window_size_;
#endif
}

void Renderer::ScissorOn(const vec2i &pos, const vec2i &size, RenderContext *) {
  glEnable(GL_SCISSOR_TEST);
  auto viewport_size = GetViewportSize();
  GL_CALL(glViewport(0, 0, viewport_size.x(), viewport_size.y()));

  auto scaling_ratio = vec2(viewport_size) / vec2(window_size_);
  auto scaled_pos = vec2(pos) * scaling_ratio;
  auto scaled_size = vec2(size) * scaling_ratio;
  glScissor(static_cast<GLint>(scaled_pos.x()),
            static_cast<GLint>(scaled_pos.y()),
            static_cast<GLsizei>(scaled_size.x()),
            static_cast<GLsizei>(scaled_size.y()));
}

void Renderer::ScissorOff(RenderContext *) { glDisable(GL_SCISSOR_TEST); }

}  // namespace fplbase

#ifndef GL_INVALID_FRAMEBUFFER_OPERATION
#define GL_INVALID_FRAMEBUFFER_OPERATION GL_INVALID_FRAMEBUFFER_OPERATION_EXT
#endif

void LogGLError(const char *file, int line, const char *call) {
  auto err = glGetError();
  if (err == GL_NO_ERROR) return;
  const char *err_str = "<unknown error enum>";
  switch (err) {
    case GL_INVALID_ENUM:
      err_str = "GL_INVALID_ENUM";
      break;
    case GL_INVALID_VALUE:
      err_str = "GL_INVALID_VALUE";
      break;
    case GL_INVALID_OPERATION:
      err_str = "GL_INVALID_OPERATION";
      break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      err_str = "GL_INVALID_FRAMEBUFFER_OPERATION";
      break;
    case GL_OUT_OF_MEMORY:
      err_str = "GL_OUT_OF_MEMORY";
      break;
  }
  LogError(fplbase::kError, "%s(%d): OpenGL Error: %s from %s", file, line,
           err_str, call);
  assert(0);
}

#if !defined(GL_GLEXT_PROTOTYPES)
#if !defined(PLATFORM_MOBILE) && !defined(__APPLE__)
#define GLEXT(type, name) type name = nullptr;
GLBASEEXTS GLEXTS
#undef GLEXT
#endif
#endif  // !defined(GL_GLEXT_PROTOTYPES)
