
#define SOURCE_TEXT \
"#ifdef GL_ES\n" \
"#extension GL_OES_EGL_image_external : require\n" \
"precision mediump float;\n" \
"#endif\n" \
"\n" \
"attribute vec4 aPosition;\n" \
"attribute vec2 aTexCoord;\n"

const char kSourceText[] =
SOURCE_TEXT;

// We're not expecting another default precision specifier to be inserted here
// since there's one in the source text.
const char kExpectedText[] =
DEFAULT_DEFINES_TEXT
SOURCE_TEXT;

#undef SOURCE_TEXT
