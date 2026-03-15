// standard_lit vertex interface — include this in vertex shaders that use the
// standard lit pipeline (UBO view/proj, push constant model, pos/color/tex).
// Do NOT add #version or main() here.

layout(binding = 0, set = 0, row_major) uniform UniformBufferObject {
  mat4 view;
  mat4 proj;
}
ubo;

layout(push_constant) uniform PushConstants { layout(row_major) mat4 model; }
pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
