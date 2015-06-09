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

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := fplbase
LOCAL_ARM_MODE := arm
LOCAL_STATIC_LIBRARIES := \
  libwebp \
  libmathfu \
  SDL2 \
  libgpg

FPLBASE_RELATIVE_DIR := ..
FPLBASE_DIR := $(LOCAL_PATH)/$(FPLBASE_RELATIVE_DIR)

include $(FPLBASE_DIR)/jni/android_config.mk
include $(DEPENDENCIES_FLATBUFFERS_DIR)/android/jni/include.mk

LOCAL_EXPORT_C_INCLUDES := \
  $(FPLBASE_DIR)/include \
  $(FPLBASE_GENERATED_OUTPUT_DIR) \

LOCAL_C_INCLUDES := \
  $(LOCAL_EXPORT_C_INCLUDES) \
  $(FPLBASE_DIR)/src \
  $(DEPENDENCIES_FLATBUFFERS_DIR)/include \
  $(DEPENDENCIES_FPLUTIL_DIR)/include \
  $(DEPENDENCIES_GPG_DIR)/include \
  $(DEPENDENCIES_SDL_DIR) \
  $(DEPENDENCIES_WEBP_DIR)/src

LOCAL_SRC_FILES := \
  $(FPLBASE_RELATIVE_DIR)/src/async_loader.cpp \
  $(FPLBASE_RELATIVE_DIR)/src/input.cpp \
  $(FPLBASE_RELATIVE_DIR)/src/main.cpp \
  $(FPLBASE_RELATIVE_DIR)/src/material.cpp \
  $(FPLBASE_RELATIVE_DIR)/src/material_manager.cpp \
  $(FPLBASE_RELATIVE_DIR)/src/mesh.cpp \
  $(FPLBASE_RELATIVE_DIR)/src/precompiled.cpp \
  $(FPLBASE_RELATIVE_DIR)/src/renderer.cpp \
  $(FPLBASE_RELATIVE_DIR)/src/renderer_android.cpp \
  $(FPLBASE_RELATIVE_DIR)/src/renderer_hmd.cpp \
  $(FPLBASE_RELATIVE_DIR)/src/shader.cpp \
  $(FPLBASE_RELATIVE_DIR)/src/utilities.cpp

FPLBASE_SCHEMA_DIR := $(FPLBASE_DIR)/schemas
FPLBASE_SCHEMA_INCLUDE_DIRS :=

FPLBASE_SCHEMA_FILES := \
  $(FPLBASE_SCHEMA_DIR)/common.fbs \
  $(FPLBASE_SCHEMA_DIR)/materials.fbs \
  $(FPLBASE_SCHEMA_DIR)/mesh.fbs

ifeq (,$(FPLBASE_RUN_ONCE))
FPLBASE_RUN_ONCE := 1
$(call flatbuffers_header_build_rules, \
  $(FPLBASE_SCHEMA_FILES), \
  $(FPLBASE_SCHEMA_DIR), \
  $(FPLBASE_GENERATED_OUTPUT_DIR), \
  $(FPLBASE_SCHEMA_INCLUDE_DIRS), \
  $(LOCAL_SRC_FILES))
endif

include $(BUILD_STATIC_LIBRARY)

$(call import-add-path,$(DEPENDENCIES_FLATBUFFERS_DIR)/..)
$(call import-add-path,$(DEPENDENCIES_MATHFU_DIR)/..)
$(call import-add-path,$(DEPENDENCIES_WEBP_DIR)/..)

$(call import-module,flatbuffers/android/jni)
$(call import-module,mathfu/jni)
$(call import-module,android/cpufeatures)
$(call import-module,webp)

