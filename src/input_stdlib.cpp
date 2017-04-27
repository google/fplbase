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

using mathfu::vec2;
using mathfu::vec2i;

namespace fplbase {

void InputSystem::Initialize() {
  UpdateConnectedJoystickList();

  // Initialize time.
  start_time_ = 0;
  time_freq_ = 1000;
  elapsed_time_ = -0.02f;  // Ensure first frame doesn't get a crazy delta.
}

// Static member function. Not used in STDLIB.
int InputSystem::HandleAppEvents(void *userdata, void *ev) {
  (void)userdata;
  (void)ev;
  return 0;
}

void InputSystem::UpdateEvents(mathfu::vec2i *window_size) {
  (void)window_size;
  // TODO: Poll events from somewhere and fill the various button/pointer
  // structures in the input system.

  // for now, fake time progressing 16ms each frame.
  // TODO: remove this if you implement RealTime() properly.
  elapsed_time_ += 0.016;
}

void InputSystem::HandleJoystickEvent(Event event) {
  (void)event;
}

// Convert joystick hat enum values into more generic 2d vectors.
vec2 InputSystem::ConvertHatToVector(uint32_t hat_enum) const {
  (void)hat_enum;
  return vec2(0, 0);
}

double InputSystem::RealTime() const {
  return elapsed_time_;  // TODO: Get time from system.
}

void InputSystem::Delay(double seconds) const {
  (void)seconds;
}

bool InputSystem::RelativeMouseMode() const {
  return false;
}

void InputSystem::SetRelativeMouseMode(bool enabled) {
  (void)enabled;
}

size_t InputSystem::UpdateDragPosition(TouchFingerEvent event,
                                       uint32_t event_type,
                                       const vec2i &window_size) {
  (void)event;
  (void)event_type;
  (void)window_size;
  return 0;
}

void InputSystem::OpenConnectedJoysticks() {}
void InputSystem::CloseOpenJoysticks() {}

void InputSystem::StartTextInput() {}
void InputSystem::StopTextInput() {}

void InputSystem::SetTextInputRect(const mathfu::vec4 &input_rect) {
  (void)input_rect;
}

JoystickId Joystick::GetJoystickId() const {
  return 0;
}

int Joystick::GetNumButtons() const {
  return 0;
}

int Joystick::GetNumAxes() const {
  return 0;
}

int Joystick::GetNumHats() const {
  return 0;
}

}  // namespace fplbase
