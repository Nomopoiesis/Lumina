#pragma once

#include "material_template.hpp"
#include "platform/platform_common/vulkan/vulkan_init_result.hpp"

#include "command_context.hpp"
#include "device_retirement_queue.hpp"
#include "draw_item_registry.hpp"
#include "frame_context.hpp"
#include "graphics_pipeline.hpp"
#include "material_instance.hpp"
#include "material_instance_handle.hpp"
#include "shaders/shader_interface.hpp"
#include "ui_renderer.hpp"
#include "vulkan_context.hpp"

#include "core/static_mesh.hpp"
#include "core/texture.hpp"
#include "render_mesh.hpp"
#include "render_texture.hpp"

#include "common/data_structures/lock_free_concurrent_queue.hpp"
#include "common/data_structures/lock_free_object_pool.hpp"

#include <atomic>
#include <optional>
#include <semaphore>
#include <thread>
#include <unordered_set>
#include <vector>

namespace lumina::renderer {

// Side of the square pixel neighbourhood a pick samples, and therefore the side
// of the pick render target: one target pixel per screen pixel. Larger is more
// forgiving on thin geometry and costs nothing measurable at this size.
//
// Defined here rather than engine-side because both ends must agree — the
// renderer sizes the target with it and the engine derives the pick
// projection's zoom from it. Two constants that had to match would eventually
// stop matching.
inline constexpr u32 PICK_REGION_SIZE = 9;

using GraphicsPipelineManager = core::ResourceRegistry<GraphicsPipeline>;
using RenderMeshManager = core::ResourceRegistry<RenderMesh>;
using RenderTextureManager = core::ResourceRegistry<RenderTexture>;
using MaterialTemplateManager = core::ResourceRegistry<MaterialTemplate>;
using MaterialInstanceManager = core::ResourceRegistry<MaterialInstance>;

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
  auto DrawUI(FrameContext &frame_context, VkCommandBuffer command_buffer,
              u32 image_index) -> void;

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

  // Scene draw calls issued by the most recently recorded frame. Counted at
  // record time rather than taken from draw_list.size(), because commands whose
  // mesh, pipeline or material is not resolvable are skipped without drawing.
  // Excludes the UI pass, which this counter is not meant to measure.
  //
  // Written by the render thread, read by the update thread: relaxed is enough,
  // as nothing is ordered against it and a one-frame-stale value is expected.
  [[nodiscard]] auto GetRecordedDrawCallCount() const noexcept -> u32 {
    return recorded_draw_call_count.load(std::memory_order_relaxed);
  }

  auto AcquireCommandContext() -> CommandContext &;

  auto AddShaderInterface(ShaderInterface &&shader_interface) -> size_t {
    auto name = shader_interface.GetName();
    shader_interfaces.push_back(std::move(shader_interface));
    shader_interface_name_map[name] = shader_interfaces.size() - 1;
    return shader_interfaces.size() - 1;
  }

  auto CreateMaterialTemplate(const MaterialTemplateDescription &desc)
      -> MaterialTemplateHandle;

  auto CreateMaterialInstance(MaterialTemplateHandle tmpl_handle)
      -> MaterialInstanceHandle;

  // Byte offset of a material instance's slice within material_ubo_buffer.
  [[nodiscard]] auto GetMaterialUniformOffset(u32 slot) const -> VkDeviceSize {
    return static_cast<VkDeviceSize>(slot) * material_ubo_slot_stride;
  }

  // Mapped pointer to a material instance's slice, for writing its uniforms.
  [[nodiscard]] auto GetMaterialUniformData(u32 slot) const -> void * {
    return static_cast<u8 *>(material_ubo_mapped) +
           GetMaterialUniformOffset(slot);
  }

  [[nodiscard]] auto GetMaterialUniformBuffer() const -> VkBuffer {
    return material_ubo_buffer;
  }

  auto EnsureInstanceBufferCapacity(FrameContextInstanceBuffer &instance_buffer,
                                    size_t capacity) -> void;

  auto CreateRenderMesh(const core::StaticMesh &mesh,
                        MaterialTemplateHandle material_template)
      -> RenderMeshHandle;
  auto DestroyRenderMesh(RenderMeshHandle handle) -> void;

  auto CreateRenderTexture(const core::Texture &texture) -> RenderTextureHandle;
  auto DestroyRenderTexture(RenderTextureHandle handle) -> void;
  [[nodiscard]] auto GetRenderMesh(RenderMeshHandle handle) noexcept
      -> std::optional<const RenderMesh *>;
  [[nodiscard]] auto GetRenderTexture(RenderTextureHandle handle) noexcept
      -> std::optional<const RenderTexture *>;

