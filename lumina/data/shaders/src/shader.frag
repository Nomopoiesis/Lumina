#version 450

#include "../interfaces/standard_lit.frag.glsl"

void main() {
  outColor = texture(texSampler, fragTexCoord * 2.0);
}
