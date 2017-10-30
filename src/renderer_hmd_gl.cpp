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

#include <EGL/egl.h>

#include "precompiled.h"
#include "fplbase/renderer_hmd.h"
#include "fplbase/utilities.h"
#include "fplbase/gpu_debug.h"

using mathfu::vec2i;

namespace fplbase {

// The framebuffer that is used for undistortion in Head Mounted Displays.
// After rendering to it, passed to Cardboard's undistortTexture call, which
// will transform and render it appropriately.
GLuint g_undistort_framebuffer_id = 0;
// The texture, needed for the color
GLuint g_undistort_texture_id = 0;
// The renderbuffer, needed for the depth
GLuint g_undistort_renderbuffer_id = 0;

void InitializeUndistortFramebuffer(int width, int height) {
  // Set up a framebuffer that matches the window, such that we can render to
  // it, and then undistort the result properly for HMDs.
  GL_CALL(glGenTextures(1, &g_undistort_texture_id));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, g_undistort_texture_id));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                       GL_UNSIGNED_BYTE, nullptr));

  GL_CALL(glGenRenderbuffers(1, &g_undistort_renderbuffer_id));
  GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, g_undistort_renderbuffer_id));
  GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width,
                                height));

  GL_CALL(glGenFramebuffers(1, &g_undistort_framebuffer_id));
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, g_undistort_framebuffer_id));

  GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_2D, g_undistort_texture_id, 0));
  GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                    GL_RENDERBUFFER,
                                    g_undistort_renderbuffer_id));

  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}

void BeginUndistortFramebuffer() {
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, g_undistort_framebuffer_id));
}

void FinishUndistortFramebuffer() {
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
  JNIEnv* env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
  jclass fpl_class = env->GetObjectClass(activity);
  jmethodID undistort = env->GetMethodID(fpl_class, "UndistortTexture", "(I)V");
  env->CallVoidMethod(activity, undistort, (jint)g_undistort_texture_id);
  env->DeleteLocalRef(fpl_class);
  env->DeleteLocalRef(activity);
}

void SetCardboardButtonEnabled(bool enabled) {
  JNIEnv* env = AndroidGetJNIEnv();
  jobject activity = AndroidGetActivity();
  jclass fpl_class = env->GetObjectClass(activity);
  jmethodID set_button =
      env->GetMethodID(fpl_class, "SetCardboardButtonEnabled", "(Z)V");
  env->CallVoidMethod(activity, set_button, (jboolean)enabled);
  env->DeleteLocalRef(fpl_class);
  env->DeleteLocalRef(activity);
}

#if FPLBASE_ANDROID_VR

// Prepare to render to a Head Mounted Display.
void HeadMountedDisplayRenderStart(
    const HeadMountedDisplayInput& head_mounted_display_input,
    Renderer* renderer, const mathfu::vec4& clear_color, bool use_undistortion,
    HeadMountedDisplayViewSettings* view_settings) {
  if (use_undistortion) {
    BeginUndistortFramebuffer();
    // Verify that the Cardboard API has not changed the rendering state.
    // If we hit this assert, we'll have to set the appropriate state to
    // `unknown` here.
    assert(ValidateRenderState(renderer->GetRenderState()));
  }
  renderer->ClearFrameBuffer(clear_color);
  renderer->set_color(mathfu::kOnes4f);
  renderer->SetDepthFunction(fplbase::kDepthFunctionLess);

  const mathfu::vec2i viewport_size = renderer->GetViewportSize();
  int window_width = viewport_size.x;
  int window_height = viewport_size.y;
  int half_width = window_width / 2;
  // Calculate settings for each viewport.
  mathfu::vec4i* viewport_extents = view_settings->viewport_extents;
  mathfu::mat4* viewport_transforms = view_settings->viewport_transforms;
  viewport_extents[0] = mathfu::vec4i(0, 0, half_width, window_height);
  viewport_extents[1] = mathfu::vec4i(half_width, 0, half_width, window_height);
  viewport_transforms[0] = head_mounted_display_input.left_eye_transform();
  viewport_transforms[1] = head_mounted_display_input.right_eye_transform();
}

// Reset viewport settings, finish applying undistortion effect (if enabled)
// and disable blending.
void HeadMountedDisplayRenderEnd(Renderer* renderer, bool use_undistortion) {
  // Reset the screen, and finish
  Viewport viewport(mathfu::kZeros2i, renderer->GetViewportSize());
  renderer->SetViewport(viewport);
  if (use_undistortion) {
    FinishUndistortFramebuffer();
    // Verify that the Cardboard API has not changed the rendering state.
    // If we hit this assert, we'll have to set the appropriate state to
    // `unknown` here.
    assert(ValidateRenderState(renderer->GetRenderState()));
    renderer->SetBlendMode(kBlendModeOff);
  }
}

#endif  // FPLBASE_ANDROID_VR

}  // namespace fplbase
