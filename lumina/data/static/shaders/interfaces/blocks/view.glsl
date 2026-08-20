// Per-frame view data (set 0, binding 0) — everything that draws with the
// frame camera. Do NOT add #version or main() here.
#ifndef LUMINA_BLOCK_VIEW_GLSL
#define LUMINA_BLOCK_VIEW_GLSL

layout(set = 0, binding = 0, row_major) uniform FrameGlobals {
  mat4 view;
  mat4 proj;
  vec3 camera_position;
}
frame_globals;

#endif
