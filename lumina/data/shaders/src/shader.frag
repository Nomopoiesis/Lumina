#version 450

#include "interface.global.glsl"

#include "simple_input_basic_mat.frag.glsl"

void main() {
  vec3 ambient =
      material_uniforms.ambient_intensity * material_uniforms.ambient_color;
  outFragColor = vec4(material_uniforms.diffuse_color, 1.0);
}
