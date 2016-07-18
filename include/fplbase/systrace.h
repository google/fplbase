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

#ifndef FPLBASE_SYSTRACE_H
#define FPLBASE_SYSTRACE_H

/// @file fplbase/systrace.h
/// @brief Functions for creating systrace log events, for Android.
///
/// To enable, \#define FPLBASE_ENABLE_SYSTRACE 1.
/// @addtogroup fplbase_systrace
/// @{

#if FPLBASE_ENABLE_SYSTRACE
#ifndef __ANDROID__
#error Systrace is only suppported for Android Builds
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif  // FPLBASE_ENABLE_SYSTRACE

#define MAX_SYSTRACE_LEN 256
int trace_marker = -1;

/// @brief Initializes the settings for systrace.
///
/// This needs to be called before any other systrace call.
void SystraceInit() {
#if FPLBASE_ENABLE_SYSTRACE
  trace_marker = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY);
  // Check that it didn't fail:
  assert(trace_marker == -1);
#endif
}

/// @brief Start a block, with the supplied name.
///
/// This will last until SystraceEnd is called. Nesting is supported!
///
/// @param name The name of the block, which will appear in the trace.
inline void SystraceBegin(const char *name) {
  (void)name;
#if FPLBASE_ENABLE_SYSTRACE
  char buf[MAX_SYSTRACE_LEN];
  int len = snprintf(buf, MAX_SYSTRACE_LEN, "B|%d|%s", getpid(), name);
  write(trace_marker, buf, len);
#endif
}

/// @brief Ends the most recently begun block.
inline void SystraceEnd() {
#if FPLBASE_ENABLE_SYSTRACE
  char c = 'E';
  write(trace_marker, &c, 1);
#endif
}

/// @brief Logs a value.
///
/// This will be displayed on a graph in the systrace.
///
/// @param name The name that will be logged.
/// @param value The value to be logged with the given name.
inline void SystraceCounter(const char *name, const int value) {
  (void)name;
  (void)value;
#if FPLBASE_ENABLE_SYSTRACE
  char buf[MAX_SYSTRACE_LEN];
  int len =
      snprintf(buf, MAX_SYSTRACE_LEN, "C|%d|%s|%i", getpid(), name, value);
  write(trace_marker, buf, len);
#endif
}

/// @brief Begins an asynchronous block.
///
/// Name/Cookie need to be unique per block.
///
/// @param name The name of the block, which will appear in the trace.
/// @param cookie Part of the unique identifier to note the block.
inline void SystraceAsyncBegin(const char *name, const int32_t cookie) {
  (void)name;
  (void)cookie;
#if FPLBASE_ENABLE_SYSTRACE
  char buf[MAX_SYSTRACE_LEN];
  int len =
      snprintf(buf, MAX_SYSTRACE_LEN, "S|%d|%s|%i", getpid(), name, cookie);
  write(trace_marker, buf, len);
#endif
}

/// @brief Ends an asynchronous block.
///
/// @param name The name of the block, which needs to match the one used to
///             begin the block.
/// @param cookie The unique identifier of the block, which needs to match the
///               one used to begin the block.
inline void SystraceAsyncEnd(const char *name, const int32_t cookie) {
  (void)name;
  (void)cookie;
#if FPLBASE_ENABLE_SYSTRACE
  char buf[MAX_SYSTRACE_LEN];
  int len =
      snprintf(buf, MAX_SYSTRACE_LEN, "F|%d|%s|%i", getpid(), name, cookie);
  write(trace_marker, buf, len);
#endif
}

/// @}
#endif  // FPLBASE_SYSTRACE_H
