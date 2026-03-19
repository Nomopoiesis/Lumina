
layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(set = 1, binding = 1) uniform MaterialUniforms {
  float ambient_intensity;
  vec3 ambient_color;
  vec3 diffuse_color;
}
material_uniforms;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outFragColor;
