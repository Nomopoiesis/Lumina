#version 450

#include "interface.global.glsl"

#include "../interfaces/simple_model_input.vert.glsl"

void main() {
  mat4 model = instance_data.models[gl_InstanceIndex];
  vec4 wolrd_pos = vec4(inPosition, 1.0) * model;
  gl_Position = wolrd_pos * frame_globals.view * frame_globals.proj;
  fragWorldPosition = wolrd_pos.xyz;
  fragWorldNormal = normalize(inNormal * mat3(model));
  fragTexCoord = inTexCoord;
}
