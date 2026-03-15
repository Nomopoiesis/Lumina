#pragma once

#include "lumina_types.hpp"

namespace lumina::core {

using ResourceHandleIndexType = u32;

constexpr ResourceHandleIndexType INVALID_RESOURCE_HANDLE_INDEX =
    static_cast<ResourceHandleIndexType>(-1);

template <typename T>
struct ResourceHandle {
  ResourceHandleIndexType index = INVALID_RESOURCE_HANDLE_INDEX;
  u32 generation = 0;

  auto operator==(const ResourceHandle &other) const -> bool = default;
};

} // namespace lumina::core