uniform sampler2D texture_unit_name_00000;
varying vec2 v_uv;

void main() {
  gl_FragColor = texture2D(texture_unit_name_00000, v_uv);
}
