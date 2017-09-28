
#define PREAMBLE_TEXT \
"// Comment.\n" \
"\n" \
"#if defined (SOMETHING)\n" \
"// Comment 2.\n" \
"#extension GL_OES_standard_derivatives : enable\n" \
"#endif\n" \
"\n" \
"//-----\n" \
"// Large comment\n" \
"//-----\n" \
"\n"

#define CODE_TEXT \
"#if defined (FOO)\n" \
"// Comment 3\n" \
"varying highp vec2 vTexCoord;\n" \
"#endif\n"

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
