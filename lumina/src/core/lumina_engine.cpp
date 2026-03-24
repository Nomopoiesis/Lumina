#include "lumina_engine.hpp"

#include <unordered_set>

#include "common/fast_random.hpp"
#include "common/logger/logger.hpp"
#include "common/path_registry.hpp"

#include "basic_geometry.hpp"
#include "components/camera.hpp"
#include "components/light_component.hpp"
#include "components/static_mesh_component.hpp"
#include "components/transform.hpp"
#include "data_parsers/obj_parser.hpp"
#include "font.hpp"
#include "input/input_action.hpp"
#include "math/basic.hpp"
#include "math/trigonometry.hpp"
#include "mesh_cache.hpp"
#include "ui/ui_system.hpp"

namespace lumina::core {

LuminaEngine::LuminaEngine() noexcept {}
LuminaEngine::~LuminaEngine() noexcept = default;

auto LuminaEngine::GetStaticInstance() -> LuminaEngine & {
  static auto *instance = new LuminaEngine(); // NOLINT
  return *instance;
}

using namespace lumina::core::components;

static auto BuildUIBatch(Clay_RenderCommandArray commands, UISystem &ui_system,
                         renderer::UIRenderBatch &batch) -> void {
  batch.Reset();
  bool has_active_scissor = false;
  VkRect2D current_scissor{};
  u32 draw_call_index_start = 0;

  auto flush_draw_call = [&]() {
    const u32 count =
        static_cast<u32>(batch.indices.size()) - draw_call_index_start;
    if (count == 0) {
      return;
    }
    batch.draw_calls.push_back(renderer::UIDrawCall{
        .index_offset = draw_call_index_start,
        .index_count = count,
        .has_scissor = has_active_scissor,
        .scissor = current_scissor,
    });
    draw_call_index_start = static_cast<u32>(batch.indices.size());
  };

  for (i32 i = 0; i < commands.length; ++i) {
    const Clay_RenderCommand &cmd = commands.internalArray[i];
    const auto &bb = cmd.boundingBox;

    switch (cmd.commandType) {
      case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
        const auto &rect = cmd.renderData.rectangle;
        const math::Vec4 color{
            rect.backgroundColor.r / 255.0F,
            rect.backgroundColor.g / 255.0F,
            rect.backgroundColor.b / 255.0F,
            rect.backgroundColor.a / 255.0F,
        };
        const auto base = static_cast<u32>(batch.vertices.size());
        batch.vertices.push_back({.position = {bb.x, bb.y},
                                  .uv = {0.0F, 0.0F},
                                  .color = color,
                                  .mode = 0u});
        batch.vertices.push_back({.position = {bb.x + bb.width, bb.y},
                                  .uv = {1.0F, 0.0F},
                                  .color = color,
                                  .mode = 0u});
        batch.vertices.push_back(
            {.position = {bb.x + bb.width, bb.y + bb.height},
             .uv = {1.0F, 1.0F},
             .color = color,
             .mode = 0u});
        batch.vertices.push_back({.position = {bb.x, bb.y + bb.height},
                                  .uv = {0.0F, 1.0F},
                                  .color = color,
                                  .mode = 0u});
        batch.indices.insert(batch.indices.end(), {base, base + 1, base + 2,
                                                   base, base + 2, base + 3});
        break;
      }

      case CLAY_RENDER_COMMAND_TYPE_TEXT: {
        const auto &text = cmd.renderData.text;
        const math::Vec4 color{
            text.textColor.r / 255.0F,
            text.textColor.g / 255.0F,
            text.textColor.b / 255.0F,
            text.textColor.a / 255.0F,
        };
        auto *font = ui_system.GetFont(text.fontId);
        if (font == nullptr) {
          break;
        }
        auto *atlas = font->GetAtlas(static_cast<i32>(text.fontSize));
        if (atlas == nullptr) {
          break;
        }
        float cursor_x = bb.x;
        const float cursor_y = bb.y + atlas->ascent;
        for (i32 ci = 0; ci < text.stringContents.length; ++ci) {
          const auto cp = static_cast<i32>(
              static_cast<unsigned char>(text.stringContents.chars[ci]));
          auto it = atlas->glyphs.find(cp);
          if (it == atlas->glyphs.end()) {
            cursor_x += static_cast<float>(text.letterSpacing);
            continue;
          }
          const GlyphInfo &g = it->second;

          const float glyph_left = cursor_x + g.cursor_top_left_offset.x;
          const float glyph_top = cursor_y + g.cursor_top_left_offset.y;
          const float glyph_right = cursor_x + g.cursor_bottom_right_offset.x;
          const float glyph_bottom = cursor_y + g.cursor_bottom_right_offset.y;

          const auto base = static_cast<u32>(batch.vertices.size());

          batch.vertices.push_back({.position = {glyph_left, glyph_top},
                                    .uv = g.uv_top_left,
                                    .color = color,
                                    .mode = 1U});
          batch.vertices.push_back(
              {.position = {glyph_right, glyph_top},
               .uv = {g.uv_bottom_right.x, g.uv_top_left.y},
               .color = color,
               .mode = 1U});
          batch.vertices.push_back({.position = {glyph_right, glyph_bottom},
                                    .uv = g.uv_bottom_right,
                                    .color = color,
                                    .mode = 1U});
          batch.vertices.push_back(
              {.position = {glyph_left, glyph_bottom},
               .uv = {g.uv_top_left.x, g.uv_bottom_right.y},
               .color = color,
               .mode = 1U});

          batch.indices.insert(batch.indices.end(), {base, base + 1, base + 2,
                                                     base, base + 2, base + 3});
          cursor_x += g.advance_x;
          if (ci < text.stringContents.length - 1) {
            cursor_x += static_cast<float>(text.letterSpacing);
          }
        }
        break;
      }

      case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
        flush_draw_call();
        has_active_scissor = true;
        current_scissor = VkRect2D{
            .offset = {static_cast<i32>(bb.x), static_cast<i32>(bb.y)},
            .extent = {static_cast<u32>(bb.width), static_cast<u32>(bb.height)},
        };
        break;
      }

      case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
        flush_draw_call();
        has_active_scissor = false;
        break;
      }

