#include "lumina_engine.hpp"

#include <algorithm>

#include "common/logger/logger.hpp"

#include "basic_geometry.hpp"
#include "components/camera.hpp"
#include "components/static_mesh_component.hpp"
#include "components/transform.hpp"
#include "input/input_action.hpp"

namespace lumina::core {

using namespace lumina::core::components;

struct FibData {
  int n;
  u64 *result;
};

static void FibJob(void *parameter) {
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

static auto InitializeInputActionMap(InputActionMap &input_action_map) -> void {
  input_action_map.BindAction(ActionID(std::string_view("MoveForward")),
                              KeyInputBinding(KeyCode::W, KeyState::Held));
  input_action_map.BindAction(ActionID(std::string_view("MoveBackward")),
                              KeyInputBinding(KeyCode::S, KeyState::Held));
  input_action_map.BindAction(ActionID(std::string_view("MoveLeft")),
                              KeyInputBinding(KeyCode::A, KeyState::Held));
  input_action_map.BindAction(ActionID(std::string_view("MoveRight")),
                              KeyInputBinding(KeyCode::D, KeyState::Held));
  input_action_map.BindAction(ActionID(std::string_view("MoveUp")),
                              KeyInputBinding(KeyCode::E, KeyState::Held));
  input_action_map.BindAction(ActionID(std::string_view("MoveDown")),
                              KeyInputBinding(KeyCode::Q, KeyState::Held));
  input_action_map.BindAction(ActionID(std::string_view("LookVertical")),
                              MouseAxisBinding(MouseAxis::Y));
  input_action_map.BindAction(ActionID(std::string_view("LookHorizontal")),
                              MouseAxisBinding(MouseAxis::X));
  input_action_map.BindAction(
      ActionID(std::string_view("TrapCursor")),
      MouseButtonInputBinding(MouseButton::Right, KeyState::Held));
}

auto LuminaEngine::Initialize(const LuminaInitializeInfo &init_info) -> void {
  auto &instance = GetStaticInstance();
  // TestJobSystem();

  instance.window_dimensions = init_info.window_dimensions;
  instance.job_manager = std::make_unique<job_system::JobManager>();
  instance.job_manager->Initialize({.num_workers = 0, .fiber_pool_size = 1024});
  instance.renderer =
      std::make_unique<renderer::LuminaRenderer>(init_info.vulkan_init_result);
  instance.renderer->Initialize();

  instance.camera_movement_controller =
      std::make_unique<CameraMovementController>(INVALID_ENTITY_ID);

  InitializeInputActionMap(instance.input_dispatcher.GetInputActionMap());
  instance.input_dispatcher.RegisterHandler(
      instance.camera_movement_controller.get(), 10);

  // Create the default world (scene)
  instance.current_world = std::make_unique<World>();
  auto &world = *instance.current_world;

  // Create a default camera entity in the world
  auto entity_id = world.CreateEntity();
  world.AddComponent<Transform>(entity_id, math::Vec3{2.0F, 2.0F, 2.0F},
                                math::Vec3{-35.0F, 45.0F, 0.0F},
                                math::Vec3{1.0F, 1.0F, 1.0F});
  CameraSettings camera_settings = {
      .fov_degrees = 45.0F,
      .aspect_ratio = static_cast<f32>(init_info.window_dimensions.width) /
                      static_cast<f32>(init_info.window_dimensions.height),
      .near_plane = 0.1F,
      .far_plane = 10.0F,
  };
  world.AddComponent<Camera>(entity_id, camera_settings);

  world.SetActiveCamera(entity_id);
  instance.camera_movement_controller->SetControlledEntity(entity_id);

  auto static_mesh_handle = instance.static_mesh_manager.Create();
  instance.static_mesh_manager.Set(static_mesh_handle, BasicGeometry::Quad());

  entity_id = world.CreateEntity();
  world.AddComponent<StaticMeshComponent>(entity_id, static_mesh_handle);

  instance.is_initialized = true;
}

auto LuminaEngine::Shutdown() -> void {
  auto &instance = GetStaticInstance();
  instance.current_world.reset();
  instance.renderer->DeviceWaitIdle();
  instance.job_manager.reset();
  instance.renderer.reset();
  instance.is_initialized = false;
}

auto LuminaEngine::ExecuteFrame(f64 delta_time) -> void {
  // Clamp the delta time to 0.1 seconds to prevent large jumps in time
  delta_time = std::clamp<f64>(delta_time, 0.0, 0.1);
  frame_time_info.delta_time = delta_time;
  frame_time_info.total_time += delta_time;
  input_dispatcher.Dispatch(input_state);
  renderer->DrawFrame();
}

} // namespace lumina::core
