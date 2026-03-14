#include "lumina_engine.hpp"

#include "common/logger/logger.hpp"

#include "basic_geometry.hpp"
#include "components/camera.hpp"
#include "components/static_mesh_component.hpp"
#include "components/transform.hpp"
#include "input/input_action.hpp"
#include "math/basic.hpp"

namespace lumina::core {

using namespace lumina::core::components;

static auto UpdateUniformBuffer(void *&mapped_data) -> bool {
  auto &world = core::LuminaEngine::Instance().GetCurrentWorld();
  auto camera_id = world.GetActiveCamera();
  auto transform = world.GetComponent<core::components::Transform>(camera_id);
  auto camera = world.GetComponent<core::components::Camera>(camera_id);
  UniformBufferObject ubo = {};
  ubo.view = CalculateViewMatrix(transform);
  ubo.proj = camera.ToProjectionMatrix();
  memcpy(mapped_data, &ubo, sizeof(UniformBufferObject));
  return true;
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

  instance.default_pipeline_handle = instance.renderer->CreateGraphicsPipeline(
      {.vertex_layout = renderer::VertexBufferLayout::Interleave(
           std::span<const VertexAttribute>(
               {VertexAttribute{.type = VertexAttributeType::Position,
                                .element_type = ElementType::Vec3},
                VertexAttribute{.type = VertexAttributeType::Color,
                                .element_type = ElementType::Vec3},
                VertexAttribute{.type = VertexAttributeType::TexCoord,
                                .element_type = ElementType::Vec2}}))});

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
      .far_plane = 100.0F,
  };
  world.AddComponent<Camera>(entity_id, camera_settings);

  world.SetActiveCamera(entity_id);
  instance.camera_movement_controller->SetControlledEntity(entity_id);

  auto static_mesh_handle = instance.static_mesh_manager.Create();
  instance.static_mesh_manager.Set(static_mesh_handle, BasicGeometry::Quad());

  entity_id = world.CreateEntity();
  world.AddComponent<Transform>(entity_id, math::Vec3{0.0F, 0.0F, 0.0F},
                                math::Vec3{0.0F, 0.0F, 0.0F},
                                math::Vec3{1.2F, 1.2F, 1.2F});
  world.AddComponent<StaticMeshComponent>(entity_id, static_mesh_handle);

  entity_id = world.CreateEntity();
  world.AddComponent<Transform>(entity_id, math::Vec3{0.0F, 0.0F, -1.0F},
                                math::Vec3{-45.0F, 0.0F, 0.0F},
                                math::Vec3{1.0F, 1.0F, 1.0F});
  world.AddComponent<StaticMeshComponent>(entity_id, static_mesh_handle);

  instance.is_initialized = true;
}

auto LuminaEngine::Shutdown() -> void {
  auto &instance = GetStaticInstance();
  instance.renderer->Shutdown();
  instance.renderer->DeviceWaitIdle();
  instance.job_manager.reset();
  instance.renderer.reset();
  instance.current_world.reset();
  instance.is_initialized = false;
}

auto LuminaEngine::BeginFrame(Timer &timer) -> void {
  renderer->AcquireFrameContextForUpdate();
  auto delta_time = timer.Tick();
  // Clamp the delta time to 0.1 seconds to prevent large jumps in time
  delta_time = math::Clamp(delta_time, 0.0, 0.1);
  frame_time_info.delta_time = delta_time;
  frame_time_info.total_time += delta_time;
}

auto LuminaEngine::EndFrame() -> void {
  renderer->ReleaseFrameContextForUpdate();
}

auto LuminaEngine::ExecuteFrame() -> void {
  // Dispatch upload jobs for any meshes not yet uploaded to the GPU
  current_world->ForEachComponent<StaticMeshComponent>(
      [this](EntityID /*id*/, const StaticMeshComponent &component) {
        auto static_mesh_handle = component.GetStaticMeshHandle();
        auto mesh = static_mesh_manager.Get(static_mesh_handle);
        if (!mesh.has_value()) {
          return;
        }
        if (mesh->render_active) {
          return;
        }
        mesh->render_active = true;
        static_mesh_manager.Set(static_mesh_handle, std::move(*mesh));
        auto *job = job_manager->AcquireJob();
        job->execute = [static_mesh_handle](void *data) -> void {
          auto *engine = static_cast<LuminaEngine *>(data);
          auto m = engine->static_mesh_manager.Get(static_mesh_handle);
          ASSERT(m.has_value(), "Static mesh not found");
          auto render_mesh_handle = engine->GetRenderer().CreateRenderMesh(
              m.value(), engine->default_pipeline_handle);
          m->render_mesh_handle = render_mesh_handle;
          engine->static_mesh_manager.Set(static_mesh_handle, std::move(*m));
        };
        job->data = this;
        job_manager->SubmitJob(job);
      });

  auto *frame_sync_counter = job_manager->AllocateCounter(2);
  auto *job = job_manager->AcquireJob();
  job->execute = [](void *data) {
    auto *engine = static_cast<LuminaEngine *>(data);
    UpdateUniformBuffer(engine->renderer->GetFrameContextForUpdate()
                            ->GetUniformBuffer()
                            .mapped);
  };
  job->data = this;
  job->signal_counter = frame_sync_counter;
  job_manager->SubmitJob(job);

  job = job_manager->AcquireJob();
  job->execute = [](void *data) {
    // Build draw list from all StaticMeshComponents with uploaded meshes
    auto *engine = static_cast<LuminaEngine *>(data);
    auto *frame_context = engine->renderer->GetFrameContextForUpdate();
    frame_context->render_draw_list.clear();
    engine->current_world->ForEachComponent<StaticMeshComponent>(
        [engine, frame_context](EntityID id,
                                const StaticMeshComponent &component) -> void {
          auto mesh =
              engine->static_mesh_manager.Get(component.GetStaticMeshHandle());
          auto transform = engine->current_world->GetComponent<Transform>(id);
          if (!mesh.has_value()) {
            return;
          }
          if (mesh->render_mesh_handle.index == INVALID_RESOURCE_HANDLE_INDEX) {
            return;
          }
          frame_context->render_draw_list.push_back(
              {.render_mesh_handle = mesh->render_mesh_handle,
               .model = transform.GetModelMatrix()});
        });
  };
  job->data = this;
  job->signal_counter = frame_sync_counter;
  job_manager->SubmitJob(job);

  input_dispatcher.Dispatch(input_state);
  job_manager->WaitForCounter(frame_sync_counter);
  job_manager->ReleaseCounter(frame_sync_counter);
}

} // namespace lumina::core
