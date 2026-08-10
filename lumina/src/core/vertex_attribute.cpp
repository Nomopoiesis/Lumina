#include "vertex_attribute.hpp"

namespace lumina::core {

auto GetElementTypeSize(ElementType element_type) noexcept -> u8 {
  switch (element_type) {
    case ElementType::Bool:
    case ElementType::Int8:
    case ElementType::Uint8:
      return 1;
    case ElementType::Int16:
    case ElementType::Uint16:
      return 2;
    case ElementType::Int32:
    case ElementType::Uint32:
    case ElementType::Float:
      return 4;
    case ElementType::Int64:
    case ElementType::Uint64:
    case ElementType::Double:
    case ElementType::Vec2:
      return 8;
    case ElementType::Vec3:
      return 12;
    case ElementType::Vec4:
      return 16;
  }
  // Outside the switch rather than a default label, so that -Wswitch reports
  // any ElementType added later instead of it silently returning 0 here.
  return 0;
}

} // namespace lumina::core
