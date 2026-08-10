#pragma once

#include "lumina_types.hpp"

#include <format>
#include <functional>
#include <limits>

namespace lumina::core {

enum class Mobility : u8 {
  Static,
  Dynamic,
};

using EntityIndexType = u32;

struct EntityID {
  EntityIndexType index = 0;
  u32 generation = 0;

  auto operator==(const EntityID &) const -> bool = default;
};

constexpr EntityID INVALID_ENTITY_ID = {
    .index = std::numeric_limits<EntityIndexType>::max(),
    .generation = std::numeric_limits<u32>::max()};

class Entity {
public:
  Entity() = default;
  Entity(Mobility mobility_) : mobility(mobility_) {}
  Entity(const Entity &) = default;
  auto operator=(const Entity &) -> Entity & = default;
  Entity(Entity &&) noexcept = default;
  auto operator=(Entity &&) noexcept -> Entity & = default;
  ~Entity() = default;

  [[nodiscard]] auto GetMobility() const -> Mobility { return mobility; }

private:
  Mobility mobility = Mobility::Dynamic;
};

} // namespace lumina::core

namespace std {

template <>
struct hash<lumina::core::EntityID> {
  auto operator()(const lumina::core::EntityID &id) const noexcept -> size_t {
    return std::hash<u64>{}((static_cast<u64>(id.index) << 32) |
                            static_cast<u64>(id.generation));
  }
};

template <>
struct formatter<lumina::core::EntityID> : formatter<string_view> {
  auto format(const lumina::core::EntityID &id, format_context &ctx) const {
    return std::format_to(ctx.out(), "{}:{}", id.index, id.generation);
  }
};

} // namespace std
