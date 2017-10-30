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

LOCAL_PATH := $(call my-dir)/..

FPLBASE_DIR := $(LOCAL_PATH)

include $(FPLBASE_DIR)/jni/android_config.mk
include $(DEPENDENCIES_FLATBUFFERS_DIR)/android/jni/include.mk

# realpath-portable From flatbuffers/android/jni/include.mk
LOCAL_PATH := $(call realpath-portable,$(LOCAL_PATH))
FPLBASE_DIR := $(LOCAL_PATH)

FPLBASE_COMMON_SRC_FILES := \
  src/asset_manager.cpp \
  src/gpu_debug_gl.cpp \
  src/input.cpp \
  src/material.cpp \
  src/mesh_common.cpp \
  src/mesh_gl.cpp \
  src/precompiled.cpp \
  src/preprocessor.cpp \
  src/render_target_common.cpp \
  src/render_target_gl.cpp \
  src/render_utils_gl.cpp \
  src/renderer_common.cpp \
  src/renderer_gl.cpp \
  src/renderer_hmd_gl.cpp \
  src/shader_common.cpp \
  src/shader_gl.cpp \
  src/texture_common.cpp \
  src/texture_gl.cpp \
  src/type_conversions_gl.cpp \
  src/utilities.cpp \
  src/version.cpp \
  src/gl3stub_android.c

FPLBASE_EXPORT_COMMON_CPPFLAGS := -std=c++11 \
                                  -DFPLBASE_OPENGL

FPLBASE_COMMON_CPPFLAGS := $(FPLBASE_EXPORT_COMMON_CPPFLAGS) \
                           -Wno-unused-function \
                           -DSTB_IMAGE_IMPLEMENTATION \
                           -DSTB_IMAGE_RESIZE_IMPLEMENTATION \
                           -DFPLBASE_ANDROID_VR=0

FPLBASE_COMMON_LIBRARIES := \
  libwebp \
  libmathfu

FPLBASE_COMMON_LDLIBS := \
  -lGLESv1_CM \
  -lGLESv2 \
  -lGLESv3 \
  -llog \
  -lz \
  -lEGL \
  -landroid \
  -latomic

FPLBASE_SCHEMA_DIR := $(FPLBASE_DIR)/schemas
FPLBASE_SCHEMA_INCLUDE_DIRS :=

FPLBASE_SCHEMA_FILES := \
  $(FPLBASE_SCHEMA_DIR)/common.fbs \
  $(FPLBASE_SCHEMA_DIR)/materials.fbs \
  $(FPLBASE_SCHEMA_DIR)/mesh.fbs \
  $(FPLBASE_SCHEMA_DIR)/shader.fbs \
  $(FPLBASE_SCHEMA_DIR)/texture_atlas.fbs

FPLBASE_COMMON_EXPORT_C_INCLUDES := \
  $(FPLBASE_DIR)/include \
  $(FPLBASE_GENERATED_OUTPUT_DIR) \
  $(DEPENDENCIES_FPLUTIL_DIR)/libfplutil/include \
  $(NDK_ROOT)/sources/android/ndk_helper

FPLBASE_COMMON_INCLUDES := \
  $(FPLBASE_COMMON_EXPORT_C_INCLUDES) \
  $(FPLBASE_DIR)/src \
  $(DEPENDENCIES_FLATBUFFERS_DIR)/include \
  $(DEPENDENCIES_WEBP_DIR)/src \
  $(DEPENDENCIES_STB_DIR)


include $(CLEAR_VARS)
# fplbase_main is a separate build target from fplbase to avoid a circular
# dependency between SDL and fplbase.  This allows SDL to link to SDL_main
# in fplbase_main.
LOCAL_MODULE := fplbase_main
LOCAL_C_INCLUDES := \
  $(FPLBASE_COMMON_EXPORT_C_INCLUDES) \
  $(DEPENDENCIES_SDL_DIR)/include
# If the JNI_OnLoad function list isn't overload use the default set.
ifeq ($(FPLBASE_JNI_ONLOAD_FUNCTIONS),)
FPLBASE_JNI_ONLOAD_FUNCTIONS := SDL_JNI_OnLoad
endif
LOCAL_CPPFLAGS := \
  -D'FPLBASE_JNI_ONLOAD_FUNCTIONS(X)=$(foreach \
       func,$(FPLBASE_JNI_ONLOAD_FUNCTIONS),X($(func)) )'

