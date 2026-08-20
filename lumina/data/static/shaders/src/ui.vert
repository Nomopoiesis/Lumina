#version 450

#include "stages/ui_2d.vert.glsl"

void main() {
  // Map screen-space pixel coords [0, screen_size] → NDC [-1, 1].
  // UIRenderer always sets a standard (non-flipped) viewport before drawing,
  // so the simple linear mapping is correct here.
  vec2 ndc = (in_position / pc.screen_size) * 2.0 - 1.0;
  gl_Position = vec4(ndc, 0.0, 1.0);
  frag_uv = in_uv;
  frag_color = in_color;
  frag_mode = in_custom0;
}
