#include "lumina_engine.hpp"

#include <array>
#include <format>
#include <span>
#include <unordered_set>
#include <vector>

#include "basic_geometry.hpp"
#include "common/data_structures/data_buffer.hpp"
#include "common/fast_random.hpp"
#include "common/logger/logger.hpp"
#include "common/lumina_check.hpp"
#include "components/camera.hpp"
#include "components/light_component.hpp"
#include "components/static_mesh_component.hpp"
#include "components/transform.hpp"
#include "culling.hpp"
#include "data_structures/data_buffer.hpp"
#include "font.hpp"
#include "frustum.hpp"
#include "input/input_action.hpp"
#include "job_system/job_manager.hpp"
#include "math/basic.hpp"
#include "math/trigonometry.hpp"
#include "platform/platform_common/file_handle.hpp"
#include "renderer/shaders/shader_gen/static_shader_api.hpp"
#include "ui/ui_system.hpp"
#include "uniform_interface/uniform_interface.hpp"

using lumina::common::data_structures::DataBuffer;
using lumina::platform::common::InvalidFileHandle;

using namespace lumina::core::components;

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

// Number of test entities populated into the default world. Raise this to
// stress the render path - entity/transform storage grows on demand, so the
// only practical limit is frame time (there is no culling yet).
static constexpr u32 DEBUG_SPAWN_MESH_COUNT = 50000;
static constexpr f32 DEBUG_SPAWN_RADIUS = 200.0F;

// Diffuse colours for the debug scene's material instances. Distinct enough to
// tell apart on screen, so a mis-bound descriptor set shows up as an entity in
// the wrong colour rather than something subtle.
static constexpr std::array<math::Vec3, 6> DEBUG_MATERIAL_COLORS = {
    math::Vec3{0.85F, 0.25F, 0.25F}, // red
    math::Vec3{0.25F, 0.75F, 0.35F}, // green
    math::Vec3{0.25F, 0.45F, 0.90F}, // blue
    math::Vec3{0.90F, 0.75F, 0.20F}, // amber
    math::Vec3{0.70F, 0.30F, 0.85F}, // violet
    math::Vec3{0.20F, 0.80F, 0.85F}, // cyan
};

// One instance per palette entry, all sharing the lit template. They live for
// the process lifetime: the renderer owns them and nothing frees a uniform slot
// yet, so creating them per-frame or per-entity would exhaust the budget.
static auto CreateDebugMaterialInstances(renderer::LuminaRenderer &renderer)
    -> std::vector<renderer::MaterialInstanceHandle> {
  std::vector<renderer::MaterialInstanceHandle> instances;
  instances.reserve(DEBUG_MATERIAL_COLORS.size());
  for (const auto &color : DEBUG_MATERIAL_COLORS) {
    instances.push_back(renderer::CreateLitMaterialInstance(&renderer, color));
  }
  return instances;
}

