attribute vec4 in_position;
attribute vec2 in_uv;
uniform mat4 model;

varying vec2 v_uv;

void main() {
  gl_Position = model * in_position;
  v_uv = in_uv;
}
