#version 450

#include "interface.global.glsl"

#include "../interfaces/simple_model_input.vert.glsl"

void main() {
  vec4 wolrd_pos = vec4(inPosition, 1.0) * pc.model;
  gl_Position = wolrd_pos * ubo.view * ubo.proj;
  fragWorldPosition = wolrd_pos.xyz;
  fragWorldNormal = normalize(inNormal * mat3(pc.model));
  fragTexCoord = inTexCoord;
}