      default:
        break;
    }
  }

  flush_draw_call();
}

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

  auto font = CreateFont("NaturalMono-Regular.ttf", std::span<const i32>({24}));
  if (!font.has_value()) {
    LOG_CRITICAL("Failed to create font: NaturalMono-Regular");
    LUMINA_TERMINATE();
  }
  instance.fonts["NaturalMono-Regular"] = std::move(font.value());

  instance.window_dimensions = init_info.window_dimensions;
  instance.job_manager = std::make_unique<job_system::JobManager>();
  instance.job_manager->Initialize({.num_workers = 0, .fiber_pool_size = 1024});
  instance.renderer =
      std::make_unique<renderer::LuminaRenderer>(init_info.vulkan_init_result);
  instance.renderer->Initialize();
  instance.ui_system = std::make_unique<UISystem>();
  instance.ui_system->Initialize(init_info.window_dimensions.width,
                                 init_info.window_dimensions.height);

  // Create CPU-side Texture entries for all font atlases; GPU upload is
  // deferred to the first ExecuteFrame via the texture upload loop.
  u16 font_id = 0;
  for (auto &[font_name, font] : instance.fonts) {
    for (i32 size : {24}) {
      if (auto *atlas = font.GetAtlas(size)) {
        auto texture_handle = instance.texture_manager.Create(Texture{
            .pixels = atlas->pixels,
            .width = static_cast<u32>(atlas->width),
            .height = static_cast<u32>(atlas->height),
            .format = TextureFormat::R8_Unorm,
        });
        atlas->texture_handle = texture_handle;
      }
    }
    instance.ui_system->RegisterFont(font_id++, &font);
  }

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
  instance.ui_system->Shutdown();
  instance.renderer->Shutdown();
  instance.job_manager.reset();
  instance.renderer.reset();
  instance.current_world.reset();
  instance.is_initialized = false;
}

auto LuminaEngine::BeginFrame(Timer &timer) -> void {
  renderer->AcquireFrameContextForUpdate();
  ui_system->BeginLayout(window_dimensions.width, window_dimensions.height);
  auto delta_time = timer.Tick();
  // Clamp the delta time to 0.1 seconds to prevent large jumps in time
  delta_time = math::Clamp(delta_time, 0.0, 0.1);
  frame_time_info.delta_time = delta_time;
  frame_time_info.total_time += delta_time;
}

auto LuminaEngine::ProcessDeferredOperations() -> void {
  static_mesh_manager.ProcessDeferredOperations();
  texture_manager.ProcessDeferredOperations();
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

  std::unordered_set<ResourceHandleIndexType> dispatched_texture_uploads;
  texture_manager.ForEach([this, &dispatched_texture_uploads](
                              TextureHandle handle, Texture &texture) -> void {
    if (dispatched_texture_uploads.contains(handle.index)) {
      return;
    }
    if (texture.render_active) {
      return;
    }
    dispatched_texture_uploads.insert(handle.index);
    texture_manager.Update(handle,
                           [](Texture &t) -> void { t.render_active = true; });
    auto *job = job_manager->AcquireJob();
    job->execute = [handle](void *data) -> void {
      auto *engine = static_cast<LuminaEngine *>(data);
      auto t_opt = engine->texture_manager.Get(handle);
      ASSERT(t_opt.has_value(), "Texture not found during GPU upload");
      auto rth = engine->renderer->CreateRenderTexture(*t_opt.value());
      engine->texture_manager.Update(
          handle, [rth](Texture &t) -> void { t.render_texture_handle = rth; });
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

  CLAY({.id = CLAY_ID("HelloWorld"),
        .floating = {
            .offset = {10.0F, 10.0F},
            .attachTo = CLAY_ATTACH_TO_ROOT,
        }}) {
    CLAY_TEXT(
        CLAY_STRING("Hello World!"),
        CLAY_TEXT_CONFIG(
            {.textColor = {255, 255, 255, 255}, .fontId = 0, .fontSize = 24}));
  }

  auto clay_commands = ui_system->EndLayout();
  auto *frame_context = renderer->GetFrameContextForUpdate();
  BuildUIBatch(clay_commands, *ui_system, frame_context->ui_batch);
}

} // namespace lumina::core
