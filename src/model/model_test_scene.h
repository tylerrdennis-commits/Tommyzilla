// "-modeltest" viewer scene: every manifest model on a ground plane, characters cycling idle/walk/run/attack/death
// with crossfades, the Dino walking a circle, vehicles and buildings, a primitive fallback for a missing model,
// and labels. Platform independent: fills the primitive RenderList (ground, fallbacks, HUD) and a ModelDrawList.
#pragma once
#include <string>
#include <vector>
#include "../game/render_types.h"
#include "animator.h"
#include "model_draw.h"
#include "model_library.h"
namespace tz {
class ModelTestScene {
 public:
  explicit ModelTestScene(mdl::ModelLibrary& lib) : lib_(lib) {}
  void init();
  void update(float dt);
  void render(RenderList& rl, ModelDrawList& ml, int w, int h);
  int loadedCount() const { return loaded_; }
  int actorCount() const { return (int)actors_.size(); }
  float camSpeed = 0.06f;   // orbit speed (rad/s)
  float camYaw = 0.55f;
  int camLock = -1;         // -1: cycle the camera presets every kCamSecs; else hold preset camLock (0..kCamPresets-1)
  static const int kCamPresets = 5;
  float wallFps = -1;       // optional: measured by the platform loop (wall clock); shown instead of the dt-based estimate
 private:
  struct Actor {
    std::string id, variant, label;
    const mdl::ModelAsset* asset = nullptr;   // null -> primitive fallback
    mdl::Animator anim;
    Vec3 pos; float yaw = 0;
    bool cycle = false;      // cycles the five logical clips
    int motion = 0;          // 0 static, 1 circle walker, 2 road driver, 3 hover/spin
    int clipSlot = 0; float clipT = 0, phase = 0;
    std::string clipName;
  };
  void startClip(Actor& a);
  mdl::ModelLibrary& lib_;
  std::vector<Actor> actors_;
  float t_ = 0, fps_ = 0;
  int loaded_ = 0;
};
}  // namespace tz
