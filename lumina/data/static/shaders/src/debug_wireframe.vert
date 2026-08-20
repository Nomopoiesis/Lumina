#version 450

#include "blocks/view.glsl"

#include "stages/position_only.vert.glsl"

void main() {
  gl_Position = vec4(inPosition, 1.0) * pc.model * frame_globals.view * frame_globals.proj;
}
