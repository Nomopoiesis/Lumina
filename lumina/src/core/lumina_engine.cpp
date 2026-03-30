#include "lumina_engine.hpp"

#include <unordered_set>

#include "common/data_structures/data_buffer.hpp"
#include "common/fast_random.hpp"
#include "common/logger/logger.hpp"
#include "common/path_registry.hpp"
#include "data_structures/data_buffer.hpp"
#include "platform/platform_common/file_handle.hpp"

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

using lumina::common::data_structures::DataBuffer;
using lumina::platform::common::InvalidFileHandle;

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

// Unit cube wireframe (line list): 8 vertices, 24 line-list indices (12 edges ×
// 2).
static auto WireframeBox() -> StaticMesh {
  StaticMesh mesh;

  std::vector<math::Vec3> positions = {
      {-0.5F, -0.5F, -0.5F}, // v0
      {+0.5F, -0.5F, -0.5F}, // v1
      {+0.5F, +0.5F, -0.5F}, // v2
      {-0.5F, +0.5F, -0.5F}, // v3
      {-0.5F, -0.5F, +0.5F}, // v4
      {+0.5F, -0.5F, +0.5F}, // v5
      {+0.5F, +0.5F, +0.5F}, // v6
      {-0.5F, +0.5F, +0.5F}, // v7
  };

  std::vector<u16> indices = {
      0, 1, 1, 2, 2, 3, 3, 0, // back face
      4, 5, 5, 6, 6, 7, 7, 4, // front face
      0, 4, 1, 5, 2, 6, 3, 7, // connecting edges
  };

  mesh.vertex_count = positions.size();
  mesh.vertex_attributes.emplace_back(
      VertexAttribute{.type = VertexAttributeType::Position,
                      .element_type = ElementType::Vec3},
      DataBuffer(reinterpret_cast<u8 *>(positions.data()),
                 positions.size() * sizeof(math::Vec3)));
  mesh.indices = std::move(indices);
  return mesh;
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

  input_action_map.BindAction(
      ActionID(std::string_view("ToggleBoundingBoxView")),
      KeyInputBinding(KeyCode::F3, KeyState::Pressed));
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

  {
    auto wireframe_box = WireframeBox();
    auto dbg_mesh_handle = instance.renderer->CreateRenderMesh(
        wireframe_box, instance.renderer->GetDebugAABBPipelineHandle());
    instance.renderer->SetDebugAABBRenderMesh(dbg_mesh_handle);
  }

  instance.camera_movement_controller =
      std::make_unique<CameraMovementController>(INVALID_ENTITY_ID);
  instance.debug_overlay_controller =
      std::make_unique<DebugOverlayController>();

  InitializeInputActionMap(instance.input_dispatcher.GetInputActionMap());
  instance.input_dispatcher.RegisterHandler(
      instance.camera_movement_controller.get(), 10);
  instance.input_dispatcher.RegisterHandler(
      instance.debug_overlay_controller.get(), 5);

  constexpr std::string_view ModelCacheKey = "suzanne";
  const auto &model_cache =
      lumina::common::PathRegistry::Instance().model_cache;

  StaticMesh static_mesh;
  if (HasCachedMesh(ModelCacheKey, model_cache)) {
    LOG_INFO("Loading mesh from cache: suzanne");
    auto result = DeserializeStaticMesh(ModelCacheKey, model_cache);
    if (!result.has_value()) {
      LOG_CRITICAL("Failed to deserialize cached mesh: %s",
                   result.error().message);
      LUMINA_TERMINATE();
    }
    static_mesh = std::move(*result);
  } else {
    LOG_INFO("Parsing OBJ: suzanne");
    auto file_handle =
        platform::common::PlatformServices::Instance().LuminaOpenFile(
            lumina::common::PathRegistry::Instance()
                .models.Resolve("suzanne.obj")
                .string()
                .c_str());
    ASSERT(file_handle != InvalidFileHandle, "Failed to open model file");
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
        DataBuffer(reinterpret_cast<u8 *>(obj_data.positions.data()),
                   obj_data.positions.size() * sizeof(math::Vec3)));
    static_mesh.vertex_attributes.emplace_back(
        VertexAttribute{.type = VertexAttributeType::Normal,
                        .element_type = ElementType::Vec3},
        DataBuffer(reinterpret_cast<u8 *>(obj_data.normals.data()),
                   obj_data.normals.size() * sizeof(math::Vec3)));
    static_mesh.vertex_attributes.emplace_back(
        VertexAttribute{.type = VertexAttributeType::TexCoord,
                        .element_type = ElementType::Vec2},
        DataBuffer(reinterpret_cast<u8 *>(obj_data.tex_coords.data()),
                   obj_data.tex_coords.size() * sizeof(math::Vec2)));
    static_mesh.indices = obj_data.indices;

    static_mesh.bounding_box = ComputeAABoundingBox(obj_data.positions.data(),
                                                    obj_data.positions.size());

    auto cache_result =
        SerializeStaticMesh(static_mesh, ModelCacheKey, model_cache);
    if (!cache_result.has_value()) {
      LOG_WARNING("Failed to write mesh cache: %s",
                  cache_result.error().message);
    }
  }

  auto static_mesh_handle =
      instance.static_mesh_manager.Create(std::move(static_mesh));

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

  instance.ProcessDeferredOperations();
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

