#pragma once

#include "platform/common/vulkan/vulkan_init_result.hpp"

#include "command_context.hpp"
#include "frame_context.hpp"
#include "graphics_pipeline.hpp"
#include "shaders/shader_interface.hpp"
#include "vulkan_context.hpp"

#include "core/static_mesh.hpp"
#include "render_mesh.hpp"

#include "common/data_structures/lock_free_concurrent_queue.hpp"
#include "common/data_structures/lock_free_object_pool.hpp"

#include <semaphore>
#include <thread>
#include <vector>

namespace lumina::renderer {

using CommandSubmissionQueue =
    common::data_structures::LockFreeConcurrentQueue<CommandContext *>;

using CommandContextPool =
    common::data_structures::LockFreeObjectPool<CommandContext>;

class LuminaRenderer {
public:
  LuminaRenderer(platform::common::vulkan::VkInitializationResult
                     vulkan_init_result) noexcept;

  LuminaRenderer(const LuminaRenderer &) = delete;
  auto operator=(const LuminaRenderer &) -> LuminaRenderer & = delete;
  LuminaRenderer(LuminaRenderer &&) noexcept = delete;
  auto operator=(LuminaRenderer &&) noexcept -> LuminaRenderer & = delete;

  ~LuminaRenderer() noexcept;

  auto Initialize() -> void;

  auto DrawFrame() -> void;

  auto Shutdown() -> void;

  auto DeviceWaitIdle() -> void;

  auto GetVulkanContext() const noexcept -> const VulkanContext & {
    return vulkan_context;
  }

  auto SetFramebufferResized(bool resized) -> void {
    is_framebuffer_resized = resized;
  }
  [[nodiscard]] auto IsFramebufferResized() const -> bool {
    return is_framebuffer_resized;
  }

  // Acquire a frame context for update (API exposed to the engine to coordinate
  // the frame update)
  auto AcquireFrameContextForUpdate() -> void;
  // Release a frame context for update (API exposed to the engine to coordinate
  // the frame update)
  auto ReleaseFrameContextForUpdate() -> void;

  [[nodiscard]] auto GetFrameContextForUpdate() noexcept -> FrameContext * {
    return frame_context_for_update;
  }

  auto AcquireCommandContext() -> CommandContext &;

  auto CreateGraphicsPipeline(const GraphicsPipelineDesc &desc)
      -> GraphicsPipelineHandle;

  auto CreateRenderMesh(const core::StaticMesh &mesh,
                        GraphicsPipelineHandle pipeline_handle)
      -> RenderMeshHandle;
  auto DestroyRenderMesh(RenderMeshHandle handle) -> void;

private:
  auto RenderThread() -> void;

  auto AcquireFrameContextForRender() -> void;
  auto ReleaseFrameContextForRender() -> void;

  auto TryReclaimFrameContexts() -> void;

  auto ReleaseCommandContext(CommandContext &cmd_ctx) -> void;

  auto SubmitCommandContext(CommandContext &cmd_ctx) -> void;

  auto PollAndExecuteCommandContexts() -> void;


  auto RecordCommandBuffer(FrameContext &frame_context,
                           u32 image_index) noexcept
      -> std::expected<void, VkInitializationError>;

  size_t current_frame_index = 0;

  bool is_framebuffer_resized = false;

  bool shutdown_requested = false;

  VulkanContext vulkan_context;

  std::thread render_thread;

  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> descriptor_sets;

  ShaderInterface shader_interface;

  VkCommandPool command_pool = VK_NULL_HANDLE;

  std::counting_semaphore<MAX_FRAMES_IN_FLIGHT> frames_available_for_update{
      MAX_FRAMES_IN_FLIGHT};
  std::counting_semaphore<MAX_FRAMES_IN_FLIGHT> frames_available_for_render{0};

  FrameContext *frame_context_for_update = nullptr;
  FrameContext *frame_context_for_render = nullptr;

  std::vector<std::unique_ptr<FrameContext>> frame_contexts;

  VkDeviceMemory texture_image_memory = VK_NULL_HANDLE;
  VkImage texture_image = VK_NULL_HANDLE;
  VkImageView texture_image_view = VK_NULL_HANDLE;
  VkSampler texture_sampler = VK_NULL_HANDLE;

  VkFormat depth_stencil_format = VK_FORMAT_UNDEFINED;
  VkDeviceMemory depth_image_memory = VK_NULL_HANDLE;
  VkImage depth_image = VK_NULL_HANDLE;
  VkImageView depth_image_view = VK_NULL_HANDLE;

  CommandContextPool command_context_pool;
  CommandSubmissionQueue global_submission_queue{256};
  std::vector<std::pair<VkFence, std::vector<CommandContext *>>>
      pending_submissions;

  RenderMeshManager render_mesh_manager;
  GraphicsPipelineManager pipeline_manager;
};

} // namespace lumina::renderer
