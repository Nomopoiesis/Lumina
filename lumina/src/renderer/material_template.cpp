#include "material_template.hpp"

#include "lumina_util.hpp"

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

namespace lumina::renderer {

auto MaterialTemplate::Create(const MaterialTemplateDescription &description)
    -> std::expected<MaterialTemplate, MaterialTemplateCreateError> {
  if (description.shader_interface == nullptr) {
    return std::unexpected(
        MaterialTemplateCreateError{.message = "Shader interface is required"});
  }

  if (description.vertex_shader_bin_path.empty() ||
      description.fragment_shader_bin_path.empty()) {
    return std::unexpected(MaterialTemplateCreateError{
        .message = "Vertex and fragment shader binary paths are required"});
  }

  MaterialTemplate material_template;
  material_template.shader_interface = description.shader_interface;
  material_template.vertex_shader_bin_path = description.vertex_shader_bin_path;
  material_template.fragment_shader_bin_path =
      description.fragment_shader_bin_path;
  return material_template;
}

} // namespace lumina::renderer