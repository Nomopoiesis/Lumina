#pragma once

#include "lumina_types.hpp"

#include "data_structures/lock_free_concurrent_queue.hpp"
#include "data_structures/lock_free_object_pool.hpp"
#include "data_structures/work_stealing_deque.hpp"
#include <atomic>
#include <functional>
#include <thread>

namespace lumina::core::job_system {

using FiberHandle = void *;

struct Counter {
  std::atomic<u32> value;
  std::atomic<u32> waiting_count;

  // Fibers suspended on this counter
  std::atomic<FiberHandle> fiber_handles[16];

  auto Reset() -> void {
    value.store(0);
    waiting_count.store(0);
    for (auto &fiber_handle : fiber_handles) {
      fiber_handle.store(nullptr);
    }
  }
};

struct Job {
  std::function<void(void *)> execute;
  void *data;
  Counter *signal_counter;

  auto Reset() -> void {
    execute = nullptr;
    data = nullptr;
    signal_counter = nullptr;
  }
};

struct FiberContext {
  FiberHandle handle;
  Job *current_job;
  std::atomic<bool> is_completed;

  auto Reset() -> void {
    current_job = nullptr;
    is_completed.store(false, std::memory_order_release);
  }
};

struct JobManagerInitializeInfo {
  size_t num_workers;
  size_t fiber_pool_size;
};

using JobStealingDeque = common::data_structures::WorkStealingDeque<Job *>;
using JobPool = common::data_structures::LockFreeObjectPool<Job>;
using FiberPool = common::data_structures::LockFreeObjectPool<FiberContext>;
using CounterPool = common::data_structures::LockFreeObjectPool<Counter>;
using FiberQueue =
    common::data_structures::LockFreeConcurrentQueue<FiberHandle>;

class JobManager {
public:
  static constexpr u32 MAX_FIBERS = 256;
  static constexpr u32 MAX_COUNTERS = 1024;

  struct WorkerContext {
    JobManager *job_manager;
    std::thread thread_handle;
    size_t worker_index;

    // Fiber State
    FiberHandle master_fiber;
    FiberHandle current_fiber;

    JobStealingDeque work_stealing_deque;
  };

  JobManager() noexcept : resume_queue(64) {}
  ~JobManager() { Shutdown(); }

  JobManager(const JobManager &) = delete;
  auto operator=(const JobManager &) -> JobManager & = delete;

  JobManager(JobManager &&) noexcept = delete;
  auto operator=(JobManager &&) noexcept -> JobManager & = delete;

  auto Initialize(JobManagerInitializeInfo initialize_info) -> void;
  auto Shutdown() -> void;

  [[nodiscard]] auto IsShutdownRequested() const -> bool {
    return is_shutdown_requested_;
  }

  [[nodiscard]] auto StealFromOtherWorker(size_t thief_index, Job *&job)
      -> bool;

  auto AllocateCounter(u32 value) -> Counter *;
  auto ReleaseCounter(Counter *counter) -> void;

  auto AllocateJob() -> Job *;
  auto ReleaseJob(Job *job) -> void;

  auto SubmitJob(Job *job) -> void;
  auto WaitForCounter(Counter *counter) -> void;
  auto Signal(Counter *counter) -> void;

  static auto GetCurrent() -> JobManager * {
    ASSERT(local_worker_context != nullptr, "Local worker context is null");
    return local_worker_context->job_manager;
  }

private:
  bool is_initialized_ = false;
  bool is_shutdown_requested_ = false;
  inline static thread_local WorkerContext *local_worker_context = nullptr;

  static auto SwitchContext(FiberHandle from_fiber, FiberHandle to_fiber)
      -> void;

  static auto WorkerEntryPoint(WorkerContext *worker_context) -> void;
  static auto FiberEntryPoint(void *data) -> void;
  static auto WorkerLoop(WorkerContext *ctx) -> void;
  static auto YieldToMasterFiber() -> void;

  std::atomic<size_t> round_robin_index = 0;
  JobPool job_pool;
  FiberPool fiber_pool;
  CounterPool counter_pool;
  std::vector<std::unique_ptr<WorkerContext>> worker_contexts;
  FiberQueue resume_queue;

  std::atomic<size_t> allocated_job_count = 0;
};

} // namespace lumina::core::job_system
