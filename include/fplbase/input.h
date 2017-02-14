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

#ifdef __ANDROID__
#include <jni.h>
#endif

#if !defined(ANDROID_GAMEPAD) && defined(__ANDROID__) && \
    !defined(FPLBASE_BACKEND_STDLIB)
// Enable the android gamepad code.  It receives input events from java, via
// JNI, and creates a local representation of the state of any connected
// gamepads.  Also enables the gamepad_controller controller class.
#define ANDROID_GAMEPAD 1
#include "pthread.h"
#endif  // !defined(ANDROID_GAMEPAD) && defined(__ANDROID__)

namespace fplbase {

/// @file
/// @addtogroup fplbase_input
/// @{

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
  /// @brief Advances the current state of the button by one frame.
  ///
  /// Important, because it tells the system where frame boundaries occur, so
  /// that went_down() and went_up() can be updated correctly.
  /// Normally called automatically by FPLBase.
  void AdvanceFrame() { went_down_ = went_up_ = false; }

  /// @brief Updates the current state of the button.
  ///
  /// For buttons that are tracked by the input system (Keyboard buttons,
  /// joysticks) this is invoked automatically by InputSystem::AdvanceFrame().)
  void Update(bool down);

  /// @brief Returns true if the button is currently pressed.
  bool is_down() const { return is_down_; }

  /// @brief Returns true if the button has been pressed since last update.
  bool went_down() const { return went_down_; }

  /// @brief Returns true if the button has been released since last update.
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

/// @class InputPointer
/// @brief Stores information about the current and recent state of a pointer.
///
/// An input pointer represents either a finger-touch on a touchscreen device,
/// or a mouse-pointer.
// Additional information stored for the pointer buttons.
struct InputPointer {
  /// @brief The pointer's ID
  ///
  /// The mouse pointer always has a pointerID of 0.  For fingertouches, they
  /// are generally allocated in order as new touches happen.
  FingerId id;

  /// @brief The position of the pointer, in pixels.
  mathfu::vec2i mousepos;

  /// @brief The amount the pointer moved since the last update.
  mathfu::vec2i mousedelta;

  /// @brief Whether this particular pointer is in use.
  ///
  /// When used is false, the pointer data is in an undefined state and
  /// should be ignored.
  bool used;

  InputPointer() : id(0), mousepos(-1), mousedelta(0), used(false){};
};

/// @class Joystick
/// @brief Represents the state of a Joystick.
class Joystick {
 public:
  /// @brief Get the current state of a button.
  /// @param button_index The index of the button to querry.
  /// @return A button object representing the current button state.
  Button &GetButton(size_t button_index);

  /// @brief Get the current state of a joystick axis control.
  /// @param axis_index The index of the axis to querry.
  /// @return A float representing the position of the axis control.  Axis data
  /// is automatically normalized to be within the range of [-1, 1].
  float GetAxis(size_t axis_index);

  /// @brief Get the current state of a joystick hat control.
  /// @param hat_index The index of the hat to querry.
  /// @return A vec2 representing the direction the hat is pointing.
  ///
  /// The returned vector will always have an X, Y component of -1, 0, or 1.
  /// (And thus will always be pointing in one of 8 possible directions, or
  /// in the neutral position.)
  mathfu::vec2 GetHat(size_t hat_index);

  /// @brief Sets the value of an axis
  /// @param axis_index The index of the axis to modify.
  /// @param axis The new value to record for the axis.
  ///
  /// Normally only called by input handlers in the Input class.
  void SetAxis(size_t axis_index, float axis);

  /// @brief Sets the position of a hat.
  /// @param hat_index The index of the hat to modify.
  /// @param hat The new value to record for the hat.
  ///
  /// Normally only called by input handlers in the Input class.
  void SetHat(size_t hat_index, const mathfu::vec2 &hat);

  /// @brief Updates the internal state of the joystick by one frame.
  ///
  /// This function ensures that the joystick readings are up-to-date, and that
  /// buttons have their change-bits set correctly.  Normally called
  /// automatically by InputSystem::AdvanceFrame().
  void AdvanceFrame();

