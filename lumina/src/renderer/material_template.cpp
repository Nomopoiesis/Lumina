#include "material_template.hpp"

#include "lumina_util.hpp"

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

namespace lumina::renderer {

auto MaterialTemplate::Create(const std::string &vertex_shader_bin_path,
                              const std::string &fragment_shader_bin_path,
                              ShaderInterface &shader_interface)
    -> std::expected<MaterialTemplate, MaterialTemplateCreateError> {
  if (vertex_shader_bin_path.empty() || fragment_shader_bin_path.empty()) {
    return std::unexpected(MaterialTemplateCreateError{
        .message = "Vertex and fragment shader binary paths are required"});
  }

  MaterialTemplate material_template;
  material_template.shader_interface = &shader_interface;
  material_template.vertex_shader_bin_path = vertex_shader_bin_path;
  material_template.fragment_shader_bin_path = fragment_shader_bin_path;
  return material_template;
}

} // namespace lumina::renderer