// standard_lit vertex interface — include this in vertex shaders that use the
// standard lit pipeline (pos/color/tex vertex inputs).
// Do NOT add #version or main() here.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
