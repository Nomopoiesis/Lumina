#include "lumina_engine.hpp"

#include "common/fast_random.hpp"
#include "common/logger/logger.hpp"
#include "common/path_registry.hpp"

#include "basic_geometry.hpp"
#include "components/camera.hpp"
#include "components/light_component.hpp"
#include "components/static_mesh_component.hpp"
#include "components/transform.hpp"
#include "data_parsers/obj_parser.hpp"
#include "input/input_action.hpp"
#include "math/basic.hpp"
#include "math/trigonometry.hpp"
#include "mesh_cache.hpp"

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
  ubo.camera_position = transform.position;
  ubo.point_light_count = 0;
  world.ForEachComponent<LightComponent>(
      [&ubo, &world](EntityID id, const LightComponent &component) -> void {
        if (ubo.point_light_count >= 16) {
          return;
        }
        ubo.point_lights[ubo.point_light_count++] = {
            .position = world.GetComponent<Transform>(id).position,
            .intensity = component.intensity,
            .color = component.color,
            .attenuation_radius = component.attenation_radius,
        };
      });
  memcpy(mapped_data, &ubo, sizeof(UniformBufferObject));
  return true;
}

static auto SpawnMeshEntities(World &world, u32 count,
                              StaticMeshHandle mesh_handle,
                              const math::Vec3 &origin, f32 radius) -> void {
  using lumina::common::random::FastRandom;
  constexpr f32 InvU32Max = 1.0F / 4294967295.0F;

  for (u32 i = 0; i < count; ++i) {
    // Uniform distribution within a sphere via spherical coordinates
    const f32 u = static_cast<f32>(FastRandom()) * InvU32Max;
    const f32 v = static_cast<f32>(FastRandom()) * InvU32Max;
    const f32 w = static_cast<f32>(FastRandom()) * InvU32Max;

    const f32 theta = math::ACos(1.0F - (2.0F * u));
    const f32 phi = math::TWO_PI * v;
    const f32 r = radius * std::cbrt(w);

    const math::Vec3 position = {
        origin.x + (r * math::Sin(theta) * math::Cos(phi)),
        origin.y + (r * math::Sin(theta) * math::Sin(phi)),
        origin.z + (r * math::Cos(theta)),
    };

    const math::Vec3 rotation = {
        static_cast<f32>(FastRandom()) * InvU32Max * 360.0F,
        static_cast<f32>(FastRandom()) * InvU32Max * 360.0F,
        static_cast<f32>(FastRandom()) * InvU32Max * 360.0F,
    };

    auto entity_id = world.CreateEntity();
    world.AddComponent<Transform>(entity_id, position, rotation,
                                  math::Vec3{1.0F, 1.0F, 1.0F});
    world.AddComponent<StaticMeshComponent>(entity_id, mesh_handle);
  }
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

  auto mat_handle = instance.renderer->GetDefaultMaterialInstanceHandle();
  auto mat_opt = instance.renderer->GetMaterialInstance(mat_handle);
  if (!mat_opt.has_value()) {
    LOG_CRITICAL("Failed to get default material instance");
    LUMINA_TERMINATE();
  }
  auto mat_template_handle = mat_opt.value().GetTemplateHandle();

  instance.default_pipeline_handle = instance.renderer->CreateGraphicsPipeline(
      {.vertex_layout = renderer::VertexBufferLayout::Interleave(
           std::span<const VertexAttribute>(
               {VertexAttribute{.type = VertexAttributeType::Position,
                                .element_type = ElementType::Vec3},
                VertexAttribute{.type = VertexAttributeType::Normal,
                                .element_type = ElementType::Vec3},
                VertexAttribute{.type = VertexAttributeType::TexCoord,
                                .element_type = ElementType::Vec2}})),
       .material_template = mat_template_handle});

  instance.camera_movement_controller =
      std::make_unique<CameraMovementController>(INVALID_ENTITY_ID);

  InitializeInputActionMap(instance.input_dispatcher.GetInputActionMap());
  instance.input_dispatcher.RegisterHandler(
      instance.camera_movement_controller.get(), 10);

  constexpr std::string_view kModelCacheKey = "suzanne";
  const auto &model_cache =
      lumina::common::PathRegistry::Instance().model_cache;

  StaticMesh static_mesh;
  if (HasCachedMesh(kModelCacheKey, model_cache)) {
    LOG_INFO("Loading mesh from cache: suzanne");
    auto result = DeserializeStaticMesh(kModelCacheKey, model_cache);
    if (!result.has_value()) {
      LOG_CRITICAL("Failed to deserialize cached mesh: %s",
                   result.error().message);
      LUMINA_TERMINATE();
    }
    static_mesh = std::move(*result);
  } else {
    LOG_INFO("Parsing OBJ: suzanne");
    void *file_handle =
        platform::common::PlatformServices::Instance().LuminaOpenFile(
            lumina::common::PathRegistry::Instance()
                .models.Resolve("suzanne.obj")
                .string()
                .c_str());
    std::size_t file_size =
        platform::common::PlatformServices::Instance().LuminaGetFileSize(
            file_handle);
    common::data_structures::DataBuffer data_buffer(file_size);
    platform::common::PlatformServices::Instance().LuminaReadFile(
        file_handle, data_buffer.Data(), file_size);
    platform::common::PlatformServices::Instance().LuminaCloseFile(file_handle);
    auto obj_data = data_parsers::ParseOBJ(data_buffer.View());
    static_mesh.vertex_count = obj_data.vertex_count;
    static_mesh.vertex_attributes.emplace_back(
        VertexAttribute{.type = VertexAttributeType::Position,
                        .element_type = ElementType::Vec3},
        std::vector<u8>(reinterpret_cast<u8 *>(obj_data.positions.data()),
                        reinterpret_cast<u8 *>(obj_data.positions.data() +
                                               obj_data.positions.size())));
    static_mesh.vertex_attributes.emplace_back(
        VertexAttribute{.type = VertexAttributeType::Normal,
                        .element_type = ElementType::Vec3},
        std::vector<u8>(reinterpret_cast<u8 *>(obj_data.normals.data()),
                        reinterpret_cast<u8 *>(obj_data.normals.data() +
                                               obj_data.normals.size())));
    static_mesh.vertex_attributes.emplace_back(
        VertexAttribute{.type = VertexAttributeType::TexCoord,
                        .element_type = ElementType::Vec2},
        std::vector<u8>(reinterpret_cast<u8 *>(obj_data.tex_coords.data()),
                        reinterpret_cast<u8 *>(obj_data.tex_coords.data() +
                                               obj_data.tex_coords.size())));
    static_mesh.indices = obj_data.indices;

    auto cache_result =
        SerializeStaticMesh(static_mesh, kModelCacheKey, model_cache);
    if (!cache_result.has_value()) {
      LOG_WARNING("Failed to write mesh cache: %s",
                  cache_result.error().message);
    }
  }

  auto static_mesh_handle = instance.static_mesh_manager.Create();
  instance.static_mesh_manager.Set(static_mesh_handle, std::move(static_mesh));

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

  entity_id = world.CreateEntity();
  world.AddComponent<Transform>(entity_id, math::Vec3{0.0F, 0.0F, -5.0F},
                                math::Vec3{0.0F, 0.0F, 0.0F},
                                math::Vec3{1.0F, 1.0F, 1.0F});
  world.AddComponent<StaticMeshComponent>(entity_id, static_mesh_handle);

  SpawnMeshEntities(world, 100, static_mesh_handle,
                    math::Vec3{0.0F, 0.0F, 0.0F}, 50.0F);

  entity_id = world.CreateEntity();
  world.AddComponent<LightComponent>(
      entity_id, LightType::Point, math::Vec3{1.0F, 1.0F, 1.0F}, 2.0F, 200.0F);
  world.AddComponent<Transform>(entity_id, math::Vec3{0.0F, 0.0F, 0.0F},
                                math::Vec3{0.0F, 0.0F, 0.0F},
                                math::Vec3{1.0F, 1.0F, 1.0F});

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
        auto *mesh = static_mesh_manager.GetRef(static_mesh_handle);
        if (mesh == nullptr) {
          return;
        }
        if (mesh->render_active) {
          return;
        }
        mesh->render_active = true;
        auto *job = job_manager->AcquireJob();
        job->execute = [static_mesh_handle](void *data) -> void {
          auto *engine = static_cast<LuminaEngine *>(data);
          auto *m = engine->static_mesh_manager.GetRef(static_mesh_handle);
          ASSERT(m != nullptr, "Static mesh not found");
          auto render_mesh_handle = engine->GetRenderer().CreateRenderMesh(
              *m, engine->default_pipeline_handle);
          m->render_mesh_handle = render_mesh_handle;
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
               .material_instance =
                   engine->renderer->GetDefaultMaterialInstanceHandle(),
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
