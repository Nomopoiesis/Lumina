#pragma once

#include "lumina_assert.hpp"
#include "lumina_types.hpp"
#include <cstddef>

#include <atomic>

#ifdef __cpp_lib_hardware_interference_size
constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
constexpr size_t CACHE_LINE_SIZE = 64;
#endif

template <typename T>
struct alignas(CACHE_LINE_SIZE) PaddedAtomic : public std::atomic<T> {
  using std::atomic<T>::atomic;
};
static_assert(sizeof(PaddedAtomic<size_t>) == CACHE_LINE_SIZE,
              "PaddedAtomic<size_t> must be a cache line size");

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
[[nodiscard]] constexpr auto SafeU64ToU32(u64 value) noexcept -> u32 {
  ASSERT(value <= 0xFFFFFFFFULL, "Value is too large to convert to u32");
  return static_cast<u32>(value);
}

[[nodiscard]] constexpr auto SafeU64ToI32(u64 value) noexcept -> i32 {
  ASSERT(value <= 0x7FFFFFFFULL, "Value is too large to convert to i32");
  return static_cast<i32>(value);
}

[[nodiscard]] constexpr auto SafeU32ToI32(u32 value) noexcept -> i32 {
  ASSERT(value <= 0x7FFFFFFFU, "Value is too large to convert to i32");
  return static_cast<i32>(value);
}

[[nodiscard]] constexpr auto IsPowerOfTwo(size_t value) noexcept -> bool {
  return (value & (value - 1)) == 0;
}

[[nodiscard]] constexpr auto PowerOfTwo(size_t value) -> size_t {
  return 1ULL << value;
}
