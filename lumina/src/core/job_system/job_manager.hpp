#pragma once

#include "common/lumina_check.hpp"
#include "lumina_types.hpp"

#include "data_structures/lock_free_concurrent_queue.hpp"
#include "data_structures/lock_free_object_pool.hpp"
#include "data_structures/work_stealing_deque.hpp"
#include <algorithm>
#include <atomic>
#include <functional>
#include <thread>
#include <type_traits>

namespace lumina::core::job_system {

using FiberHandle = void *;

class JobManager;
struct FiberContext;
struct WorkerContext;

// Upper bound on fibers that can suspend on a single counter.
inline constexpr size_t MaxCounterWaiters = 16;

struct Counter {
  std::atomic<u32> value;
  std::atomic<u32> waiting_count;

  // Fibers suspended on this counter. Whoever resumes one must also be able to
  // return it to the fiber pool once its job completes, so this tracks the
  // owning context rather than the bare fiber handle.
  std::atomic<FiberContext *> waiting_fibers[MaxCounterWaiters];

  auto Reset() -> void {
    value.store(0);
    waiting_count.store(0);
    for (auto &waiting_fiber : waiting_fibers) {
      waiting_fiber.store(nullptr);
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

  // Which worker this fiber is running on, republished by that worker's master
  // fiber immediately before every switch.
  //
  // This is what job-system code reads instead of a thread_local. A pooled
  // fiber can be resumed on a different thread than it last ran on, and
  // compilers cache the thread's TLS block address across calls — so a
  // thread_local read after a resume can name the thread the fiber *used* to
  // run on. Atomic so the load cannot be hoisted across a fiber switch.
  std::atomic<WorkerContext *> owner;

  // Set by this fiber before it yields, consumed by the master fiber once the
  // switch away has actually completed. A fiber must not publish itself to a
  // counter directly: it keeps running until the switch finishes, and another
  // worker could resume it in that window.
  Counter *pending_wait_counter;

  auto Reset() -> void {
    current_job = nullptr;
    pending_wait_counter = nullptr;
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
    common::data_structures::LockFreeConcurrentQueue<FiberContext *>;
using ExternalJobQueue =
    common::data_structures::LockFreeConcurrentQueue<Job *>;

// At namespace scope so FiberContext can point back at it. A worker's master
// fiber is bound to its thread by ConvertThreadToFiber and never migrates, so
// this is the one place thread identity is stable.
struct WorkerContext {
  JobManager *job_manager;
  std::thread thread_handle;
  size_t worker_index;

  // Fiber State
  FiberHandle master_fiber;
  FiberHandle current_fiber;

  JobStealingDeque work_stealing_deque;
  ExternalJobQueue external_job_queue{256};
};

class JobManager {
public:
  static constexpr u32 MAX_FIBERS = 256;
  static constexpr u32 MAX_COUNTERS = 1024;

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

  [[nodiscard]] auto GetWorkerCount() const -> size_t {
    return worker_contexts.size();
  }

  [[nodiscard]] auto StealFromOtherWorker(size_t thief_index, Job *&job)
      -> bool;

  auto AllocateCounter(u32 value) -> Counter *;
  auto ReleaseCounter(Counter *counter) -> void;

  auto AcquireJob() -> Job *;

  auto SubmitJob(Job *job) -> void;
  auto WaitForCounter(Counter *counter) -> void;
  auto Signal(Counter *counter) -> void;

private:
  bool is_initialized_ = false;
  bool is_shutdown_requested_ = false;

  // Identity of the fiber running on this call stack, or nullptr on a thread
  // that is not executing a job fiber (the main thread, or a worker's master
  // fiber). Backed by fiber-local storage rather than thread_local — see
  // PlatformServices::LuminaGetFiberSelf for why that distinction matters.
  static auto CurrentFiber() -> FiberContext *;

  static auto SwitchContext(FiberHandle from_fiber, FiberHandle to_fiber)
      -> void;

  static auto WorkerEntryPoint(WorkerContext *worker_context) -> void;
  static auto FiberEntryPoint(void *data) -> void;
  static auto WorkerLoop(WorkerContext *ctx) -> void;
  static auto YieldToMaster(FiberContext *fiber_context) -> void;

  auto ReleaseJob(Job *job) -> void;

  // Publishes a fiber that has just suspended itself onto the counter it asked
  // to wait on. Runs on the master fiber, after the context switch completed.
  auto PublishPendingWait(FiberContext *parked_fiber) -> void;
  auto PushToResumeQueue(FiberContext *fiber_context) -> void;

  std::atomic<size_t> round_robin_index = 0;
  JobPool job_pool;
  FiberPool fiber_pool;
  CounterPool counter_pool;
  std::vector<std::unique_ptr<WorkerContext>> worker_contexts;
  FiberQueue resume_queue;

  std::atomic<size_t> allocated_job_count = 0;
};

// The process-wide job system instance.
//
// JobManager itself stays an ordinary constructible class: tests build their
// own on the stack with whatever worker count a case needs. This is only the
// single instance the engine runs on, exposed here so any subsystem can submit
// work by including this header alone rather than depending on LuminaEngine.
//
// Lifetime is explicit, and the storage is a pointer rather than a function
// local static, on purpose. Workers are std::threads that Shutdown() joins;
// leaving that join to static destruction would run it after main() returns.
// InitializeJobSystem/ShutdownJobSystem are called once each, by the engine.
auto InitializeJobSystem(JobManagerInitializeInfo initialize_info) -> void;
auto ShutdownJobSystem() -> void;

// Valid only between InitializeJobSystem and ShutdownJobSystem.
auto GetJobManager() -> JobManager &;

// Upper bound on the jobs a single ParallelForBatched may submit. The binding
// limits are the 1024-entry job pool and, when called from a worker fiber, that
// worker's deque — SubmitJob from a fiber pushes locally rather than
// round-robin, so every chunk job lands in one deque. Failing here names the
// cause; overrunning either limit does not.
inline constexpr size_t MaxParallelForJobs = 256;

// Splits [0, count) into batch_size ranges and runs them across the worker
// pool, one job per chunk — so batch_size is also how the caller controls how
// many jobs this creates.
//
// `func` is invoked as func(begin, end) once per chunk, concurrently from
// several threads. It must be safe to call that way, and chunks must not write
// to overlapping state. The chunk index is begin / batch_size, which is what
// lets callers write into per-chunk output slices without atomics.
template <typename F>
auto ParallelForBatched(size_t count, size_t batch_size, F &&func) -> void {
  if (count == 0) {
    return;
  }
  ASSERT(batch_size > 0, "ParallelForBatched batch_size must be nonzero");

  if (count <= batch_size) {
    func(size_t{0}, count);
    return;
  }

  struct DrainState {
    std::atomic<size_t> cursor{0};
    size_t count;
    size_t batch_size;
    std::remove_reference_t<F> *body;
  } state = {0, count, batch_size, &func};

  auto drain = [](void *data) {
    auto *drain_state = static_cast<DrainState *>(data);
    for (;;) {
      auto begin = drain_state->cursor.fetch_add(drain_state->batch_size,
                                                 std::memory_order_relaxed);
      if (begin >= drain_state->count) {
        break;
      }
      (*drain_state->body)(
          begin, std::min(begin + drain_state->batch_size, drain_state->count));
    }
  };

  const size_t chunk_count = (count + batch_size - 1) / batch_size;
  LUMINA_CHECK(chunk_count <= MaxParallelForJobs,
               "ParallelForBatched batch_size too small for the job budget");

  auto &manager = GetJobManager();
  auto *counter = manager.AllocateCounter(static_cast<u32>(chunk_count));
  for (size_t i = 0; i < chunk_count; ++i) {
    auto *job = manager.AcquireJob();
    job->signal_counter = counter;
    job->data = &state;
    job->execute = drain;
    manager.SubmitJob(job);
  }
  drain(&state);

  manager.WaitForCounter(counter);
  manager.ReleaseCounter(counter);
}

} // namespace lumina::core::job_system
