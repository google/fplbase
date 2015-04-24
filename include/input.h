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

#ifndef FPLBASE_INPUT_SYSTEM_H
#define FPLBASE_INPUT_SYSTEM_H

#include <queue>
#include <map>
#include "keyboard_keycodes.h"
#include "mathfu/constants.h"
#include "mathfu/glsl_mappings.h"

#ifdef __ANDROID__
// Enable the android gamepad code.  It receives input events from java, via
// JNI, and creates a local representation of the state of any connected
// gamepads.  Also enables the gamepad_controller controller class.
#define ANDROID_GAMEPAD
#include "pthread.h"
#endif

namespace fpl {

typedef uint64_t FingerId;
typedef void *JoystickData;
typedef uint64_t JoystickId;

typedef void *Event;
typedef void *TouchFingerEvent;

using mathfu::vec2;
using mathfu::vec2i;

#ifdef ANDROID_GAMEPAD
typedef int AndroidInputDeviceId;
#endif

// Used to record state for fingers, mousebuttons, keys and gamepad buttons.
// Allows you to know if a button went up/down this frame.
class Button {
 public:
  Button() : is_down_(false) { AdvanceFrame(); }
  void AdvanceFrame() { went_down_ = went_up_ = false; }
  void Update(bool down);

  bool is_down() const { return is_down_; }
  bool went_down() const { return went_down_; }
  bool went_up() const { return went_up_; }

 private:
  bool is_down_;
  bool went_down_;
  bool went_up_;
};

// This enum extends the FPL keycodes, which are normally positive values.
// Negative values will represent finger / mouse and gamepad buttons.
// `button_map` below maps from one of these values to a Button.
enum {
  K_POINTER1 = -10,  // Left mouse or first finger down.
  K_POINTER2,        // Right mouse or second finger.
  K_POINTER3,        // Middle mouse or third finger.
  K_POINTER4,
  K_POINTER5,
  K_POINTER6,
  K_POINTER7,
  K_POINTER8,
  K_POINTER9,
  K_POINTER10,

  K_PAD_UP = -20,
  K_PAD_DOWN,
  K_PAD_LEFT,
  K_PAD_RIGHT,
  K_PAD_A,
  K_PAD_B
};

// Additional information stored for the pointer buttons.
struct Pointer {
  FingerId id;
  vec2i mousepos;
  vec2i mousedelta;
  bool used;

  Pointer() : id(0), mousepos(-1), mousedelta(0), used(false){};
};

// Used to record state for axes
class JoystickAxis {
 public:
  JoystickAxis() : value_(0), previous_value_(0) {}
  void AdvanceFrame() { previous_value_ = value_; }
  void Update(float new_value) { value_ = new_value; }
  float Value() const { return value_; }
  float PreviousValue() const { return previous_value_; }

 private:
  float value_;           // current value
  float previous_value_;  // value last update
};

// Used to record state for hats
class JoystickHat {
 public:
  JoystickHat() : value_(mathfu::kZeros2f), previous_value_(mathfu::kZeros2f) {}
  void AdvanceFrame() { previous_value_ = value_; }
  void Update(const vec2 &new_value) { value_ = new_value; }
  const vec2 &Value() const { return value_; }
  vec2 PreviousValue() const { return previous_value_; }

 private:
  vec2 value_;           // current value
  vec2 previous_value_;  // value last update
};

class Joystick {
 public:
  // Get a Button object for a pointer index.
  Button &GetButton(size_t button_index);
  JoystickAxis &GetAxis(size_t axis_index);
  JoystickHat &GetHat(size_t hat_index);
  void AdvanceFrame();
  JoystickData joystick_data() { return joystick_data_; }
  void set_joystick_data(JoystickData joy) { joystick_data_ = joy; }
  JoystickId GetJoystickId() const;
  int GetNumButtons() const;
  int GetNumAxes() const;
  int GetNumHats() const;

 private:
  JoystickData joystick_data_;
  std::vector<JoystickAxis> axis_list_;
  std::vector<Button> button_list_;
  std::vector<JoystickHat> hat_list_;
};

#ifdef ANDROID_GAMEPAD
// Gamepad input class.  Represents the state of a connected gamepad, based on
// events passed in from java.
class Gamepad {
 public:
  enum GamepadInputButton : int {
    kInvalid = -1,
    kUp = 0,
    kDown,
    kLeft,
    kRight,
    kButtonA,
    kButtonB,
    kButtonC,

    kControlCount
  };

  Gamepad() { button_list_.resize(Gamepad::kControlCount); }

  void AdvanceFrame();
  Button &GetButton(GamepadInputButton i);
  const Button &GetButton(GamepadInputButton i) const {
    return const_cast<Gamepad *>(this)->GetButton(i);
  }

  AndroidInputDeviceId controller_id() { return controller_id_; }
  void set_controller_id(AndroidInputDeviceId controller_id) {
    controller_id_ = controller_id;
  }