LOCAL_SRC_FILES := src/main_sdl.cpp
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := fplbase
LOCAL_ARM_MODE := arm
LOCAL_STATIC_LIBRARIES := \
  $(FPLBASE_COMMON_LIBRARIES) \
  libSDL2_static \
  fplbase_main

LOCAL_CPPFLAGS := $(FPLBASE_COMMON_CPPFLAGS)

# NOTE: Static library dependencies don't need to be exported, they're
# automatically included by ndk-build.
LOCAL_EXPORT_LDLIBS := $(FPLBASE_COMMON_LDLIBS)

LOCAL_EXPORT_C_INCLUDES := \
  $(FPLBASE_COMMON_EXPORT_C_INCLUDES)

LOCAL_EXPORT_CPPFLAGS := $(FPLBASE_EXPORT_COMMON_CPPFLAGS)

LOCAL_C_INCLUDES := $(FPLBASE_COMMON_INCLUDES) $(DEPENDENCIES_SDL_DIR)

LOCAL_SRC_FILES := \
  $(FPLBASE_COMMON_SRC_FILES) \
  src/async_loader_sdl.cpp \
  src/environment_sdl.cpp \
  src/input_sdl.cpp \
  src/renderer_android.cpp \
  src/utilities_sdl.cpp

ifeq (,$(FPLBASE_RUN_ONCE))
FPLBASE_RUN_ONCE := 1
$(call flatbuffers_header_build_rules, \
  $(FPLBASE_SCHEMA_FILES), \
  $(FPLBASE_SCHEMA_DIR), \
  $(FPLBASE_GENERATED_OUTPUT_DIR), \
  $(FPLBASE_SCHEMA_INCLUDE_DIRS), \
  $(LOCAL_SRC_FILES), \
  fplbase_generated_includes)
endif
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := fplbase_stdlib
LOCAL_ARM_MODE := arm
LOCAL_STATIC_LIBRARIES := \
  $(FPLBASE_COMMON_LIBRARIES)

LOCAL_CPPFLAGS := \
  $(FPLBASE_COMMON_CPPFLAGS) \
  -DFPLBASE_BACKEND_STDLIB \

LOCAL_EXPORT_C_INCLUDES := \
  $(FPLBASE_COMMON_EXPORT_C_INCLUDES)

LOCAL_EXPORT_LDLIBS := $(FPLBASE_COMMON_LDLIBS)

LOCAL_EXPORT_CPPFLAGS := $(FPLBASE_EXPORT_COMMON_CPPFLAGS)

LOCAL_C_INCLUDES := \
  $(FPLBASE_COMMON_INCLUDES)

LOCAL_SRC_FILES := \
  $(FPLBASE_COMMON_SRC_FILES) \
  src/async_loader_stdlib.cpp \
  src/environment_stdlib.cpp \
  src/input_stdlib.cpp \
  src/renderer_android.cpp \
  src/utilities_stdlib.cpp

ifeq (,$(FPLBASE_RUN_ONCE))
FPLBASE_RUN_ONCE := 1
$(call flatbuffers_header_build_rules, \
  $(FPLBASE_SCHEMA_FILES), \
  $(FPLBASE_SCHEMA_DIR), \
  $(FPLBASE_GENERATED_OUTPUT_DIR), \
  $(FPLBASE_SCHEMA_INCLUDE_DIRS), \
  $(LOCAL_SRC_FILES), \
  fplbase_generated_includes)
endif
include $(BUILD_STATIC_LIBRARY)


$(call import-add-path,$(DEPENDENCIES_FLATBUFFERS_DIR)/..)
$(call import-add-path,$(DEPENDENCIES_MATHFU_DIR)/..)
$(call import-add-path,$(DEPENDENCIES_WEBP_DIR)/..)
# NOTE: $(DEPENDENCIES_SDL_DIR) is not included here as we have patched the
# makefile in jni/sdl/Android.mk
$(call import-add-path,$(FPLBASE_DIR)/..)

$(call import-module,flatbuffers/android/jni)
$(call import-module,mathfu/jni)
$(call import-module,android/cpufeatures)
$(call import-module,$(notdir $(DEPENDENCIES_WEBP_DIR)))
# NOTE: We're including $(FPLBASE_DIR)/jni/SDL/Android.mk here.
$(call import-module,fplbase/jni/sdl)
