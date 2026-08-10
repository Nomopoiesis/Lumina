#include "lumina_engine.hpp"

#include <array>
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

    auto entity_id = world.CreateEntity(Mobility::Static);
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

  input_action_map.BindAction(ActionID(std::string_view("ToggleDebugOverlay")),
                              KeyInputBinding(KeyCode::F1, KeyState::Pressed));

  input_action_map.BindAction(
      ActionID(std::string_view("PickEntity")),
      MouseButtonInputBinding(MouseButton::Left, KeyState::Pressed));
}

// Groups the visible proxies into one instanced batch per draw item, using a
// counting sort: tally per item, prefix sum to get each item's slice offset,
// then scatter the model matrices into their slices.
//
// A comparison sort is unnecessary because draw item ids are dense, and the
// grouping is all that matters — nothing downstream cares what order the items
// themselves come out in.
static auto BuildDrawList(const DrawableProxyManager &proxies,
                          const BatchedVisibilityIndex &visibility,
                          const renderer::DrawItemRegistry &draw_item_registry,
                          std::span<math::Mat4> instances,
                          DrawListScratch &scratch,
                          std::vector<renderer::DrawMeshBatch> &batches)
    -> void {
  LUMINA_PROFILE_SCOPE("BuildDrawList");

  const auto item_count = draw_item_registry.GetCount();

  scratch.counts.assign(item_count, 0);
  visibility.ForEachVisible([&](size_t proxy_index) -> void {
    ++scratch.counts[proxies.draw_item_indices[proxy_index]];
  });

  // Exclusive prefix sum: offsets[i] is where item i's instances begin.
  scratch.offsets.resize(item_count);
  u32 running = 0;
  for (size_t i = 0; i < item_count; ++i) {
    scratch.offsets[i] = running;
    running += scratch.counts[i];
  }
  LUMINA_CHECK(running <= instances.size(),
               "Instance buffer smaller than the visible set");

  // Emitted before the scatter, while offsets still hold each item's start
  // rather than its write cursor.
  batches.clear();
  for (u32 item_index = 0; item_index < item_count; ++item_index) {
    if (scratch.counts[item_index] == 0) {
      // Registered, but nothing visible uses it this frame. Emitting it would
      // be a zero-instance draw that still inflates the draw call count.
      continue;
    }
    batches.push_back(renderer::DrawMeshBatch{
        .draw_item_index = item_index,
        .first_instance = scratch.offsets[item_index],
        .instance_count = scratch.counts[item_index],
    });
  }

  visibility.ForEachVisible([&](size_t proxy_index) -> void {
    const u32 item_index = proxies.draw_item_indices[proxy_index];
    instances[scratch.offsets[item_index]++] = proxies.model[proxy_index];
  });
}

static auto
BuildDebugAABBDrawList(const DrawableProxyManager &proxies,
                       const BatchedVisibilityIndex &visibility,
                       renderer::RenderMeshHandle debug_mesh,
                       std::vector<renderer::DrawDebugAABBCommand> &debug_draws)
    -> void {
  debug_draws.clear();
  visibility.ForEachVisible([&](size_t index) -> void {
    const auto bounds = proxies.GetProxyAABB(index);
    debug_draws.emplace_back(renderer::DrawDebugAABBCommand{
        .render_mesh_handle = debug_mesh,
        .model = math::Dot(math::ScaleMatrix(bounds.extent * 2.0F),
                           math::TranslationMatrix(bounds.center))});
  });
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
  const auto scene_materials = CreateDebugMaterialInstances(*instance.renderer);

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

  entity_id = world.CreateEntity(Mobility::Static);
  world.AddComponent<Transform>(entity_id, math::Vec3{0.0F, 0.0F, -5.0F},
                                math::Vec3{0.0F, 0.0F, 0.0F},
                                math::Vec3{1.0F, 1.0F, 1.0F});
  world.AddComponent<StaticMeshComponent>(entity_id, static_mesh_handle,
                                          scene_materials.front());

  SpawnMeshEntities(world, DEBUG_SPAWN_MESH_COUNT, scene_meshes,
                    scene_materials, math::Vec3{0.0F, 0.0F, 0.0F},
                    DEBUG_SPAWN_RADIUS);

  entity_id = world.CreateEntity(Mobility::Static);
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
  {
    // Detached because it closes before the Update root opens, so it has to
    // stay out of the accounted/unaccounted arithmetic. This is not work: the
    // update thread blocks on the frame-context semaphore until the render
    // thread releases one, so the number is how much of the frame period the
    // update thread spends waiting for the renderer. A large value here with a
    // small Update means the frame is bound by the render side, not by us.
    LUMINA_PROFILE_SCOPE_DETACHED("WaitRender");
    renderer->AcquireFrameContextForUpdate();
  }
  ui_system->BeginLayout(window_dimensions.width, window_dimensions.height);
  auto delta_time = timer.Tick();
  frame_time_info.delta_time = delta_time;
  // Clamp the delta time to 0.1 seconds to prevent large jumps in time
  frame_time_info.simulation_delta_time = math::Clamp(delta_time, 0.0, 0.1);
  frame_time_info.total_time += delta_time;
  frame_stats.Update(delta_time);
  input_dispatcher.Dispatch(input_state);
}

