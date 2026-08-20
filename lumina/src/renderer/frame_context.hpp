#pragma once

#include "vulkan_fwd.hpp"
#include <atomic>

#include "common/lumina_check.hpp"
#include "math/matrix.hpp"
#include "render_mesh.hpp"
#include "shaders/global_shader_family.hpp"
#include "ui_types.hpp"
#include "vulkan_context.hpp"

#include <array>
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

// One instanced draw: everything to bind comes from the draw item, and the
// instances occupy [first_instance, first_instance + instance_count) in the
// frame's instance buffer.
struct DrawMeshBatch {
  u32 draw_item_index;
  u32 first_instance;
  u32 instance_count;
};

// Debug wireframe boxes are still one draw per object with the model matrix in
// a push constant. They keep their own list rather than sharing the batch path
// because their template has no material instances and no set 1.
struct DrawDebugAABBCommand {
  RenderMeshHandle render_mesh_handle;
  math::Mat4 model;
};

// One object drawn into the pick target, one instance each. `mvp` is
// pre-multiplied model * view * pick_proj because the pick pass uses a
// projection the frame globals do not carry, and `slot` is 1-based so 0 stays
// the clear value meaning "nothing here".
//
// A draw item index rather than resolved objects, the same as DrawMeshBatch:
// resolving it reads the resource registries, which the render thread mutates
// in ProcessDeferredOperations, so that lookup belongs at record time.
struct PickDraw {
  math::Mat4 mvp;
  u32 draw_item_index = 0;
  u32 slot = 0;
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

// Per-frame data backing the Scene3D family's set 0. Grouped rather than held
// as loose members so a family's resources arrive as one struct instead of the
// member list growing every time a binding is added.
//
// instance_buffer is not in here yet: it has its own capacity-growth path and
// several call sites, and it joins when the family table lands.
struct Scene3DFrameResources {
  FrameContextUniformBuffer frame_globals; // set 0, binding 0
  FrameContextUniformBuffer lighting;      // set 0, binding 2
};

struct FrameContextInstanceBuffer {
  FrameContextInstanceBuffer() noexcept = default;
  FrameContextInstanceBuffer(const FrameContextInstanceBuffer &) = delete;
  auto operator=(const FrameContextInstanceBuffer &)
      -> FrameContextInstanceBuffer & = delete;
  FrameContextInstanceBuffer(FrameContextInstanceBuffer &&other) noexcept =
      default;
  auto operator=(FrameContextInstanceBuffer &&other) noexcept
      -> FrameContextInstanceBuffer & = default;
  ~FrameContextInstanceBuffer() noexcept = default;

  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  void *mapped = nullptr;
  u32 capacity = 0;
};

class FrameContext {
public:
  FrameContext(VulkanContext &vulkan_context_) noexcept
      : vulkan_context(vulkan_context_) {}
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

  auto SetTransientDescriptorPool(VkDescriptorPool descriptor_pool) noexcept
      -> void {
    LUMINA_CHECK(descriptor_pool != VK_NULL_HANDLE, "Invalid descriptor pool");
    frame_transient_descriptor_pool = descriptor_pool;
  }

  [[nodiscard]] auto GetTransientDescriptorPool() const noexcept
      -> const VkDescriptorPool & {
    return frame_transient_descriptor_pool;
  }

  auto SetTransientDescriptorSet(GlobalShaderFamily family,
                                 VkDescriptorSet descriptor_set) noexcept
      -> void {
    frame_transient_descriptor_sets[FamilyIndex(family)] = descriptor_set;
  }

  [[nodiscard]] auto
  GetTransientDescriptorSet(GlobalShaderFamily family) const noexcept
      -> const VkDescriptorSet & {
    return frame_transient_descriptor_sets[FamilyIndex(family)];
  }

  // The state of the pipeline for this frame context
  // This is used to determine which pipeline to use for this frame context
  // IDLE: The pipeline is idle
  // UPDATE: The pipeline is updating the uniform buffer
  // RENDER: The pipeline is rendering the scene
  std::atomic<FrameContextPipelineState> pipeline_state =
      FrameContextPipelineState::IDLE;

  auto GetShaderFamilyScene3DResources() noexcept -> Scene3DFrameResources & {
    return scene3d;
  }

  auto GetInstanceBuffer() noexcept -> FrameContextInstanceBuffer & {
    return instance_buffer;
  }

  UIRenderBatch ui_batch;

  // Both are cleared and refilled every frame rather than freed, so the steady
  // state allocates nothing. Recorded in this order: opaque batches first, then
  // debug wireframes on top.
  std::vector<DrawMeshBatch> draw_batches;
  std::vector<DrawDebugAABBCommand> debug_draws;

  // Non-empty only on a frame carrying a pick request, which the renderer's
  // in-flight guard limits to one at a time. The render thread reads this to
  // decide whether to record the pick pass, and again on reclaim to decide
  // whether a result is waiting in the readback buffer.
  std::vector<PickDraw> pick_draws;

private:
  VulkanContext &vulkan_context;

  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  VkSemaphore frame_begin_semaphore = VK_NULL_HANDLE;
  VkFence frame_begin_ready_fence = VK_NULL_HANDLE;

  VkDescriptorPool frame_transient_descriptor_pool = VK_NULL_HANDLE;
  std::array<VkDescriptorSet, kGlobalShaderFamilyCount>
      frame_transient_descriptor_sets{};

  Scene3DFrameResources scene3d;
  FrameContextInstanceBuffer instance_buffer;
};

} // namespace lumina::renderer
