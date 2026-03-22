// Global per-frame interface (set 0) — shared by ALL shader interfaces.
// Every interface .glsl file MUST #include this file.
// Do NOT add #version or main() here.

struct PointLight {
  vec3 position;
  float intensity;
  vec3 color;
  float attenuation_radius;
};

layout(set = 0, binding = 0, row_major) uniform UniformBufferObject {
  mat4 view;
  mat4 proj;
  PointLight point_lights[16];
  vec3 camera_position;
  int point_light_count;
}
ubo;

layout(push_constant) uniform PushConstants { layout(row_major) mat4 model; }
pc;
