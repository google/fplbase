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
#endif  // !defined(ANDROID_GAMEPAD) && defined(__ANDROID__)

#if !defined(ANDROID_HMD) && defined(__ANDROID__)
// Enable the android head mounted display (cardboard) code.  It receives
// events about Cardboard from java, via JNI, and creates a local
// representation of the state to be used.
#define ANDROID_HMD 1
#endif  //  !defined(ANDROID_HMD) && defined(__ANDROID__)

namespace fplbase {

typedef uint64_t FingerId;
typedef void *JoystickData;
typedef uint64_t JoystickId;

typedef void *Event;
typedef void *TouchFingerEvent;

#if ANDROID_GAMEPAD
typedef int AndroidInputDeviceId;
#endif

/// @class Button
/// @brief Used to record state for fingers, mousebuttons, keys and gamepad
///        buttons.
///
/// Allows you to know if a button went up/down this frame.
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
  mathfu::vec2i mousepos;
  mathfu::vec2i mousedelta;
  bool used;

  InputPointer() : id(0), mousepos(-1), mousedelta(0), used(false) {};
};

/// @class Joystick
/// @brief Represents the state of an SDL Joystick.
class Joystick {
 public:
  // Get a Button object for a pointer index.
  Button &GetButton(size_t button_index);
  float GetAxis(size_t axis_index);
  mathfu::vec2 GetHat(size_t hat_index);
  void SetAxis(size_t axis_index, float axis);
  void SetHat(size_t hat_index, const mathfu::vec2 &hat);
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
  std::vector<mathfu::vec2> hat_list_;
};

#if ANDROID_GAMEPAD
/// @class Gamepad
/// @brief Represents the state of a connected gamepad.
///
/// This is based on events passed in from java. Depends on ANDROID_GAMEPAD
/// being defined.
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

#if ANDROID_HMD
/// @class HeadMountedDisplayInput
/// @brief Represents the state of the device in a head mounted input device,
///        like Cardboard.
///
/// Manages the state of the device in a head mounted input device based on
/// events passed in from java, and read via JNI. Depends on ANDROID_HMD being
/// defined.
class HeadMountedDisplayInput {
 public:
  HeadMountedDisplayInput()
      : head_transform_(),
        left_eye_transform_(),
        right_eye_transform_(),
        is_in_head_mounted_display_(false),
        triggered_(false),
        pending_trigger_(false),
        use_device_orientation_correction_(false),
        device_orientation_(0) {}

  bool is_in_head_mounted_display() const {
    return is_in_head_mounted_display_;
  }
  void set_is_in_head_mounted_display(bool is_in_head_mounted_display) {
    is_in_head_mounted_display_ = is_in_head_mounted_display;
  }
  bool triggered() const { return triggered_; }

  const mathfu::mat4 &head_transform() const { return head_transform_; }
  const mathfu::mat4 &left_eye_transform() const { return left_eye_transform_; }
  const mathfu::mat4 &right_eye_transform() const {
    return right_eye_transform_;
  }

  // The rightwards direction of the head.
  mathfu::vec3 right() const {
    return (mathfu::kAxisX4f * head_transform_).xyz();
  }
  // The upwards direction of the head.
  mathfu::vec3 up() const { return (mathfu::kAxisY4f * head_transform_).xyz(); }
  // The forward direction of the head.  Note that it points into -Z.
  mathfu::vec3 forward() const {
    return (-mathfu::kAxisZ4f * head_transform_).xyz();
  }
  // The translation of the left eye
  mathfu::vec3 left_eye_translation() const {
    return (left_eye_transform_ * mathfu::kAxisW4f).xyz();
  }
  // The translation of the right eye
  mathfu::vec3 right_eye_translation() const {
    return (right_eye_transform_ * mathfu::kAxisW4f).xyz();
  }
  // The translation of the left eye, factoring in the Cardboard rotation
  mathfu::vec3 left_eye_rotated_translation() const {
    return (mathfu::vec4(left_eye_translation(), 0) * left_eye_transform_)
        .xyz();
  }
  // The translation of the right eye, factoring in the Cardboard rotation
  mathfu::vec3 right_eye_rotated_translation() const {
    return (mathfu::vec4(right_eye_translation(), 0) * right_eye_transform_)
        .xyz();
  }

  void AdvanceFrame();
  void OnTrigger() { pending_trigger_ = true; }

  // Realign the head tracking with the current phone heading
  void ResetHeadTracker();

  // Version 0.5.6 of the Cardboard SDK has a bug concerning not handling a
  // device's default orientation. Calling this enables correction of that in
  // the Cardboard Input.
  void EnableDeviceOrientationCorrection();

