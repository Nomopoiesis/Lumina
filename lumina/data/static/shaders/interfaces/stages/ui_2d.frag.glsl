// Fragment interface for the screen-space UI pass. Do NOT add #version or
// main() here.
//
// The font atlas is not declared here: it belongs to the Screen2D family at
// set 0 (interface.screen2d.global.glsl), because it is shared by every UI draw
// rather than being per-material. Shaders include both.
#ifndef LUMINA_STAGE_UI_2D_FRAG_GLSL
#define LUMINA_STAGE_UI_2D_FRAG_GLSL

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec4 frag_color;
layout(location = 2) in flat uint frag_mode;

layout(location = 0) out vec4 out_color;

#endif
