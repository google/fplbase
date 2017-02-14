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

#include "precompiled.h"
#include "fplbase/input.h"
#include "fplbase/utilities.h"

//#if defined(_DEBUG) || DEBUG==1
//#define LOG_FRAMERATE
//#endif

using mathfu::mat4;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;
using mathfu::vec4i;

namespace fplbase {

InputSystem::InputSystem()
    : exit_requested_(false),
      minimized_(false),
      frame_time_(0),
      elapsed_time_(0),
      start_time_(0),
      time_freq_(0),
      frames_(0),
      minimized_frame_(0),
      mousewheel_delta_(mathfu::kZeros2i),
      record_text_input_(false),
      touch_device_(true) {
  pointers_.assign(kMaxSimultanuousPointers, InputPointer());
#if FPLBASE_ANDROID_VR
  head_mounted_display_input_.InitHMDJNIReference();
#endif  // FPLBASE_ANDROID_VR
}

InputSystem::~InputSystem() {
#if FPLBASE_ANDROID_VR
  head_mounted_display_input_.ClearHMDJNIReference();
#endif  // FPLBASE_ANDROID_VR
}


void InputSystem::ResetInputState() {
  button_map_.clear();
#if ANDROID_GAMEPAD
  gamepad_map_.clear();
#endif
}

void InputSystem::AddAppEventCallback(AppEventCallback callback) {
  app_event_callbacks_.push_back(callback);
}

void InputSystem::AdvanceFrame(vec2i *window_size) {
  // Update timing.
  auto current = RealTime();
  frame_time_ = current - elapsed_time_;
  elapsed_time_ = current;
  frames_++;

#ifdef __ANDROID__
  // For performance mode, we send keypress events to the Android system, so
  // that it knows it's still in active use, even if the user isn't currently
  // touching the screen.
  if (GetPerformanceMode() == kHighPerformance) {
    const HighPerformanceParams &hp_params = GetHighPerformanceParameters();
    double current_time = Time();
    if (current_time >
        last_android_keypress_ + hp_params.time_between_presses) {
      last_android_keypress_ = current_time;
      SendKeypressEventToAndroid(hp_params.android_key_code);
    }
  }
#endif

#ifdef LOG_FRAMERATE
  // Framerate statistics over the last N frames.
  const int kNumDeltaTimes = 64;
  static int cur_delta_idx = 0;
  static double delta_times[kNumDeltaTimes];
  delta_times[cur_delta_idx++] = DeltaTime();
  if (cur_delta_idx == kNumDeltaTimes) {
    cur_delta_idx = 0;
    double sum_delta = 0, min_delta = 99999, max_delta = 0;
    const int kMaxBins = 8;
    int bins[kMaxBins] = {0};
    for (int i = 0; i < kNumDeltaTimes; i++) {
      sum_delta += delta_times[i];
      min_delta = std::min(min_delta, delta_times[i]);
      max_delta = std::max(max_delta, delta_times[i]);
      // Frame-count "bin".
      // Bin 0: 0 ~ 16.6ms (=1/60s)
      // Bin 1: 16.6 ~ 32.3ms (=2/60s)
      // Bin 2: 32.3 ~ 50ms (=3/60s)
      auto bin = static_cast<int>(delta_times[i] * 60.0);
      bins[std::max(1, std::min(bin, kMaxBins - 1))]++;
    }
    std::ostringstream os;
    for (int i = 1; i < kMaxBins; i++) {
      os << bins[i] << ";";
    }
    LogInfo(kApplication,
            "DeltaTime (ms) avg: %.1f, max: %.1f, min: %.1f, bins: %s",
            (sum_delta / kNumDeltaTimes * 1000), max_delta * 1000,
            min_delta * 1000, os.str().c_str());
  }
#endif

  // Reset our per-frame input state.
  mousewheel_delta_ = mathfu::kZeros2i;
  for (auto it = button_map_.begin(); it != button_map_.end(); ++it) {
    it->second.AdvanceFrame();
  }
  for (auto it = pointers_.begin(); it != pointers_.end(); ++it) {
    it->mousedelta = mathfu::kZeros2i;
    if (touch_device_ && !it->used) {
      // A lifted finger should not intersect any position on screen.
      it->mousepos = vec2i(-1, -1);
    }
  }
  for (auto it = joystick_map_.begin(); it != joystick_map_.end(); ++it) {
    it->second.AdvanceFrame();
  }
#if ANDROID_GAMEPAD
  for (auto it = gamepad_map_.begin(); it != gamepad_map_.end(); ++it) {
    it->second.AdvanceFrame();
  }
  HandleGamepadEvents();
#endif  // ANDROID_GAMEPAD
  if (!record_text_input_) {
    text_input_events_.clear();
  }

  UpdateEvents(window_size);

  // Update the head mounted display input. Note this is after the mouse
  // input, as that can be treated as a trigger.
#if FPLBASE_ANDROID_VR
  head_mounted_display_input_.AdvanceFrame();
#endif  // FPLBASE_ANDROID_VR
}

double InputSystem::Time() const { return elapsed_time_; }

double InputSystem::DeltaTime() const { return frame_time_; }

Button &InputSystem::GetButton(int button) {
  auto it = button_map_.find(button);
  return it != button_map_.end() ? it->second
                                 : (button_map_[button] = Button());
}

Joystick &InputSystem::GetJoystick(JoystickId joystick_id) {
  auto it = joystick_map_.find(joystick_id);
  assert(it != joystick_map_.end());
  return it->second;
}

void InputSystem::RemovePointer(size_t i) {
  // Can't clear the rest of the state here yet, since it may still
  // need to be queried this frame.
  pointers_[i].used = false;
}

size_t InputSystem::FindPointer(FingerId id) {
  for (size_t i = 0; i < pointers_.size(); i++) {
    if (pointers_[i].used && pointers_[i].id == id) {
      return i;
    }
  }
  for (size_t i = 0; i < pointers_.size(); i++) {
    if (!pointers_[i].used) {
      pointers_[i].id = id;
      pointers_[i].used = true;
      return i;
    }
  }
  assert(0);
  return 0;
}

void InputSystem::UpdateConnectedJoystickList() {
  CloseOpenJoysticks();
  OpenConnectedJoysticks();
}

const std::vector<TextInputEvent> *InputSystem::GetTextInputEvents() {
  return &text_input_events_;
}

void Button::Update(bool down) {
  if (!is_down_ && down) {
    went_down_ = true;
  } else if (is_down_ && !down) {
    went_up_ = true;
  }
  is_down_ = down;
}

Button &Joystick::GetButton(size_t button_index) {
  if (button_index >= button_list_.size()) {
    button_list_.resize(button_index + 1);
  }
  return button_list_[button_index];
}

float Joystick::GetAxis(size_t axis_index) {
  while (axis_index >= axis_list_.size()) {
    axis_list_.push_back(0);
  }
  return axis_list_[axis_index];
}

vec2 Joystick::GetHat(size_t hat_index) {
  while (hat_index >= hat_list_.size()) {
    hat_list_.push_back(mathfu::kZeros2f);
  }
  return hat_list_[hat_index];
}

void Joystick::SetAxis(size_t axis_index, float axis) {
  GetAxis(axis_index);
  axis_list_[axis_index] = axis;
}

void Joystick::SetHat(size_t hat_index, const vec2 &hat) {
  GetHat(hat_index);
  hat_list_[hat_index] = hat;
}

// Reset the per-frame input on all our sub-elements
void Joystick::AdvanceFrame() {
  for (size_t i = 0; i < button_list_.size(); i++) {
    button_list_[i].AdvanceFrame();
  }
}

// Constructors and destroctor of TextInputEvent union.
TextInputEvent::TextInputEvent(TextInputEventType t) {
  type = t;
  switch (type) {
    case kTextInputEventTypeEdit:
    case kTextInputEventTypeText:
      text = std::string();
      break;
    case kTextInputEventTypeKey:
      break;
  }
}

TextInputEvent::TextInputEvent(TextInputEventType t, int32_t state, bool repeat,
                               int32_t symbol, int32_t modifier) {
  type = t;
  switch (type) {
    case kTextInputEventTypeKey:
      key.state = (state != 0);
      key.repeat = repeat;
      key.symbol = symbol;
      key.modifier = static_cast<FPL_Keymod>(modifier);
      break;
    case kTextInputEventTypeEdit:
    case kTextInputEventTypeText:
      assert(0);
      break;
  }
}

TextInputEvent::TextInputEvent(TextInputEventType t, const char *str) {
  type = t;
  switch (type) {
    case kTextInputEventTypeText:
      text = std::string(str);
      break;
    case kTextInputEventTypeEdit:
    case kTextInputEventTypeKey:
      assert(0);
      break;
  }
}

TextInputEvent::TextInputEvent(TextInputEventType t, const char *str,
                               int32_t start, int32_t length) {
  type = t;
  switch (type) {
    case kTextInputEventTypeEdit:
      text = std::string(str);
      edit.start = start;
      edit.length = length;
      break;
    case kTextInputEventTypeText:
    case kTextInputEventTypeKey:
      assert(0);
      break;
  }
}

}  // namespace fplbase