auto LuminaEngine::ProcessDeferredOperations() -> void {
  static_mesh_manager.GetRegistry().ProcessDeferredOperations();
  texture_manager.ProcessDeferredOperations();
}

// Weight given to the newest frame when folding zone times into the moving
// average. At ~100 FPS this settles in roughly a tenth of a second, which is
// also the overlay's refresh interval — fast enough to react to a change,
// slow enough that the digits stay readable.
static constexpr f64 ZONE_EMA_ALPHA = 0.1;

auto LuminaEngine::EndFrame() -> void {
  // After ExecuteFrame has returned, so the frame root zone has closed and its
  // accumulator holds a complete span. Render-thread zones are whatever landed
  // in the table by now, which is why they are registered as Detached and kept
  // out of the accounted total.
  profiling::EndFrame(frame_profile);
  for (u32 i = 0; i < frame_profile.zone_count; ++i) {
    const auto &sample = frame_profile.samples[i];
    if (sample.name == nullptr) {
      continue;
    }
    zone_seconds_ema[i] +=
        (sample.seconds - zone_seconds_ema[i]) * ZONE_EMA_ALPHA;
  }

  ProcessDeferredOperations();
  current_world->ClearAdditions();
  renderer->ReleaseFrameContextForUpdate();
}

// Builds the pick projection for a requested screen pixel: a camera that sees
// only the PICK_REGION_SIZE-wide neighbourhood under the cursor, stretched to
// fill the whole target.
//
// The scale-and-translate is applied *after* the projection, in clip space,
// which is what makes it a plain scale on x and y. Clip x is divided by clip w
// to reach NDC, so scaling clip x scales the NDC directly, and adding a
// multiple of clip w adds a constant to the NDC — the w cancels against the
// divide, which is the only way to shift every object by the same amount
// regardless of its depth. w itself must be left alone or the divide changes
// for everything.
static auto CalculatePickProjection(const math::Mat4 &projection,
                                    const PickRequestPosition &pixel,
                                    const WindowDimensions &window)
    -> math::Mat4 {
  const auto width = static_cast<f32>(window.width);
  const auto height = static_cast<f32>(window.height);

  // Mouse coordinates are client pixels with a top-left origin, and the passes
  // render through a negative-height viewport, so NDC +y is the top of the
  // screen. Hence the y remap is inverted relative to x.
  const f32 ndc_x = (2.0F * static_cast<f32>(pixel.x) / width) - 1.0F;
  const f32 ndc_y = 1.0F - (2.0F * static_cast<f32>(pixel.y) / height);

  // One pixel spans 2/width in NDC, so the region spans 2*R/width. Stretching
  // that to the full 2 the target covers is a factor of width/R.
  const f32 zoom_x = width / static_cast<f32>(renderer::PICK_REGION_SIZE);
  const f32 zoom_y = height / static_cast<f32>(renderer::PICK_REGION_SIZE);

  // Row-vector convention: row 3 is the one multiplied by the incoming w, so
  // putting the translation there is how "multiply this term by w" is spelled.
  auto pick_transform = math::Mat4::Identity();
  pick_transform[0].x = zoom_x;
  pick_transform[1].y = zoom_y;
  pick_transform[3].x = -zoom_x * ndc_x;
  pick_transform[3].y = -zoom_y * ndc_y;

  // Applied last, so it acts on clip space rather than on the world.
  return math::Dot(projection, pick_transform);
}

