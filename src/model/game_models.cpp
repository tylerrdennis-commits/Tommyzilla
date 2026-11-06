#include <unordered_set>
#include "game_models.h"
#include <algorithm>
#include <cctype>
#include <cmath>
namespace tz {
int GameModels::find(const std::string& id) {
  auto it = byId_.find(id);
  if (it != byId_.end()) return it->second;
  const mdl::ModelAsset* a = id.empty() ? nullptr : lib_.get(id);
  int idx = -1;
  if (a) {
    Entry e; e.id = id; e.asset = a; e.variants.push_back("");
    for (auto& v : a->desc.variants) e.variants.push_back(v.first);
    const Vec3 mn = a->model.boundsMin, mx = a->model.boundsMax;
    Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
    for (int i = 0; i < 8; i++) {
      Vec3 p = a->offset.transformPoint(Vec3((i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z));
      lo = Vec3(std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)); hi = Vec3(std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z));
    }
    e.lo = lo; e.hi = hi; e.radius = std::max(0.5f, length(hi - lo) * 0.5f);
    idx = (int)assets_.size(); assets_.push_back(std::move(e));
  }
  byId_[id] = idx;
  return idx;
}
int GameModels::variant(int asset, const std::string& name) {
  if (asset < 0 || asset >= (int)assets_.size() || name.empty()) return 0;
  auto& v = assets_[(size_t)asset].variants;
  for (size_t i = 1; i < v.size(); i++) if (v[i] == name) return (int)i;
  return 0;
}
bool GameModels::bounds(int asset, Vec3& lo, Vec3& hi) {
  if (asset < 0 || asset >= (int)assets_.size()) return false;
  lo = assets_[(size_t)asset].lo; hi = assets_[(size_t)asset].hi;
  return true;
}
int GameModels::clipFor(int asset, int logical) const {
  const mdl::ModelAsset* a = this->asset(asset);
  if (!a || a->model.clips.empty() || logical < 0 || logical >= MC_COUNT) return -1;
  // fallback chains: a model without "eat" bites with its attack, a missing stun holds the death pose, ...
  static const int chain[MC_COUNT][4] = {
      {MC_IDLE, -1, -1, -1},         {MC_WALK, MC_IDLE, -1, -1},     {MC_RUN, MC_WALK, MC_IDLE, -1},
      {MC_ATTACK, MC_IDLE, -1, -1},  {MC_DEATH, MC_IDLE, -1, -1},    {MC_HIT, MC_IDLE, -1, -1},
      {MC_EAT, MC_ATTACK, MC_IDLE, -1}, {MC_STUN, MC_HIT, MC_DEATH, MC_IDLE}, {MC_JUMP, MC_IDLE, -1, -1},
      {MC_CHEER, MC_IDLE, -1, -1}};
  for (int c : chain[logical]) {
    if (c < 0) break;
    int ci = a->clip(kModelClipNames[c]);
    if (ci >= 0) return ci;
  }
  return -1;
}

void GameModels::drive(Anim& an, const ModelReq& r, int clip, float dist, bool isShared) {
  const mdl::ModelAsset* a = asset(r.asset);
  if (an.asset != r.asset) { an.a.bind(&a->model); an.asset = r.asset; an.clip = -2; an.started = false; }
  an.lastSeen = frame_;
  float spd = r.speed;
  if (clip >= 0 && r.fit > 0) { float d = a->model.clips[(size_t)clip].duration; if (d > 0) spd = d / r.fit; }
  if (clip != an.clip || (!r.loop && r.trigger != an.trigger && !isShared)) {
    // walk <-> run keeps the stride phase, so the feet don't pop (swap legs) at the switch
    bool loco = an.started && r.loop && an.clip >= 0 && clip >= 0 && (an.logical == MC_WALK || an.logical == MC_RUN) && (r.clip == MC_WALK || r.clip == MC_RUN);
    float keepPhase = loco ? an.a.phase() : -1.f;
    if (clip >= 0) an.a.play(clip, r.loop, an.started ? (loco ? 0.12f : 0.18f) : 0.f, spd, true); else an.a.stop(0.2f);
    if (keepPhase >= 0) an.a.setPhase(keepPhase);
    an.logical = r.clip;
    if (!an.started && isShared && clip >= 0) an.a.update(r.phase * a->model.clips[(size_t)clip].duration);  // crowd phase offset
    an.clip = clip; an.trigger = r.trigger; an.started = true;
    an.a.update(0); an.acc = 0; an.lastUpdate = frame_; stats.updated++;
    return;
  }
  if (clip >= 0) an.a.play(clip, r.loop, 0.18f, spd, false);  // keeps time, refreshes loop/speed
  an.acc += dt_;
  if (an.lastUpdate == frame_) return;  // shared: already advanced this frame
  int every = dist < lodNear ? 1 : dist < lodFar ? 2 : 4;
  if (an.acc > 0 && (frame_ - an.lastUpdate) >= every) { an.a.update(an.acc); an.acc = 0; an.lastUpdate = frame_; stats.updated++; }
}

