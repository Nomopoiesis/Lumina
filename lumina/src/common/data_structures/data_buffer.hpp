#pragma once

#include "lumina_assert.hpp"
#include "lumina_types.hpp"

#include <cstring>
#include <memory>
#include <span>

namespace lumina::common::data_structures {

class DataBufferView;

// Owned, heap-allocated byte buffer. Copyable (deep copy) and movable.
class DataBuffer {
public:
  DataBuffer() noexcept = default;

  explicit DataBuffer(size_t size) noexcept
      : data_(std::make_unique<u8[]>(size)), size_(size) {}

  DataBuffer(const void *src, size_t size) noexcept
      : data_(std::make_unique<u8[]>(size)), size_(size) {
    std::memcpy(data_.get(), src, size);
  }

  template <typename T>
  explicit DataBuffer(std::span<const T> src) noexcept
      : DataBuffer(src.data(), src.size_bytes()) {}

  DataBuffer(const DataBuffer &other) noexcept
      : data_(std::make_unique<u8[]>(other.size_)), size_(other.size_) {
    std::memcpy(data_.get(), other.data_.get(), size_);
  }

  auto operator=(const DataBuffer &other) noexcept -> DataBuffer & {
    if (this != &other) {
      data_ = std::make_unique<u8[]>(other.size_);
      size_ = other.size_;
      std::memcpy(data_.get(), other.data_.get(), size_);
    }
    return *this;
  }

  DataBuffer(DataBuffer &&) noexcept = default;
  auto operator=(DataBuffer &&) noexcept -> DataBuffer & = default;
  ~DataBuffer() noexcept = default;

  [[nodiscard]] auto Data() noexcept -> u8 * { return data_.get(); }
  [[nodiscard]] auto Data() const noexcept -> const u8 * { return data_.get(); }
  [[nodiscard]] auto Size() const noexcept -> size_t { return size_; }
  [[nodiscard]] auto IsEmpty() const noexcept -> bool { return size_ == 0; }

  // Reinterpret a byte range starting at byte_offset as a typed reference.
  template <typename T>
  [[nodiscard]] auto As(size_t byte_offset = 0) noexcept -> T & {
    ASSERT(byte_offset + sizeof(T) <= size_,
           "DataBuffer::As — out-of-bounds access");
    return *reinterpret_cast<T *>(data_.get() + byte_offset);
  }

  template <typename T>
  [[nodiscard]] auto As(size_t byte_offset = 0) const noexcept -> const T & {
    ASSERT(byte_offset + sizeof(T) <= size_,
           "DataBuffer::As — out-of-bounds access");
    return *reinterpret_cast<const T *>(data_.get() + byte_offset);
  }

  // Reinterpret the entire buffer as a span of T.
  // Buffer size must be a multiple of sizeof(T).
  template <typename T>
  [[nodiscard]] auto AsSpan() noexcept -> std::span<T> {
    ASSERT(size_ % sizeof(T) == 0,
           "DataBuffer::AsSpan — size not a multiple of element size");
    return std::span<T>(reinterpret_cast<T *>(data_.get()), size_ / sizeof(T));
  }

  template <typename T>
  [[nodiscard]] auto AsSpan() const noexcept -> std::span<const T> {
    ASSERT(size_ % sizeof(T) == 0,
           "DataBuffer::AsSpan — size not a multiple of element size");
    return std::span<const T>(reinterpret_cast<const T *>(data_.get()),
                              size_ / sizeof(T));
  }

  // Copy src bytes into the buffer at byte_offset.
  auto Write(size_t byte_offset, const void *src, size_t size) noexcept
      -> void {
    ASSERT(byte_offset + size <= size_, "DataBuffer::Write — out-of-bounds");
    std::memcpy(data_.get() + byte_offset, src, size);
  }

  template <typename T>
  auto Write(size_t byte_offset, const T &value) noexcept -> void {
    Write(byte_offset, &value, sizeof(T));
  }

  // Create a non-owning read-only view over the buffer or a sub-range.
  [[nodiscard]] auto View() const noexcept -> DataBufferView;
  [[nodiscard]] auto View(size_t offset, size_t size) const noexcept
      -> DataBufferView;

private:
  std::unique_ptr<u8[]> data_ = nullptr;
  size_t size_ = 0;
};

// Non-owning, read-only view into a contiguous byte range.
class DataBufferView {
public:
  DataBufferView() noexcept = default;

  DataBufferView(const u8 *data, size_t size) noexcept
      : data_(data), size_(size) {}

  explicit DataBufferView(const DataBuffer &buffer) noexcept
      : data_(buffer.Data()), size_(buffer.Size()) {}

  DataBufferView(const DataBuffer &buffer, size_t offset, size_t size) noexcept
      : data_(buffer.Data() + offset), size_(size) {
    ASSERT(offset + size <= buffer.Size(),
           "DataBufferView — range out of buffer bounds");
  }

  DataBufferView(const DataBufferView &) noexcept = default;
  auto operator=(const DataBufferView &) noexcept -> DataBufferView & = default;
  DataBufferView(DataBufferView &&) noexcept = default;
  auto operator=(DataBufferView &&) noexcept -> DataBufferView & = default;
  ~DataBufferView() noexcept = default;

  [[nodiscard]] auto Data() const noexcept -> const u8 * { return data_; }
  [[nodiscard]] auto Size() const noexcept -> size_t { return size_; }
  [[nodiscard]] auto IsEmpty() const noexcept -> bool { return size_ == 0; }

  template <typename T>
  [[nodiscard]] auto As(size_t byte_offset = 0) const noexcept -> const T & {
    ASSERT(byte_offset + sizeof(T) <= size_,
           "DataBufferView::As — out-of-bounds access");
    return *reinterpret_cast<const T *>(data_ + byte_offset);
  }

  template <typename T>
  [[nodiscard]] auto AsSpan() const noexcept -> std::span<const T> {
    ASSERT(size_ % sizeof(T) == 0,
           "DataBufferView::AsSpan — size not a multiple of element size");
    return std::span<const T>(reinterpret_cast<const T *>(data_),
                              size_ / sizeof(T));
  }

  [[nodiscard]] auto Slice(size_t offset, size_t size) const noexcept
      -> DataBufferView {
    ASSERT(offset + size <= size_, "DataBufferView::Slice — out of range");
    return {data_ + offset, size};
  }

private:
  const u8 *data_ = nullptr;
  size_t size_ = 0;
};

// Inline definitions deferred until DataBufferView is complete.
inline auto DataBuffer::View() const noexcept -> DataBufferView {
  return DataBufferView(*this);
}

inline auto DataBuffer::View(size_t offset, size_t size) const noexcept
    -> DataBufferView {
  return {*this, offset, size};
}

} // namespace lumina::common::data_structures
