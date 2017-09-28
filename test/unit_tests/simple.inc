#define PRE_VERSION_TEXT \
"// #if Single line comment.\n" \
"/* #version\n" \
"within a\n" \
"  multi-line comment should be ignored */\n" \
"// A multi-part single-line comment should also be ignored \\\n" \
"#version 900 is still part of the comment\n" \
"\n"

#define VERSION_TEXT \
"#version 100  // The actual version.\n"

#define POST_VERSION_TEXT \
"#define TEST_A 1\n" \
"#define TEST_B 2\n" \
"\n" \
"#if GL_ES\n" \
"// No code can come before #extension directives.\n" \
"#extension GL_OES_standard_derivatives : enable\n" \
"#endif\n" \
"\n"

#define MAIN_TEXT \
"void main() { gl_FragColor = vec4(1, 1, 1, 1); }\n"

const char kSourceText[] =
PRE_VERSION_TEXT
VERSION_TEXT
POST_VERSION_TEXT
MAIN_TEXT;

const char kExpectedText[] =
DEFAULT_DEFINES_TEXT
PRE_VERSION_TEXT
// Version string should have been moved to top of file.
POST_VERSION_TEXT
DEFAULT_PRECISION_TEXT
MAIN_TEXT;

#undef PRE_VERSION_TEXT
#undef POST_VERSION_TEXT
#undef MAIN_TEXT
