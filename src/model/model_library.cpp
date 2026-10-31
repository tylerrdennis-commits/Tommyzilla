#include "model_library.h"
#include <cctype>
#include "../core/json.h"
#include "model_loader.h"
namespace tz {
namespace mdl {
const char* const kLogicalClips[5] = {"idle", "walk", "run", "attack", "death"};

static const std::vector<std::string>& candidates(const std::string& logical) {
  static const std::map<std::string, std::vector<std::string>> c = {
      {"idle", {"Idle", "Idle_Neutral", "Flying_Idle", "Idle_Gun", "idle"}},
      {"walk", {"Walk", "Walking", "Fast_Flying", "Flying_Idle", "Run"}},
      {"run", {"Run", "Running", "Fast_Flying", "Walk"}},
      {"attack", {"Punch", "Attack", "Headbutt", "Weapon", "Bite_Front", "Punch_Right", "Sword_Slash", "Gun_Shoot", "Idle_Shoot", "Kick_Right"}},
      {"death", {"Death", "Die", "Dead"}}};
  static const std::vector<std::string> none;
  auto it = c.find(logical); return it == c.end() ? none : it->second;
}
static bool matches(const std::string& pattern, const std::string& name) { return matchPattern(pattern, name); }
std::vector<uint8_t> nodeVisibility(const Model& m, const Look& l) {
  if (l.hide.empty()) return {};
  std::vector<uint8_t> v(m.nodes.size(), 1);
  for (int n : m.order) {  // parents first: hidden parents hide their children
    const Node& nd = m.nodes[(size_t)n];
    bool hidden = nd.parent >= 0 && !v[(size_t)nd.parent];
    for (auto& p : l.hide) hidden |= matches(p, nd.name);
    for (auto& p : l.show) if (matches(p, nd.name)) hidden = false;
    v[(size_t)n] = hidden ? 0 : 1;
  }
  return v;
}
std::vector<Color> applyLook(const Model& m, const Look& l) {
  std::vector<Color> out;
  for (const Material& mt : m.materials) {
    Color c = mt.baseColor;
    for (const Recolor& r : l.recolor) {
      if (!matches(r.material, mt.name)) continue;
      if (r.mode == Recolor::Replace) c = {r.color.r, r.color.g, r.color.b, c.a};
      else if (r.mode == Recolor::Multiply) c = {c.r * r.color.r, c.g * r.color.g, c.b * r.color.b, c.a};
      else c = hsvRecolor(c, r.color);
    }
    out.push_back({c.r * l.tint.r, c.g * l.tint.g, c.b * l.tint.b, c.a * l.tint.a});
  }
  return out;
}

int ModelAsset::clip(const std::string& logical) const {
  auto it = clipMap.find(logical);
  if (it != clipMap.end()) return it->second;
  return model.findClip(logical);
}
const std::vector<Color>* ModelAsset::look(const std::string& variant) const {
  auto it = looks.find(variant);
  if (it == looks.end()) it = looks.find("");
  return it == looks.end() ? nullptr : &it->second;
}
const std::vector<uint8_t>* ModelAsset::visible(const std::string& variant) const {
  auto it = visibility.find(variant);
  if (it == visibility.end()) it = visibility.find("");
  return it == visibility.end() || it->second.empty() ? nullptr : &it->second;
}
ModelDraw ModelAsset::makeDraw(const Mat4& entityWorld, const Pose* pose, const std::string& variant) const {
  ModelDraw d; d.model = &model; d.world = offset * entityWorld; d.pose = pose ? pose : &restPose; d.materialColors = look(variant);
  d.nodeVisible = visible(variant);
  return d;
}

bool ModelLibrary::loadManifestFile(const std::string& path, const std::string& root, std::string& err) {
  std::string text;
  if (!readTextFile(path, text)) { err = "cannot read " + path; return false; }
  return loadManifestText(text, root, err);
}
bool ModelLibrary::loadManifestText(const std::string& text, const std::string& root, std::string& err) {
  cache_.clear(); log_.clear(); root_ = root;
  if (!root_.empty() && root_.back() != '/' && root_.back() != '\\') root_ += '/';
  return parseManifest(text, manifest_, err);
}
const ModelAsset* ModelLibrary::get(const std::string& id) {
  auto it = cache_.find(id);
  if (it != cache_.end()) return it->second.get();
  const ModelDesc* d = manifest_.find(id);
  if (!d) { cache_[id] = nullptr; return nullptr; }
  std::unique_ptr<ModelAsset> a(new ModelAsset());
  a->desc = *d;
  std::string err;
  if (!loadModel(root_ + d->file, a->model, err)) {
    log_.push_back("[model] " + id + ": " + err + " -> primitive fallback");
    cache_[id] = nullptr; return nullptr;
  }
  Model& m = a->model;
  // placement: rotate, then scale (target height), then ground-align
  Mat4 rot = Mat4::rotX(d->rotationDeg.x * PI / 180) * Mat4::rotY(d->rotationDeg.y * PI / 180) * Mat4::rotZ(d->rotationDeg.z * PI / 180);
  Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
  for (int i = 0; i < 8; i++) {
    Vec3 c((i & 1) ? m.boundsMax.x : m.boundsMin.x, (i & 2) ? m.boundsMax.y : m.boundsMin.y, (i & 4) ? m.boundsMax.z : m.boundsMin.z);
    Vec3 r = rot.transformPoint(c);
    lo = Vec3(std::min(lo.x, r.x), std::min(lo.y, r.y), std::min(lo.z, r.z)); hi = Vec3(std::max(hi.x, r.x), std::max(hi.y, r.y), std::max(hi.z, r.z));
  }
  float h = hi.y - lo.y;
  a->scale = d->height > 0 && h > 1e-6f ? d->height / h : d->scale;
  Vec3 shift = d->groundAlign ? Vec3(-(lo.x + hi.x) * 0.5f, -lo.y, -(lo.z + hi.z) * 0.5f) * a->scale : Vec3();
  a->offset = rot * Mat4::scale(a->scale, a->scale, a->scale) * Mat4::translation(shift.x + d->offset.x, shift.y + d->offset.y, shift.z + d->offset.z);
  computePose(m, m.restLocals(), a->restPose);
  // clips: explicit mapping first, then the candidate list
  std::string names;
  for (auto& c : m.clips) names += (names.empty() ? "" : ", ") + c.name;
  log_.push_back("[model] " + id + ": " + describeModel(m));
  log_.push_back("[model] " + id + " clips: " + (names.empty() ? "(none)" : names));
  std::string mapping;
  for (const char* lc : kLogicalClips) {
    int ci = -1;
    auto e = d->clips.find(lc);
    if (e != d->clips.end()) { ci = m.findClip(e->second); if (ci < 0) log_.push_back("[model] " + id + ": mapped clip '" + e->second + "' not found"); }
    const std::string lcName(lc);
    const std::vector<std::string>& cands = candidates(lcName);
    for (const std::string& cand : cands) if (ci < 0) ci = m.findClip(cand);
    a->clipMap[lc] = ci;
    mapping += std::string(mapping.empty() ? "" : ", ") + lc + "=" + (ci >= 0 ? m.clips[(size_t)ci].name : "-");
  }
  for (auto& e : d->clips) if (!a->clipMap.count(e.first)) a->clipMap[e.first] = m.findClip(e.second);  // extra game-specific names
  if (!m.clips.empty()) log_.push_back("[model] " + id + " clip map: " + mapping);
  for (auto& w : m.warnings) log_.push_back("[model] " + id + " warning: " + w);
  a->looks[""] = applyLook(m, d->look); a->visibility[""] = nodeVisibility(m, d->look);
  for (auto& v : d->variants) { a->looks[v.first] = applyLook(m, v.second); a->visibility[v.first] = nodeVisibility(m, v.second); }
  ModelAsset* raw = a.get();
  cache_[id] = std::move(a);
  return raw;
}
int ModelLibrary::loadAll() { int n = 0; for (auto& d : manifest_.models) n += get(d.id) != nullptr; return n; }
}  // namespace mdl
}  // namespace tz
