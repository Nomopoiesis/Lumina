#pragma once

#include "core/resource_manager_handle.hpp"

namespace lumina::renderer {

// Forward declaration — full definition in material_instance.hpp
class MaterialInstance;
using MaterialInstanceHandle = core::ResourceHandle<MaterialInstance>;

} // namespace lumina::renderer