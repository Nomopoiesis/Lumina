#include "material_instance.hpp"

namespace lumina::renderer {

auto MaterialInstance::Create(MaterialTemplateHandle tmpl_handle) noexcept
    -> std::expected<MaterialInstance, MaterialInstanceCreateError> {
  MaterialInstance instance;
  instance.material_template_handle = tmpl_handle;
  return instance;
}

} // namespace lumina::renderer