// What the game hands to the DX12 model renderer each frame (in addition to the primitive RenderList).
// Kept separate from game/render_types.h so the model module merges without touching shared headers.
#pragma once
#include <vector>
#include "model.h"
namespace tz {
struct ModelDraw {
  const mdl::Model* model = nullptr;
  Mat4 world = Mat4::identity();                  // model space -> world (manifest offset * entity transform)
  const mdl::Pose* pose = nullptr;                // animated pose, or the asset's rest pose (must not be null)
  const std::vector<Color>* materialColors = nullptr;  // per-material colour (look/variant), or null = material base colours
  const std::vector<uint8_t>* nodeVisible = nullptr;   // per node (look hide/show), or null = all visible
  Color tint = {1, 1, 1, 1};                      // extra per-instance multiply (hit flash, fade-out alpha...)
  float glow = 0;                                 // adds base * glow (like Instance params.y)
  Color mix = {1, 1, 1, 1}; float mixK = 0;       // material colour -> lerp(material, mix, mixK) before tint (tier paint, hit flash)
  int finish = 0; float finishT = 0;              // 1 gold, 2 chrome, 3 holographic (pixel shader), animated by finishT
};
struct ModelDrawList {
  std::vector<ModelDraw> draws;
  void clear() { draws.clear(); }
};
}  // namespace tz