  /// @brief Returns the pointer to the raw joystick data.
  ///
  /// The type and layout of the joystick_data are implementation specific,
  /// and should not generally be used by code outside of the FPLBase library.
  JoystickData joystick_data() { return joystick_data_; }

  /// @brief Sets the pointer to the raw joystick data.
  /// @param joy A pointer to the joystick data.
  ///
  /// The type and layout of the joystick_data are implementation specific,
  /// and should not generally be used by code outside of the FPLBase library.
  void set_joystick_data(JoystickData joy) { joystick_data_ = joy; }

  /// @brief Gets the ID of the joystick.
  ///
  /// Joystick IDs are guaranteed to be unique between joysticks, and on some
  /// platforms, will be preserved even if the same controller is unplugged and
  /// reconnected.  (Platform dependent.)  The Joystick ID is used to reference
  /// a particular device.
  JoystickId GetJoystickId() const;

  /// @brief Returns the number of buttons available on the joystick.
  int GetNumButtons() const;

  /// @brief Returns the number of control axes available on the joystick.
  int GetNumAxes() const;

  /// @brief Returns the number of hats available on the joystick.
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
/// Gamepads are an android-specific abstraction for controllers that are
/// specifically gamepad-like.  (They have a d-pad and one or more buttons.)
/// This is only present if ANDROID_GAMEPAD is defined.
class Gamepad {
 public:
  /// @brief Enum describing all possible button inputs on a gamepad.
  enum GamepadInputButton : int {
    kInvalid = -1,
    kUp = 0,
    kDown,
    kLeft,
    kRight,
    kButtonA,
    kButtonB,
    kButtonC,
    kButtonX,
    kButtonY,
    kButtonZ,
    kButtonL1,
    kButtonR1,
    kButtonL2,
    kButtonR2,
    kButtonThumbL,
    kButtonThumbR,
    kButtonBack,
    kButtonStart,
    kButtonSelect,
    kButtonMode,
    kControlCount
  };

  /// @brief Default constructor for a Gamepad.
  Gamepad() { button_list_.resize(Gamepad::kControlCount); }

  /// @brief Advances the internal state by one frame.
  ///
  /// This function ensures that the gamepad readings are up-to-date, and that
  /// buttons have their change-bits set correctly.  Normally called
  /// automatically by InputSystem::AdvanceFrame().
  void AdvanceFrame();

  /// @brief Get the Button from a given enum of all possible input buttons.
  /// @param[in] i A GamepadInputButton enum describing all of the possible
  /// button inputs on a gamepad.
  Button &GetButton(GamepadInputButton i);

  /// @brief Get the Button from a given enum of all possible input buttons.
  /// @param[in] i A GamepadInputButton enum describing all of the possible
  /// button inputs on a gamepad.
  const Button &GetButton(GamepadInputButton i) const {
    return const_cast<Gamepad *>(this)->GetButton(i);
  }

  /// @brief Returns the Android controller_id of the gamepad
  ///
  /// The controller_id is the android-specific identifier for a given
  /// gamepad.
  AndroidInputDeviceId controller_id() { return controller_id_; }

  /// @brief Set the controller ID.
  /// @param[in] controller_id An AndroidInputDeviceId to set as the controller
  /// ID.
  void set_controller_id(AndroidInputDeviceId controller_id) {
    controller_id_ = controller_id;
  }

  /// @brief Internal function for translating android input.
  static int GetGamepadCodeFromJavaKeyCode(int java_keycode);

