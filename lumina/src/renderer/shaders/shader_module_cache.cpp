#include "shader_module_cache.hpp"

#include "lumina_types.hpp"

#include <cstring>
#include <string>
#include <vector>

#ifdef _PLATFORM_WINDOWS
#include <Windows.h>
#endif

#include "platform/common/platform_services.hpp"

#include "lumina_assert.hpp"
#include "scope_guard.hpp"

namespace lumina::renderer {

ShaderModuleCache::ShaderModuleCache(VkDevice device_) noexcept
    : device(device_) {}

ShaderModuleCache::ShaderModuleCache(ShaderModuleCache &&other) noexcept
    : device(other.device),
      shader_module_cache(std::move(other.shader_module_cache)) {
  other.device = VK_NULL_HANDLE;
  other.shader_module_cache.clear();
}

ShaderModuleCache::~ShaderModuleCache() noexcept {
  for (auto &[file_path, cached_shader_module] : shader_module_cache) {
    if (cached_shader_module.handle != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device, cached_shader_module.handle, nullptr);
    }
  }
  shader_module_cache.clear();
}

auto ShaderModuleCache::operator=(ShaderModuleCache &&other) noexcept
    -> ShaderModuleCache & {
  if (this != &other) {
    shader_module_cache.clear();

    device = other.device;
    shader_module_cache = std::move(other.shader_module_cache);

    other.device = VK_NULL_HANDLE;
    other.shader_module_cache.clear();
  }
  return *this;
}

auto ShaderModuleCache::CreateShaderModule(const std::string &file_path,
                                           VkShaderStageFlagBits stage)
    -> std::expected<VkShaderModule, ShaderLoadError> {
  // Read file content
  std::vector<u8> file_data;
  void *file_handle =
      platform::common::PlatformServices::Instance().LuminaOpenFile(
          file_path.c_str());
  if (file_handle == nullptr) {
    return std::unexpected(
        ShaderLoadError{.message = "Failed to open file: " + file_path});
  } else {
    ScopeGuard file_handle_guard([&file_handle]() {
      platform::common::PlatformServices::Instance().LuminaCloseFile(
          file_handle);
    });

    std::size_t file_size =
        platform::common::PlatformServices::Instance().LuminaGetFileSize(
            file_handle);

    if (file_size == 0) {
      return std::unexpected(ShaderLoadError{
          .message = "Failed to get file size or file is empty: " + file_path});
    }

    file_data.resize(file_size);

    auto read_success =
        platform::common::PlatformServices::Instance().LuminaReadFile(
            file_handle, file_data.data(), file_size);
    if (!read_success) {
      return std::unexpected(
          ShaderLoadError{.message = "Failed to read file: " + file_path});
    }
  }

  ASSERT(file_data.size() > 0, "File data is empty");

  // Create shader module
  VkShaderModuleCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .codeSize = file_data.size(),
      .pCode = reinterpret_cast<const u32 *>(file_data.data()),
  };
  VkShaderModule shader_module = VK_NULL_HANDLE;
  VkResult result =
      vkCreateShaderModule(device, &create_info, nullptr, &shader_module);
  if (result != VK_SUCCESS || shader_module == VK_NULL_HANDLE) {
    return std::unexpected(ShaderLoadError{
        .message = "Failed to create shader module from: " + file_path});
  }

  // Cache the shader metadata and handle
  CachedShaderModule cached_shader_module = {
      .handle = shader_module,
      .file_path = file_path,
      .stage = stage,
  };
  shader_module_cache.emplace(file_path, cached_shader_module);

  // Return the handle (content hash)
  return shader_module;
}

auto ShaderModuleCache::GetShaderModule(const std::string &file_path,
                                        VkShaderStageFlagBits stage)
    -> std::expected<VkShaderModule, ShaderLoadError> {
  // Look up shader module in cache
  auto full_path = "C:/projects/Lumina/lumina/data/" + file_path;
  auto cache_it = shader_module_cache.find(full_path);
  if (cache_it == shader_module_cache.end()) {
    // Shader module not found in cache, create it
    return CreateShaderModule(full_path, stage);
  }

  return cache_it->second.handle;
}

} // namespace lumina::renderer
