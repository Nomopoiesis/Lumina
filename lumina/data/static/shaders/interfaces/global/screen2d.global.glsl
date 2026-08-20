// The Screen2D family: set 0 for the screen-space UI pass. Do NOT add #version
// or main() here.
//
// One binding, so this file is both the family union that gets reflected and
// the only fragment in it. Shaders include it directly.
//
// The atlas lives at set 0 rather than in the per-material set because it is
// global to the pass: one atlas, sampled by every UI draw. Putting it in set 1
// would make every future UI material redeclare it.
#ifndef LUMINA_GLOBAL_SCREEN2D_GLSL
#define LUMINA_GLOBAL_SCREEN2D_GLSL

// R8_UNORM coverage for text; unused for solid-colour rectangles.
layout(set = 0, binding = 0) uniform sampler2D font_atlas;

#endif
