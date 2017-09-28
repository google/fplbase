
#define PREAMBLE_TEXT \
"#extension GL_OES_standard_derivatives : enable\n" \
"\n" \
"#include \"some/file.glslh\"\n" \
"\n"

#define CODE_TEXT \
"varying highp vec2 vTexCoord;\n" \
"uniform sampler2D texture_unit_0;\n" \
"uniform lowp vec4 color;\n"

const char kSourceText[] =
PREAMBLE_TEXT
CODE_TEXT;

const char kExpectedText[] =
DEFAULT_DEFINES_TEXT
PREAMBLE_TEXT
DEFAULT_PRECISION_TEXT
CODE_TEXT;

#undef PREAMBLE_TEXT
#undef CODE_TEXT
