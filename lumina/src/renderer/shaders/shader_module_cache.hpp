#pragma once

#include "lumina_types.hpp"

#include <vulkan/vulkan.h>

#include <expected>
#include <string>
#include <unordered_map>

namespace lumina::renderer {

struct ShaderLoadError {
  std::string message;
};

/**
 * This class is used for storing/caching compiled shader modules or creating
 * them on demand. It allows us to temporarly store shader modules in order to
 * reuse them in pipeline creation. Upon destruction, all shader modules will be
 * destroyed.
 */
class ShaderModuleCache final {
public:
  explicit ShaderModuleCache(VkDevice device) noexcept;
  ShaderModuleCache(ShaderModuleCache &&other) noexcept;
  auto operator=(ShaderModuleCache &&other) noexcept -> ShaderModuleCache &;

  ShaderModuleCache(const ShaderModuleCache &) = delete;
  auto operator=(const ShaderModuleCache &) -> ShaderModuleCache & = delete;

  ~ShaderModuleCache() noexcept;

  [[nodiscard]] auto GetShaderModule(const std::string &file_path,
                                     VkShaderStageFlagBits stage)
      -> std::expected<VkShaderModule, ShaderLoadError>;

private:
  struct CachedShaderModule {
    VkShaderModule handle = VK_NULL_HANDLE;
    std::string file_path;
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
  };

  auto CreateShaderModule(const std::string &file_path,
                          VkShaderStageFlagBits stage)
      -> std::expected<VkShaderModule, ShaderLoadError>;

  VkDevice device = VK_NULL_HANDLE;
  std::unordered_map<std::string, CachedShaderModule> shader_module_cache;
};

} // namespace lumina::renderer