  void set_device_orientation(int rotation) { device_orientation_ = rotation; }
  int device_orientation() { return device_orientation_; }

 private:
  void UpdateTransforms();

  mathfu::mat4 head_transform_;
  mathfu::mat4 left_eye_transform_;
  mathfu::mat4 right_eye_transform_;
  bool is_in_head_mounted_display_;
  bool triggered_;
  bool pending_trigger_;
  // Whether correction should be applied to the view matrices.
  bool use_device_orientation_correction_;
  // The device's default rotation, as defined here:
  // http://developer.android.com/reference/android/view/Surface.html#ROTATION_0
  int device_orientation_;
  // The device's rotation the last time reset head tracker was called.
  int device_orientation_at_reset_;
};
#endif  // ANDROID_HMD

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

/// @class InputSystem
/// @brief Use to handle time, touch/mouse/keyboard/etc input, and lifecyle
///        events.
class InputSystem {
 public:
  /// @brief Construct an uninitialized InputSystem.
  InputSystem();
  ~InputSystem();

  static const int kMaxSimultanuousPointers = 10;  // All current touch screens.

  /// @brief Initialize the input system.
  ///
  /// Call this after SDL is initialized by the renderer.
  void Initialize();

  /// @brief Call once a frame to update the input state.
  ///
  /// Call this once a frame to process all new events and update the input
  /// state. The window_size argument may get updated whenever the window
  /// resizes.
  ///
  /// @param window_size The current window size of the application.
  void AdvanceFrame(mathfu::vec2i *window_size);

  /// @brief Get time in seconds since the start of the game. Updated once per
  ///        frame.
  ///
  /// This is the time you'd want to use for any gameplay simulation or
  /// animation the game does, such that you are in sync with what's
  /// rendered each frame.
  ///
  /// @return Return the time in seconds since the start of the game.
  double Time() const;

  /// @brief Get time in seconds since start of the game. Updated every call.
  ///
  /// Unlike Time(), it is recomputed every time it is called (slower).
  /// Mostly useful for profiling/benchmarking.
  ///
  /// @return Return the time in seconds since the start of the game.
  double RealTime() const;

  /// @brief The time in seconds since the last frame. Updated once per frame.
  ///
  /// @return Return the incremental time, in seconds.
  double DeltaTime() const;

  /// @brief Get a Button object describing the input state of the specified
  ///        button ID.
  ///
  /// @param button The ID of the button, see the SDLK_ enum.
  /// @return Returns the corresponding button.
  Button &GetButton(int button);

  /// @brief Checks if relative mouse mode is enabled.
  ///
  /// @return Returns the current state of relative mouse mode.
  bool RelativeMouseMode() const;
  /// @brief Enable / disable relative mouse mode (disabled by default).
  ///
  /// @param enabled The state to set relative mouse mode to.
  /// @note Relative mouse mode is currently ignored on Android devices.
  void SetRelativeMouseMode(bool enabled);

  /// @brief Get a Joystick object describing the input state of the specified
  ///        joystick ID.
  ///
  /// @param joystick_id The ID of the joystick, contained in every joystick
  ///        event.
  /// @return Returns the corresponding button.
  Joystick &GetJoystick(JoystickId joystick_id);

  /// @brief Get a map containing all currently connected joysticks.
  ///
  /// @return Returns the map of all joysticks.
  const std::map<JoystickId, Joystick> &JoystickMap() const {
    return joystick_map_;
  }

#if ANDROID_GAMEPAD
  /// @brief Get a Gamepad object describing the input state of the specified
  ///        device ID.
  ///
  /// Get the ID either from an android event, or by checking a known gamepad.
  ///
  /// @param gamepad_device_id The ID of the gamepad device.
  /// @return Returns the corresponding gamepad.
  Gamepad &GetGamepad(AndroidInputDeviceId gamepad_device_id);

  /// @brief Get a map containing all currently connected gamepads.
  ///
  /// @return Returns the map of all gamepads.
  const std::map<AndroidInputDeviceId, Gamepad> &GamepadMap() const {
    return gamepad_map_;
  }

  // Receives events from java, and stuffs them into a vector until we're ready.
  static void ReceiveGamepadEvent(int controller_id, int event_code,
                                  int control_code, float x, float y);

  // Runs through all the received events and processes them.
  void HandleGamepadEvents();
#endif  // ANDROID_GAMEPAD

#if ANDROID_HMD
  /// @brief Get the current input state of the Head Mounted Display device.
  ///
  /// @return Returns the current input state.
  HeadMountedDisplayInput &head_mounted_display_input() {
    return head_mounted_display_input_;
  }

