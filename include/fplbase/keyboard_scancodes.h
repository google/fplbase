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
 *  \brief The SDL keyboard scancode representation.
 *
 *  Values of this type are used to represent keyboard keys, among other places
 *  in the \link SDL_Keysym::scancode key.keysym.scancode \endlink field of the
 *  SDL_Event structure.
 *
 *  The values in this enumeration are based on the USB usage page standard:
 *  http://www.usb.org/developers/devclass_docs/Hut1_12v2.pdf
 */
typedef enum {
  FPL_SCANCODE_UNKNOWN = 0,

  /**
   *  \name Usage page 0x07
   *
   *  These values are from usage page 0x07 (USB keyboard page).
   */
  /* @{ */
  FPL_SCANCODE_A = 4,
  FPL_SCANCODE_B = 5,
  FPL_SCANCODE_C = 6,
  FPL_SCANCODE_D = 7,
  FPL_SCANCODE_E = 8,
  FPL_SCANCODE_F = 9,
  FPL_SCANCODE_G = 10,
  FPL_SCANCODE_H = 11,
  FPL_SCANCODE_I = 12,
  FPL_SCANCODE_J = 13,
  FPL_SCANCODE_K = 14,
  FPL_SCANCODE_L = 15,
  FPL_SCANCODE_M = 16,
  FPL_SCANCODE_N = 17,
  FPL_SCANCODE_O = 18,
  FPL_SCANCODE_P = 19,
  FPL_SCANCODE_Q = 20,
  FPL_SCANCODE_R = 21,
  FPL_SCANCODE_S = 22,
  FPL_SCANCODE_T = 23,
  FPL_SCANCODE_U = 24,
  FPL_SCANCODE_V = 25,
  FPL_SCANCODE_W = 26,
  FPL_SCANCODE_X = 27,
  FPL_SCANCODE_Y = 28,
  FPL_SCANCODE_Z = 29,
  FPL_SCANCODE_1 = 30,
  FPL_SCANCODE_2 = 31,
  FPL_SCANCODE_3 = 32,
  FPL_SCANCODE_4 = 33,
  FPL_SCANCODE_5 = 34,
  FPL_SCANCODE_6 = 35,
  FPL_SCANCODE_7 = 36,
  FPL_SCANCODE_8 = 37,
  FPL_SCANCODE_9 = 38,
  FPL_SCANCODE_0 = 39,
  FPL_SCANCODE_RETURN = 40,
  FPL_SCANCODE_ESCAPE = 41,
  FPL_SCANCODE_BACKSPACE = 42,
  FPL_SCANCODE_TAB = 43,
  FPL_SCANCODE_SPACE = 44,
  FPL_SCANCODE_MINUS = 45,
  FPL_SCANCODE_EQUALS = 46,
  FPL_SCANCODE_LEFTBRACKET = 47,
  FPL_SCANCODE_RIGHTBRACKET = 48,
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
                                *   instead of this code. As a user, you
                                *   should not rely on this code because SDL
                                *   will never generate it with most (all?)
                                *   keyboards.
                                */
  FPL_SCANCODE_SEMICOLON = 51,
  FPL_SCANCODE_APOSTROPHE = 52,
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
  FPL_SCANCODE_COMMA = 54,
  FPL_SCANCODE_PERIOD = 55,
  FPL_SCANCODE_SLASH = 56,
  FPL_SCANCODE_CAPSLOCK = 57,
  FPL_SCANCODE_F1 = 58,
  FPL_SCANCODE_F2 = 59,
  FPL_SCANCODE_F3 = 60,
  FPL_SCANCODE_F4 = 61,
  FPL_SCANCODE_F5 = 62,
  FPL_SCANCODE_F6 = 63,
  FPL_SCANCODE_F7 = 64,
  FPL_SCANCODE_F8 = 65,
  FPL_SCANCODE_F9 = 66,
  FPL_SCANCODE_F10 = 67,
  FPL_SCANCODE_F11 = 68,
  FPL_SCANCODE_F12 = 69,
  FPL_SCANCODE_PRINTSCREEN = 70,
  FPL_SCANCODE_SCROLLLOCK = 71,
  FPL_SCANCODE_PAUSE = 72,
  FPL_SCANCODE_INSERT = 73, /**< insert on PC, help on some Mac keyboards (but
                                 does send code 73, not 117) */
  FPL_SCANCODE_HOME = 74,
  FPL_SCANCODE_PAGEUP = 75,
  FPL_SCANCODE_DELETE = 76,
  FPL_SCANCODE_END = 77,
  FPL_SCANCODE_PAGEDOWN = 78,
  FPL_SCANCODE_RIGHT = 79,
  FPL_SCANCODE_LEFT = 80,
  FPL_SCANCODE_DOWN = 81,
  FPL_SCANCODE_UP = 82,
  FPL_SCANCODE_NUMLOCKCLEAR = 83, /**< num lock on PC, clear on Mac keyboards
                                   */
  FPL_SCANCODE_KP_DIVIDE = 84,
  FPL_SCANCODE_KP_MULTIPLY = 85,
  FPL_SCANCODE_KP_MINUS = 86,
  FPL_SCANCODE_KP_PLUS = 87,
  FPL_SCANCODE_KP_ENTER = 88,
  FPL_SCANCODE_KP_1 = 89,
  FPL_SCANCODE_KP_2 = 90,
  FPL_SCANCODE_KP_3 = 91,
  FPL_SCANCODE_KP_4 = 92,
  FPL_SCANCODE_KP_5 = 93,
  FPL_SCANCODE_KP_6 = 94,
  FPL_SCANCODE_KP_7 = 95,
  FPL_SCANCODE_KP_8 = 96,
  FPL_SCANCODE_KP_9 = 97,
  FPL_SCANCODE_KP_0 = 98,
  FPL_SCANCODE_KP_PERIOD = 99,
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
  FPL_SCANCODE_KP_EQUALS = 103,
  FPL_SCANCODE_F13 = 104,
  FPL_SCANCODE_F14 = 105,
  FPL_SCANCODE_F15 = 106,
  FPL_SCANCODE_F16 = 107,
  FPL_SCANCODE_F17 = 108,
  FPL_SCANCODE_F18 = 109,
  FPL_SCANCODE_F19 = 110,
  FPL_SCANCODE_F20 = 111,
  FPL_SCANCODE_F21 = 112,
  FPL_SCANCODE_F22 = 113,
  FPL_SCANCODE_F23 = 114,
  FPL_SCANCODE_F24 = 115,
  FPL_SCANCODE_EXECUTE = 116,
  FPL_SCANCODE_HELP = 117,
  FPL_SCANCODE_MENU = 118,
  FPL_SCANCODE_SELECT = 119,
  FPL_SCANCODE_STOP = 120,
  FPL_SCANCODE_AGAIN = 121, /**< redo */
  FPL_SCANCODE_UNDO = 122,
  FPL_SCANCODE_CUT = 123,
  FPL_SCANCODE_COPY = 124,
  FPL_SCANCODE_PASTE = 125,
  FPL_SCANCODE_FIND = 126,
  FPL_SCANCODE_MUTE = 127,
  FPL_SCANCODE_VOLUMEUP = 128,
  FPL_SCANCODE_VOLUMEDOWN = 129,
  /* not sure whether there's a reason to enable these */
  /*     FPL_SCANCODE_LOCKINGCAPSLOCK = 130,  */
  /*     FPL_SCANCODE_LOCKINGNUMLOCK = 131, */
  /*     FPL_SCANCODE_LOCKINGSCROLLLOCK = 132, */
  FPL_SCANCODE_KP_COMMA = 133,
  FPL_SCANCODE_KP_EQUALSAS400 = 134,
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
  FPL_SCANCODE_MODE = 257, /**< I'm not sure if this is really not covered
                            *   by any of the above, but since there's a
                            *   special KMOD_MODE for it I'm adding it here
                            */

  /* @} */ /* Usage page 0x07 */

  /**
   *  \name Usage page 0x0C
   *
   *  These values are mapped from usage page 0x0C (USB consumer page).
   */
  /* @{ */
  FPL_SCANCODE_AUDIONEXT = 258,
  FPL_SCANCODE_AUDIOPREV = 259,
  FPL_SCANCODE_AUDIOSTOP = 260,
  FPL_SCANCODE_AUDIOPLAY = 261,
  FPL_SCANCODE_AUDIOMUTE = 262,
  FPL_SCANCODE_MEDIASELECT = 263,
  FPL_SCANCODE_WWW = 264,
  FPL_SCANCODE_MAIL = 265,
  FPL_SCANCODE_CALCULATOR = 266,
  FPL_SCANCODE_COMPUTER = 267,
  FPL_SCANCODE_AC_SEARCH = 268,
  FPL_SCANCODE_AC_HOME = 269,
  FPL_SCANCODE_AC_BACK = 270,
  FPL_SCANCODE_AC_FORWARD = 271,
  FPL_SCANCODE_AC_STOP = 272,
  FPL_SCANCODE_AC_REFRESH = 273,
  FPL_SCANCODE_AC_BOOKMARKS = 274,

  /* @} */ /* Usage page 0x0C */

  /**
   *  \name Walther keys
   *
   *  These are values that Christian Walther added (for mac keyboard?).
   */
  /* @{ */
  FPL_SCANCODE_BRIGHTNESSDOWN = 275,
  FPL_SCANCODE_BRIGHTNESSUP = 276,
  FPL_SCANCODE_DISPLAYSWITCH = 277, /**< display mirroring/dual display
                                         switch, video mode switch */
  FPL_SCANCODE_KBDILLUMTOGGLE = 278,
  FPL_SCANCODE_KBDILLUMDOWN = 279,
  FPL_SCANCODE_KBDILLUMUP = 280,
  FPL_SCANCODE_EJECT = 281,
  FPL_SCANCODE_SLEEP = 282,
  FPL_SCANCODE_APP1 = 283,
  FPL_SCANCODE_APP2 = 284,

  /* @} */ /* Walther keys */

  /* Add any other keys here. */
  SDL_NUM_SCANCODES = 512 /**< not a key, just marks the number of scancodes
                               for array bounds */
} FPL_SCANCODE;

}  // namespace fplbase

#endif  // FPLBASE_KEYBOARD_SCANCODES_H
