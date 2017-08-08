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

#include "fplbase/gpu_debug.h"
#include "fplbase/preprocessor.h"
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

// static member variables
std::weak_ptr<RendererBase> RendererBase::the_base_weak_;
RendererBase* RendererBase::the_base_raw_;
fplutil::Mutex RendererBase::the_base_mutex_;

RendererBase::RendererBase()
    : impl_(CreateRendererBaseImpl()),
      time_(0),
      supports_texture_format_(-1),
      supports_texture_npot_(false),
      supports_multiview_(false),
      supports_instancing_(false),
      force_shader_(nullptr),
      force_blend_mode_(kBlendModeCount),
      max_vertex_uniform_components_(0),
      version_(&Version()) {
  assert(the_base_raw_ == nullptr);
}

RendererBase::~RendererBase() {
  assert(the_base_raw_ != nullptr);
  ShutDown();

  // Delete platform dependent data.
  DestroyRendererBaseImpl(impl_);
}

Renderer::Renderer()
    : impl_(CreateRendererImpl()),
      model_view_projection_(mathfu::mat4::Identity()),
      model_(mathfu::mat4::Identity()),
      color_(mathfu::kOnes4f),
      light_pos_(mathfu::kZeros3f),
      camera_pos_(mathfu::kZeros3f),
      bone_transforms_(nullptr),
      num_bones_(0),
      blend_mode_(kBlendModeUnknown),
      blend_amount_(0.0f),
      cull_mode_(kCullingModeUnknown),
      depth_function_(kDepthFunctionUnknown),
      stencil_mode_(kStencilUnknown),
      stencil_ref_(0),
      stencil_mask_(~0u) {
  // This is the only place that the RendererBase singleton can be created,
  // so ensure it's guarded by the mutex.
  fplutil::MutexLock lock(RendererBase::the_base_mutex_);

  if (RendererBase::the_base_weak_.expired()) {
    // Create a new Renderer if one doesn't exist.
    base_ = std::make_shared<RendererBase>();
    RendererBase::the_base_weak_ = base_;
    RendererBase::the_base_raw_ = base_.get();
  } else {
    // Make this Renderer one of the shared owners of the singleton.
    base_ = RendererBase::the_base_weak_.lock();
  }
}

Renderer::~Renderer() {
  // This is the only place that the RenderBase singleton can be destroyed,
  // so ensure it's guarded by the mutex.
  fplutil::MutexLock lock(RendererBase::the_base_mutex_);
  assert(!RendererBase::the_base_weak_.expired() &&
         RendererBase::the_base_raw_ != nullptr);
  base_.reset();

  // Manually keep the_base_raw_ in sync with the_base_weak_.
  if (RendererBase::the_base_weak_.expired()) {
    RendererBase::the_base_raw_ = nullptr;
  }

  // Delete platform dependent data.
  DestroyRendererImpl(impl_);
}

bool RendererBase::Initialize(const vec2i &window_size,
                              const char *window_title,
                              WindowMode window_mode) {
  if (!environment_.Initialize(window_size, window_title, window_mode)) {
    last_error_ = environment_.last_error();
    return false;
  }
  // Non-environment-specific initialization continues here:
  return InitializeRenderingState();
}

void Renderer::BeginRendering() {
#ifdef FPLBASE_VERIFY_GPU_STATE
  ValidateRenderState(render_state_);
#endif  // FPLBASE_VERIFY_GPU_STATE
}

void Renderer::EndRendering() {
#ifdef FPLBASE_VERIFY_GPU_STATE
  ValidateRenderState(render_state_);
#endif  // FPLBASE_VERIFY_GPU_STATE
}

bool RendererBase::SupportsTextureFormat(TextureFormat texture_format) const {
  return (supports_texture_format_ & (1LL << texture_format)) != 0;
}

bool RendererBase::SupportsTextureNpot() const {
  return supports_texture_npot_;
}

bool RendererBase::SupportsMultiview() const {
  return supports_multiview_;
}

Shader *RendererBase::CompileAndLinkShader(const char *vs_source,
                                           const char *ps_source) {
  return CompileAndLinkShaderHelper(vs_source, ps_source, nullptr);
}

Shader *RendererBase::RecompileShader(const char *vs_source,
                                      const char *ps_source, Shader *shader) {
  return CompileAndLinkShaderHelper(vs_source, ps_source, shader);
}

void Renderer::SetBlendMode(BlendMode blend_mode) {
  SetBlendMode(blend_mode, 0.5f);
}

BlendMode Renderer::GetBlendMode() {
  return blend_mode_;
}

void Renderer::AdvanceFrame(bool minimized, double time) {
  base_->AdvanceFrame(minimized, time);
  SetDepthFunction(kDepthFunctionLess);

  auto viewport_size = environment().GetViewportSize();
  SetViewport(Viewport(0, 0, viewport_size.x, viewport_size.y));
}

void Renderer::UpdateCachedRenderState(const RenderState &render_state) {
  render_state_ = render_state;

  const BlendMode prev_blend_mode = blend_mode_;
  const CullingMode prev_cull_mode = cull_mode_;
  const DepthFunction prev_depth_function = depth_function_;
  const StencilMode prev_stencil_mode = stencil_mode_;

  blend_mode_ = kBlendModeUnknown;
  cull_mode_ = kCullingModeUnknown;
  depth_function_ = kDepthFunctionUnknown;
  stencil_mode_ = kStencilUnknown;

  SetBlendMode(prev_blend_mode, blend_amount_);
  SetCulling(prev_cull_mode);
  SetDepthFunction(prev_depth_function);
  SetStencilMode(prev_stencil_mode, stencil_ref_, stencil_mask_);
}

}  // namespace fplbase