 private:
  AndroidInputDeviceId controller_id_;
  std::vector<Button> button_list_;
};

// Threshold for when we register a hat direction.  (The range is [0, 1]
const float kGamepadHatThreshold = 0.5f;

/// @brief Structure used for storing gamepad events when we get them from jni
/// until we can handle them.
struct AndroidInputEvent {
  /// @brief AndroidInputEvent default constructor.
  AndroidInputEvent() {}
  /// @brief Constructor for an AndroidInputeEvent.
  /// @param[in] device_id_ The device ID of the Android device.
  /// @param[in] event_code_ The event code for the gamepad event.
  /// @param[in] control_code_ The control code for the gamepad event.
  /// @param[in] x_ The x position of the event.
  /// @param[in] y_ The y position for the event.
  AndroidInputEvent(AndroidInputDeviceId device_id_, int event_code_,
                    int control_code_, float x_, float y_)
      : device_id(device_id_),
        event_code(event_code_),
        control_code(control_code_),
        x(x_),
        y(y_) {}
  /// @brief The device ID of the Android device.
  AndroidInputDeviceId device_id;
  /// @brief The event code.
  int event_code;
  /// @brief The control code.
  int control_code;
  /// @brief The `x` coordinate for the event.
  float x;
  /// @brief The `y` coordinate for the event.
  float y;
};
#endif  // ANDROID_GAMEPAD

#if FPLBASE_ANDROID_VR
/// @class HeadMountedDisplayInput
/// @brief Represents the state of the device in a head mounted input device,
///        like Cardboard.
///
/// Manages the state of the device in a head mounted input device based on
/// events passed in from java, and read via JNI. Depends on FPLBASE_ANDROID_VR being
/// defined.
class HeadMountedDisplayInput {
 public:
  /// @brief Default constructor for HeadMountedDisplayInput.
  HeadMountedDisplayInput()
      : head_transform_(),
        left_eye_transform_(),
        right_eye_transform_(),
        is_in_head_mounted_display_(false),
        triggered_(false),
        pending_trigger_(false),
        use_device_orientation_correction_(false),
        device_orientation_(0) {}

  /// @brief Check if the device is in head mounted display mode.
  /// @return Returns `true` if `set_is_in_head_mounted_display()` was set
  /// to `true`.
  bool is_in_head_mounted_display() const {
    return is_in_head_mounted_display_;
  }

  /// @brief Set whether or not hte device is in head mounted display mode.
  /// @param[in] is_in_head_mounted_display A `bool` determining if the
  /// device is set to be in head mounted display mode.
  void set_is_in_head_mounted_display(bool is_in_head_mounted_display) {
    is_in_head_mounted_display_ = is_in_head_mounted_display;
  }

  /// @brief Check if triggered.
  /// @return Returns `true` if the a pending trigger was handled in
  /// `AdvanceFrame`.
  bool triggered() const { return triggered_; }

  /// @brief Get the head transform.
  /// @return Returns the head transform as a const `mathfu::mat4` reference.
  const mathfu::mat4 &head_transform() const { return head_transform_; }

  /// @brief Get the left eye transform.
  /// @return Returns the left eye transform as a const `mathfu::mat4`
  /// reference.
  const mathfu::mat4 &left_eye_transform() const { return left_eye_transform_; }

  /// @brief Get the right eye transform.
  /// @return Returns the right eye transform as a const `mathfu::mat4`
  /// reference.
  const mathfu::mat4 &right_eye_transform() const {
    return right_eye_transform_;
  }

  /// @brief The rightwards direction of the head.
  /// @return Returns the rightwards direction of the head as a `mathfu::vec3`.
  mathfu::vec3 right() const {
    return (mathfu::kAxisX4f * head_transform_).xyz();
  }
  /// @brief The upwards direction of the head.
  /// @return Returns the upwards direction of the head as a `mathfu::vec3`.
  mathfu::vec3 up() const { return (mathfu::kAxisY4f * head_transform_).xyz(); }

  /// @brief The forward direction of the head.
  /// @note It points into -Z.
  /// @return Returns the forward direction of the head as a `mathfu::vec3`.
  mathfu::vec3 forward() const {
    return (-mathfu::kAxisZ4f * head_transform_).xyz();
  }

  /// @brief The translation of the left eye.
  /// @return Returns the left eye translation as a `mathfu::vec`.
  mathfu::vec3 left_eye_translation() const {
    return (left_eye_transform_ * mathfu::kAxisW4f).xyz();
  }

