#version 450

#include "interface.global.glsl"

#include "simple_input_basic_mat.frag.glsl"

void main() {
  vec3 normal = normalize(fragWorldNormal);
  vec3 ambient =
      material_uniforms.ambient_intensity * material_uniforms.ambient_color;
  PointLight point_light = ubo.point_lights[0];
  vec3 light_direction = normalize(point_light.position - fragWorldPosition);
  vec3 view_direction = normalize(ubo.camera_position - fragWorldPosition);
  vec3 reflect_direction = reflect(-light_direction, normal);
  float distance = length(point_light.position - fragWorldPosition);
  vec3 diffuse = vec3(0.0, 0.0, 0.0);
  vec3 specular = vec3(0.0, 0.0, 0.0);
  if (distance > point_light.attenuation_radius) {
    diffuse = vec3(0.0, 0.0, 0.0);
  } else {
    float diff_intensity = max(dot(normal, light_direction), 0.0);
    float spec_intensity = pow(max(dot(view_direction, reflect_direction), 0.0), 256);
    specular = 0.5 * spec_intensity * point_light.color;
    diffuse = diff_intensity * point_light.color;
  }
  vec3 result = (ambient + diffuse + specular) * material_uniforms.diffuse_color;
  result = clamp(result, 0.0, 1.0);
  outFragColor = vec4(result, 1.0);
}
