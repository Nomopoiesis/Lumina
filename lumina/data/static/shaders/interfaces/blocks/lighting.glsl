// Scene lighting (set 0, binding 2). Do NOT add #version or main() here.
//
// Its own binding rather than a member of FrameGlobals: only lit shaders read
// it, and separating it is what makes the eventual lighting work — storage
// buffer instead of a fixed array, clustered assignment, directional/spot
// separation — a change to this one file rather than to the block every 3D
// shader includes.
//
// The 16-element array and the hard attenuation cut are unchanged from when
// this lived in FrameGlobals. See local_dev/GLOBAL_INTERFACE_REFACTOR_PLAN.md
// §7: relocating it is phase 2, redesigning it is later work.
#ifndef LUMINA_BLOCK_LIGHTING_GLSL
#define LUMINA_BLOCK_LIGHTING_GLSL

struct PointLight {
  vec3 position;
  float intensity;
  vec3 color;
  float attenuation_radius;
};

layout(set = 0, binding = 2, row_major) uniform Lighting {
  PointLight point_lights[16];
  int point_light_count;
}
lighting;

#endif