// Runs on a worker fiber, gated behind pick_sync_gate so the proxy arrays it
// reads are settled. Everything it touches is either read-only for the rest of
// the frame (the proxy arrays, the draw item registry, the camera) or owned
// solely by the pick (pick_visibility_index, pick_draws, pick_candidates), so
// it needs no synchronisation of its own beyond that gate.
auto LuminaEngine::IssuePickRequest() -> void {
  if (!pick_request_position.has_value()) {
    return;
  }

  LUMINA_PROFILE_SCOPE("Pick");

  const auto request = *pick_request_position;
  pick_request_position.reset();

  auto camera_id = current_world->GetActiveCamera();
  auto transform = current_world->GetTransform(camera_id);
  auto camera = current_world->GetComponent<Camera>(camera_id);
  auto view = CalculateViewMatrix(transform);
  auto pick_projection = CalculatePickProjection(camera.ToProjectionMatrix(),
                                                 request, window_dimensions);
  auto pick_view_projection = math::Dot(view, pick_projection);

  const auto pick_frustum = ExtractFrustumFromMatrix(pick_view_projection);

  // Narrowed from the frame's visible set rather than culled from scratch. The
  // pick transform only scales and translates clip x and y — z and w are
  // untouched — so the pick frustum shares the main frustum's near and far
  // planes and has strictly tighter sides. It is therefore contained in it, and
  // anything the frame culled away could not have been clicked on.
  //
  // The one place containment leaks is a click near the window edge, where part
  // of the region falls outside the screen. Whatever lives out there was never
  // rendered, so dropping it is right anyway.
  //
  // Serial on purpose: a second FrustumCulling would contend with the main cull
  // for the same worker pool to re-test proxies that were just rejected, and
  // what is left here is a handful of plane tests on a click frame.
  //
  // Handles and matrices only — no registry lookups and no Vulkan calls. The
  // render thread resolves the draw item when it records the pass, because
  // resolving it here would race ProcessDeferredOperations.
  auto *frame_context = renderer->GetFrameContextForUpdate();
  auto &draws = frame_context->pick_draws;
  draws.clear();
  pick_candidates.clear();

  visibility_index.ForEachVisible([&](size_t proxy_index) -> void {
    if (pick_candidates.size() >= MAX_PICK_CANDIDATES) {
      return;
    }
    if (TestAABoundingBox(pick_frustum,
                          drawable_proxy_manager.GetProxyAABB(proxy_index)) ==
        FrustumTestResult::Outside) {
      return;
    }
    pick_candidates.push_back(drawable_proxy_manager.entity_ids[proxy_index]);
    draws.push_back(renderer::PickDraw{
        .mvp = math::Dot(drawable_proxy_manager.model[proxy_index],
                         pick_view_projection),
        .draw_item_index = drawable_proxy_manager.draw_item_indices[proxy_index],
        // 1-based: 0 is the target's clear value and means "nothing here".
        .slot = static_cast<u32>(pick_candidates.size()),
    });
  });

  // A click on empty sky. Resolved here rather than round-tripping an empty
  // pass through the GPU — the answer is already known, and clearing the flag
  // now means the next click is accepted immediately instead of a frame later.
  if (draws.empty()) {
    ResolvePick(0);
  }
}

auto LuminaEngine::ResolvePick(u32 slot) -> void {
  const auto previous = picked_entity_id;
  picked_entity_id = (slot == 0 || slot > pick_candidates.size())
                         ? INVALID_ENTITY_ID
                         : pick_candidates[slot - 1];
  pick_in_flight = false;

  if (picked_entity_id != previous) {
    LOG_INFO("Picked entity: {}", picked_entity_id);
  }
}

auto LuminaEngine::PollPickResult() -> void {
  if (auto slot_opt = renderer->TakePickResult(last_pick_sequence);
      slot_opt.has_value()) {
    ResolvePick(slot_opt.value());
  }
}

