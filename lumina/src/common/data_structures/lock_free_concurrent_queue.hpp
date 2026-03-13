#pragma once

#include "lumina_util.hpp"
#include <vector>

namespace lumina::common::data_structures {

template <typename T>
class LockFreeConcurrentQueue {
public:
  LockFreeConcurrentQueue(size_t capacity) noexcept;
  ~LockFreeConcurrentQueue() noexcept = default;

  LockFreeConcurrentQueue(const LockFreeConcurrentQueue &) = delete;
  LockFreeConcurrentQueue(LockFreeConcurrentQueue &&) noexcept = default;
  auto operator=(const LockFreeConcurrentQueue &)
      -> LockFreeConcurrentQueue & = delete;
  auto operator=(LockFreeConcurrentQueue &&) noexcept
      -> LockFreeConcurrentQueue & = default;

  auto Push(const T &value) -> bool;
  auto Pop(T &value) -> bool;

  auto Size() const noexcept -> size_t;

private:
  struct alignas(CACHE_LINE_SIZE) Slot {
    std::atomic<size_t> sequence_number;
    T value;
  };

  size_t buffer_mask;
  alignas(CACHE_LINE_SIZE) std::vector<Slot> storage;
  PaddedAtomic<size_t> enqueue_index;
  PaddedAtomic<size_t> dequeue_index;
};

template <typename T>
LockFreeConcurrentQueue<T>::LockFreeConcurrentQueue(size_t capacity) noexcept
    : buffer_mask(capacity - 1), storage(capacity), enqueue_index(0),
      dequeue_index(0) {
  ASSERT(IsPowerOfTwo(capacity), "Capacity must be a power of two");
  for (size_t i = 0; i < capacity; ++i) {
    storage[i].sequence_number.store(i);
  }
}

template <typename T>
auto LockFreeConcurrentQueue<T>::Push(const T &value) -> bool {
  Slot *slot = nullptr;
  auto pos = enqueue_index.load(std::memory_order_relaxed);
  for (;;) {
    slot = &storage[pos & buffer_mask];
    auto seq = slot->sequence_number.load(std::memory_order_acquire);
    auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
    if (diff == 0) {
      if (enqueue_index.compare_exchange_weak(pos, pos + 1,
                                              std::memory_order_release,
                                              std::memory_order_relaxed)) {
        break;
      }
    } else if (diff < 0) {
      return false;
    } else {
      pos = enqueue_index.load(std::memory_order_relaxed);
    }
  }
  slot->value = value;
  slot->sequence_number.store(pos + 1, std::memory_order_release);
  return true;
}

template <typename T>
auto LockFreeConcurrentQueue<T>::Pop(T &value) -> bool {
  Slot *slot = nullptr;
  auto pos = dequeue_index.load(std::memory_order_relaxed);
  for (;;) {
    slot = &storage[pos & buffer_mask];
    auto seq = slot->sequence_number.load(std::memory_order_acquire);
    auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
    if (diff == 0) {
      if (dequeue_index.compare_exchange_weak(pos, pos + 1,
                                              std::memory_order_release,
                                              std::memory_order_relaxed)) {
        break;
      }
    } else if (diff < 0) {
      return false;
    } else {
      pos = dequeue_index.load(std::memory_order_relaxed);
    }
  }
  value = slot->value;
  slot->sequence_number.store(pos + buffer_mask + 1, std::memory_order_release);
  return true;
}

template <typename T>
auto LockFreeConcurrentQueue<T>::Size() const noexcept -> size_t {
  return enqueue_index.load(std::memory_order_relaxed) -
         dequeue_index.load(std::memory_order_relaxed);
}

} // namespace lumina::common::data_structures
