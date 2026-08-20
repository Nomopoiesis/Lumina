#include "ui_renderer.hpp"

#include "common/logger/logger.hpp"
#include "common/lumina_assert.hpp"
#include "vk_check.hpp"

#include <cstring>
#include <vector>

namespace lumina::renderer {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static auto FindMemoryTypeIndex(VulkanContext &ctx, u32 type_bits,
                                VkMemoryPropertyFlags props) -> u32 {
  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(ctx.GetPhysicalDevice(), &mem_props);
  for (u32 i = 0; i < mem_props.memoryTypeCount; ++i) {
    if ((type_bits & (1u << i)) &&
        (mem_props.memoryTypes[i].propertyFlags & props) == props) {
      return i;
    }
  }
  // Falling through to an arbitrary memory type index would produce a bogus
  // allocation rather than a diagnosable failure.
  LOG_CRITICAL("No suitable memory type for UI buffer (type_bits: {:#x})",
               type_bits);
  LUMINA_TERMINATE();
  return 0;
}

static auto CreateHostBuffer(VulkanContext &ctx, VkDeviceSize size,
                             VkBufferUsageFlags usage, VkBuffer &buffer,
                             VkDeviceMemory &memory, void *&mapped) -> void {
  VkBufferCreateInfo buf_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  VK_CHECK(vkCreateBuffer(ctx.GetDevice(), &buf_info, nullptr, &buffer),
           "Failed to create UI buffer");

  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(ctx.GetDevice(), buffer, &req);

  VkMemoryAllocateInfo alloc{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = req.size,
      .memoryTypeIndex =
          FindMemoryTypeIndex(ctx, req.memoryTypeBits,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
  };
  VK_CHECK(vkAllocateMemory(ctx.GetDevice(), &alloc, nullptr, &memory),
           "Failed to allocate UI buffer memory");
  VK_CHECK(vkBindBufferMemory(ctx.GetDevice(), buffer, memory, 0),
           "Failed to bind UI buffer memory");
  VK_CHECK(vkMapMemory(ctx.GetDevice(), memory, 0, size, 0, &mapped),
           "Failed to map UI buffer memory");
}

// ---------------------------------------------------------------------------
// Initialize / Shutdown
// ---------------------------------------------------------------------------

auto UIRenderer::Initialize(VulkanContext &ctx) -> void { CreateBuffers(ctx); }

auto UIRenderer::Shutdown(VulkanContext &ctx) -> void {
  auto *device = ctx.GetDevice();
  for (auto &fr : frame_resources) {
    if (fr.vertex_buffer != VK_NULL_HANDLE) {
      vkUnmapMemory(device, fr.vertex_buffer_memory);
      vkDestroyBuffer(device, fr.vertex_buffer, nullptr);
      vkFreeMemory(device, fr.vertex_buffer_memory, nullptr);
    }
    if (fr.index_buffer != VK_NULL_HANDLE) {
      vkUnmapMemory(device, fr.index_buffer_memory);
      vkDestroyBuffer(device, fr.index_buffer, nullptr);
      vkFreeMemory(device, fr.index_buffer_memory, nullptr);
    }
  }
}

// ---------------------------------------------------------------------------
// RecordCommands
// ---------------------------------------------------------------------------

auto UIRenderer::RecordCommands(
    VkCommandBuffer cmd_buf, u32 frame_index, const UIRenderBatch &batch,
    u32 screen_width, u32 screen_height, VkPipeline pipeline,
    VkPipelineLayout pipeline_layout,
    const std::vector<VkPushConstantRange> &push_ranges,
    VkDescriptorSet font_atlas_set) -> void {
  // font_atlas_set is null until the atlas upload completes, and sampling an
  // unbound descriptor is undefined rather than merely blank.
  if (batch.IsEmpty() || pipeline == VK_NULL_HANDLE ||
      font_atlas_set == VK_NULL_HANDLE) {
    return;
  }

  auto &fr = frame_resources[frame_index];

  // Upload vertex data
  const size_t vb_size = batch.vertices.size() * sizeof(UIVertex);
  ASSERT(vb_size <= kMaxVertices * sizeof(UIVertex),
         "UI vertex buffer overflow");
  std::memcpy(fr.vertex_mapped, batch.vertices.data(), vb_size);

  // Upload index data
  const size_t ib_size = batch.indices.size() * sizeof(u32);
  ASSERT(ib_size <= kMaxIndices * sizeof(u32), "UI index buffer overflow");
  std::memcpy(fr.index_mapped, batch.indices.data(), ib_size);

  // Use a standard (non-flipped) viewport for UI rendering so that screen Y=0
  // is the top.
  VkViewport ui_viewport{
      .x = 0.0F,
      .y = 0.0F,
      .width = static_cast<float>(screen_width),
      .height = static_cast<float>(screen_height),
      .minDepth = 0.0F,
      .maxDepth = 1.0F,
  };
  vkCmdSetViewport(cmd_buf, 0, 1, &ui_viewport);

  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  // Set 0 of the Screen2D family, not of the scene's: this pipeline's layout
  // declares the atlas there, and binding the scene's set instead would be a
  // layout mismatch rather than a wrong picture.
  vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout, 0, 1, &font_atlas_set, 0, nullptr);

  const float push[2] = {static_cast<float>(screen_width),
                         static_cast<float>(screen_height)};
  // Flags from the reflected range rather than a literal — see the equivalent
  // loop in RecordCommandBuffer.
  for (const auto &range : push_ranges) {
    vkCmdPushConstants(cmd_buf, pipeline_layout, range.stageFlags, range.offset,
                       range.size, push);
  }

  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd_buf, 0, 1, &fr.vertex_buffer, &offset);
  vkCmdBindIndexBuffer(cmd_buf, fr.index_buffer, 0, VK_INDEX_TYPE_UINT32);

  // Full-screen default scissor (restored after scoped scissors)
  const VkRect2D full_scissor{{0, 0}, {screen_width, screen_height}};
  vkCmdSetScissor(cmd_buf, 0, 1, &full_scissor);

  for (const auto &dc : batch.draw_calls) {
    if (dc.index_count == 0) {
      continue;
    }
    if (dc.has_scissor) {
      vkCmdSetScissor(cmd_buf, 0, 1, &dc.scissor);
    } else {
      vkCmdSetScissor(cmd_buf, 0, 1, &full_scissor);
    }
    vkCmdDrawIndexed(cmd_buf, dc.index_count, 1, dc.index_offset, 0, 0);
  }

  // Restore full-screen scissor for subsequent passes
  vkCmdSetScissor(cmd_buf, 0, 1, &full_scissor);
}

// ---------------------------------------------------------------------------
// CreateBuffers
// ---------------------------------------------------------------------------

auto UIRenderer::CreateBuffers(VulkanContext &ctx) -> void {
  for (auto &fr : frame_resources) {
    CreateHostBuffer(ctx, kMaxVertices * sizeof(UIVertex),
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, fr.vertex_buffer,
                     fr.vertex_buffer_memory, fr.vertex_mapped);
    CreateHostBuffer(ctx, kMaxIndices * sizeof(u32),
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT, fr.index_buffer,
                     fr.index_buffer_memory, fr.index_mapped);
  }
}

} // namespace lumina::renderer
