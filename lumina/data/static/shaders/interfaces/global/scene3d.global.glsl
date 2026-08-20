// The Scene3D family: the union of the set 0 fragments used by shaders that
// draw world geometry with the frame camera, and the only file among them that
// is reflected. Do NOT add #version or main() here.
//
// Still named "global" for now. It is the descriptor contract of one family,
// not of the whole engine — the UI and post-process passes get their own — and
// phase 3.1 of local_dev/GLOBAL_INTERFACE_REFACTOR_PLAN.md renames it to
// interface.scene3d.glsl. The rename is deferred only so the C++ consumers are
// edited once, when they also gain the family parameter.
//
// This file exists for the C++ side: spirv_generate_interface.py reflects it
// into headers/interface.global.hpp, and CreateGlobalDescriptorSetLayout,
// GetGlobalDescriptorPoolSizes and WriteTransientDescriptors all iterate that
// header's bindings. Nothing else declares the family's set 0 in one place.
//
// Shaders include the individual fragments they use rather than this file. The
// fragments carry include guards, so pulling in both is harmless but pointless.

// interface.push_constants.glsl is deliberately absent. Push constant ranges
// are now per shader, derived from each stage's reflected ShaderLayout, and a
// range declared here would be forced onto every pipeline in the family
// whether its shaders push or not.
#include "blocks/view.glsl"
#include "blocks/instancing.glsl"
#include "blocks/lighting.glsl"
