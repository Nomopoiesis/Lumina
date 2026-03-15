#pragma once

#include <atomic>
#include <vulkan/vulkan.h>

#include "math/matrix.hpp"
#include "render_mesh.hpp"
#include "vulkan_context.hpp"

#include <memory>
#include <vector>

namespace lumina::renderer {

static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

struct FrameContextCreationError {
  std::string message;
};

enum class FrameContextPipelineState : u8 {
  IDLE,
  UPDATE,
  UPDATE_COMPLETE,
  RENDER,
  RENDER_COMPLETE,
};

struct DrawMeshInfo {
  RenderMeshHandle render_mesh_handle;
  math::Mat4 model;
};

struct FrameContextUniformBuffer {
  FrameContextUniformBuffer() noexcept = default;
  FrameContextUniformBuffer(const FrameContextUniformBuffer &) = delete;
  auto operator=(const FrameContextUniformBuffer &)
      -> FrameContextUniformBuffer & = delete;
  FrameContextUniformBuffer(FrameContextUniformBuffer &&other) noexcept =
      default;
  auto operator=(FrameContextUniformBuffer &&other) noexcept
      -> FrameContextUniformBuffer & = default;
  ~FrameContextUniformBuffer() noexcept = default;

  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  void *mapped = nullptr;
};

class FrameContext {
public:
  FrameContext(VulkanContext &vulkan_context) noexcept
      : vulkan_context(vulkan_context) {}
  ~FrameContext() noexcept;

  FrameContext(FrameContext &&other) noexcept = delete;
  auto operator=(FrameContext &&other) noexcept -> FrameContext & = delete;

  FrameContext(const FrameContext &) = delete;
  auto operator=(const FrameContext &) -> FrameContext & = delete;

  static auto Create(VulkanContext &vulkan_context, VkCommandPool command_pool)
      -> std::expected<std::unique_ptr<FrameContext>,
                       FrameContextCreationError>;

  [[nodiscard]] auto GetCommandBuffer() const noexcept
      -> const VkCommandBuffer & {
    return command_buffer;
  }

  [[nodiscard]] auto GetFrameBeginSemaphore() const noexcept
      -> const VkSemaphore & {
    return frame_begin_semaphore;
  }

  [[nodiscard]] auto GetFrameBeginReadyFence() const noexcept
      -> const VkFence & {
    return frame_begin_ready_fence;
  }

  // The state of the pipeline for this frame context
  // This is used to determine which pipeline to use for this frame context
  // IDLE: The pipeline is idle
  // UPDATE: The pipeline is updating the uniform buffer
  // RENDER: The pipeline is rendering the scene
  std::atomic<FrameContextPipelineState> pipeline_state =
      FrameContextPipelineState::IDLE;

  auto GetUniformBuffer() noexcept -> FrameContextUniformBuffer & {
    return uniform_buffer;
  }

  std::vector<DrawMeshInfo> render_draw_list;

private:
  VulkanContext &vulkan_context;

  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  VkSemaphore frame_begin_semaphore = VK_NULL_HANDLE;
  VkFence frame_begin_ready_fence = VK_NULL_HANDLE;

  FrameContextUniformBuffer uniform_buffer;
};

} // namespace lumina::renderer
