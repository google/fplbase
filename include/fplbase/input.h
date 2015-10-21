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

#include <functional>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "fplbase/config.h"  // Must come first.

#include "keyboard_keycodes.h"
#include "mathfu/constants.h"
#include "mathfu/glsl_mappings.h"

#if !defined(ANDROID_GAMEPAD) && defined(__ANDROID__)
// Enable the android gamepad code.  It receives input events from java, via
// JNI, and creates a local representation of the state of any connected
// gamepads.  Also enables the gamepad_controller controller class.
#define ANDROID_GAMEPAD 1
#include "pthread.h"
#endif

#if !defined(ANDROID_CARDBOARD) && defined(__ANDROID__)
// Enable the android cardboard code.  It receives events about Cardboard from
// java, via JNI, and creates a local representation of the state to be used.
#define ANDROID_CARDBOARD 1
#endif

namespace fpl {

typedef uint64_t FingerId;
typedef void *JoystickData;
typedef uint64_t JoystickId;

typedef void *Event;
typedef void *TouchFingerEvent;

using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;
using mathfu::mat4;

#if ANDROID_GAMEPAD
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
struct InputPointer {
  FingerId id;
  vec2i mousepos;
  vec2i mousedelta;
  bool used;

  InputPointer() : id(0), mousepos(-1), mousedelta(0), used(false){};
};

class Joystick {
 public:
  // Get a Button object for a pointer index.
  Button &GetButton(size_t button_index);
  float GetAxis(size_t axis_index);
  vec2 GetHat(size_t hat_index);
  void SetAxis(size_t axis_index, float axis);
  void SetHat(size_t hat_index, const vec2 &hat);
  void AdvanceFrame();
  JoystickData joystick_data() { return joystick_data_; }
  void set_joystick_data(JoystickData joy) { joystick_data_ = joy; }
  JoystickId GetJoystickId() const;
  int GetNumButtons() const;
  int GetNumAxes() const;
  int GetNumHats() const;

 private:
  JoystickData joystick_data_;
  std::vector<float> axis_list_;
  std::vector<Button> button_list_;
  std::vector<vec2> hat_list_;
};

#if ANDROID_GAMEPAD
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

#if ANDROID_CARDBOARD
// Cardboard input class.  Manages the state of the device in cardboard
// based on events passed in from java, and read via JNI.
class CardboardInput {
 public:
  CardboardInput()
      : left_eye_transform_(),
        right_eye_transform_(),
        is_in_cardboard_(false),
        triggered_(false),
        pending_trigger_(false) {}

  bool is_in_cardboard() const { return is_in_cardboard_; }
  void set_is_in_cardboard(bool is_in_cardboard) {
    is_in_cardboard_ = is_in_cardboard;
  }
  bool triggered() const { return triggered_; }

  const mat4 &left_eye_transform() const { return left_eye_transform_; }
  const mat4 &right_eye_transform() const { return right_eye_transform_; }

  // The rightwards direction of the head.
  vec3 right() const { return (mathfu::kAxisX4f * left_eye_transform_).xyz(); }
  // The upwards direction of the head.
  vec3 up() const { return (mathfu::kAxisY4f * left_eye_transform_).xyz(); }
  // The forward direction of the head.  Note that it points into -Z.
  vec3 forward() const {
    return (-mathfu::kAxisZ4f * left_eye_transform_).xyz();
  }
  // The translation of the left eye
  vec3 left_eye_translation() const {
    return (left_eye_transform_ * mathfu::kAxisW4f).xyz();
  }
  // The translation of the right eye
  vec3 right_eye_translation() const {
    return (right_eye_transform_ * mathfu::kAxisW4f).xyz();
  }
  // The translation of the left eye, factoring in the Cardboard rotation
  vec3 left_eye_rotated_translation() const {
    return (vec4(left_eye_translation(), 0) * left_eye_transform_).xyz();
  }
  // The translation of the right eye, factoring in the Cardboard rotation
  vec3 right_eye_rotated_translation() const {
    return (vec4(right_eye_translation(), 0) * right_eye_transform_).xyz();
  }

  void AdvanceFrame();
  void OnCardboardTrigger() { pending_trigger_ = true; }

  // Realign the head tracking with the current phone heading
  void ResetHeadTracker();

 private:
  void UpdateCardboardTransforms();

