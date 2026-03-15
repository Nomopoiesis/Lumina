#pragma once

#include "lumina_assert.hpp"
#include "lumina_types.hpp"
#include <cstddef>

// Size conversion functions
constexpr auto Kilobytes(u64 size) -> u64 { return size * 1024ULL; }
constexpr auto Megabytes(u64 size) -> u64 { return Kilobytes(size) * 1024ULL; }
constexpr auto Gigabytes(u64 size) -> u64 { return Megabytes(size) * 1024ULL; }
constexpr auto Terabytes(u64 size) -> u64 { return Gigabytes(size) * 1024ULL; }

// Array utilities
template <typename T, std::size_t N>
[[nodiscard]] constexpr auto ArrayCount(T (&array)[N]) noexcept -> std::size_t {
  return N;
}

// Safe type conversion
[[nodiscard]] inline auto SafeU64ToU32(u64 value) -> u32 {
  ASSERT(value <= 0xFFFFFFFFULL, "Value is too large to convert to u32");
  return static_cast<u32>(value);
}

[[nodiscard]] inline auto SafeU64ToI32(u64 value) -> i32 {
  ASSERT(value <= 0x7FFFFFFFULL, "Value is too large to convert to i32");
  return static_cast<i32>(value);
}

[[nodiscard]] inline auto SafeU32ToI32(u32 value) -> i32 {
  ASSERT(value <= 0x7FFFFFFFU, "Value is too large to convert to i32");
  return static_cast<i32>(value);
}