auto LuminaEngine::ProcessDeferredOperations() -> void {
  static_mesh_manager.ProcessDeferredOperations();
}

auto LuminaEngine::EndFrame() -> void {
  ProcessDeferredOperations();
  renderer->ReleaseFrameContextForUpdate();
}

auto LuminaEngine::ExecuteFrame() -> void {
  // Dispatch upload jobs for any meshes not yet uploaded to the GPU.
  // Track dispatched handles locally to avoid multiple jobs for the same mesh
  // when several entities share a handle (render_active update is deferred).
  std::unordered_set<ResourceHandleIndexType> dispatched_mesh_uploads;
  current_world->ForEachComponent<StaticMeshComponent>(
      [this, &dispatched_mesh_uploads](EntityID /*id*/,
                                       const StaticMeshComponent &component) {
        auto static_mesh_handle = component.GetStaticMeshHandle();
        if (dispatched_mesh_uploads.contains(static_mesh_handle.index)) {
          return;
        }
        auto mesh_opt = static_mesh_manager.Get(static_mesh_handle);
        if (!mesh_opt.has_value()) {
          return;
        }
        auto &mesh = mesh_opt.value();
        if (mesh->render_active) {
          return;
        }
        dispatched_mesh_uploads.insert(static_mesh_handle.index);
        static_mesh_manager.Update(
            static_mesh_handle,
            [](StaticMesh &mesh) -> void { mesh.render_active = true; });
        auto *job = job_manager->AcquireJob();
        job->execute = [static_mesh_handle](void *data) -> void {
          auto *engine = static_cast<LuminaEngine *>(data);
          auto m_opt = engine->static_mesh_manager.Get(static_mesh_handle);
          ASSERT(m_opt.has_value(), "Static mesh not found");
          auto &m = m_opt.value();
          auto render_mesh_handle = engine->GetRenderer().CreateRenderMesh(
              *m, engine->GetRenderer().GetDefaultGraphicsPipelineHandle());
          engine->static_mesh_manager.Update(
              static_mesh_handle,
              [render_mesh_handle](StaticMesh &mesh) -> void {
                mesh.render_mesh_handle = render_mesh_handle;
              });
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
    frame_context->draw_list.clear();
    engine->current_world->ForEachComponent<StaticMeshComponent>(
        [engine, frame_context](EntityID id,
                                const StaticMeshComponent &component) -> void {
          auto mesh_opt =
              engine->static_mesh_manager.Get(component.GetStaticMeshHandle());
          auto transform = engine->current_world->GetComponent<Transform>(id);
          if (!mesh_opt.has_value()) {
            return;
          }
          auto &mesh = mesh_opt.value();
          if (mesh->render_mesh_handle.index == INVALID_RESOURCE_HANDLE_INDEX) {
            return;
          }
          frame_context->draw_list.emplace_back(renderer::DrawMeshCommand{
              .render_mesh_handle = mesh->render_mesh_handle,
              .material_instance =
                  engine->renderer->GetDefaultMaterialInstanceHandle(),
              .model = transform.GetModelMatrix()});
          if (engine->show_bounding_boxes) {
            auto world_aabb = TransformAABoundingBox(
                mesh->bounding_box, transform.GetModelMatrix());
            math::Vec3 center{
                (world_aabb.min.x + world_aabb.max.x) * 0.5F,
                (world_aabb.min.y + world_aabb.max.y) * 0.5F,
                (world_aabb.min.z + world_aabb.max.z) * 0.5F,
            };
            math::Vec3 size{
                world_aabb.max.x - world_aabb.min.x,
                world_aabb.max.y - world_aabb.min.y,
                world_aabb.max.z - world_aabb.min.z,
            };
            frame_context->draw_list.emplace_back(
                renderer::DrawDebugAABBCommand{
                    .model = math::Dot(math::ScaleMatrix(size),
                                       math::TranslationMatrix(center))});
          }
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
