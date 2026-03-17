#pragma once

#include "core/resource_manager_handle.hpp"

namespace lumina::renderer {

// Forward declaration — full definition in graphics_pipeline.hpp
struct GraphicsPipeline;
using GraphicsPipelineHandle = core::ResourceHandle<GraphicsPipeline>;

} // namespace lumina::renderer