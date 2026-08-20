// Per-frame instance model matrices (set 0, binding 1) — the instanced scene
// path only. Do NOT add #version or main() here.
//
// This is an alternative to the push-constant model matrix in
// interface.push_constants.glsl, not a companion to it: a shader either reads
// its matrix out of this array with gl_InstanceIndex or has one pushed, never
// both.
#ifndef LUMINA_BLOCK_INSTANCING_GLSL
#define LUMINA_BLOCK_INSTANCING_GLSL

layout(set = 0, binding = 1, row_major) readonly buffer InstanceData {
  mat4 models[];
} instance_data;

#endif