  static int GetGamepadCodeFromJavaKeyCode(int java_keycode);

 private:
  AndroidInputDeviceId controller_id_;
  std::vector<Button> button_list_;
};

const float kGamepadHatThreshold = 0.5f;

// Structure used for storing gamepad events when we get them from jni
// until we can deal with them.
struct AndroidInputEvent {
  AndroidInputEvent() {}
  AndroidInputEvent(AndroidInputDeviceId device_id_, int event_code_,
                    int control_code_, float x_, float y_)
      : device_id(device_id_),
        event_code(event_code_),
        control_code(control_code_),
        x(x_),
        y(y_) {}
  AndroidInputDeviceId device_id;
  int event_code;
  int control_code;
  float x, y;
};
#endif  // ANDROID_GAMEPAD

class InputSystem {
 public:
  InputSystem()
      : exit_requested_(false),
        minimized_(false),
        frame_time_(0),
        last_millis_(0),
        start_time_(0),
        frames_(0),
        minimized_frame_(0) {
    pointers_.assign(kMaxSimultanuousPointers, Pointer());
  }

  static const int kMaxSimultanuousPointers = 10;  // All current touch screens.

  // Initialize the input system. Call this after SDL is initialized by
  // the renderer.
  void Initialize();

  // Call this once a frame to process all new events and update the input
  // state. The window_size argument may get updated whenever the window
  // resizes.
  void AdvanceFrame(vec2i *window_size);

  // Get time in second since the start of the game, or since the last frame.
  float Time() const;
  float DeltaTime() const;

  // Get a Button object describing the current input state (see SDLK_ enum
  // above.
  Button &GetButton(int button);

  // Get a joystick object describing the current input state of the specified
  // joystick ID.  (Contained in every joystick event.)
  Joystick &GetJoystick(JoystickId joystick_id);

  const std::map<JoystickId, Joystick> &JoystickMap() const {
    return joystick_map_;
  }

#ifdef ANDROID_GAMEPAD
  // Returns an object describing a gamepad, based on the android device ID.
  // Get the ID either from an android event, or by checking a known gamepad.
  Gamepad &GetGamepad(AndroidInputDeviceId gamepad_device_id);

  const std::map<AndroidInputDeviceId, Gamepad> &GamepadMap() const {
    return gamepad_map_;
  }

  // Receives events from java, and stuffs them into a vector until we're ready.
  static void ReceiveGamepadEvent(int controller_id, int event_code,
                                  int control_code, float x, float y);

  // Runs through all the received events and processes them.
  void HandleGamepadEvents();
#endif  // ANDROID_GAMEPAD

  // Get a Button object for a pointer index.
  Button &GetPointerButton(FingerId pointer) {
    return GetButton(static_cast<int>(pointer + K_POINTER1));
  }

  void OpenConnectedJoysticks();
  void CloseOpenJoysticks();
  void UpdateConnectedJoystickList();
  void HandleJoystickEvent(Event event);

  typedef std::function<void(Event)> AppEventCallback;
  std::vector<AppEventCallback> &app_event_callbacks() {
    return app_event_callbacks_;
  }
  void AddAppEventCallback(AppEventCallback callback);

  int minimized_frame() const { return minimized_frame_; }
  void set_minimized_frame(int minimized_frame) {
    minimized_frame_ = minimized_frame;
  }

  int frames() const { return frames_; }

 private:
  std::vector<JoystickData> open_joystick_list;
  size_t FindPointer(FingerId id);
  size_t UpdateDragPosition(TouchFingerEvent e, uint32_t event_type,
                            const vec2i &window_size);
  void RemovePointer(size_t i);
  vec2 ConvertHatToVector(uint32_t hat_enum) const;
  std::vector<AppEventCallback> app_event_callbacks_;

 public:
  bool exit_requested_;
  bool minimized_;
  std::vector<Pointer> pointers_;

 private:
  std::map<int, Button> button_map_;
  std::map<JoystickId, Joystick> joystick_map_;

#ifdef ANDROID_GAMEPAD
  std::map<AndroidInputDeviceId, Gamepad> gamepad_map_;
  static pthread_mutex_t android_event_mutex;
  static std::queue<AndroidInputEvent> unhandled_java_input_events_;
#endif  // ANDROID_GAMEPAD

  // Most recent frame delta, in milliseconds.
  int frame_time_;

  // Time since start, in milliseconds.
  int last_millis_;

  // World time at start, in milliseconds. Set with SDL_GetTicks().
  int start_time_;

  // Number of frames so far. That is, number of times AdvanceFrame() has been
  // called.
  int frames_;

  // Most recent frame at which we were minimized or maximized.
  int minimized_frame_;

 public:
  static const int kMillisecondsPerSecond = 1000;
};

}  // namespace fpl

#endif  // FPLBASE_INPUT_SYSTEM_H
