#version 450

#include "interface.global.glsl"

#include "../interfaces/simple_model_input.vert.glsl"

void main() {
  gl_Position = vec4(inPosition, 1.0) * pc.model * ubo.view * ubo.proj;
  fragNormal = inNormal;
  fragTexCoord = inTexCoord;
}
