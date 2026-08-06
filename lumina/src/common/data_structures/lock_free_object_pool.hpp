#pragma once

#include "lumina_types.hpp"
#include "lumina_util.hpp"

#include <atomic>
#include <functional>
#include <memory>

namespace lumina::common::data_structures {

// Lock-free fixed-capacity object pool backed by an intrusive free list.
//
// The list head packs a 32-bit generation tag alongside the 32-bit node index.
// A bare pointer head cannot detect ABA: if a node is popped, reused and pushed
// back between another thread's load and its compare-exchange, the stale CAS
// still succeeds and the same node is handed to two callers. The tag changes on
// every successful update, so that CAS fails and retries instead.
template <typename T> class LockFreeObjectPool {
private:
  static constexpr u32 InvalidIndex = ~0U;

  struct Node {
    T object;
    // Acquire() reads this speculatively while another thread may be writing it
    // in Release(). The tag makes a stale read harmless, but the read itself
    // still has to be race-free.
    std::atomic<u32> next;
  };

  static constexpr auto PackHead(u32 tag, u32 index) noexcept -> u64 {
    return (static_cast<u64>(tag) << 32U) | static_cast<u64>(index);
  }
  static constexpr auto HeadTag(u64 head_value) noexcept -> u32 {
    return static_cast<u32>(head_value >> 32U);
  }
  static constexpr auto HeadIndex(u64 head_value) noexcept -> u32 {
    return static_cast<u32>(head_value);
  }

public:
  LockFreeObjectPool() noexcept = default;
  LockFreeObjectPool(size_t capacity) noexcept { Initialize(capacity); }
  ~LockFreeObjectPool() noexcept { Deinitialize(); }

  LockFreeObjectPool(const LockFreeObjectPool &) = delete;
  auto operator=(const LockFreeObjectPool &) -> LockFreeObjectPool & = delete;
  LockFreeObjectPool(LockFreeObjectPool &&) noexcept = default;
  auto operator=(LockFreeObjectPool &&) noexcept
      -> LockFreeObjectPool & = default;

  auto Initialize(size_t capacity) noexcept -> void;
  auto Initialize(size_t capacity,
                  std::function<void(T &)> initializer) noexcept -> void;
  auto Deinitialize() noexcept -> void {
    storage.reset();
    capacity_ = 0;
    head.store(PackHead(0, InvalidIndex), std::memory_order_relaxed);
  }
  auto Deinitialize(std::function<void(T &)> cleanup) noexcept -> void {
    for (size_t i = 0; i < capacity_; ++i) {
      cleanup(storage[i].object);
    }
    Deinitialize();
  }
  auto Acquire() noexcept -> T *;
  auto Release(T *object) noexcept -> void;

private:
  // Packed {generation tag, node index}; see the class comment.
  PaddedAtomic<u64> head{PackHead(0, InvalidIndex)};
  std::unique_ptr<Node[]> storage;
  size_t capacity_ = 0;
};

template <typename T>
auto LockFreeObjectPool<T>::Initialize(size_t capacity) noexcept -> void {
  Initialize(capacity, nullptr);
}

template <typename T>
auto LockFreeObjectPool<T>::Initialize(
    size_t capacity, std::function<void(T &)> initializer) noexcept -> void {
  ASSERT(capacity > 0, "LockFreeObjectPool — capacity must be greater than 0");
  ASSERT(capacity < InvalidIndex,
         "LockFreeObjectPool — capacity exceeds the index space");

  storage = std::make_unique<Node[]>(capacity);
  capacity_ = capacity;

  for (size_t i = 0; i < capacity; ++i) {
    if (initializer) {
      initializer(storage[i].object);
    }
    const u32 next_index =
        (i + 1 < capacity) ? static_cast<u32>(i + 1) : InvalidIndex;
    storage[i].next.store(next_index, std::memory_order_relaxed);
  }

  head.store(PackHead(0, 0), std::memory_order_release);
}

template <typename T> auto LockFreeObjectPool<T>::Acquire() noexcept -> T * {
  u64 old_head = head.load(std::memory_order_acquire);
  while (true) {
    const u32 index = HeadIndex(old_head);
    if (index == InvalidIndex) {
      return nullptr;
    }
    const u64 new_head = PackHead(
        HeadTag(old_head) + 1, storage[index].next.load(std::memory_order_relaxed));
    if (head.compare_exchange_weak(old_head, new_head,
                                   std::memory_order_acquire,
                                   std::memory_order_acquire)) {
      return &storage[index].object;
    }
  }
}

template <typename T>
auto LockFreeObjectPool<T>::Release(T *object) noexcept -> void {
  auto *node = reinterpret_cast<Node *>(object);
  const auto index = static_cast<u32>(node - storage.get());
  ASSERT(index < capacity_,
         "LockFreeObjectPool::Release — object does not belong to this pool");

  u64 old_head = head.load(std::memory_order_acquire);
  u64 new_head = 0;
  do {
    node->next.store(HeadIndex(old_head), std::memory_order_relaxed);
    new_head = PackHead(HeadTag(old_head) + 1, index);
  } while (!head.compare_exchange_weak(old_head, new_head,
                                       std::memory_order_release,
                                       std::memory_order_acquire));
}

} // namespace lumina::common::data_structures