void GameModels::build(const RenderList& rl, ModelDrawList& out) { buildFrom(rl.models, rl, nullptr, out); }
void GameModels::build(const RenderList& rl, const RenderList& second, ModelDrawList& out) {
  merged_ = rl.models;
  // the same entity appears in both views' lists: keyed ones by key, statics by (asset, variant, transform);
  // a duplicate would be drawn twice per view (double-blended alpha, wasted draws)
  auto sig = [](const ModelReq& r) {
    uint64_t h = 1469598103934665603ull ^ (uint64_t)(uint32_t)r.asset ^ ((uint64_t)(uint32_t)r.variant << 32);
    const uint32_t* w = reinterpret_cast<const uint32_t*>(&r.world.m[0][0]);
    for (int i = 0; i < 16; i++) h = (h ^ w[i]) * 1099511628211ull;
    return r.key ? (uint64_t)r.key | (1ull << 63) : h & ~(1ull << 63);
  };
  std::unordered_set<uint64_t> seen; seen.reserve(rl.models.size() * 2);
  for (const ModelReq& r : rl.models) seen.insert(sig(r));
  for (const ModelReq& r : second.models) if (seen.insert(sig(r)).second) merged_.push_back(r);
  buildFrom(merged_, rl, &second, out);
}
void GameModels::buildFrom(const std::vector<ModelReq>& reqs, const RenderList& rl, const RenderList* second, ModelDrawList& out) {
  stats = Stats();
  frame_++;
  dt_ = rl.animDt;
  Frustum fr; fr.fromMatrix(rl.view * rl.proj);
  Frustum fr2; if (second) fr2.fromMatrix(second->view * second->proj);
  const size_t n = reqs.size();
  stats.requests = (int)n;
  // pass 1: cull, and rank the animated non-crowd requests by distance for the solo cap
  order_.clear();
  std::vector<float> dist(n, 0); std::vector<uint8_t> vis(n, 0);
  for (size_t i = 0; i < n; i++) {
    const ModelReq& r = reqs[i];
    if (!asset(r.asset)) continue;
    const Entry& e = assets_[(size_t)r.asset];
    Vec3 c = r.world.transformPoint((e.lo + e.hi) * 0.5f);
    float sc = std::max({length(Vec3(r.world.m[0][0], r.world.m[0][1], r.world.m[0][2])), length(Vec3(r.world.m[1][0], r.world.m[1][1], r.world.m[1][2])), length(Vec3(r.world.m[2][0], r.world.m[2][1], r.world.m[2][2]))});
    bool in1 = fr.sphere(c, e.radius * sc), in2 = second && fr2.sphere(c, e.radius * sc);
    if (!in1 && !in2) { stats.culled++; continue; }
    vis[i] = 1; dist[i] = std::min(in1 ? length(c - rl.eye) : 1e9f, in2 ? length(c - second->eye) : 1e9f);
    if (r.key && !r.shared) order_.push_back(i);
  }
  std::sort(order_.begin(), order_.end(), [&](size_t a, size_t b) {
    auto pri = [&](size_t i) { uint32_t k = reqs[i].key >> 24; return k == MK_KAIJU || k == MK_BOSS || k == MK_PREVIEW ? 0 : 1; };
    int pa = pri(a), pb = pri(b);
    return pa != pb ? pa < pb : dist[a] < dist[b];
  });
  std::vector<uint8_t> soloOk(n, 0);
  for (size_t j = 0; j < order_.size() && (int)j < soloMax; j++) soloOk[order_[j]] = 1;
  for (size_t j = (size_t)std::max(0, soloMax); j < order_.size(); j++) if ((reqs[order_[j]].key >> 24) == MK_KAIJU || (reqs[order_[j]].key >> 24) == MK_BOSS) soloOk[order_[j]] = 1;
  // pass 2: animate + emit
  for (size_t i = 0; i < n; i++) {
    if (!vis[i]) continue;
    const ModelReq& r = reqs[i];
    const mdl::ModelAsset* a = asset(r.asset);
    const Entry& e = assets_[(size_t)r.asset];
    const std::string& vname = r.variant > 0 && r.variant < (int)e.variants.size() ? e.variants[(size_t)r.variant] : e.variants[0];
    ModelDraw d = a->makeDraw(r.world, nullptr, vname);
    if (r.key && !a->model.clips.empty()) {
      int clip = clipFor(r.asset, r.clip);
      if (soloOk[i]) { Anim& an = solo_[r.key]; drive(an, r, clip, dist[i], false); d.pose = &an.a.pose(); stats.solo++; }
      else {
        int bucket = std::min(2, std::max(0, (int)(r.phase * 3)));
        uint64_t k = ((uint64_t)(uint32_t)r.asset << 32) | ((uint64_t)(uint32_t)(clip + 1) << 8) | (uint64_t)(r.loop ? 0 : 4) | (uint64_t)bucket;
        Anim& an = shared_[k]; drive(an, r, clip, dist[i], true); d.pose = &an.a.pose(); stats.shared++;
      }
    }
    d.tint = r.tint; d.glow = r.glow; d.mix = r.mix; d.mixK = r.mixK; d.finish = r.finish; d.finishT = r.finishT;
    if (r.palette) d.materialColors = paletteColors(r.asset, d.materialColors, r);
    out.draws.push_back(d);
    stats.drawn++;
  }
  // forget animators nobody asked for in a while (dead units, people out of range)
  if (frame_ % 30 == 0) {
    for (auto it = solo_.begin(); it != solo_.end();) it = frame_ - it->second.lastSeen > gcFrames ? solo_.erase(it) : std::next(it);
    for (auto it = shared_.begin(); it != shared_.end();) it = frame_ - it->second.lastSeen > gcFrames ? shared_.erase(it) : std::next(it);
  }
  stats.animators = (int)(solo_.size() + shared_.size());
}
// rolled kaiju look: *_Main -> primary, *_Secondary -> accent, names with "eye" -> eyes; a model without a *_Main
// material is tinted toward the primary colour instead (keeps its shading)
const std::vector<Color>* GameModels::paletteColors(int ai, const std::vector<Color>* base, const ModelReq& r) {
  const mdl::ModelAsset* a = asset(ai); if (!a) return base;
  auto q = [](float v) { return (uint64_t)std::lround(clampf(v, 0, 1) * 255); };
  uint64_t key = (uint64_t)(uint32_t)ai * 0x9E3779B97F4A7C15ull;
  for (const Color& c : {r.primary, r.accent, r.eyes}) key = (key ^ (q(c.r) << 16 | q(c.g) << 8 | q(c.b))) * 0x100000001B3ull;
  key ^= (uint64_t)(uintptr_t)base;
  auto it = palettes_.find(key); if (it != palettes_.end()) return &it->second;
  if (palettes_.size() > 512) palettes_.clear();
  const auto& mats = a->model.materials;
  std::vector<Color> out(mats.size());
  bool anyMain = false;
  for (size_t i = 0; i < mats.size(); i++) { out[i] = base && i < base->size() ? (*base)[i] : mats[i].baseColor; if (mats[i].name.find("_Main") != std::string::npos) anyMain = true; }
  for (size_t i = 0; i < mats.size(); i++) {
    std::string n = mats[i].name; for (auto& ch : n) ch = (char)std::tolower((unsigned char)ch);
    Color keepA = out[i];
    if (n.find("_main") != std::string::npos) out[i] = r.primary;
    else if (n.find("_secondary") != std::string::npos) out[i] = r.accent;
    else if (n.find("eye") != std::string::npos && n.find("black") == std::string::npos) out[i] = r.eyes;
    else if (!anyMain) out[i] = mixColor(out[i], r.primary, 0.5f);
    out[i].a = keepA.a;
  }
  return &(palettes_[key] = out);
}
}  // namespace tz
