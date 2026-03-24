#version 450

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;
layout(location = 3) in uint in_mode;

layout(push_constant) uniform PushConstants {
  vec2 screen_size;
} pc;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_color;
layout(location = 2) out flat uint frag_mode;

void main() {
  // Map screen-space pixel coords [0, screen_size] → NDC [-1, 1].
  // UIRenderer always sets a standard (non-flipped) viewport before drawing,
  // so the simple linear mapping is correct here.
  vec2 ndc = (in_position / pc.screen_size) * 2.0 - 1.0;
  gl_Position = vec4(ndc, 0.0, 1.0);
  frag_uv = in_uv;
  frag_color = in_color;
  frag_mode = in_mode;
}
