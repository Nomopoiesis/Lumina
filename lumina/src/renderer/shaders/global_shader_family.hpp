#pragma once

#include "lumina_types.hpp"

namespace lumina::renderer {

// One set 0 descriptor contract: its layout, the resources bound to it, and how
// often they are rewritten. Each family costs a descriptor set per frame, a
// write path and a slice of the pool budget, so add one only for a pass that
// consumes genuinely different global data. A shader that reads nothing from
// set 0 needs none — it simply never binds the set, as the pick pass does.
//
// Not a compatibility class: push constant ranges feed layout compatibility too
// and are per shader, so two pipelines can share a family and still need set 0
// rebound between them (see RecordCommandBuffer).
enum class GlobalShaderFamily : u8 {
  // View, instancing and lighting. Rewritten every frame. Used by everything
  // that draws world geometry with the frame camera.
  Scene3D = 0,
  // The UI font atlas. Rewritten only when the atlas upload completes, but
  // still allocated per frame in flight, because a descriptor set cannot be
  // rewritten while a frame using it is still in flight.
  Screen2D,

  Count_,
};

inline constexpr size_t kGlobalShaderFamilyCount =
    static_cast<size_t>(GlobalShaderFamily::Count_);

inline constexpr auto FamilyIndex(GlobalShaderFamily family) noexcept
    -> size_t {
  return static_cast<size_t>(family);
}

} // namespace lumina::renderer
