#include "lumina_engine.hpp"

#include "common/logger/logger.hpp"

namespace lumina::core {

struct FibData {
  int n;
  u64 *result;
};

void FibJob(void *parameter) {
  auto *data = static_cast<FibData *>(parameter);

  if (data->n <= 1) {
    *data->result = data->n;
    return;
  }

  u64 res1 = 0;
  u64 res2 = 0;

  auto *job_manager = job_system::JobManager::GetCurrent();
  ASSERT(job_manager != nullptr, "Job manager is null");

  // Create a counter initialized to 2 (waiting for two sub-jobs)
  // 'this' refers to the JobManager instance
  auto *counter = job_manager->AllocateCounter(2);

  FibData d1 = {.n = data->n - 1, .result = &res1};
  FibData d2 = {.n = data->n - 2, .result = &res2};

  // Submit two sub-jobs that will signal 'counter' when done
  auto *job1 = job_manager->AllocateJob();
  ASSERT(job1 != nullptr, "Job1 is null");
  job1->execute = FibJob;
  job1->data = &d1;
  job1->signal_counter = counter;
  job_manager->SubmitJob(job1);
  auto *job2 = job_manager->AllocateJob();
  ASSERT(job2 != nullptr, "Job2 is null");
  job2->execute = FibJob;
  job2->data = &d2;
  job2->signal_counter = counter;
  job_manager->SubmitJob(job2);

  // CRITICAL TEST: The fiber suspends here!
  // The worker thread will go find other work while this waits.
  job_manager->WaitForCounter(counter);
  job_manager->ReleaseCounter(counter);
  job_manager->ReleaseJob(job1);
  job_manager->ReleaseJob(job2);

  // Once we resume, we have the results
  *data->result = res1 + res2;
}

static auto TestJobSystem() -> void {
  job_system::JobManager job_manager;
  job_system::JobManagerInitializeInfo initialize_info = {
      .num_workers = 0, .fiber_pool_size = 1024};
  job_manager.Initialize(initialize_info);

  u64 final_result = 0;
  FibData root_data = {.n = 13, .result = &final_result};
  auto *sync_counter = job_manager.AllocateCounter(1);
  auto *job = job_manager.AllocateJob();
  job->execute = FibJob;
  job->data = &root_data;
  job->signal_counter = sync_counter;
  job_manager.SubmitJob(job);
  job_manager.WaitForCounter(sync_counter);
  LOG_INFO("Final result: {}", final_result);
  job_manager.ReleaseCounter(sync_counter);
  job_manager.ReleaseJob(job);
  job_manager.Shutdown();
}

auto LuminaEngine::Initialize(const LuminaInitializeInfo &init_info) -> void {
  auto &instance = GetStaticInstance();
  //TestJobSystem();
  instance.window_dimensions = init_info.window_dimensions;
  instance.job_manager = std::make_unique<job_system::JobManager>();
  instance.job_manager->Initialize({.num_workers = 0, .fiber_pool_size = 1024});
  instance.renderer =
      std::make_unique<renderer::LuminaRenderer>(init_info.vulkan_init_result);
  instance.renderer->Initialize();
  instance.is_initialized = true;
}

auto LuminaEngine::Shutdown() -> void {
  auto &instance = GetStaticInstance();
  instance.renderer->DeviceWaitIdle();
  instance.job_manager.reset();
  instance.renderer.reset();
  instance.is_initialized = false;
}

auto LuminaEngine::ExecuteFrame() -> void { renderer->DrawFrame(); }

} // namespace lumina::core
