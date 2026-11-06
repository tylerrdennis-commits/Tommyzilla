// The gameplay bridge on the model side: implements the core's ModelCatalog over a ModelLibrary (asset ids / look
// variants / bounds) and turns the RenderList's ModelReq entries into a ModelDrawList each frame, owning the
// animation state. Performance (CPU skinning): crowd requests share one animator per (asset, clip, phase bucket);
// solo animators are capped (nearest first, the rest share); animation updates are rate-throttled by distance;
// requests are frustum-culled by the asset bounds; animators unseen for a while are dropped.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "../game/render_types.h"
#include "animator.h"
#include "model_draw.h"
#include "model_library.h"
namespace tz {
class GameModels : public ModelCatalog {
 public:
  explicit GameModels(mdl::ModelLibrary& lib) : lib_(lib) {}
  int find(const std::string& id) override;
  int variant(int asset, const std::string& name) override;
  bool bounds(int asset, Vec3& lo, Vec3& hi) override;
  bool hasDistinctRun(int asset) override { return clipFor(asset, MC_RUN) != clipFor(asset, MC_WALK); }
  const mdl::ModelAsset* asset(int index) const { return index >= 0 && index < (int)assets_.size() ? assets_[(size_t)index].asset : nullptr; }
  int clipFor(int asset, int logical) const;   // logical ModelClip -> clip index with fallbacks (-1 = rest pose)

  void build(const RenderList& rl, ModelDrawList& out);  // rl.animDt advances the animations (0 = paused)
  // split screen: one draw list for both views (each entity animated once; the renderer draws it in each viewport).
  // A request is kept if either view sees it; keyed entities seen in both lists are merged.
  void build(const RenderList& rl, const RenderList& second, ModelDrawList& out);
  struct Stats { int requests = 0, drawn = 0, culled = 0, solo = 0, shared = 0, updated = 0, animators = 0; } stats;
  int soloMax = 32;                 // animators with their own pose (nearest first); beyond that requests share
  float lodNear = 50, lodFar = 130; // animation update every frame / every 2nd / every 4th frame by distance
  int gcFrames = 120;               // drop animators unseen for this many frames
 private:
  struct Entry { std::string id; const mdl::ModelAsset* asset = nullptr; std::vector<std::string> variants; Vec3 lo, hi; float radius = 1; };
  struct Anim { mdl::Animator a; int asset = -1, clip = -2, lastSeen = 0, lastUpdate = 0; uint16_t trigger = 0; float acc = 0; bool started = false; uint8_t logical = 255; };
  void drive(Anim& an, const ModelReq& r, int clip, float dist, bool isShared);
  mdl::ModelLibrary& lib_;
  std::vector<Entry> assets_;
  std::unordered_map<std::string, int> byId_;
  std::unordered_map<uint32_t, Anim> solo_;
  std::unordered_map<uint64_t, Anim> shared_;
  std::vector<size_t> order_;
  std::vector<ModelReq> merged_;
  void buildFrom(const std::vector<ModelReq>& reqs, const RenderList& rl, const RenderList* second, ModelDrawList& out);
  std::unordered_map<uint64_t, std::vector<Color>> palettes_;  // rolled-look material colours (stable storage for ModelDraw)
  const std::vector<Color>* paletteColors(int asset, const std::vector<Color>* base, const ModelReq& r);
  int frame_ = 0;
  float dt_ = 0;
};
}  // namespace tz