  const HeadMountedDisplayInput &head_mounted_display_input() const {
    return head_mounted_display_input_;
  }
#endif  // ANDROID_HMD

  /// @brief Get a Button object for a pointer index.
  ///
  /// @param pointer The FingerId of the button.
  /// @return Returns the corresponding button.
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

  /// @brief Most recent frame at which we were minimized or maximized.
  int minimized_frame() const { return minimized_frame_; }
  void set_minimized_frame(int minimized_frame) {
    minimized_frame_ = minimized_frame;
  }

  /// @brief The total number of frames elapsed so far.
  int frames() const { return frames_; }
  /// @brief Accumulated mousewheel delta since the previous frame.
  mathfu::vec2i mousewheel_delta() { return mousewheel_delta_; }

  /// @brief Start/Stop recording text input events.
  ///
  /// Recorded event can be retrieved by GetTextInputEvents().
  void RecordTextInput(bool b) {
    record_text_input_ = b;
    if (!b) text_input_events_.clear();
  }
  /// @brief Checks if text input is being recorded.
  bool IsRecordingTextInput() { return record_text_input_; }

  /// @brief Retrieve a vector of text input events.
  ///
  /// The caller uses this API to retrieve text input related events
  /// (all key down/up events, keyboard input and IME's intermediate states)
  /// and use them to edit and display texts.
  /// To start/stop a recording, call RecordTextInput() API.
  ///
  /// @return Returns a vector containing text input events.
  const std::vector<TextInputEvent> *GetTextInputEvents();

  /// @brief Clear the recorded text input events.
  ///
  /// The user needs to call the API once they have handled input events.
  void ClearTextInputEvents() { text_input_events_.clear(); }

  // Text input related APIs.

  /// @brief Start a text input.
  ///
  /// In mobile devices, it may show a software keyboard on the screen.
  void StartTextInput();

  /// @brief Stop a text input.
  ///
  /// In mobile devices, it may dismiss a software keyboard.
  void StopTextInput();

  /// @brief Indicates a text input region to IME(Input Method Editor).
  ///
  /// @param input_rect The input region rectangle.
  void SetTextInputRect(const mathfu::vec4 &input_rect);

  /// @brief Gets the vector of all the input pointers in the system.
  const std::vector<InputPointer> &get_pointers() { return pointers_; }

  /// @brief Gets if the application is currently minimized.
  bool minimized() { return minimized_; }
  /// @brief Sets if the application is currently minimized.
  void set_minimized(bool b) { minimized_ = b; }

  /// @brief Gets if exit has been requested by the system.
  bool exit_requested() { return exit_requested_; }

 private:
  // Reset pointer/gamepad input state to released state.
  void ResetInputState();

  static const int kMillisecondsPerSecond = 1000;

  static int HandleAppEvents(void *userdata, void *event);

  bool exit_requested_;
  bool minimized_;
  std::vector<InputPointer> pointers_;
  std::vector<JoystickData> open_joystick_list;
  size_t FindPointer(FingerId id);
  size_t UpdateDragPosition(TouchFingerEvent e, uint32_t event_type,
                            const mathfu::vec2i &window_size);
  void RemovePointer(size_t i);
  mathfu::vec2 ConvertHatToVector(uint32_t hat_enum) const;
  std::vector<AppEventCallback> app_event_callbacks_;
  std::map<int, Button> button_map_;
  std::map<JoystickId, Joystick> joystick_map_;

#if ANDROID_GAMEPAD
  std::map<AndroidInputDeviceId, Gamepad> gamepad_map_;
  static pthread_mutex_t android_event_mutex;
  static std::queue<AndroidInputEvent> unhandled_java_input_events_;
#endif  // ANDROID_GAMEPAD

#if ANDROID_HMD
  // Head mounted display input class.
  HeadMountedDisplayInput head_mounted_display_input_;
#endif  // ANDROID_HMD

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
  mathfu::vec2i mousewheel_delta_;

  // Event queue for a text input events.
  std::vector<TextInputEvent> text_input_events_;

  // A flag indicating a text input status.
  bool record_text_input_;

  // True if most recent pointer events are coming from a touch screen,
  // false if coming from a mouse or similar.
  bool touch_device_;

#ifdef __ANDROID__
  // Store current relative mouse mode before entering background.
  bool relative_mouse_mode_;

  // How long since we've sent a keypress event to keep the CPU alive.
  double last_android_keypress_;
#endif  // __ANDROID__
};

}  // namespace fplbase

#endif  // FPLBASE_INPUT_SYSTEM_H
