# Copyright 2015 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# NOTE: This file is a fork of sdl/Android.mk in order to workaround the
# following limitations:
#   1. The SDL2_static build target was broken when included as a module as
#      LOCAL_PATH will end up being incorrectly defined resulting in the
#      target being unable to find SDL_android_main.c.
#   2. LOCAL_ARM_MODE isn't set to "arm" resulting in all code being generated
#      in the "thumb" instruction set.
#   3. SDLActivity attempts to load the libSDL2 shared library even if the
#      application is statically linked with SDL.  Therefore, the SDL2_static
#      build target has been modified to depend upon a libSDL2 build
#      target that generates an empty shared library to satisfy SDLActivity.
#   4. Static linking SDL results in JNI_OnLoad() being exclusively hooked by
#      SDL which prevents applications from performing initialization on this
#      entry point.  The library build has been changed to rename JNI_OnLoad()
#      to SDL_JNI_OnLoad() so that applications have the option of providing
#      their own initialization prior to calling SDL.
#      See SDL/src/core/android/SDL_android.c
#
# Lines that are patches are marked up with (X) where X corresponds to an issue
# described above.

LOCAL_PATH := $(call my-dir)

# The following block is required to point LOCAL_PATH at SDL.
# Project directory relative to this file.
FPLBASE_DIR := $(LOCAL_PATH)/../..
include $(FPLBASE_DIR)/jni/android_config.mk
# Modify the local path to point to SDL library source.
LOCAL_PATH := $(DEPENDENCIES_SDL_DIR)

###########################
#
# SDL shared library
#
###########################

include $(CLEAR_VARS)

LOCAL_MODULE := SDL2

SDL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
SDL_C_INCLUDES := $(SDL_EXPORT_C_INCLUDES)

LOCAL_C_INCLUDES := $(SDL_C_INCLUDES)
LOCAL_EXPORT_C_INCLUDES := $(SDL_EXPORT_C_INCLUDES)
LOCAL_ARM_MODE := arm  # (2) generate "arm" instruction set code.

SDL_SRC_FILES := \
	$(subst $(LOCAL_PATH)/,, \
	$(wildcard $(LOCAL_PATH)/src/*.c) \
	$(wildcard $(LOCAL_PATH)/src/audio/*.c) \
	$(wildcard $(LOCAL_PATH)/src/audio/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/audio/dummy/*.c) \
	$(LOCAL_PATH)/src/atomic/SDL_atomic.c \
	$(LOCAL_PATH)/src/atomic/SDL_spinlock.c.arm \
	$(wildcard $(LOCAL_PATH)/src/core/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/cpuinfo/*.c) \
	$(wildcard $(LOCAL_PATH)/src/dynapi/*.c) \
	$(wildcard $(LOCAL_PATH)/src/events/*.c) \
	$(wildcard $(LOCAL_PATH)/src/file/*.c) \
	$(wildcard $(LOCAL_PATH)/src/haptic/*.c) \
	$(wildcard $(LOCAL_PATH)/src/haptic/dummy/*.c) \
	$(wildcard $(LOCAL_PATH)/src/joystick/*.c) \
	$(wildcard $(LOCAL_PATH)/src/joystick/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/loadso/dlopen/*.c) \
	$(wildcard $(LOCAL_PATH)/src/power/*.c) \
	$(wildcard $(LOCAL_PATH)/src/power/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/filesystem/dummy/*.c) \
	$(wildcard $(LOCAL_PATH)/src/render/*.c) \
	$(wildcard $(LOCAL_PATH)/src/render/*/*.c) \
	$(wildcard $(LOCAL_PATH)/src/stdlib/*.c) \
	$(wildcard $(LOCAL_PATH)/src/thread/*.c) \
	$(wildcard $(LOCAL_PATH)/src/thread/pthread/*.c) \
	$(wildcard $(LOCAL_PATH)/src/timer/*.c) \
	$(wildcard $(LOCAL_PATH)/src/timer/unix/*.c) \
	$(wildcard $(LOCAL_PATH)/src/video/*.c) \
	$(wildcard $(LOCAL_PATH)/src/video/android/*.c) \
    $(wildcard $(LOCAL_PATH)/src/test/*.c))
LOCAL_SRC_FILES := $(SDL_SRC_FILES)

SDL_CFLAGS := -DGL_GLEXT_PROTOTYPES

LOCAL_CFLAGS += $(SDL_CFLAGS)
LOCAL_LDLIBS := -ldl -lGLESv1_CM -lGLESv2 -llog -landroid

include $(BUILD_SHARED_LIBRARY)

# (3) SDL build target
include $(CLEAR_VARS)
LOCAL_MODULE := SDL2_empty
LOCAL_C_INCLUDES := $(SDL_C_INCLUDES)
LOCAL_SRC_FILES :=
LOCAL_CFLAGS += $(SDL_CFLAGS)
LOCAL_LDLIBS := -ldl -lGLESv1_CM -lGLESv2 -llog -landroid
include $(BUILD_SHARED_LIBRARY)
# Replace libSDL2.so with libSDL2_empty.so.
# NOTE: This is only invoked if libSDL2.a (static library) is built.
SDL2_EMPTY_INSTALLED := $(subst _empty,,$(LOCAL_INSTALLED))
SDL2_EMPTY_INSTALL := sdl2-replace-with-empty-$(TARGET_ARCH_ABI)
$(SDL2_EMPTY_INSTALL): PRIVATE_INSTALL_PATH:=$(SDL2_EMPTY_INSTALLED)
$(SDL2_EMPTY_INSTALL): $(LOCAL_INSTALLED)
	$(call host-install,$<,$(PRIVATE_INSTALL_PATH))
# (3) Dummy SDL build target: end

###########################
#
# SDL static library
#
###########################

include $(CLEAR_VARS)

LOCAL_MODULE := SDL2_static

LOCAL_MODULE_FILENAME := libSDL2

LOCAL_C_INCLUDES := $(SDL_C_INCLUDES)
LOCAL_EXPORT_C_INCLUDES := $(SDL_EXPORT_C_INCLUDES)
LOCAL_ARM_MODE := arm  # (2) generate "arm" instruction set code.

LOCAL_SRC_FILES += \
	$(SDL_SRC_FILES) \
	src/main/android/SDL_android_main.c  # (1) fix path.

LOCAL_CFLAGS += $(SDL_CFLAGS)
LOCAL_CFLAGS += -DJNI_OnLoad=SDL_JNI_OnLoad  # (4) Rename JNI_OnLoad
LOCAL_EXPORT_LDLIBS := -Wl,--undefined=Java_org_libsdl_app_SDLActivity_nativeInit -ldl -lGLESv1_CM -lGLESv2 -llog -landroid

include $(BUILD_STATIC_LIBRARY)

$(LOCAL_BUILT_MODULE): $(SDL2_EMPTY_INSTALL)  # (3) SDL2_empty as libSDL2
