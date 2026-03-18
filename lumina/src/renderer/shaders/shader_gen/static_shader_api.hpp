#pragma once

#include "renderer.hpp"

namespace lumina::renderer {

auto BuildStaticMaterialTemplates(LuminaRenderer *renderer) -> void;

auto CreateGlobalDescriptorSetLayout(LuminaRenderer *renderer)
    -> std::expected<void, VkInitializationError>;

auto GetGlobalDescriptorPoolSizes(std::vector<VkDescriptorPoolSize> &pool_sizes)
    -> void;

} // namespace lumina::renderer