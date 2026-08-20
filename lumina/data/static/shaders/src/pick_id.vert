#version 450


#include "stages/pick_id.vert.glsl"

void main() {
  // pc.model carries a *pre-multiplied* model * view * pick_proj, not a model
  // matrix. The pick projection is not the one in frame_globals, which is the
  // whole point of the pass, so set 0 is never read and never bound.
  gl_Position = vec4(inPosition, 1.0) * pc.model;
  vPickID = uint(gl_InstanceIndex);
}
