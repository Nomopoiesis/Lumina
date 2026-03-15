#include "job_manager.hpp"

#include "common/fast_random.hpp"
#include "common/logger/logger.hpp"
#include "common/lumina_terminate.hpp"
#include "common/lumina_util.hpp"
#include "lumina_assert.hpp"
#include "lumina_types.hpp"
#include "platform/common/platform_services.hpp"

#include <thread>

namespace lumina::core::job_system {

auto JobManager::Initialize(JobManagerInitializeInfo initialize_info) -> void {
  if (is_initialized_) {
    LOG_CRITICAL("JobManager already initialized");
    LUMINA_TERMINATE();
  }

  if (initialize_info.num_workers == 0) {
    initialize_info.num_workers = std::thread::hardware_concurrency();
  }

  ASSERT(initialize_info.fiber_pool_size > 0,
         "Fiber pool size must be greater than 0");

  job_pool.Initialize(1024);
  fiber_pool.Initialize(initialize_info.fiber_pool_size, [](auto &ctx) {
    ctx.handle =
        platform::common::PlatformServices::Instance().LuminaCreateFiber(
            64 * 1024, FiberEntryPoint, &ctx);
    ASSERT(ctx.handle != nullptr, "Fiber context handle is null");
    ctx.current_job = nullptr;
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

auto JobManager::WorkerEntryPoint(WorkerContext *worker_context) -> void {
  local_worker_context = worker_context;
  platform::common::PlatformServices::Instance().LuminaSetThreadName(
      std::format("JM_Worker_{}", worker_context->worker_index).c_str());
  platform::common::PlatformServices::Instance().LuminaPinThread(
      worker_context->thread_handle.native_handle(),
      worker_context->worker_index);
  worker_context->master_fiber =
      platform::common::PlatformServices::Instance().LuminaConvertThreadToFiber(
          worker_context);
  worker_context->current_fiber = worker_context->master_fiber;

  JobManager::WorkerLoop(worker_context);
}

auto JobManager::WorkerLoop(WorkerContext *ctx) -> void {
  while (!ctx->job_manager->IsShutdownRequested()) {
    Job *job = nullptr;
    FiberHandle handle = nullptr;
    // first try to resume a fiber from the resume queue, we we get nothing,
    // try to pop a job from the work stealing deque
    if (ctx->job_manager->resume_queue.Pop(handle)) {
      ASSERT(handle != nullptr, "Fiber handle is null");
      ctx->current_fiber = handle;
      SwitchContext(ctx->master_fiber, handle);
      ctx->current_fiber = ctx->master_fiber;
    } else if (ctx->work_stealing_deque.Pop(job) ||
               ctx->job_manager->StealFromOtherWorker(ctx->worker_index, job)) {
      ASSERT(job != nullptr, "Job is null");
      auto *fiber_context = ctx->job_manager->fiber_pool.Acquire();
      if (fiber_context == nullptr) {
        // failed to acquire a fiber context, we should push the job back to the
        // work stealing deque yield and restart
        LOG_WARNING("Failed to acquire a fiber context, this is not a good "
                    "sign (somehow we exhaused the fiber pool), pushing job "
                    "back to the work stealing deque and yielding");
        ctx->work_stealing_deque.Push(job);
        std::this_thread::yield();
        continue;
      }
      ASSERT(fiber_context->handle != nullptr, "Fiber context handle is null");
      ctx->current_fiber = fiber_context->handle;
      fiber_context->current_job = job;
      SwitchContext(ctx->master_fiber, ctx->current_fiber);
      ctx->current_fiber = ctx->master_fiber;
      if (fiber_context->is_completed.load(std::memory_order_acquire)) {
        fiber_context->Reset();
        ctx->job_manager->fiber_pool.Release(fiber_context);
      }
    } else {
      // No jobs were found (for now we just yield the thread, may do something
      // smarter later)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

auto JobManager::FiberEntryPoint(void *data) -> void {
  ASSERT(data != nullptr, "Fiber data is null");
  auto *fiber_context = static_cast<FiberContext *>(data);
  while (true) {
    ASSERT(fiber_context->current_job != nullptr, "Current job is null");
    fiber_context->current_job->execute(fiber_context->current_job->data);
    if (fiber_context->current_job->signal_counter != nullptr) {
      GetCurrent()->Signal(fiber_context->current_job->signal_counter);
    }
    fiber_context->is_completed.store(true, std::memory_order_release);
    fiber_context->current_job = nullptr;
    YieldToMasterFiber();
  }
}

auto JobManager::YieldToMasterFiber() -> void {
  SwitchContext(local_worker_context->current_fiber,
                local_worker_context->master_fiber);
}

auto JobManager::SwitchContext(FiberHandle from_fiber, FiberHandle to_fiber)
    -> void {
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
  counter->value.store(value);
  counter->waiting_count.store(0);
  for (auto &fiber_handle : counter->fiber_handles) {
    fiber_handle = nullptr;
  }
  return counter;
}

auto JobManager::ReleaseCounter(Counter *counter) -> void {
  counter->Reset();
  counter_pool.Release(counter);
}

auto JobManager::AllocateJob() -> Job * {
  auto *job = job_pool.Acquire();
  allocated_job_count.fetch_add(1, std::memory_order_relaxed);
  return job;
}

auto JobManager::ReleaseJob(Job *job) -> void {
  job->Reset();
  job_pool.Release(job);
  allocated_job_count.fetch_sub(1, std::memory_order_relaxed);
}

auto JobManager::SubmitJob(Job *job) -> void {
  ASSERT(job != nullptr, "Job is null");
  ASSERT(job->execute != nullptr, "Job execute function is null");

  if (local_worker_context != nullptr) {
    // We are a worker which created a new job, we can push it to the local
    // queue
    if (local_worker_context->work_stealing_deque.Push(job)) {
      return;
    }
    ASSERT(false, "Failed to push job to local work stealing deque");
  }

  // We are a outsider thread which created a new job, we need to push it to a
  // random worker, we use a round robin approach to avoid bias
  auto worker_index =
      round_robin_index.fetch_add(1, std::memory_order_relaxed) %
      worker_contexts.size();
  for (size_t i = 0; i < worker_contexts.size(); ++i) {
    size_t index = (worker_index + i) % worker_contexts.size();
    if (worker_contexts[index]->work_stealing_deque.Push(job)) {
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

  if (local_worker_context != nullptr) {
    // we are aworker thread (fiber), we can suspend ourselves and do other work
    auto *ctx = local_worker_context;
    auto slot = counter->waiting_count.fetch_add(1, std::memory_order_relaxed);
    ASSERT(slot < 16, "Slot is out of bounds");
    counter->fiber_handles[slot] = ctx->current_fiber;

    // edge case, we added the fiber to waiting list, but the counter is already
    // at the target value or below, we want to take the handle back, unless the
    // signaler already signaled it...
    if (counter->value.load(std::memory_order_acquire) == 0) {
      auto *handle =
          std::atomic_exchange(&counter->fiber_handles[slot], nullptr);
      if (handle != nullptr) {
        counter->waiting_count.fetch_sub(1, std::memory_order_release);
        // SUCCESS: We took it back. The signaler never saw us.
        // Just return and continue execution (no context switch).
        return;
      }
      // FAILURE: self is nullptr, meaning the Signaler ALREADY took our
      // pointer. We MUST switch context now, because the Signaler is currently
      // pushing us back to the queue
    }

    // We return to master and possibly pick other jobs while this fiber is
    // suspended
    YieldToMasterFiber();
  } else {
    // we are a outside thread, we need to wait for the counter to be signaled
    while (counter->value.load(std::memory_order_acquire) > 0) {
      std::this_thread::yield();
    }
  }
}

auto JobManager::Signal(Counter *counter) -> void {
  // signal decrements the wait counter, if it reaches 0, we need to resume
  // waiting fibers
  if (counter->value.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    // We need to resume waiting fibers
    while (counter->waiting_count.load(std::memory_order_acquire) > 0) {
      for (size_t i = 0; i < ArrayCount(counter->fiber_handles); ++i) {
        FiberHandle handle =
            std::atomic_exchange(&counter->fiber_handles[i], nullptr);
        if (handle != nullptr) {
          counter->waiting_count.fetch_sub(1, std::memory_order_release);
          u32 retry_count = 0;
          while (!resume_queue.Push(handle)) {
            if (retry_count > 100) {
              // TODO: This is a critical error, we should handle it better
              LOG_CRITICAL(
                  "Failed to push fiber handle to resume queue, this will lead "
                  "to a lost job, and potential deadlock");
              LUMINA_TERMINATE();
            }
            retry_count++;
            std::this_thread::yield();
          }
        }
      }
    }
  }
}

} // namespace lumina::core::job_system