  [[nodiscard]] auto GetDefaultMaterialInstanceHandle() const noexcept
      -> MaterialInstanceHandle {
    return default_material_instance_handle;
  }

  [[nodiscard]] auto GetDefaultMaterialTemplateHandle() const noexcept
      -> MaterialTemplateHandle {
    return default_material_template_handle;
  }

  [[nodiscard]] auto GetMaterialInstance(MaterialInstanceHandle handle) noexcept
      -> std::optional<const MaterialInstance *> {
    return material_instance_manager.Get(handle);
  }

  [[nodiscard]] auto GetMaterialTemplate(MaterialTemplateHandle handle) noexcept
      -> std::optional<const MaterialTemplate *> {
    return material_template_manager.Get(handle);
  }

  [[nodiscard]] auto GetMaterialTemplate(MaterialInstanceHandle handle) noexcept
      -> std::optional<const MaterialTemplate *> {
    auto instance_opt = GetMaterialInstance(handle);
    if (!instance_opt) {
      return std::nullopt;
    }
    return GetMaterialTemplate(instance_opt.value()->GetTemplateHandle());
  }

  auto SetDefaultMaterialTemplate(MaterialTemplateHandle handle) -> void {
    default_material_template_handle = handle;
  }

  auto SetDebugWireframeMaterialTemplate(MaterialTemplateHandle handle)
      -> void {
    debug_wireframe_material_template_handle = handle;
  }

  auto SetPickIdMaterialTemplate(MaterialTemplateHandle handle) -> void {
    pick_id_material_template_handle = handle;
  }

  [[nodiscard]] auto GetDebugWireframeMaterialTemplateHandle() const noexcept
      -> MaterialTemplateHandle {
    return debug_wireframe_material_template_handle;
  }

  [[nodiscard]] auto GetGlobalDescriptorSetLayout() const noexcept
      -> VkDescriptorSetLayout {
    return global_descriptor_set_layout;
  }

  [[nodiscard]] auto GetDrawItemRegistry() -> DrawItemRegistry & {
    return draw_item_registry;
  }

  // Callable from any thread — the watcher polls on the update thread. Only
  // enqueues: resolving which pipelines are affected reads pipeline_manager,
  // which the render thread mutates, so that scan happens in
  // DispatchShaderReloads instead.
  auto RequestShaderReload(std::string_view shader_bin_path) -> void;

  // Returns the slot of the most recent pick if it has not been seen yet, and
  // advances `last_sequence`. Slot 0 means the pick hit nothing.
  //
  // There is one readback buffer, so only one frame may carry a pick at a time.
  // That is enforced by the caller — see LuminaEngine::pick_in_flight — which
  // means **every path that abandons a pick must still publish a result here**,
  // or the caller never learns it may issue another. The two are RecordPickPass
  // giving up on an unready pipeline, and DrawFrame abandoning a frame on
  // swapchain recreation. Both are silent and permanent if missed.
  [[nodiscard]] auto TakePickResult(u32 &last_sequence) -> std::optional<u32> {
    const u64 packed = pick_result.load(std::memory_order_acquire);
    const auto sequence = static_cast<u32>(packed >> 32U);
    if (sequence == last_sequence) {
      return std::nullopt;
    }
    last_sequence = sequence;
    return static_cast<u32>(packed & 0xFFFFFFFFU);
  }

private:
  // Sequence and slot move together so a reader can never pair a fresh sequence
  // with a stale slot. Only one pick is ever in flight, so the
  // read-modify-write of the sequence has no competing writer.
  auto PublishPickResult(u32 slot) -> void {
    const u64 previous = pick_result.load(std::memory_order_relaxed);
    const u32 next_sequence = static_cast<u32>(previous >> 32U) + 1U;
    pick_result.store((static_cast<u64>(next_sequence) << 32U) | slot,
                      std::memory_order_release);
  }

  [[nodiscard]] auto GetShaderInterfaceFor(const MaterialTemplate &tmpl)
      -> ShaderInterface & {
    return shader_interfaces[tmpl.GetShaderInterfaceIndex()];
  }

  auto RenderThread() -> void;

  auto AcquireFrameContextForRender() -> void;
  auto ReleaseFrameContextForRender() -> void;

  auto TryReclaimFrameContexts() -> void;

  // The single exit from RENDER_COMPLETE back to IDLE.
  auto ReclaimFrameContext(FrameContext &frame_context) -> void;

  // Completes GPU work whose result the CPU asked for. Called once per render
  // loop iteration, above DrawFrame — see its definition for why that placement
  // is both required and sufficient.
  auto ProcessReadbacks() -> void;

