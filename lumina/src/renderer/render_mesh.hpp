#pragma once

#include "core/resource_manager.hpp"

#include <vulkan/vulkan.h>

namespace lumina::renderer {

struct RenderVertexStream {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  size_t stride = 0;
};

class RenderMesh {
public:
  RenderMesh() noexcept = default;
  RenderMesh(const RenderMesh &other) noexcept = default;
  RenderMesh(RenderMesh &&other) noexcept = default;
  auto operator=(const RenderMesh &other) noexcept -> RenderMesh & = default;
  auto operator=(RenderMesh &&other) noexcept -> RenderMesh & = default;
  ~RenderMesh() noexcept = default;

  size_t vertex_count;
  std::vector<RenderVertexStream> vertex_streams;

  bool ready = false;
  size_t index_count = 0;
  VkBuffer index_buffer = VK_NULL_HANDLE;
  VkDeviceMemory index_buffer_memory = VK_NULL_HANDLE;
};

using RenderMeshHandle = core::ResourceHandle<RenderMesh>;
using RenderMeshManager = core::ResourceManager<RenderMesh>;

} // namespace lumina::renderer