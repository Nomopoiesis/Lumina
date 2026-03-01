#include "lumina_engine.hpp"

#include <algorithm>

#include "common/logger/logger.hpp"
#include "math/basic.hpp"

#include "input/input_action.hpp"

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
  Transform transform = {
      .position = {2.0F, 2.0F, 2.0F},
      .rotation = {-35.0F, 45.0F, 0.0F},
      .scale = {1.0F, 1.0F, 1.0F},
  };
  CameraSettings camera_settings = {
      .fov_degrees = 45.0F,
      .aspect_ratio = static_cast<f32>(init_info.window_dimensions.width) /
                      static_cast<f32>(init_info.window_dimensions.height),
      .near_plane = 0.1F,
      .far_plane = 10.0F,
  };
  instance.camera = Camera(transform, camera_settings);
  instance.window_dimensions = init_info.window_dimensions;
  instance.job_manager = std::make_unique<job_system::JobManager>();
  instance.job_manager->Initialize({.num_workers = 0, .fiber_pool_size = 1024});
  instance.renderer =
      std::make_unique<renderer::LuminaRenderer>(init_info.vulkan_init_result);
  instance.renderer->Initialize();

  InitializeInputActionMap(instance.input_dispatcher.GetInputActionMap());
  instance.input_dispatcher.RegisterHandler(
      instance.camera_movement_input_handler.get(), 10);

  instance.is_initialized = true;
}

auto LuminaEngine::Shutdown() -> void {
  auto &instance = GetStaticInstance();
  instance.renderer->DeviceWaitIdle();
  instance.job_manager.reset();
  instance.renderer.reset();
  instance.is_initialized = false;
}

auto LuminaEngine::ExecuteFrame(f64 delta_time) -> void {
  // Clamp the delta time to 0.1 seconds to prevent large jumps in time
  delta_time = std::clamp<f64>(delta_time, 0.0, 0.1F);
  frame_time_info.delta_time = delta_time;
  frame_time_info.total_time += delta_time;
  input_dispatcher.Dispatch(input_state);
  renderer->DrawFrame();
}

auto CameraMovementInputHandler::HandleInput(
    const std::span<const ActionEvent> &action_events) -> void {
  math::Vec3 move_direction{};
  math::Vec3 look_rotation{};
  bool trap_cursor = false;
  const auto &transform = camera.GetTransform();
  for (const auto &action_event : action_events) {
    if (action_event.action_id == ActionID(std::string_view("MoveForward"))) {
      move_direction += transform.Forward();
    }
    if (action_event.action_id == ActionID(std::string_view("MoveBackward"))) {
      move_direction -= transform.Forward();
    }
    if (action_event.action_id == ActionID(std::string_view("MoveLeft"))) {
      move_direction -= transform.Right();
    }
    if (action_event.action_id == ActionID(std::string_view("MoveRight"))) {
      move_direction += transform.Right();
    }
    if (action_event.action_id == ActionID(std::string_view("MoveUp"))) {
      move_direction += EngineCoordinates::UP;
    }
    if (action_event.action_id == ActionID(std::string_view("MoveDown"))) {
      move_direction -= EngineCoordinates::UP;
    }
    if (action_event.action_id == ActionID(std::string_view("LookVertical"))) {
      look_rotation.pitch += -action_event.axis_value;
      look_rotation.pitch = math::Clamp(look_rotation.pitch, -89.0F, 89.0F);
    }
    if (action_event.action_id ==
        ActionID(std::string_view("LookHorizontal"))) {
      look_rotation.yaw += -action_event.axis_value;
    }
    if (action_event.action_id == ActionID(std::string_view("TrapCursor"))) {
      trap_cursor = action_event.key_state == KeyState::Held;
    }
  }
  camera.Move(move_direction * move_speed);
  if (trap_cursor) {
    engine.SetCursorTrapped(true);
    look_rotation *= look_speed;
    camera.Rotate(look_rotation);
  } else {
    engine.SetCursorTrapped(false);
  }
}

} // namespace lumina::core