  mat4 left_eye_transform_;
  mat4 right_eye_transform_;
  bool is_in_cardboard_;
  bool triggered_;
  bool pending_trigger_;
};
#endif  // ANDROID_CARDBOARD

// Text input structures and enums.

// Text input event type.
enum TextInputEventType {
  kTextInputEventTypeEdit = 0,  // An event for a text edit in IME.
  kTextInputEventTypeText = 1,  // An event for a text input.
  kTextInputEventTypeKey = 2,   // An event for a key event.
};

// An event parameters for a text edit in IME (Input Method Editor).
// The information passed in the event is an intermediate state and only used
// for UI represents. Once IME finalizes an edit, the user recieves an
// TextInputEventText event for the finalized strings.
struct TextInputEventEdit {
  int32_t start;   // A start index of a focus region in the text.
  int32_t length;  // A length of a focus region in the text.
};

// An event parameters for a keyboard input.
// The user recieves all input strings through kTextInputEventTypeText type of
// an event, these paremeters should be used for an input control such as moving
// caret.
struct TextInputEventKey {
  bool state;           // key state, true:pressed, false:released.
  bool repeat;          // A flag indicates if the key is repeated input.
  FPL_Keycode symbol;   // Key symbol, refer keyboard_keycodes.h for a
                        // detail.
  FPL_Keymod modifier;  // Modifier key state, refer keyboard_keycodes.h for
                        // a detail.
};

// Union of Text input event.
struct TextInputEvent {
  TextInputEventType type;  // Type of the event.
  std::string text;         // Input string.
  union {
    TextInputEventKey key;
    TextInputEventEdit edit;
  };
  // Constructors to emblace_back() them.
  TextInputEvent(TextInputEventType t);
  TextInputEvent(TextInputEventType t, int32_t state, bool repeat,
                 int32_t symbol, int32_t modifier);
  TextInputEvent(TextInputEventType t, const char *str);
  TextInputEvent(TextInputEventType t, const char *str, int32_t start,
                 int32_t length);
};

class InputSystem {
 public:
  InputSystem()
      : exit_requested_(false),
        minimized_(false),
        frame_time_(0),
        elapsed_time_(0),
        start_time_(0),
        time_freq_(0),
        frames_(0),
        minimized_frame_(0),
        mousewheel_delta_(mathfu::kZeros2i),
        record_text_input_(false) {
    pointers_.assign(kMaxSimultanuousPointers, InputPointer());
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
  double Time() const;
  double DeltaTime() const;

  // Get a Button object describing the current input state (see SDLK_ enum
  // above.
  Button &GetButton(int button);

  bool RelativeMouseMode() const;
  void SetRelativeMouseMode(bool enabled);

  // Get a joystick object describing the current input state of the specified
  // joystick ID.  (Contained in every joystick event.)
  Joystick &GetJoystick(JoystickId joystick_id);

  const std::map<JoystickId, Joystick> &JoystickMap() const {
    return joystick_map_;
  }

#if ANDROID_GAMEPAD
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

#if ANDROID_CARDBOARD
  CardboardInput &cardboard_input() const { return cardboard_input_; }

  static void OnCardboardTrigger();
  static void SetDeviceInCardboard(bool in_cardboard);
#endif  // ANDROID_CARDBOARD

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
  vec2i mousewheel_delta() { return mousewheel_delta_; }

  // Start/Stop recording text input events.
  // Recorded event can be retrieved by GetTextInputEvents().
  void RecordTextInput(bool b) {
    record_text_input_ = b;
    if (!b) text_input_events_.clear();
  }
  bool IsRecordingTextInput() { return record_text_input_; }

  // Retrieve a vector of text input events.
  // The caller uses this API to retrieve text input related events
  // (all key down/up events, keyboard input and IME's intermediate states)
  // and use them to edit and display texts.
  // To start/stop a recording, call RecordTextInput() API.
  const std::vector<TextInputEvent> *GetTextInputEvents();

  // Clear a recorded text input events. The user needs to call the API once
  // it handled input events.
  void ClearTextInputEvents() { text_input_events_.clear(); }

  // Text input related APIs.

  // Start a text input.
  // In mobile devices, it may show a software keyboard on the screen.
  void StartTextInput();

  // Stop a text input.
  // In mobile devices, it may dismiss a software keyboard.
  void StopTextInput();

  // Indicates a text input region to IME(Input Method Editor).
  void SetTextInputRect(const mathfu::vec4 &input_rect);

  // Accessors for instance variables.
  const std::vector<InputPointer> &get_pointers() { return pointers_; }

  bool minimized() { return minimized_; }
  void set_minimized(bool b) { minimized_ = b; }

  bool exit_requested() { return exit_requested_; }

 private:
  static const int kMillisecondsPerSecond = 1000;

  static int HandleAppEvents(void *userdata, void *event);

  bool exit_requested_;
  bool minimized_;
  std::vector<InputPointer> pointers_;
  std::vector<JoystickData> open_joystick_list;
  size_t FindPointer(FingerId id);
  size_t UpdateDragPosition(TouchFingerEvent e, uint32_t event_type,
                            const vec2i &window_size);
  void RemovePointer(size_t i);
  vec2 ConvertHatToVector(uint32_t hat_enum) const;
  std::vector<AppEventCallback> app_event_callbacks_;
  std::map<int, Button> button_map_;
  std::map<JoystickId, Joystick> joystick_map_;

#if ANDROID_GAMEPAD
  std::map<AndroidInputDeviceId, Gamepad> gamepad_map_;
  static pthread_mutex_t android_event_mutex;
  static std::queue<AndroidInputEvent> unhandled_java_input_events_;
#endif  // ANDROID_GAMEPAD

#if ANDROID_CARDBOARD
  static CardboardInput cardboard_input_;
#endif

  // Most recent frame delta, in seconds.
  double frame_time_;

  // Time since start, in seconds.
  double elapsed_time_;

  // World time at start, in seconds.
  uint64_t start_time_;

  // Timer frequency.
  uint64_t time_freq_;

  // Number of frames so far. That is, number of times AdvanceFrame() has been
  // called.
  int frames_;

  // Most recent frame at which we were minimized or maximized.
  int minimized_frame_;

  // Accumulated mousewheel delta since the last frame.
  vec2i mousewheel_delta_;

  // Event queue for a text input events.
  std::vector<TextInputEvent> text_input_events_;

  // A flag indicating a text input status.
  bool record_text_input_;

#ifdef __ANDROID__
  // Store current relative mouse mode before entering background.
  bool relative_mouse_mode_;

  // How long since we've sent a keypress event to keep the CPU alive.
  int32_t last_android_keypress_;
#endif
};

}  // namespace fpl

#endif  // FPLBASE_INPUT_SYSTEM_H
