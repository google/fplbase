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

// Functions for creating systrace log events.
// To enable, #define ENABLE_SYSTRACE 1

#if ENABLE_SYSTRACE
#ifndef __ANDROID__
#error  Systrace is only suppported for Android Builds
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif  // ENABLE_SYSTRACE

#define MAX_SYSTRACE_LEN 256
int     trace_marker = -1;

// This needs to be called before other
void SystraceInit()
{
#if ENABLE_SYSTRACE
  trace_marker = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY);
  // Check that it didn't fail:
  assert(trace_marker == -1);
#endif
}

// Start a block, with the supplied name.  This will last until SystraceEnd is
// called.  (Nesting is supported!)
inline void SystraceBegin(const char *name) {
  (void) name;
#if ENABLE_SYSTRACE
  char buf[MAX_SYSTRACE_LEN];
  int len = snprintf(buf, MAX_SYSTRACE_LEN, "B|%d|%s", getpid(), name);
  write(trace_marker, buf, len);
#endif
}

// Ends the most recently begun block.
inline void SystraceEnd() {
#if ENABLE_SYSTRACE
  char c = 'E';
  write(trace_marker, &c, 1);
#endif
}

// Logs a value.  This will be displayed on a graph in the systrace.
inline void SystraceCounter(const char *name, const int value) {
  (void) name;
  (void) value;
#if ENABLE_SYSTRACE
    char buf[MAX_SYSTRACE_LEN];
    int len = snprintf(buf, MAX_SYSTRACE_LEN, "C|%d|%s|%i", getpid(), name, value);
    write(trace_marker, buf, len);
#endif
}

// Begins an asynchronous block.  Name/Cookie need to be unique per
// block.
inline void SystraceAsyncBegin(const char *name, const int32_t cookie) {
  (void) name;
  (void) cookie;
#if ENABLE_SYSTRACE
  char buf[MAX_SYSTRACE_LEN];
  int len = snprintf(buf, MAX_SYSTRACE_LEN, "S|%d|%s|%i", getpid(), name, cookie);
  write(trace_marker, buf, len);
#endif
}

// Ends an asynchronous block.
inline void SystraceAsyncEnd(const char *name, const int32_t cookie) {
  (void) name;
  (void) cookie;
#if ENABLE_SYSTRACE
  char buf[MAX_SYSTRACE_LEN];
  int len = snprintf(buf, MAX_SYSTRACE_LEN, "F|%d|%s|%i", getpid(), name, cookie);
  write(trace_marker, buf, len);
#endif
}

#endif  // FPLBASE_SYSTRACE_H
