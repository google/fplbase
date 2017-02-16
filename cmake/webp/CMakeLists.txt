# Copyright 2014 Google Inc. All rights reserved.
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
cmake_minimum_required(VERSION 2.8.12)

include(CheckIncludeFiles)

set(webp_dir ${dependencies_webp_distr_dir})
option(webp_build_cwebp "Build the cwebp command line tool." OFF)
option(webp_build_dwebp "Build the dwebp command line tool." OFF)

# Find the standard C math library.
if(webp_build_cwebp OR webp_build_dwebp)
  if(UNIX)
    find_library(MATH_LIBRARY NAMES m)
  endif()

  find_package(PNG)
  if(PNG_FOUND)
    set(WEBP_HAVE_PNG 1)
  endif()
  find_package(JPEG)
  if(JPEG_FOUND)
    set(WEBP_HAVE_JPEG 1)
  endif()
  find_package(TIFF)
  if(TIFF_FOUND)
    set(WEBP_HAVE_TIFF 1)
  endif()
  find_package(GIF)
  if(GIF_FOUND)
    set(WEBP_HAVE_GIF 1)
  endif()

  check_include_files(wincodec.h shlwapi.h windows.h WEBP_HAVE_WIC)
  configure_file(${CMAKE_CURRENT_LIST_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/webp/config.h)
endif()

FILE(GLOB webp_SRCS
    ${webp_dir}/src/demux/*
    ${webp_dir}/src/dec/*
    ${webp_dir}/src/utils/*
    ${webp_dir}/src/end/*
    ${webp_dir}/src/dsp/*)

# Includes for this project.
include_directories(${webp_dir}/src)
include_directories(${webp_dir}/include)
# Include config.h if the command line tools are being built.
if(webp_build_cwebp OR webp_build_dwebp)
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/include/webp)
endif()

# Executable target.
if(webp_build_cwebp OR webp_build_dwebp)
  add_definitions(-DHAVE_CONFIG_H)
endif()

add_library(webp ${webp_SRCS})

if(PNG_FOUND)
  include_directories(${PNG_INCLUDE_DIRS})
  target_link_libraries(webp ${PNG_LIBRARIES})
endif(PNG_FOUND)
if(JPEG_FOUND)
  include_directories(${JPEG_INCLUDE_DIRS})
  target_link_libraries(webp ${JPEG_LIBRARIES})
endif(JPEG_FOUND)
if(TIFF_FOUND)
  include_directories(${TIFF_INCLUDE_DIRS})
  target_link_libraries(webp ${TIFF_LIBRARIES})
endif(TIFF_FOUND)
if(GIF_FOUND)
  include_directories(${GIF_INCLUDE_DIRS})
  target_link_libraries(webp ${GIF_LIBRARIES})
endif(GIF_FOUND)
if(MATH_LIBRARY)
  target_link_libraries(webp ${MATH_LIBRARY})
endif(MATH_LIBRARY)

# Create a build rule for an example executable.
function(add_example_executable executable_name)
  add_executable(${executable_name} ${ARGN})
  target_link_libraries(${executable_name} exampleutil webp)
  if(PNG_FOUND)
    include_directories(${PNG_INCLUDE_DIRS})
    target_link_libraries(webp ${PNG_LIBRARIES})
  endif(PNG_FOUND)
  if(JPEG_FOUND)
    include_directories(${JPEG_INCLUDE_DIRS})
    target_link_libraries(webp ${JPEG_LIBRARIES})
  endif(JPEG_FOUND)
  if(TIFF_FOUND)
    include_directories(${TIFF_INCLUDE_DIRS})
    target_link_libraries(webp ${TIFF_LIBRARIES})
  endif(TIFF_FOUND)
  if(GIF_FOUND)
    include_directories(${GIF_INCLUDE_DIRS})
    target_link_libraries(webp ${GIF_LIBRARIES})
  endif(GIF_FOUND)
  if(MATH_LIBRARY)
    target_link_libraries(webp ${MATH_LIBRARY})
  endif(MATH_LIBRARY)
endfunction()

if(webp_build_cwebp OR webp_build_dwebp)
  # Example utility library.
  set(exampleutil_SRCS
      ${webp_dir}/examples/example_util.c
      ${webp_dir}/examples/example_util.h
      ${webp_dir}/examples/stopwatch.h)
  add_library(exampleutil ${exampleutil_SRCS})
endif()

if(webp_build_dwebp)
  # dwebp
  set(dwebp_SRCS
      ${webp_dir}/examples/dwebp.c
      ${webp_dir}/examples/stopwatch.h)
  add_example_executable(dwebp ${dwebp_SRCS})
endif()

if(webp_build_cwebp)
  # cwebp
  set(cwebp_SRCS
      ${webp_dir}/examples/cwebp.c
      ${webp_dir}/examples/metadata.c
      ${webp_dir}/examples/metadata.h
      ${webp_dir}/examples/stopwatch.h
      ${webp_dir}/examples/jpegdec.c
      ${webp_dir}/examples/jpegdec.h
      ${webp_dir}/examples/pngdec.c
      ${webp_dir}/examples/pngdec.h
      ${webp_dir}/examples/tiffdec.c
      ${webp_dir}/examples/tiffdec.h
      ${webp_dir}/examples/webpdec.c
      ${webp_dir}/examples/webpdec.h
      ${webp_dir}/examples/wicdec.c
      ${webp_dir}/examples/wicdec.h)
  add_example_executable(cwebp ${cwebp_SRCS})
endif()
