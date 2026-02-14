#pragma once

#include "lumina_types.hpp"
#include "lumina_util.hpp"

#include <functional>
#include <memory>

namespace lumina::common::data_structures {

template <typename T> class LockFreeObjectPool {
private:
  struct Node {
    T object;
    Node *next;
  };

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
    head.store(nullptr, std::memory_order_relaxed);
  }
  auto Acquire() noexcept -> T *;
  auto Release(T *object) noexcept -> void;

private:
  PaddedAtomic<Node *> head;
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
  storage = std::make_unique<Node[]>(capacity);
  capacity_ = capacity;
  for (size_t i = 0; i < capacity - 1; ++i) {
    if (initializer) {
      initializer(storage[i].object);
    }
    storage[i].next = &storage[i + 1];
  }
  storage[capacity - 1].next = nullptr;
  if (initializer) {
    initializer(storage[capacity - 1].object);
  }
  head.store(&storage[0], std::memory_order_relaxed);
}

template <typename T> auto LockFreeObjectPool<T>::Acquire() noexcept -> T * {
  auto *old_head = head.load(std::memory_order_acquire);
  while (old_head && !head.compare_exchange_weak(old_head, old_head->next,
                                                 std::memory_order_release,
                                                 std::memory_order_acquire)) {
  }
  return old_head ? &old_head->object : nullptr;
}

template <typename T>
auto LockFreeObjectPool<T>::Release(T *object) noexcept -> void {
  auto *node = reinterpret_cast<Node *>(object);
  auto *old_head = head.load(std::memory_order_acquire);
  do { // NOLINT
    node->next = old_head;
  } while (!head.compare_exchange_weak(
      old_head, node, std::memory_order_release, std::memory_order_acquire));
}

} // namespace lumina::common::data_structures
