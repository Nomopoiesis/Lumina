#include "uniform_interface.hpp"

#include <cstring>

#include "components/camera.hpp"
#include "components/light_component.hpp"
#include "components/transform.hpp"
#include "headers/interface.global.hpp"
#include "lumina_engine.hpp"
#include "renderer/frame_context.hpp"

namespace lumina::renderer {

namespace g = lumina::shaders::interface::global;

static constexpr u32 max_point_lights =
    sizeof(g::FrameGlobals::point_lights) / sizeof(g::PointLight);

static auto WriteFrameGlobals(FrameContext &ctx, core::World &world) -> void {
  using namespace core::components;

  auto camera_id = world.GetActiveCamera();
  auto transform = world.GetComponent<Transform>(camera_id);
  auto camera = world.GetComponent<Camera>(camera_id);

  g::FrameGlobals globals = {};
  globals.view = CalculateViewMatrix(transform);
  globals.proj = camera.ToProjectionMatrix();
  globals.camera_position = transform.position;
  globals.point_light_count = 0;

  world.ForEachComponent<LightComponent>(
      [&globals, &world](core::EntityID id, const LightComponent &lc) -> void {
        if (static_cast<u32>(globals.point_light_count) >= max_point_lights) {
          return;
        }
        globals.point_lights[globals.point_light_count++] = {
            .position = world.GetComponent<Transform>(id).position,
            .intensity = lc.intensity,
            .color = lc.color,
            .attenuation_radius = lc.attenation_radius,
        };
      });

  memcpy(ctx.GetUniformBuffer().mapped, &globals, sizeof(g::FrameGlobals));
}

auto UpdateFrameUniforms(core::LuminaEngine &engine) -> void {
  auto &ctx = *engine.renderer->GetFrameContextForUpdate();
  auto &world = *engine.current_world;

  WriteFrameGlobals(ctx, world);
}

} // namespace lumina::renderer
