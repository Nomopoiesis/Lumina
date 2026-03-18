#version 450

#include "interface.global.glsl"

#include "../interfaces/standard_lit.frag.glsl"

void main() { outColor = texture(texSampler, fragTexCoord * 2.0); }