  auto ProcessDeferredOperations() -> void;

  auto ReleaseCommandContext(CommandContext &cmd_ctx) -> void;

  auto SubmitCommandContext(CommandContext &cmd_ctx) -> void;

  auto PollAndExecuteCommandContexts() -> void;

  auto RecordCommandBuffer(FrameContext &frame_context,
                           VkCommandBuffer command_buffer,
                           u32 image_index) noexcept
      -> std::expected<void, VkInitializationError>;

  // Records the pick pass into the frame's command buffer, next to the scene
  // and UI passes. Render thread only — it resolves draw items and render
  // meshes, which ProcessDeferredOperations mutates on this same thread.
  auto RecordPickPass(FrameContext &frame_context,
                      VkCommandBuffer command_buffer) -> void;

  // Reads the winning slot out of the readback buffer and publishes it. Called
  // once the submission that recorded the pick has completed.
  auto PublishPickFromReadback() -> void;

  auto PrepareFrameDescriptors(FrameContext &frame_context) -> void;

  auto AllocatePersistentDescriptorSets(MaterialInstanceHandle instance_handle)
      -> bool;
  auto FreePersistentDescriptorSets(MaterialInstanceHandle instance_handle)
      -> void;

  auto AccumulateDescriptorBudget(const ShaderLayout &vert_layout,
                                  const ShaderLayout &frag_layout,
                                  size_t max_instances) -> void;

  auto CreateDescriptorPools() -> std::expected<void, VkInitializationError>;

  auto CreateGraphicsPipeline(const GraphicsPipelineDesc &desc)
      -> std::expected<GraphicsPipelineHandle, common::LuminaError>;

  // One pipeline's worth of rebuild work, fully self-contained: a job holding
  // this touches no renderer state until it publishes the result.
  struct ShaderReloadRequest {
    GraphicsPipelineHandle target;
    u64 epoch = 0;
    GraphicsPipelineShaderInputs inputs;
    GraphicsPipelineDesc desc;
  };

  auto DispatchShaderReloads() -> void;
  auto RunShaderReload(const ShaderReloadRequest &request) -> void;

  size_t current_frame_index = 0;

  bool is_framebuffer_resized = false;

  bool shutdown_requested = false;

  VulkanContext vulkan_context;

  std::thread render_thread;

  // TODO: this is ugly, make it proper
  std::unordered_map<std::string, size_t> shader_interface_name_map;
  std::vector<ShaderInterface> shader_interfaces;

  VkCommandPool command_pool = VK_NULL_HANDLE;

  std::counting_semaphore<MAX_FRAMES_IN_FLIGHT> frames_available_for_update{
      MAX_FRAMES_IN_FLIGHT};
  std::counting_semaphore<MAX_FRAMES_IN_FLIGHT> frames_available_for_render{0};

  FrameContext *frame_context_for_update = nullptr;
  FrameContext *frame_context_for_render = nullptr;

  std::atomic<u32> recorded_draw_call_count{0};

  std::vector<std::unique_ptr<FrameContext>> frame_contexts;

  VkDeviceMemory texture_image_memory = VK_NULL_HANDLE;
  VkImage texture_image = VK_NULL_HANDLE;
  VkImageView texture_image_view = VK_NULL_HANDLE;
  VkSampler texture_sampler = VK_NULL_HANDLE;

  VkFormat depth_stencil_format = VK_FORMAT_UNDEFINED;
  VkDeviceMemory depth_image_memory = VK_NULL_HANDLE;
  VkImage depth_image = VK_NULL_HANDLE;
  VkImageView depth_image_view = VK_NULL_HANDLE;

  // picking resources
  VkDeviceMemory pick_readback_buffer_memory = VK_NULL_HANDLE;
  VkBuffer pick_readback_buffer = VK_NULL_HANDLE;
  void *pick_readback_buffer_mapped = nullptr;
  VkDeviceMemory pick_color_image_memory = VK_NULL_HANDLE;
  VkImage pick_color_image = VK_NULL_HANDLE;
  VkImageView pick_color_image_view = VK_NULL_HANDLE;
  VkDeviceMemory pick_depth_image_memory = VK_NULL_HANDLE;
  VkImage pick_depth_image = VK_NULL_HANDLE;
  VkImageView pick_depth_image_view = VK_NULL_HANDLE;

  // The frame fence of the submission carrying an outstanding pick, or NULL.
  //
  // Frame fences are recycled, so this is only sound because ProcessReadbacks
  // is guaranteed to observe a signal before the fence can be reset. Replacing
  // it with a monotonic submission counter (or a timeline semaphore) would
  // remove that constraint entirely, and is the right move once a second
  // readback consumer exists.
  VkFence pending_pick_fence = VK_NULL_HANDLE;

