// The push constant block: one model matrix, vertex stage. Do NOT add #version
// or main() here.
//
// Used by the one-draw-per-object paths (debug wireframe, pick). The instanced
// scene path uses interface.instancing.glsl instead — see the note there.
//
// The range is currently taken from whichever layout is passed to
// ShaderInterface::Create as the global layout, so every pipeline declares it
// whether or not its shaders include this file. Phase 3 of
// local_dev/GLOBAL_INTERFACE_REFACTOR_PLAN.md makes the range per shader.
#ifndef LUMINA_BLOCK_MODEL_PUSH_CONSTANT_GLSL
#define LUMINA_BLOCK_MODEL_PUSH_CONSTANT_GLSL

layout(push_constant) uniform PushConstants { layout(row_major) mat4 model; }
pc;

#endif
