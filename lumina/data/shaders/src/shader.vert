#version 450

#include "interface.global.glsl"

#include "../interfaces/standard_lit.vert.glsl"

void main() {
  gl_Position = vec4(inPosition, 1.0) * pc.model * ubo.view * ubo.proj;
  fragColor = inColor;
  fragTexCoord = inTexCoord;
}
