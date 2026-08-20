
// Carries a pre-multiplied model * view * pick_proj rather than a model
// matrix — see pick_id.vert. It is still the same push block; only the
// contents differ, which is why this includes the shared fragment rather than
// declaring its own.
#include "blocks/model_push_constant.glsl"

layout(location = 0) in vec3 inPosition;

layout(location = 0) flat out uint vPickID;
