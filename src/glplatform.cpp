#include "fplbase/glplatform.h"

#include <assert.h>
#include <stdio.h>

#if defined(__ANDROID__)
#include <android/log.h>
#endif  // defined(__ANDROID__)

void LogGLError(const char *file, int line, const char *call) {
  auto err = glGetError();
  if (err == GL_NO_ERROR) return;
  const char *err_str = "<unknown error enum>";
  switch (err) {
    case GL_INVALID_ENUM:
      err_str = "GL_INVALID_ENUM";
      break;
    case GL_INVALID_VALUE:
      err_str = "GL_INVALID_VALUE";
      break;
    case GL_INVALID_OPERATION:
      err_str = "GL_INVALID_OPERATION";
      break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      err_str = "GL_INVALID_FRAMEBUFFER_OPERATION";
      break;
    case GL_OUT_OF_MEMORY:
      err_str = "GL_OUT_OF_MEMORY";
      break;
  }

#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_ERROR, "fplbase",
                      "%s(%d): OpenGL Error: %s from %s", file, line, err_str,
                      call);
#else
  fprintf(stderr, "%s(%d): OpenGL Error: %s from %s", file, line, err_str,
          call);
#endif

  assert(0);
}