  // High 32 bits are a sequence number, low 32 the hit slot. Written by the
  // render thread once the submission carrying the pick completes, read by the
  // update thread; the sequence is what tells a reader a result is new rather
  // than the one it already consumed.
  std::atomic<u64> pick_result{0};

  CommandContextPool command_context_pool;
  CommandSubmissionQueue global_submission_queue{256};
  std::vector<std::pair<VkFence, std::vector<CommandContext *>>>
      pending_submissions;

  std::unordered_map<DescriptorBindingType, size_t>
      persistent_descriptor_pool_budget;
  size_t max_persistent_descriptor_sets = 0;

  DeviceRetirementQueue device_retirement_queue;

  MaterialTemplateHandle default_material_template_handle;
  MaterialTemplateHandle debug_wireframe_material_template_handle;
  MaterialTemplateHandle pick_id_material_template_handle;
  MaterialInstanceHandle default_material_instance_handle;
  GraphicsPipelineHandle default_pipeline_handle;
  GraphicsPipelineHandle debug_aabb_pipeline_handle;
  GraphicsPipelineHandle pick_pipeline_handle;

  UIRenderer ui_renderer;

  RenderMeshManager render_mesh_manager;
  RenderTextureManager render_texture_manager;
  MaterialTemplateManager material_template_manager;
  MaterialInstanceManager material_instance_manager;
  GraphicsPipelineManager pipeline_manager;

  DrawItemRegistry draw_item_registry;

  VkDescriptorPool persistent_descriptor_pool = VK_NULL_HANDLE;

  VkDescriptorSetLayout global_descriptor_set_layout = VK_NULL_HANDLE;
  VkPipelineLayout global_pipeline_layout = VK_NULL_HANDLE;

  // One host-visible buffer sub-divided into equally sized slots, one per
  // material instance. A single allocation rather than a buffer per instance:
  // MaterialUniforms is tens of bytes but Vulkan requires descriptor offsets to
  // respect minUniformBufferOffsetAlignment, so per-instance buffers would be
  // almost entirely padding.
  VkBuffer material_ubo_buffer = VK_NULL_HANDLE;
  VkDeviceMemory material_ubo_memory = VK_NULL_HANDLE;
  void *material_ubo_mapped = nullptr;

  // Every instance gets this stride regardless of template. It is derived from
  // one specific material's uniform block, so a second material type with a
  // larger block would overrun its slot — at that point this needs to become
  // max(sizeof) across templates, or a pool per template.
  VkDeviceSize material_ubo_slot_stride = 0;

  // Summed from every template's max_instances, so the slot count and the
  // descriptor pool are sized from the same declaration and cannot disagree.
  u32 material_instance_budget = 0;
  u32 next_material_ubo_slot = 0;

  // Changed shader paths handed over by the watcher thread. Drained on the
  // render thread into `pending_shader_reloads`, which is render-thread-owned
  // and only ever non-empty inside DispatchShaderReloads.
  common::data_structures::LockFreeConcurrentQueue<std::string>
      shader_reload_requests{16};
  std::unordered_set<std::string> pending_shader_reloads;

  // Render-thread-only. Stamped onto each request so a build that finishes
  // after a newer one can be discarded instead of overwriting it.
  u64 next_shader_reload_epoch = 0;

  // Reload jobs publish their result through pipeline_manager, so they must
  // all have finished before the renderer tears down. Shutdown waits on this.
  std::atomic<u32> outstanding_shader_reload_jobs = 0;

  friend auto CreateGlobalDescriptorSetLayout(LuminaRenderer *renderer)
      -> std::expected<void, VkInitializationError>;
  friend auto
  WriteLitMaterialDescriptors(LuminaRenderer *renderer,
                              MaterialInstanceHandle instance_handle) -> void;
  friend auto UpdateLitMaterialTextures(LuminaRenderer *renderer,
                                        VkSampler sampler,
                                        VkImageView image_view) -> void;
  friend auto CreateLitMaterialInstance(LuminaRenderer *renderer,
                                        const math::Vec3 &diffuse_color)
      -> MaterialInstanceHandle;
  friend auto SetLitMaterialDiffuseColor(LuminaRenderer *renderer,
                                         MaterialInstanceHandle instance_handle,
                                         const math::Vec3 &diffuse_color)
      -> void;
  friend auto WriteTransientDescriptors(LuminaRenderer *renderer,
                                        FrameContext &frame_context,
                                        VkDescriptorSet descriptor_set) -> void;
};

} // namespace lumina::renderer