  /// @brief The translation of the right eye.
  /// @return Returns the right eye translation as a `mathfu::vec`.
  mathfu::vec3 right_eye_translation() const {
    return (right_eye_transform_ * mathfu::kAxisW4f).xyz();
  }

  /// @brief The translation of the left eye, factoring in the Cardboard
  /// rotation.
  /// @return Returns the left eye translation, factoring in the Cardboard
  /// rotation.
  mathfu::vec3 left_eye_rotated_translation() const {
    return (mathfu::vec4(left_eye_translation(), 0) * left_eye_transform_)
        .xyz();
  }

  /// @brief The translation of the right eye, factoring in the Cardboard
  /// rotation.
  /// @return Returns the right eye translation, factoring in the Cardboard
  /// rotation.
  mathfu::vec3 right_eye_rotated_translation() const {
    return (mathfu::vec4(right_eye_translation(), 0) * right_eye_transform_)
        .xyz();
  }

  /// @brief Called once per frame to handle the input.
  void AdvanceFrame();

  /// @brief Sets `pending_trigger_` to `true` when called.
  void OnTrigger() { pending_trigger_ = true; }

  /// @brief Realign the head tracking with the current phone heading
  void ResetHeadTracker();

  /// @brief Enable correction for version 0.5.6 of the Cardboard SDK, which
  /// has a bug concerning not handling a device's default orientation. Calling
  /// this enables correction of that in the Cardboard Input.
  void EnableDeviceOrientationCorrection();

  /// @brief Set the device orientation.
  /// @param[in] rotation The rotation to set for the orientation.
  void set_device_orientation(int rotation) { device_orientation_ = rotation; }

  /// @brief Get the device orientation.
  /// @return Returns an `int` corresponding to the rotation of the device's
  /// orientation.
  int device_orientation() { return device_orientation_; }

#if FPLBASE_ANDROID_VR
  void InitHMDJNIReference();
  void ClearHMDJNIReference();
#endif

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
#endif  // FPLBASE_ANDROID_VR

// Text input structures and enums.

// Text input event type.
enum TextInputEventType {
  kTextInputEventTypeEdit = 0,  // An event for a text edit in IME.
  kTextInputEventTypeText = 1,  // An event for a text input.
  kTextInputEventTypeKey = 2,   // An event for a key event.
};

/// @brief An event parameter for a text edit in IME (Input Method Editor).
/// The information passed in the event is an intermediate state and only used
/// for UI represents. Once IME finalizes an edit, the user recieves an
/// TextInputEventText event for the finalized strings.
struct TextInputEventEdit {
  int32_t start;   ///< @brief A start index of a focus region in the text.
  int32_t length;  ///< @brief A length of a focus region in the text.
};

/// @brief An event parameter for a keyboard input.
///
/// The user recieves all input strings through `kTextInputEventTypeText` type
/// of an event, these paremeters should be used for an input control such as
/// moving caret.
struct TextInputEventKey {
  bool state;           ///< @brief key state, true:pressed, false:released.
  bool repeat;          ///< @brief A flag indicates if the key is repeated
                        /// input.
  FPL_Keycode symbol;   ///< @brief Key symbol, refer `keyboard_keycodes.h` for
                        /// details.
  FPL_Keymod modifier;  ///< @brief Modifier key state, refer
                        /// `keyboard_keycodes.h` for details.
};

/// @brief Holds text input events.
struct TextInputEvent {
  TextInputEventType type;  ///< @brief Type of the event.
  std::string text;         ///< @brief Input string.
  /// @brief Union of Text input events.
  union {
    TextInputEventKey key; ///< @brief An event parameter for a keyboard input.
    TextInputEventEdit edit; ///< @brief An event parameter for a text edit in
                             /// IME (Input Method Editor).
  };
  /// @brief Constructor for TextInputEvent.
  TextInputEvent(TextInputEventType t);
  /// @brief Constructor for TextInputEvent.
  TextInputEvent(TextInputEventType t, int32_t state, bool repeat,
                 int32_t symbol, int32_t modifier);
  /// @brief Constructor for TextInputEvent.
  TextInputEvent(TextInputEventType t, const char *str);
  /// @brief Constructor for TextInputEvent.
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
  /// Call this after the Renderer is initialized.
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

