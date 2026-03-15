#pragma once

#include "lumina_types.hpp"
#include "lumina_util.hpp"

#include <atomic>
#include <vector>

namespace lumina::common::data_structures {

template <typename T> class WorkStealingDeque {
public:
  WorkStealingDeque() noexcept {
    top_index.store(0, std::memory_order_relaxed);
    bottom_index.store(0, std::memory_order_relaxed);
    storage.store(new InternalBuffer<T>(INITIAL_CAPACITY_SCALE),
                  std::memory_order_relaxed);
  }

  ~WorkStealingDeque() noexcept {
    delete storage.load();
    for (auto *storage_ptr : garbage_list) {
      delete storage_ptr; // NOLINT
    }
    garbage_list.clear();
  }

  WorkStealingDeque(const WorkStealingDeque &) = delete;
  auto operator=(const WorkStealingDeque &) -> WorkStealingDeque & = delete;

  WorkStealingDeque(WorkStealingDeque &&) noexcept = default;
  auto operator=(WorkStealingDeque &&) noexcept
      -> WorkStealingDeque & = default;

  auto Push(const T &value) -> bool;
  auto Pop(T &value) -> bool;
  auto Steal(T &value) -> bool;

private:
  template <typename TYPE> struct InternalBuffer {
    size_t capacity;
    size_t capacity_mask;
    std::unique_ptr<std::atomic<TYPE>[]> data;

    InternalBuffer(size_t capacity_scale) noexcept
        : capacity(1ULL << capacity_scale), capacity_mask(capacity - 1),
          data(std::make_unique<std::atomic<TYPE>[]>(capacity)) {
      std::memset(data.get(), 0, capacity * sizeof(std::atomic<TYPE>));
    }

    auto At(size_t index) -> std::atomic<TYPE> & {
      return data[index & capacity_mask];
    }
  };

  static constexpr size_t MAX_CAPACITY_SCALE = 10;
  static constexpr size_t INITIAL_CAPACITY_SCALE = 6;

  auto Grow() -> bool;

  PaddedAtomic<size_t> top_index{};
  PaddedAtomic<size_t> bottom_index{};
  std::atomic<InternalBuffer<T> *> storage{};
  std::vector<InternalBuffer<T> *> garbage_list{};
};

template <typename T> auto WorkStealingDeque<T>::Grow() -> bool {
  auto *old_storage = storage.load(std::memory_order_relaxed);
  size_t old_scale = std::log2(old_storage->capacity);
  if (old_scale >= MAX_CAPACITY_SCALE) {
    return false;
  }

  auto new_storage = new InternalBuffer<T>(old_scale + 1); // NOLINT

  auto bottom_index_value = bottom_index.load(std::memory_order_relaxed);
  auto top_index_value = top_index.load(std::memory_order_acquire);

  for (size_t i = top_index_value; i < bottom_index_value; ++i) {
    new_storage->At(i).store(old_storage->At(i).load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
  }

  storage.store(new_storage, std::memory_order_release);
  garbage_list.push_back(old_storage);
  return true;
}

template <typename T> auto WorkStealingDeque<T>::Push(const T &value) -> bool {
  size_t bottom_index_value = bottom_index.load(std::memory_order_relaxed);
  size_t top_index_value = top_index.load(std::memory_order_acquire);

  auto *storage_ptr = storage.load(std::memory_order_relaxed);

  size_t element_count = bottom_index_value - top_index_value;
  if (element_count >= storage_ptr->capacity - 1) {
    if (!Grow()) {
      return false;
    }
  }

  storage_ptr->At(bottom_index_value).store(value, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);
  bottom_index.store(bottom_index_value + 1, std::memory_order_release);
  return true;
}

template <typename T> auto WorkStealingDeque<T>::Pop(T &value) -> bool {
  size_t bottom_index_value = bottom_index.load(std::memory_order_relaxed);
  if (bottom_index_value == 0) {
    // Queue is empty - early out to avoid underflow
    return false;
  }
  --bottom_index_value;
  bottom_index.store(bottom_index_value, std::memory_order_release);

  auto *storage_ptr = storage.load(std::memory_order_relaxed);

  std::atomic_thread_fence(std::memory_order_seq_cst);

  size_t top_index_value = top_index.load(std::memory_order_relaxed);
  bool result = true;
  if (top_index_value > bottom_index_value) {
    // Queue is empty
    bottom_index.store(bottom_index_value + 1, std::memory_order_relaxed);
    result = false;
  } else {
    // we got at least one item
    value = storage_ptr->At(bottom_index_value).load(std::memory_order_relaxed);
    if (top_index_value == bottom_index_value) {
      // Last item race: Try to claim it by bumping 'top'
      // If this fails, a thief beat us to it.
      if (!top_index.compare_exchange_strong(
              top_index_value, top_index_value + 1, std::memory_order_seq_cst,
              std::memory_order_relaxed)) {
        result = false;
      }
      bottom_index.store(bottom_index_value + 1, std::memory_order_relaxed);
    }
  }

  return result;
}

template <typename T> auto WorkStealingDeque<T>::Steal(T &value) -> bool {
  auto top_index_value = top_index.load(std::memory_order_acquire);
  std::atomic_thread_fence(std::memory_order_seq_cst);
  auto bottom_index_value = bottom_index.load(std::memory_order_acquire);

  if (top_index_value >= bottom_index_value) {
    // Queue is empty
    return false;
  }

  auto *storage_ptr = storage.load(std::memory_order_consume);
  value = storage_ptr->At(top_index_value).load(std::memory_order_relaxed);
  return top_index.compare_exchange_strong(top_index_value, top_index_value + 1,
                                           std::memory_order_seq_cst,
                                           std::memory_order_relaxed);
}

} // namespace lumina::common::data_structures
