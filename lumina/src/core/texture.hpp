#pragma once

#include "lumina_types.hpp"

#include "common/data_structures/data_buffer.hpp"
#include "renderer/render_texture.hpp"
#include "resource_registry.hpp"

namespace lumina::core {

enum class TextureFormat { R8_Unorm, RGBA8_Srgb };

struct Texture {
  common::data_structures::DataBuffer pixels;
  u32 width = 0;
  u32 height = 0;
  TextureFormat format = TextureFormat::RGBA8_Srgb;
  bool render_active = false;
  renderer::RenderTextureHandle render_texture_handle;
};

using TextureHandle = ResourceHandle<Texture>;
using TextureManager = ResourceRegistry<Texture>;

} // namespace lumina::core
