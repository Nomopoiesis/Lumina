#include "job_manager.hpp"

#include "common/fast_random.hpp"
#include "common/logger/logger.hpp"
#include "common/lumina_check.hpp"
#include "common/lumina_terminate.hpp"
#include "common/lumina_util.hpp"
#include "lumina_assert.hpp"
#include "lumina_types.hpp"
#include "platform/platform_common/platform_services.hpp"

#include <memory>
#include <thread>

namespace lumina::core::job_system {

namespace {
std::unique_ptr<JobManager> g_job_manager; // NOLINT
} // namespace

auto InitializeJobSystem(JobManagerInitializeInfo initialize_info) -> void {
  LUMINA_CHECK(g_job_manager == nullptr, "Job system already initialized");
  g_job_manager = std::make_unique<JobManager>();
  g_job_manager->Initialize(initialize_info);
}

auto ShutdownJobSystem() -> void {
  // ~JobManager runs Shutdown(), which is the single teardown path: it joins
  // the workers before the pools they draw from are released.
  g_job_manager.reset();
}

auto GetJobManager() -> JobManager & {
  LUMINA_CHECK(g_job_manager != nullptr,
               "Job system not initialized, call InitializeJobSystem() first");
  return *g_job_manager;
}

auto JobManager::Initialize(JobManagerInitializeInfo initialize_info) -> void {
  if (is_initialized_) {
    LOG_CRITICAL("JobManager already initialized");
    LUMINA_TERMINATE();
  }

  if (initialize_info.num_workers == 0) {
    initialize_info.num_workers = std::thread::hardware_concurrency();
  }

  LUMINA_CHECK(initialize_info.fiber_pool_size > 0,
               "Fiber pool size must be greater than 0");

  job_pool.Initialize(1024);
  fiber_pool.Initialize(initialize_info.fiber_pool_size, [](auto &ctx) {
    ctx.handle =
        platform::common::PlatformServices::Instance().LuminaCreateFiber(
            64 * 1024, FiberEntryPoint, &ctx);
    // A null handle here surfaces later as SwitchToFiber(nullptr), which
    // transfers control to address 0 with no usable stack trace.
    LUMINA_CHECK(ctx.handle != nullptr, "Failed to create fiber");
    ctx.current_job = nullptr;
    ctx.pending_wait_counter = nullptr;
    ctx.owner.store(nullptr, std::memory_order_relaxed);
  });
  counter_pool.Initialize(1024);
  worker_contexts.reserve(initialize_info.num_workers);

  for (size_t i = 0; i < initialize_info.num_workers; ++i) {
    worker_contexts.push_back(std::make_unique<WorkerContext>());
    worker_contexts[i]->job_manager = this;
    worker_contexts[i]->worker_index = i;
    worker_contexts[i]->thread_handle =
        std::thread(&JobManager::WorkerEntryPoint, worker_contexts[i].get());
  }

  // Pin threads from the main thread after all std::thread objects are fully
  // constructed.
  for (size_t i = 0; i < initialize_info.num_workers; ++i) {
    platform::common::PlatformServices::Instance().LuminaPinThread(
        worker_contexts[i]->thread_handle.native_handle(), i);
  }

  is_initialized_ = true;
}
auto JobManager::Shutdown() -> void {
  is_shutdown_requested_ = true;
  for (auto &worker_context : worker_contexts) {
    worker_context->thread_handle.join();
  }
  worker_contexts.clear();
  job_pool.Deinitialize();
  fiber_pool.Deinitialize();
  counter_pool.Deinitialize();
  is_initialized_ = false;
}

auto JobManager::CurrentFiber() -> FiberContext * {
  return static_cast<FiberContext *>(
      platform::common::PlatformServices::Instance().LuminaGetFiberSelf());
}

auto JobManager::WorkerEntryPoint(WorkerContext *worker_context) -> void {
  platform::common::PlatformServices::Instance().LuminaSetThreadName(
      std::format("JM_Worker_{}", worker_context->worker_index).c_str());
  worker_context->master_fiber =
      platform::common::PlatformServices::Instance().LuminaConvertThreadToFiber(
          worker_context);
  // Was previously unchecked: a failed conversion leaves master_fiber null and
  // every subsequent yield back to the master jumps to address 0.
  LUMINA_CHECK(worker_context->master_fiber != nullptr,
               "Failed to convert worker thread to fiber");
  worker_context->current_fiber = worker_context->master_fiber;

  JobManager::WorkerLoop(worker_context);
}

auto JobManager::WorkerLoop(WorkerContext *ctx) -> void {
  while (!ctx->job_manager->IsShutdownRequested()) {
    // Drain any externally submitted jobs into the local work-stealing deque
    // so they become stealable by other workers.
    Job *ext_job = nullptr;
    while (ctx->external_job_queue.Pop(ext_job)) {
      auto success = ctx->work_stealing_deque.Push(ext_job);
      ASSERT(success, "Failed to drain external job into work stealing deque");
    }

    Job *job = nullptr;
    FiberContext *fiber_context = nullptr;
    // first try to resume a fiber from the resume queue, we we get nothing,
    // try to pop a job from the work stealing deque
    if (ctx->job_manager->resume_queue.Pop(fiber_context)) {
      LUMINA_CHECK(fiber_context != nullptr, "Resumed fiber context is null");
    } else if (ctx->work_stealing_deque.Pop(job) ||
               ctx->job_manager->StealFromOtherWorker(ctx->worker_index, job)) {
      LUMINA_CHECK(job != nullptr, "Popped job is null");
      fiber_context = ctx->job_manager->fiber_pool.Acquire();
      if (fiber_context == nullptr) {
        // failed to acquire a fiber context, we should push the job back to the
        // work stealing deque yield and restart
        LOG_WARNING("Failed to acquire a fiber context, this is not a good "
                    "sign (somehow we exhaused the fiber pool), pushing job "
                    "back to the work stealing deque and yielding");
        const bool returned = ctx->work_stealing_deque.Push(job);
        LUMINA_CHECK(returned, "Failed to return job to work stealing deque");
        std::this_thread::yield();
        continue;
      }
      fiber_context->current_job = job;
    } else {
      // No jobs were found (for now we just yield the thread, may do something
      // smarter later)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    // Resumed and freshly-scheduled fibers are handled identically from here.
    LUMINA_CHECK(fiber_context->handle != nullptr,
                 "Fiber context has a null handle");

    // Tell the fiber which worker it is about to run on. It may last have run
    // on a different thread, and this is what it reads instead of a
    // thread_local once it resumes.
    fiber_context->owner.store(ctx, std::memory_order_release);

    ctx->current_fiber = fiber_context->handle;
    SwitchContext(ctx->master_fiber, fiber_context->handle);
    ctx->current_fiber = ctx->master_fiber;

    // The fiber is now genuinely parked, so it is safe to publish it to a
    // counter it asked to wait on.
    ctx->job_manager->PublishPendingWait(fiber_context);

    if (fiber_context->is_completed.load(std::memory_order_acquire)) {
      fiber_context->Reset();
      ctx->job_manager->fiber_pool.Release(fiber_context);
    }
  }
}

[[noreturn]] auto JobManager::FiberEntryPoint(void *data) -> void {
  ASSERT(data != nullptr, "Fiber data is null");
  auto *fiber_context = static_cast<FiberContext *>(data);

  // Bind this fiber's identity into its fiber-local slot. The FiberContext to
  // OS-fiber pairing is fixed for the lifetime of the pool, so once is enough,
  // and the value travels with the fiber when it is resumed on another thread.
  platform::common::PlatformServices::Instance().LuminaSetFiberSelf(
      fiber_context);

  while (true) {
    LUMINA_CHECK(fiber_context->current_job != nullptr,
                 "Fiber resumed with no current job");
    fiber_context->current_job->execute(fiber_context->current_job->data);

    // Re-read the owner after running the job: it may have suspended on a
    // counter and been resumed by a different worker on a different thread.
    auto *owner = fiber_context->owner.load(std::memory_order_acquire);
    if (fiber_context->current_job->signal_counter != nullptr) {
      owner->job_manager->Signal(fiber_context->current_job->signal_counter);
    }
    owner->job_manager->ReleaseJob(fiber_context->current_job);
    fiber_context->is_completed.store(true, std::memory_order_release);
    fiber_context->current_job = nullptr;
    YieldToMaster(fiber_context);
  }
}

auto JobManager::YieldToMaster(FiberContext *fiber_context) -> void {
  auto *owner = fiber_context->owner.load(std::memory_order_acquire);
  SwitchContext(fiber_context->handle, owner->master_fiber);
}

auto JobManager::SwitchContext(FiberHandle from_fiber, FiberHandle to_fiber)
    -> void {
  // Switching to a null fiber transfers control to address 0: the debugger
  // reports "access violation executing at 0x0" with an empty call stack,
  // because there is no frame left to unwind. Catch it here instead.
  LUMINA_CHECK(to_fiber != nullptr, "Attempted to switch to a null fiber");
  platform::common::PlatformServices::Instance().LuminaSwitchToFiber(from_fiber,
                                                                     to_fiber);
}

auto JobManager::StealFromOtherWorker(size_t thief_index, Job *&job) -> bool {
  u32 start_index =
      common::random::FastRandom() % SafeU64ToU32(worker_contexts.size());

  for (size_t i = 0; i < worker_contexts.size(); ++i) {
    size_t victim_index = (start_index + i) % worker_contexts.size();
    if (victim_index == thief_index) {
      continue;
    }
    auto &victim_context = worker_contexts[victim_index];
    if (victim_context->work_stealing_deque.Steal(job)) {
      return true;
    }
  }
  return false;
}

auto JobManager::AllocateCounter(u32 value) -> Counter * {
  auto *counter = counter_pool.Acquire();
  LUMINA_CHECK(counter != nullptr, "Counter pool exhausted");
  counter->value.store(value);
  counter->waiting_count.store(0);
  for (auto &waiting_fiber : counter->waiting_fibers) {
    waiting_fiber = nullptr;
  }
  return counter;
}

auto JobManager::ReleaseCounter(Counter *counter) -> void {
  counter->Reset();
  counter_pool.Release(counter);
}

auto JobManager::AcquireJob() -> Job * {
  auto *job = job_pool.Acquire();
  LUMINA_CHECK(job != nullptr, "Job pool exhausted");
  allocated_job_count.fetch_add(1, std::memory_order_relaxed);
  return job;
}

auto JobManager::ReleaseJob(Job *job) -> void {
  job->Reset();
  job_pool.Release(job);
  allocated_job_count.fetch_sub(1, std::memory_order_relaxed);
}

auto JobManager::SubmitJob(Job *job) -> void {
  LUMINA_CHECK(job != nullptr, "Submitted job is null");
  LUMINA_CHECK(job->execute != nullptr,
               "Submitted job has no execute function");

  auto *self = CurrentFiber();
  if (self != nullptr) {
    // We are a worker which created a new job, we can push it to the local
    // queue
    auto *owner = self->owner.load(std::memory_order_acquire);
    if (owner->work_stealing_deque.Push(job)) {
      return;
    }
    LUMINA_CHECK(false, "Failed to push job to local work stealing deque");
  }

  // We are an external thread (main, render, etc.). Push to a worker's external
  // job queue via round-robin. The external queue is safe for multiple
  // producers and will be drained into the worker's work-stealing deque, making
  // the job stealable by other workers.
  auto worker_index =
      round_robin_index.fetch_add(1, std::memory_order_relaxed) %
      worker_contexts.size();
  for (size_t i = 0; i < worker_contexts.size(); ++i) {
    size_t index = (worker_index + i) % worker_contexts.size();
    if (worker_contexts[index]->external_job_queue.Push(job)) {
      return;
    }
  }

  // TODO: This is a critical error, we should handle it better
  LOG_CRITICAL(
      "Failed to submit job to any worker, this will lead to a lost job, "
      "and potential deadlock");
  LUMINA_TERMINATE();
}

auto JobManager::WaitForCounter(Counter *counter) -> void {
  // fast path - the counter is already at the target value or below
  if (counter->value.load(std::memory_order_acquire) == 0) {
    return;
  }
  // it seems we are gonna have to wait

  auto *self = CurrentFiber();
  if (self != nullptr) {
    // we are aworker thread (fiber), we can suspend ourselves and do other work

    // Publishing ourselves here would be a race: this fiber keeps running until
    // the switch below completes, and as soon as it is visible, Signal() can
    // hand it to another worker which then resumes a fiber that is still
    // executing on this thread. Hand the counter to the master fiber and let it
    // publish once the switch has actually happened.
    self->pending_wait_counter = counter;

    // We return to master and possibly pick other jobs while this fiber is
    // suspended
    YieldToMaster(self);
  } else {
    // we are a outside thread, we need to wait for the counter to be signaled
    while (counter->value.load(std::memory_order_acquire) > 0) {
      std::this_thread::yield();
    }
  }
}

auto JobManager::PushToResumeQueue(FiberContext *fiber_context) -> void {
  u32 retry_count = 0;
  while (!resume_queue.Push(fiber_context)) {
    if (retry_count > 100) {
      // TODO: This is a critical error, we should handle it better
      LOG_CRITICAL("Failed to push fiber context to resume queue, this will "
                   "lead to a lost job, and potential deadlock");
      LUMINA_TERMINATE();
    }
    retry_count++;
    std::this_thread::yield();
  }
}

auto JobManager::PublishPendingWait(FiberContext *parked_fiber) -> void {
  auto *counter = parked_fiber->pending_wait_counter;
  if (counter == nullptr) {
    return;
  }
  parked_fiber->pending_wait_counter = nullptr;

  counter->waiting_count.fetch_add(1, std::memory_order_relaxed);

  // waiting_count cannot double as a slot index: both Signal() and the
  // take-back path below decrement it, so fetch_add would hand the same index
  // to two fibers once any waiter has been released — and past 16 waiters it
  // indexed straight off the end of waiting_fibers, writing into the
  // neighbouring pooled Counter. Claim a slot by CAS instead.
  size_t slot = ArrayCount(counter->waiting_fibers);
  for (size_t i = 0; i < ArrayCount(counter->waiting_fibers); ++i) {
    FiberContext *expected = nullptr;
    if (counter->waiting_fibers[i].compare_exchange_strong(
            expected, parked_fiber, std::memory_order_acq_rel,
            std::memory_order_relaxed)) {
      slot = i;
      break;
    }
  }
  LUMINA_CHECK(slot < ArrayCount(counter->waiting_fibers),
               "No free wait slot on counter (more than 16 fibers waiting)");

  // The counter may have reached zero between the fiber's fast-path check and
  // this publish. If we can take the fiber back, Signal() never saw it and
  // nobody else will resume it, so queue it ourselves.
  if (counter->value.load(std::memory_order_acquire) == 0) {
    FiberContext *fiber =
        std::atomic_exchange(&counter->waiting_fibers[slot], nullptr);
    if (fiber != nullptr) {
      counter->waiting_count.fetch_sub(1, std::memory_order_release);
      PushToResumeQueue(fiber);
    }
    // Otherwise the signaler already took it and is queueing it right now.
  }
}

auto JobManager::Signal(Counter *counter) -> void {
  // signal decrements the wait counter, if it reaches 0, we need to resume
  // waiting fibers
  if (counter->value.fetch_sub(1, std::memory_order_acq_rel) != 1) {
    return;
  }

  // Collected first, queued afterwards. Queueing a waiter makes it runnable on
  // any worker, and the first thing it does on waking is ReleaseCounter() —
  // which resets this counter and hands it back to the pool, possibly straight
  // into another AllocateCounter. Anything we read or write here after that
  // point belongs to somebody else.
  FiberContext *to_resume[MaxCounterWaiters];
  u32 resume_count = 0;

  while (counter->waiting_count.load(std::memory_order_acquire) > 0) {
    for (size_t i = 0; i < MaxCounterWaiters; ++i) {
      FiberContext *fiber =
          std::atomic_exchange(&counter->waiting_fibers[i], nullptr);
      if (fiber != nullptr) {
        counter->waiting_count.fetch_sub(1, std::memory_order_release);
        // Bounded because WaitForCounter's fast path returns once value hits
        // zero, so no genuinely new waiter can register past this point.
        LUMINA_CHECK(resume_count < MaxCounterWaiters,
                     "More waiters collected than the counter has slots");
        to_resume[resume_count++] = fiber;
      }
    }
  }

  // `counter` must not be touched past this line.
  for (u32 i = 0; i < resume_count; ++i) {
    PushToResumeQueue(to_resume[i]);
  }
}

} // namespace lumina::core::job_system
