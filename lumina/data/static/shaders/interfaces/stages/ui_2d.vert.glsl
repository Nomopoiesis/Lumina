// Vertex interface for the screen-space UI pass. Do NOT add #version or main()
// here.
//
// Named ui_2d rather than 2d_ui because the file stem becomes a C++ namespace
// in the generated header, and an identifier cannot start with a digit.
#ifndef LUMINA_STAGE_UI_2D_VERT_GLSL
#define LUMINA_STAGE_UI_2D_VERT_GLSL

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

// Solid-colour vs text selector. Reflection derives a VertexAttributeType from
// this name, and "custom0" is the opt-in spelling for a channel with no
// rendering semantics of its own — see VertexAttributeType::Custom0. The
// varying it feeds keeps the readable name.
layout(location = 3) in uint in_custom0;

layout(push_constant) uniform PushConstants {
  vec2 screen_size;
} pc;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_color;
layout(location = 2) out flat uint frag_mode;

#endif