  /// @brief Make the application go to sleep a certain duration.
  void Delay(double seconds) const;

  /// @brief Get a Button object describing the input state of the specified
  ///        button ID.
  ///
  /// @param button The ID of the button.
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

  /// @brief Receives events from java, and stuffs them into a vector until
  /// we're ready.
  static void ReceiveGamepadEvent(int controller_id, int event_code,
                                  int control_code, float x, float y);

  /// @brief Runs through all the received events and processes them.
  void HandleGamepadEvents();
#endif  // ANDROID_GAMEPAD

#if FPLBASE_ANDROID_VR
  /// @brief Get the current input state of the Head Mounted Display device.
  ///
  /// @return Returns the current input state.
  HeadMountedDisplayInput &head_mounted_display_input() {
    return head_mounted_display_input_;
  }

  /// @brief Get the current input state of the Head Mounted Display device.
  ///
  /// @return Returns the current input state.
  const HeadMountedDisplayInput &head_mounted_display_input() const {
    return head_mounted_display_input_;
  }
#endif  // FPLBASE_ANDROID_VR

  /// @brief Get a Button object for a pointer index.
  ///
  /// @param pointer The FingerId of the button.
  /// @return Returns the corresponding button.
  Button &GetPointerButton(FingerId pointer) {
    return GetButton(static_cast<int>(pointer + K_POINTER1));
  }

  /// @brief Open all connected joysticks.
  void OpenConnectedJoysticks();

  /// @brief Close all opened joysticks.
  void CloseOpenJoysticks();

  /// @brief Close any open joysticks, and then refresh the list by opening any
  /// joysticks that are still connected.
  void UpdateConnectedJoystickList();

  /// @brief Handle a joystick event.
  /// @param[in] event The joystick event that should be handled.
  void HandleJoystickEvent(Event event);

  /// @brief A function pointer to an app event callback function.
  typedef std::function<void(Event)> AppEventCallback;

  /// @brief Get the vector of all app event callbacks.
  /// @return Returns a `std::vector<AppEventCallback>` with all app event
  /// callbacks.
  std::vector<AppEventCallback> &app_event_callbacks() {
    return app_event_callbacks_;
  }

  /// @brief Add an app event callback function.
  void AddAppEventCallback(AppEventCallback callback);

  /// @brief Get the most recent frame at which we were minimized or maximized.
  /// @return Returns the most recent frame at which we were minimized or
  /// maximized.
  int minimized_frame() const { return minimized_frame_; }

  /// @brief Set the most recent frame at which we were minimized or maximized.
  /// @param[in] minimized_frame The most recent frame at which we were
  /// minimized or maximized.
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
  const std::vector<InputPointer> &get_pointers() const { return pointers_; }
  std::vector<InputPointer> &get_pointers() { return pointers_; }

  /// @brief Gets if the application is currently minimized.
  bool minimized() { return minimized_; }
  /// @brief Sets if the application is currently minimized.
  void set_minimized(bool b) { minimized_ = b; }

  /// @brief Gets if exit has been requested by the system.
  bool exit_requested() { return exit_requested_; }
  /// @brief Sets if exit has been requested.
  void set_exit_requested(bool b) { exit_requested_ = b; }

 private:
  // Reset pointer/gamepad input state to released state.
  void ResetInputState();

  static const int kMillisecondsPerSecond = 1000;

  static int HandleAppEvents(void *userdata, void *event);

  // The event specific part of AdvanceFrame().
  void UpdateEvents(mathfu::vec2i *window_size);

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

#if FPLBASE_ANDROID_VR
  // Head mounted display input class.
  HeadMountedDisplayInput head_mounted_display_input_;
#endif  // FPLBASE_ANDROID_VR

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

/// @}
}  // namespace fplbase

#endif  // FPLBASE_INPUT_SYSTEM_H
