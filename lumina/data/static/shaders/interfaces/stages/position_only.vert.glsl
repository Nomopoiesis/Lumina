
// The model matrix arrives as a push constant rather than through
// interface.instancing.glsl: every user of this stage interface is a
// one-draw-per-object path (debug wireframe, selection mask), not an instanced
// one. Including it here is what puts the range into this stage's reflected
// ShaderLayout, which is where ShaderInterface::Create now reads it from.
#include "blocks/model_push_constant.glsl"

layout(location = 0) in vec3 inPosition;
