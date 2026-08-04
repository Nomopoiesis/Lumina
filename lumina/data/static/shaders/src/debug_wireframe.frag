#version 450

#include "interface.global.glsl"

#include "../interfaces/color_output.frag.glsl"

void main() {
  outFragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
