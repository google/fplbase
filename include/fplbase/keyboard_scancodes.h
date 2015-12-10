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

#ifndef FPLBASE_KEYBOARD_SCANCODES_H
#define FPLBASE_KEYBOARD_SCANCODES_H

#include "fplbase/config.h"  // Must come first.

#include "keyboard_scancodes.h"

namespace fplbase {

/**
 *  \file
 *  \defgroup fplbase_scancodes Scan Codes
 *  \brief The keyboard scan codes used by FPLBase, which correspond to the
 *  physical location of a certain key on the keyboard. This is ideal for
 *  layout-specific controls, regardless of the operating system's mapping of
 *  the keys.
 *
 *  For example, if a given key (`a`) is pressed, it will result in
 *  `FPL_SCANCODE_A` being generated, even if the `a` key is mapped to a
 *  different character -- perhaps the German A-umlaut (`ä`). This would be
 *  perfect in the case of `WASD` (for secondary arrow-keys), where the location
 *  of those four keys is important, regardless of the keystoke they generate.
 *
 *  The values in this enumeration are based on the USB usage page standard:
 *  <a href="http://www.usb.org/developers/hidpage/Hut1_12v2.pdf">
 *  Universal Serial Bus HID Usage Tables
 *  </a>
 *
 * \ingroup fplbase_keyboardcodes
 * \{
 */
typedef enum {
  FPL_SCANCODE_UNKNOWN = 0, /**< Sentinel value for unknown keypress. */

  /**
   *  \defgroup page07 Usage page 0x07
   *  \ingroup fplbase_scancodes
   *
   *  \brief These values are from usage page 0x07
   *  (<a href="http://www.usb.org/developers/hidpage/Hut1_12v2.pdf">
   *   USB keyboard page</a>).
   *
   *  @{
   */
  FPL_SCANCODE_A = 4, /**< Keyboard `a` and `A`. */
  FPL_SCANCODE_B = 5, /**< Keyboard `b` and `B`. */
  FPL_SCANCODE_C = 6, /**< Keyboard `c` and `C`. */
  FPL_SCANCODE_D = 7, /**< Keyboard `d` and `D`. */
  FPL_SCANCODE_E = 8, /**< Keyboard `e` and `E`. */
  FPL_SCANCODE_F = 9, /**< Keyboard `f` and `F`. */
  FPL_SCANCODE_G = 10, /**< Keyboard `g` and `G`. */
  FPL_SCANCODE_H = 11, /**< Keyboard `h` and `H`. */
  FPL_SCANCODE_I = 12, /**< Keyboard `i` and `I`. */
  FPL_SCANCODE_J = 13, /**< Keyboard `j` and `J`. */
  FPL_SCANCODE_K = 14, /**< Keyboard `k` and `K`. */
  FPL_SCANCODE_L = 15, /**< Keyboard `l` and `L`. */
  FPL_SCANCODE_M = 16, /**< Keyboard `m` and `M`. */
  FPL_SCANCODE_N = 17, /**< Keyboard `n` and `N`. */
  FPL_SCANCODE_O = 18, /**< Keyboard `o` and `O`. */
  FPL_SCANCODE_P = 19, /**< Keyboard `p` and `p`. */
  FPL_SCANCODE_Q = 20, /**< Keyboard `q` and `Q`. */
  FPL_SCANCODE_R = 21, /**< Keyboard `r` and `R`. */
  FPL_SCANCODE_S = 22, /**< Keyboard `s` and `S`. */
  FPL_SCANCODE_T = 23, /**< Keyboard `t` and `T`. */
  FPL_SCANCODE_U = 24, /**< Keyboard `u` and `U`. */
  FPL_SCANCODE_V = 25, /**< Keyboard `v` and `V`. */
  FPL_SCANCODE_W = 26, /**< Keyboard `w` and `W`. */
  FPL_SCANCODE_X = 27, /**< Keyboard `x` and `X`. */
  FPL_SCANCODE_Y = 28, /**< Keyboard `y` and `Y`. */
  FPL_SCANCODE_Z = 29, /**< Keyboard `z` and `Z`. */
  FPL_SCANCODE_1 = 30, /**< Keyboard `1` and `!`. */
  FPL_SCANCODE_2 = 31, /**< Keyboard `2` and `@`. */
  FPL_SCANCODE_3 = 32, /**< Keyboard `3` and `#`. */
  FPL_SCANCODE_4 = 33, /**< Keyboard `4` and `$`. */
  FPL_SCANCODE_5 = 34, /**< Keyboard `5` and `%`. */
  FPL_SCANCODE_6 = 35, /**< Keyboard `6` and `^`. */
  FPL_SCANCODE_7 = 36, /**< Keyboard `7` and `&`. */
  FPL_SCANCODE_8 = 37, /**< Keyboard `8` and `*`. */
  FPL_SCANCODE_9 = 38, /**< Keyboard `9` and `(`. */
  FPL_SCANCODE_0 = 39, /**< Keyboard `0` and `)`. */
  FPL_SCANCODE_RETURN = 40, /**< Keyboard `Return`/`ENTER`. */
  FPL_SCANCODE_ESCAPE = 41, /**< Keyboard `ESCAPE`. */
  FPL_SCANCODE_BACKSPACE = 42, /**< Keyboard `DELETE`/`Backspace`. */
  FPL_SCANCODE_TAB = 43, /**< Keyboard `Tab`. */
  FPL_SCANCODE_SPACE = 44, /**< Keyboard `Spacebar`. */
  FPL_SCANCODE_MINUS = 45, /**< Keyboard `-` and `_`. */
  FPL_SCANCODE_EQUALS = 46, /**< Keyboard `=` and `+`. */
  FPL_SCANCODE_LEFTBRACKET = 47, /**< Keyboard `[` and `{`. */
  FPL_SCANCODE_RIGHTBRACKET = 48, /**< Keyboard `]` and `}`. */
  FPL_SCANCODE_BACKSLASH = 49, /**< Located at the lower left of the return
                                *   key on ISO keyboards and at the right end
                                *   of the QWERTY row on ANSI keyboards.
                                *   Produces REVERSE SOLIDUS (backslash) and
                                *   VERTICAL LINE in a US layout, REVERSE
                                *   SOLIDUS and VERTICAL LINE in a UK Mac
                                *   layout, NUMBER SIGN and TILDE in a UK
                                *   Windows layout, DOLLAR SIGN and POUND SIGN
                                *   in a Swiss German layout, NUMBER SIGN and
                                *   APOSTROPHE in a German layout, GRAVE
                                *   ACCENT and POUND SIGN in a French Mac
                                *   layout, and ASTERISK and MICRO SIGN in a
                                *   French Windows layout.
                                */
  FPL_SCANCODE_NONUSHASH = 50, /**< ISO USB keyboards actually use this code
                                *   instead of 49 for the same key, but all
                                *   OSes I've seen treat the two codes
                                *   identically. So, as an implementor, unless
                                *   your keyboard generates both of those
                                *   codes and your OS treats them differently,
                                *   you should generate FPL_SCANCODE_BACKSLASH
                                *   instead of this code.
                                */
  FPL_SCANCODE_SEMICOLON = 51, /**< Keyboard `;` and `:`. */
  FPL_SCANCODE_APOSTROPHE = 52, /**< Keyboard `'` and `"`. */
  FPL_SCANCODE_GRAVE = 53, /**< Located in the top left corner (on both ANSI
                            *   and ISO keyboards). Produces GRAVE ACCENT and
                            *   TILDE in a US Windows layout and in US and UK
                            *   Mac layouts on ANSI keyboards, GRAVE ACCENT
                            *   and NOT SIGN in a UK Windows layout, SECTION
                            *   SIGN and PLUS-MINUS SIGN in US and UK Mac
                            *   layouts on ISO keyboards, SECTION SIGN and
                            *   DEGREE SIGN in a Swiss German layout (Mac:
                            *   only on ISO keyboards), CIRCUMFLEX ACCENT and
                            *   DEGREE SIGN in a German layout (Mac: only on
                            *   ISO keyboards), SUPERSCRIPT TWO and TILDE in a
                            *   French Windows layout, COMMERCIAL AT and
                            *   NUMBER SIGN in a French Mac layout on ISO
                            *   keyboards, and LESS-THAN SIGN and GREATER-THAN
                            *   SIGN in a Swiss German, German, or French Mac
                            *   layout on ANSI keyboards.
                            */
  FPL_SCANCODE_COMMA = 54, /**< Keyboard `,` and `<`. */
  FPL_SCANCODE_PERIOD = 55, /**< Keyboard `.` and `>`. */
  FPL_SCANCODE_SLASH = 56, /**< Keyboard `\` and `|`. */
  FPL_SCANCODE_CAPSLOCK = 57, /**< Keyboard `Caps Lock`. */
  FPL_SCANCODE_F1 = 58, /**< Keyboard `F1`. */
  FPL_SCANCODE_F2 = 59, /**< Keyboard `F2`. */
  FPL_SCANCODE_F3 = 60, /**< Keyboard `F3`. */
  FPL_SCANCODE_F4 = 61, /**< Keyboard `F4`. */
  FPL_SCANCODE_F5 = 62, /**< Keyboard `F5`. */
  FPL_SCANCODE_F6 = 63, /**< Keyboard `F6`. */
  FPL_SCANCODE_F7 = 64, /**< Keyboard `F7`. */
  FPL_SCANCODE_F8 = 65, /**< Keyboard `F8`. */
  FPL_SCANCODE_F9 = 66, /**< Keyboard `F9`. */
  FPL_SCANCODE_F10 = 67, /**< Keyboard `F10`. */
  FPL_SCANCODE_F11 = 68, /**< Keyboard `F11`. */
  FPL_SCANCODE_F12 = 69, /**< Keyboard `F12`. */
  FPL_SCANCODE_PRINTSCREEN = 70, /**< Keyboard `PrintScreen`. */
  FPL_SCANCODE_SCROLLLOCK = 71, /**< Keyboard `Scroll Lock`. */
  FPL_SCANCODE_PAUSE = 72, /**< Keyboard `Pause`. */
  FPL_SCANCODE_INSERT = 73, /**< insert on PC, help on some Mac keyboards (but
                                 does send code 73, not 117) */
  FPL_SCANCODE_HOME = 74, /**< Keyboard `Home`. */
  FPL_SCANCODE_PAGEUP = 75, /**< Keyboard `PageUp`. */
  FPL_SCANCODE_DELETE = 76, /**< Keyboard `Delete Forward`. */
  FPL_SCANCODE_END = 77, /**< Keyboard `End`. */
  FPL_SCANCODE_PAGEDOWN = 78, /**< Keyboard `PageDown`. */
  FPL_SCANCODE_RIGHT = 79, /**< Keyboard `RightArrow`. */
  FPL_SCANCODE_LEFT = 80, /**< Keyboard `LeftArrow`. */
  FPL_SCANCODE_DOWN = 81, /**< Keyboard `DownArrow`. */
  FPL_SCANCODE_UP = 82, /**< Keyboard `F1`. */
  FPL_SCANCODE_NUMLOCKCLEAR = 83, /**< `num lock` on PC, `clear` on Mac
                                   * keyboards
                                   */
  FPL_SCANCODE_KP_DIVIDE = 84, /**< Keyboard `Keypad \`. */
  FPL_SCANCODE_KP_MULTIPLY = 85, /**< Keyboard `Keypad *`. */
  FPL_SCANCODE_KP_MINUS = 86, /**< Keyboard `Keypad -`. */
  FPL_SCANCODE_KP_PLUS = 87, /**< Keyboard `Keypad +`. */
  FPL_SCANCODE_KP_ENTER = 88, /**< Keyboard `Keypad ENTER`. */
  FPL_SCANCODE_KP_1 = 89, /**< Keyboard `Keypad 1` and `Keypad End`. */
  FPL_SCANCODE_KP_2 = 90, /**< Keyboard `Keypad 2` and `Keypad DownArrow`. */
  FPL_SCANCODE_KP_3 = 91, /**< Keyboard `Keypad 3` and `Keypad PageDown`. */
  FPL_SCANCODE_KP_4 = 92, /**< Keyboard `Keypad 4` and `Keypad LeftArrow`. */
  FPL_SCANCODE_KP_5 = 93, /**< Keyboard `Keypad 5`. */
  FPL_SCANCODE_KP_6 = 94, /**< Keyboard `Keypad 6` and `Keypad RightArrow`. */
  FPL_SCANCODE_KP_7 = 95, /**< Keyboard `Keypad 7` and `Keypad Home`. */
  FPL_SCANCODE_KP_8 = 96, /**< Keyboard `Keypad 8` and `Keypad UpArrow`. */
  FPL_SCANCODE_KP_9 = 97, /**< Keyboard `Keypad 9` and `Keypad PageUp`. */
  FPL_SCANCODE_KP_0 = 98, /**< Keyboard `Keypad 0` and `Keypad Insert`. */
  FPL_SCANCODE_KP_PERIOD = 99, /**< Keyboard `Keypad .` and `Keypad Delete`. */
  FPL_SCANCODE_NONUSBACKSLASH = 100, /**< This is the additional key that ISO
                                      *   keyboards have over ANSI ones,
                                      *   located between left shift and Y.
                                      *   Produces GRAVE ACCENT and TILDE in a
                                      *   US or UK Mac layout, REVERSE SOLIDUS
                                      *   (backslash) and VERTICAL LINE in a
                                      *   US or UK Windows layout, and
                                      *   LESS-THAN SIGN and GREATER-THAN SIGN
                                      *   in a Swiss German, German, or French
                                      *   layout. */
  FPL_SCANCODE_APPLICATION = 101, /**< windows contextual menu, compose */
  FPL_SCANCODE_POWER = 102, /**< The USB document says this is a status flag,
                             *   not a physical key - but some Mac keyboards
                             *   do have a power key. */
  FPL_SCANCODE_KP_EQUALS = 103, /**< Keyboard `Keypad =`. */
  FPL_SCANCODE_F13 = 104, /**< Keyboard `F13`. */
  FPL_SCANCODE_F14 = 105, /**< Keyboard `F14`. */
  FPL_SCANCODE_F15 = 106, /**< Keyboard `F15`. */
  FPL_SCANCODE_F16 = 107, /**< Keyboard `F16`. */
  FPL_SCANCODE_F17 = 108, /**< Keyboard `F17`. */
  FPL_SCANCODE_F18 = 109, /**< Keyboard `F18`. */
  FPL_SCANCODE_F19 = 110, /**< Keyboard `F19`. */
  FPL_SCANCODE_F20 = 111, /**< Keyboard `F20`. */
  FPL_SCANCODE_F21 = 112, /**< Keyboard `F21`. */
  FPL_SCANCODE_F22 = 113, /**< Keyboard `F22`. */
  FPL_SCANCODE_F23 = 114, /**< Keyboard `F23`. */
  FPL_SCANCODE_F24 = 115, /**< Keyboard `F24`. */
  FPL_SCANCODE_EXECUTE = 116, /**< Keyboard `Execute`. */
  FPL_SCANCODE_HELP = 117, /**< Keyboard `Help`. */
  FPL_SCANCODE_MENU = 118, /**< Keyboard `Menu`. */
  FPL_SCANCODE_SELECT = 119, /**< Keyboard `Select`. */
  FPL_SCANCODE_STOP = 120, /**< Keyboard `Stop`. */
  FPL_SCANCODE_AGAIN = 121, /**< Keyboard `Again`. */
  FPL_SCANCODE_UNDO = 122, /**< Keyboard `Undo`. */
  FPL_SCANCODE_CUT = 123, /**< Keyboard `Cut`. */
  FPL_SCANCODE_COPY = 124, /**< Keyboard `Copy`. */
  FPL_SCANCODE_PASTE = 125, /**< Keyboard `Paste`. */
  FPL_SCANCODE_FIND = 126, /**< Keyboard `Find`. */
  FPL_SCANCODE_MUTE = 127, /**< Keyboard `Mute`. */
  FPL_SCANCODE_VOLUMEUP = 128, /**< Keyboard `Volume Up`. */
  FPL_SCANCODE_VOLUMEDOWN = 129, /**< Keyboard `Volume Down`. */
  /* not sure whether there's a reason to enable these */
  /*     FPL_SCANCODE_LOCKINGCAPSLOCK = 130,  */
  /*     FPL_SCANCODE_LOCKINGNUMLOCK = 131, */
  /*     FPL_SCANCODE_LOCKINGSCROLLLOCK = 132, */
  FPL_SCANCODE_KP_COMMA = 133, /**< Keyboard `,`. */
  FPL_SCANCODE_KP_EQUALSAS400 = 134, /**< Keyboard `Equal Sign`. */
  FPL_SCANCODE_INTERNATIONAL1 = 135, /**< used on Asian keyboards, see
                                          footnotes in USB doc */
  FPL_SCANCODE_INTERNATIONAL2 = 136,
  FPL_SCANCODE_INTERNATIONAL3 = 137, /**< Yen */
  FPL_SCANCODE_INTERNATIONAL4 = 138,
  FPL_SCANCODE_INTERNATIONAL5 = 139,
  FPL_SCANCODE_INTERNATIONAL6 = 140,
  FPL_SCANCODE_INTERNATIONAL7 = 141,
  FPL_SCANCODE_INTERNATIONAL8 = 142,
  FPL_SCANCODE_INTERNATIONAL9 = 143,
  FPL_SCANCODE_LANG1 = 144,    /**< Hangul/English toggle */
  FPL_SCANCODE_LANG2 = 145,    /**< Hanja conversion */
  FPL_SCANCODE_LANG3 = 146,    /**< Katakana */
  FPL_SCANCODE_LANG4 = 147,    /**< Hiragana */
  FPL_SCANCODE_LANG5 = 148,    /**< Zenkaku/Hankaku */
  FPL_SCANCODE_LANG6 = 149,    /**< reserved */
  FPL_SCANCODE_LANG7 = 150,    /**< reserved */
  FPL_SCANCODE_LANG8 = 151,    /**< reserved */
  FPL_SCANCODE_LANG9 = 152,    /**< reserved */
  FPL_SCANCODE_ALTERASE = 153, /**< Erase-Eaze */
  FPL_SCANCODE_SYSREQ = 154,
  FPL_SCANCODE_CANCEL = 155,
  FPL_SCANCODE_CLEAR = 156,
  FPL_SCANCODE_PRIOR = 157,
  FPL_SCANCODE_RETURN2 = 158,
  FPL_SCANCODE_SEPARATOR = 159,
  FPL_SCANCODE_OUT = 160,
  FPL_SCANCODE_OPER = 161,
  FPL_SCANCODE_CLEARAGAIN = 162,
  FPL_SCANCODE_CRSEL = 163,
  FPL_SCANCODE_EXSEL = 164,
  FPL_SCANCODE_KP_00 = 176,
  FPL_SCANCODE_KP_000 = 177,
  FPL_SCANCODE_THOUSANDSSEPARATOR = 178,
  FPL_SCANCODE_DECIMALSEPARATOR = 179,
  FPL_SCANCODE_CURRENCYUNIT = 180,
  FPL_SCANCODE_CURRENCYSUBUNIT = 181,
  FPL_SCANCODE_KP_LEFTPAREN = 182,
  FPL_SCANCODE_KP_RIGHTPAREN = 183,
  FPL_SCANCODE_KP_LEFTBRACE = 184,
  FPL_SCANCODE_KP_RIGHTBRACE = 185,
  FPL_SCANCODE_KP_TAB = 186,
  FPL_SCANCODE_KP_BACKSPACE = 187,
  FPL_SCANCODE_KP_A = 188,
  FPL_SCANCODE_KP_B = 189,
  FPL_SCANCODE_KP_C = 190,
  FPL_SCANCODE_KP_D = 191,
  FPL_SCANCODE_KP_E = 192,
  FPL_SCANCODE_KP_F = 193,
  FPL_SCANCODE_KP_XOR = 194,
  FPL_SCANCODE_KP_POWER = 195,
  FPL_SCANCODE_KP_PERCENT = 196,
  FPL_SCANCODE_KP_LESS = 197,
  FPL_SCANCODE_KP_GREATER = 198,
  FPL_SCANCODE_KP_AMPERSAND = 199,
  FPL_SCANCODE_KP_DBLAMPERSAND = 200,
  FPL_SCANCODE_KP_VERTICALBAR = 201,
  FPL_SCANCODE_KP_DBLVERTICALBAR = 202,
  FPL_SCANCODE_KP_COLON = 203,
  FPL_SCANCODE_KP_HASH = 204,
  FPL_SCANCODE_KP_SPACE = 205,
  FPL_SCANCODE_KP_AT = 206,
  FPL_SCANCODE_KP_EXCLAM = 207,
  FPL_SCANCODE_KP_MEMSTORE = 208,
  FPL_SCANCODE_KP_MEMRECALL = 209,
  FPL_SCANCODE_KP_MEMCLEAR = 210,
  FPL_SCANCODE_KP_MEMADD = 211,
  FPL_SCANCODE_KP_MEMSUBTRACT = 212,
  FPL_SCANCODE_KP_MEMMULTIPLY = 213,
  FPL_SCANCODE_KP_MEMDIVIDE = 214,
  FPL_SCANCODE_KP_PLUSMINUS = 215,
  FPL_SCANCODE_KP_CLEAR = 216,
  FPL_SCANCODE_KP_CLEARENTRY = 217,
  FPL_SCANCODE_KP_BINARY = 218,
  FPL_SCANCODE_KP_OCTAL = 219,
  FPL_SCANCODE_KP_DECIMAL = 220,
  FPL_SCANCODE_KP_HEXADECIMAL = 221,
  FPL_SCANCODE_LCTRL = 224,
  FPL_SCANCODE_LSHIFT = 225,
  FPL_SCANCODE_LALT = 226, /**< alt, option */
  FPL_SCANCODE_LGUI = 227, /**< windows, command (apple), meta */
  FPL_SCANCODE_RCTRL = 228,
  FPL_SCANCODE_RSHIFT = 229,
  FPL_SCANCODE_RALT = 230, /**< alt gr, option */
  FPL_SCANCODE_RGUI = 231, /**< windows, command (apple), meta */

  /* @} */ /* Usage page 0x07 */

  FPL_SCANCODE_MODE = 257,
  FPL_SCANCODE_AUDIONEXT = 258, /**< Keyboard `Next audio track` button. */
  FPL_SCANCODE_AUDIOPREV = 259, /**< Keyboard `Previous audio track` button. */
  FPL_SCANCODE_AUDIOSTOP = 260, /**< Keyboard `Stop audio` button. */
  FPL_SCANCODE_AUDIOPLAY = 261, /**< Keyboard `Play audio` button. */
  FPL_SCANCODE_AUDIOMUTE = 262, /**< Keyboard `Mute` button. */
  FPL_SCANCODE_MEDIASELECT = 263,
  FPL_SCANCODE_WWW = 264,
  FPL_SCANCODE_MAIL = 265,
  FPL_SCANCODE_CALCULATOR = 266, /**< Keyboard `Calculator` button. */
  FPL_SCANCODE_COMPUTER = 267,
  FPL_SCANCODE_AC_SEARCH = 268,
  FPL_SCANCODE_AC_HOME = 269,
  FPL_SCANCODE_AC_BACK = 270,
  FPL_SCANCODE_AC_FORWARD = 271,
  FPL_SCANCODE_AC_STOP = 272,
  FPL_SCANCODE_AC_REFRESH = 273,
  FPL_SCANCODE_AC_BOOKMARKS = 274,
  FPL_SCANCODE_BRIGHTNESSDOWN = 275, /**< Keyboard `Brightness Down`. */
  FPL_SCANCODE_BRIGHTNESSUP = 276, /**< Keyboard `Brightness Up`. */
  FPL_SCANCODE_DISPLAYSWITCH = 277, /**< display mirroring/dual display
                                         switch, video mode switch */
  FPL_SCANCODE_KBDILLUMTOGGLE = 278,
  FPL_SCANCODE_KBDILLUMDOWN = 279,
  FPL_SCANCODE_KBDILLUMUP = 280,
  FPL_SCANCODE_EJECT = 281, /**< Keyboard `Eject`. */
  FPL_SCANCODE_SLEEP = 282, /**< Keyboard `Sleep`. */
  FPL_SCANCODE_APP1 = 283,
  FPL_SCANCODE_APP2 = 284,

  /* Add any other keys here. */
  FPL_NUM_SCANCODES = 512 /**< not a key, just marks the number of scancodes
                               for array bounds */
} FPL_SCANCODE;

/** @} */
}  // namespace fplbase

#endif  // FPLBASE_KEYBOARD_SCANCODES_H