auto LuminaEngine::ExecuteFrame() -> void {
  // The frame root. Everything the update thread does is inside this span, so
  // it is the denominator the phase zones and the unaccounted row are read
  // against. Exactly one call site may use LUMINA_PROFILE_FRAME.
  LUMINA_PROFILE_FRAME("Update");

  // Consumed at the head of the frame rather than beside the request that
  // produced it: a pick answer takes one or two frames to come back, so this
  // collects whatever the render thread published since last frame. It is an
  // atomic load, not a wait.
  PollPickResult();

  // Dispatch upload jobs for any meshes not yet uploaded to the GPU.
  // Track dispatched handles locally to avoid multiple jobs for the same mesh
  // when several entities share a handle (render_active update is deferred).
  {
    LUMINA_PROFILE_SCOPE("DispatchMeshUploads");
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
  }

  {
    LUMINA_PROFILE_SCOPE("DispatchTextureUploads");
    std::unordered_set<ResourceHandleIndexType> dispatched_texture_uploads;
    texture_manager.ForEach([this, &dispatched_texture_uploads](
                                TextureHandle handle,
                                Texture &texture) -> void {
      if (dispatched_texture_uploads.contains(handle.index)) {
        return;
      }
      if (texture.render_active) {
        return;
      }
      dispatched_texture_uploads.insert(handle.index);
      texture_manager.Update(
          handle, [](Texture &t) -> void { t.render_active = true; });
      auto *job = job_system::GetJobManager().AcquireJob();
      job->execute = [handle](void *data) -> void {
        auto *engine = static_cast<LuminaEngine *>(data);
        auto t_opt = engine->texture_manager.Get(handle);
        LUMINA_CHECK(t_opt.has_value(), "Texture not found during GPU upload");
        auto rth = engine->renderer->CreateRenderTexture(*t_opt.value());
        engine->texture_manager.Update(handle, [rth](Texture &t) -> void {
          t.render_texture_handle = rth;
        });
      };
      job->data = this;
      job_system::GetJobManager().SubmitJob(job);
    });
  }

  // The pick job is conditional, so the counter it signals has to be sized to
  // match. A count that outruns the jobs actually submitted never reaches zero
  // and hangs the frame in WaitForCounter.
  const bool has_pick_request = pick_request_position.has_value();
  auto *frame_sync_counter =
      job_system::GetJobManager().AllocateCounter(has_pick_request ? 3 : 2);
  pick_sync_gate = has_pick_request
                       ? job_system::GetJobManager().AllocateCounter(1)
                       : nullptr;

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
    frame_context->draw_batches.clear();
    frame_context->debug_draws.clear();
    // Cleared unconditionally, like the lists above. IssuePickRequest only runs
    // on frames that requested a pick, so leaving it to that would let a
    // context still holding a consumed pick re-record it.
    frame_context->pick_draws.clear();
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
    FrustumCulling(proxy_manager, frustum, visibility);

    // Everything the pick needs is settled here: Sync is the only writer of the
    // proxy arrays and the draw item registry, and the cull is the only writer
    // of the visibility index. The rest of this job only reads them, so the
    // pick can run beside the draw-list build.
    if (engine->pick_sync_gate != nullptr) {
      job_system::GetJobManager().Signal(engine->pick_sync_gate);
    }

    engine->renderer->EnsureInstanceBufferCapacity(
        frame_context->GetInstanceBuffer(), visibility.GetVisibleCount());

    // The span is built here rather than inside BuildDrawList so the draw list
    // build stays a core function writing into plain memory, with no knowledge
    // of Vulkan buffers.
    auto &instance_buffer = frame_context->GetInstanceBuffer();
    std::span<math::Mat4> instances{
        static_cast<math::Mat4 *>(instance_buffer.mapped),
        instance_buffer.capacity};

    BuildDrawList(proxy_manager, visibility,
                  engine->renderer->GetDrawItemRegistry(), instances,
                  engine->draw_list_scratch, frame_context->draw_batches);

    if (engine->show_bounding_boxes) {
      auto aabb_mesh_opt =
          engine->static_mesh_manager.Get(engine->debug_aabb_mesh_handle);
      if (aabb_mesh_opt.has_value()) {
        BuildDebugAABBDrawList(proxy_manager, visibility,
                               aabb_mesh_opt.value()->render_mesh_handle,
                               frame_context->debug_draws);
      }
    }
  };
  job->data = this;
  job->signal_counter = frame_sync_counter;
  job_system::GetJobManager().SubmitJob(job);

  if (has_pick_request) {
    job = job_system::GetJobManager().AcquireJob();
    job->execute = [](void *data) -> void {
      auto *engine = static_cast<LuminaEngine *>(data);
      // Parks this fiber until Sync has finished rebuilding the proxy arrays.
      // The worker picks up other jobs meanwhile, so the wait costs a context
      // switch rather than a thread.
      job_system::GetJobManager().WaitForCounter(engine->pick_sync_gate);
      job_system::GetJobManager().ReleaseCounter(engine->pick_sync_gate);
      engine->IssuePickRequest();
    };
    job->data = this;
    job->signal_counter = frame_sync_counter;
    job_system::GetJobManager().SubmitJob(job);
  }

  job_system::GetJobManager().WaitForCounter(frame_sync_counter);
  job_system::GetJobManager().ReleaseCounter(frame_sync_counter);

  // The pick job released the gate itself; clearing the pointer is safe only
  // here, once that job is known to have finished.
  pick_sync_gate = nullptr;

  // The profile is last frame's — EndFrame snapshots after ExecuteFrame has
  // returned — so the zone rows trail by one frame, the same as the draw call
  // count above them.
  debug_overlay.Draw({
      .average_frame_delta_time = frame_stats.GetAverageFrameDeltaTime(),
      .total_time = frame_time_info.total_time,
      .draw_calls = renderer->GetRecordedDrawCallCount(),
      .zone_samples =
          std::span{frame_profile.samples.data(), frame_profile.zone_count},
      .zone_seconds_ema =
          std::span{zone_seconds_ema.data(), frame_profile.zone_count},
      .picked_entity = picked_entity_id,
  });

  auto clay_commands = ui_system->EndLayout();
  auto *frame_context = renderer->GetFrameContextForUpdate();
  BuildUIBatch(clay_commands, *ui_system, frame_context->ui_batch);
}

} // namespace lumina::core