// Each entity gets a mesh and a material instance picked at random, so the
// scene ends up with all available meshes and materials mixed together — which
// is what makes draw sorting and batching observable rather than trivially
// correct.
static auto SpawnMeshEntities(
    World &world, u32 count, std::span<const StaticMeshHandle> mesh_handles,
    std::span<const renderer::MaterialInstanceHandle> material_handles,
    const math::Vec3 &origin, f32 radius) -> void {
  using lumina::common::random::FastRandom;
  constexpr f32 InvU32Max = 1.0F / 4294967295.0F;

  LUMINA_CHECK(!mesh_handles.empty(),
               "SpawnMeshEntities requires at least one mesh");
  LUMINA_CHECK(!material_handles.empty(),
               "SpawnMeshEntities requires at least one material instance");

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

    const auto mesh_handle = mesh_handles[FastRandom() % mesh_handles.size()];
    const auto material_handle =
        material_handles[FastRandom() % material_handles.size()];

    auto entity_id = world.CreateEntity();
    world.AddComponent<Transform>(entity_id, position, rotation,
                                  math::Vec3{1.0F, 1.0F, 1.0F});
    world.AddComponent<StaticMeshComponent>(entity_id, mesh_handle,
                                            material_handle);
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

  mesh.topology = renderer::PrimitiveTopology::LineList;
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

  auto font = CreateFont("NaturalMono-Regular.ttf", std::span<const i32>({24}));
  if (!font.has_value()) {
    LOG_CRITICAL("Failed to create font: NaturalMono-Regular");
    LUMINA_TERMINATE();
  }
  instance.fonts["NaturalMono-Regular"] = std::move(font.value());

  instance.window_dimensions = init_info.window_dimensions;
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

  {
    auto wireframe_box = WireframeBox();
    auto render_mesh_handle = instance.renderer->CreateRenderMesh(
        wireframe_box,
        instance.renderer->GetDebugWireframeMaterialTemplateHandle());
    wireframe_box.render_mesh_handle = render_mesh_handle;
    auto create_mesh_result = instance.static_mesh_manager.CreateMesh(
        "builtin:WireframeBox", std::move(wireframe_box));
    ASSERT(create_mesh_result.has_value(),
           create_mesh_result.error().Message());
    instance.debug_aabb_mesh_handle = create_mesh_result.value();
    instance.static_mesh_manager.GetRegistry().ProcessDeferredOperations();
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

  // Meshes the debug scene draws from. Source paths come from the models
  // resolver rather than being spelled out: the resolved path doubles as the
  // mesh cache key, so a hardcoded one would break on any other checkout.
  const auto &models = lumina::common::PathRegistry::Instance().models;

  auto load_obj_mesh = [&models,
                        &instance](const char *asset_name) -> StaticMeshHandle {
    const auto source_path = models.Resolve(asset_name).string();
    auto result = instance.static_mesh_manager.LoadMesh(source_path);
    if (!result.has_value()) {
      LOG_CRITICAL("Failed to load model {}: {}", asset_name,
                   result.error().Message());
      LUMINA_TERMINATE();
    }
    return result.value();
  };

  std::vector<StaticMeshHandle> scene_meshes;
  scene_meshes.push_back(load_obj_mesh("suzanne.obj"));
  scene_meshes.push_back(load_obj_mesh("sphere.obj"));

  {
    // Procedural rather than loaded, but it goes through the same registry so
    // it is referable by name and picks up the deferred GPU upload like any
    // other mesh.
    auto cube_result = instance.static_mesh_manager.CreateMesh(
        "builtin:Cube", BasicGeometry::Cube());
    if (!cube_result.has_value()) {
      LOG_CRITICAL("Failed to create builtin cube mesh: {}",
                   cube_result.error().Message());
      LUMINA_TERMINATE();
    }
    scene_meshes.push_back(cube_result.value());
  }

  const auto static_mesh_handle = scene_meshes.front();

  // After renderer initialization: instance creation writes descriptor sets and
  // flushes deferred resource operations, which is only safe before frames
  // start.
  const auto scene_materials =
      CreateDebugMaterialInstances(*instance.renderer);

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
      .far_plane = 500.0F,
  };
  world.AddComponent<Camera>(entity_id, camera_settings);

  world.SetActiveCamera(entity_id);
  instance.camera_movement_controller->SetControlledEntity(entity_id);

  entity_id = world.CreateEntity();
  world.AddComponent<Transform>(entity_id, math::Vec3{0.0F, 0.0F, -5.0F},
                                math::Vec3{0.0F, 0.0F, 0.0F},
                                math::Vec3{1.0F, 1.0F, 1.0F});
  world.AddComponent<StaticMeshComponent>(entity_id, static_mesh_handle,
                                          scene_materials.front());

  SpawnMeshEntities(world, DEBUG_SPAWN_MESH_COUNT, scene_meshes,
                    scene_materials, math::Vec3{0.0F, 0.0F, 0.0F},
                    DEBUG_SPAWN_RADIUS);

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
  static_mesh_manager.GetRegistry().ProcessDeferredOperations();
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
        static_mesh_manager.GetRegistry().Update(
            static_mesh_handle,
            [](StaticMesh &mesh) -> void { mesh.render_active = true; });
        auto *job = job_system::GetJobManager().AcquireJob();
        job->execute = [static_mesh_handle](void *data) -> void {
          auto *engine = static_cast<LuminaEngine *>(data);
          auto m_opt = engine->static_mesh_manager.Get(static_mesh_handle);
          LUMINA_CHECK(m_opt.has_value(), "Static mesh not found");
          auto &m = m_opt.value();
          auto render_mesh_handle = engine->GetRenderer().CreateRenderMesh(
              *m, engine->GetRenderer().GetDefaultMaterialTemplateHandle());
          engine->static_mesh_manager.GetRegistry().Update(
              static_mesh_handle,
              [render_mesh_handle](StaticMesh &mesh) -> void {
                mesh.render_mesh_handle = render_mesh_handle;
              });
        };
        job->data = this;
        job_system::GetJobManager().SubmitJob(job);
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
    auto *job = job_system::GetJobManager().AcquireJob();
    job->execute = [handle](void *data) -> void {
      auto *engine = static_cast<LuminaEngine *>(data);
      auto t_opt = engine->texture_manager.Get(handle);
      LUMINA_CHECK(t_opt.has_value(), "Texture not found during GPU upload");
      auto rth = engine->renderer->CreateRenderTexture(*t_opt.value());
      engine->texture_manager.Update(
          handle, [rth](Texture &t) -> void { t.render_texture_handle = rth; });
    };
    job->data = this;
    job_system::GetJobManager().SubmitJob(job);
  });

  auto *frame_sync_counter = job_system::GetJobManager().AllocateCounter(2);
  auto *job = job_system::GetJobManager().AcquireJob();
  job->execute = [](void *data) {
    renderer::UpdateFrameUniforms(*static_cast<LuminaEngine *>(data));
  };
  job->data = this;
  job->signal_counter = frame_sync_counter;
  job_system::GetJobManager().SubmitJob(job);

  job = job_system::GetJobManager().AcquireJob();
  job->execute = [](void *data) {
    // Build draw list from all StaticMeshComponents with uploaded meshes
    auto *engine = static_cast<LuminaEngine *>(data);
    auto *frame_context = engine->renderer->GetFrameContextForUpdate();
    frame_context->draw_list.clear();
    engine->drawable_proxy_manager.Sync(
        *engine->current_world, engine->static_mesh_manager.GetRegistry(),
        *engine->renderer);
    auto camera_id = engine->current_world->GetActiveCamera();
    auto transform = engine->current_world->GetTransform(camera_id);
    auto camera = engine->current_world->GetComponent<Camera>(camera_id);
    auto view = CalculateViewMatrix(transform);
    auto proj = camera.ToProjectionMatrix();
    auto frustum = ExtractFrustumFromMatrix(math::Dot(view, proj));

    auto &proxy_manager = engine->drawable_proxy_manager;
    auto &visibility = engine->visibility_index;
    CullProxies(proxy_manager, frustum, visibility);

    AppendDrawCommands(proxy_manager, visibility, frame_context->draw_list);

    if (engine->show_bounding_boxes) {
      auto aabb_mesh_opt =
          engine->static_mesh_manager.Get(engine->debug_aabb_mesh_handle);
      if (aabb_mesh_opt.has_value()) {
        AppendDebugAABBDrawCommands(proxy_manager, visibility,
                                    aabb_mesh_opt.value()->render_mesh_handle,
                                    frame_context->draw_list);
      }
    }
  };
  job->data = this;
  job->signal_counter = frame_sync_counter;
  job_system::GetJobManager().SubmitJob(job);

  input_dispatcher.Dispatch(input_state);
  job_system::GetJobManager().WaitForCounter(frame_sync_counter);
  job_system::GetJobManager().ReleaseCounter(frame_sync_counter);

  // Build FPS text string - only integer fps and ms up to 2 decimal places
  auto fps_text = std::format("FPS: {} ({:.2f}ms)",
                              static_cast<int>(1.0F / GetFrameDeltaTime()),
                              GetFrameDeltaTime() * 1000.0F);
  auto fps_string = Clay_String{
      .isStaticallyAllocated = false,
      .length = static_cast<int32_t>(fps_text.length()),
      .chars = fps_text.c_str(),
  };
  CLAY({.id = CLAY_ID("HelloWorld"),
        .floating = {
            .offset = {10.0F, 10.0F},
            .attachTo = CLAY_ATTACH_TO_ROOT,
        }}) {
    CLAY_TEXT(fps_string, CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255},
                                            .fontId = 0,
                                            .fontSize = 24}));
  }

  auto clay_commands = ui_system->EndLayout();
  auto *frame_context = renderer->GetFrameContextForUpdate();
  BuildUIBatch(clay_commands, *ui_system, frame_context->ui_batch);
}

} // namespace lumina::core
