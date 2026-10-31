#include "model_manifest.h"
#include <cctype>
#include <cstdlib>
#include "../core/json.h"
namespace tz {
namespace mdl {
const ModelDesc* Manifest::find(const std::string& id) const { for (auto& m : models) if (m.id == id) return &m; return nullptr; }

bool parseColor(const std::string& s0, Color& out) {
  std::string s = !s0.empty() && s0[0] == '#' ? s0.substr(1) : s0;
  if (s.size() != 6 && s.size() != 8) return false;
  for (char c : s) if (!std::isxdigit((unsigned char)c)) return false;
  unsigned long v = std::strtoul(s.c_str(), nullptr, 16);
  if (s.size() == 6) out = hexColor((uint32_t)v);
  else out = hexColor((uint32_t)(v >> 8), (float)(v & 255) / 255.f);
  return true;
}
static bool color(const Json& j, Color& c) {
  if (j.type == Json::String) return parseColor(j.str, c);
  if (j.type == Json::Array && j.size() >= 3) { c = {(float)j[0].numOr(1), (float)j[1].numOr(1), (float)j[2].numOr(1), (float)(j.size() > 3 ? j[3].numOr(1) : 1)}; return true; }
  return false;
}
static Vec3 vec3(const Json& j, Vec3 d) { return j.type == Json::Array && j.size() >= 3 ? Vec3((float)j[0].numOr(0), (float)j[1].numOr(0), (float)j[2].numOr(0)) : d; }
bool matchPattern(const std::string& pattern, const std::string& name) {
  if (pattern == "*") return true;
  if (!pattern.empty() && pattern.back() == '*') return name.compare(0, pattern.size() - 1, pattern, 0, pattern.size() - 1) == 0;
  return pattern == name;
}
static void strings(const Json& j, std::vector<std::string>& out) { out.clear(); for (size_t i = 0; i < j.size(); i++) out.push_back(j[i].strOr("")); }
static bool look(const Json& j, Look& l, const std::string& where, std::string& err) {
  if (j.has("tint") && !color(j["tint"], l.tint)) { err = where + ": bad tint"; return false; }
  if (j.has("hide")) strings(j["hide"], l.hide);
  if (j.has("show")) strings(j["show"], l.show);
  if (j.has("recolor")) l.recolor.clear();
  const Json& rc = j["recolor"];
  for (size_t i = 0; i < rc.size(); i++) {
    Recolor r; r.material = rc[i]["material"].strOr("*");
    if (!color(rc[i]["color"], r.color)) { err = where + ": bad recolor colour"; return false; }
    std::string m = rc[i]["mode"].strOr("hue");
    r.mode = m == "multiply" ? Recolor::Multiply : m == "replace" ? Recolor::Replace : Recolor::Hue;
    l.recolor.push_back(r);
  }
  return true;
}

bool parseManifest(const std::string& text, Manifest& out, std::string& err) {
  out = Manifest();
  Json root;
  if (!Json::parse(text, root, &err)) { err = "models.json: " + err; return false; }
  out.enabled = root["enabled"].boolOr(true);
  const Json& ms = root["models"];
  if (ms.type != Json::Object) { err = "models.json: missing \"models\" object"; return false; }
  for (auto& kv : ms.obj) {
    const Json& j = kv.second; ModelDesc d; d.id = kv.first;
    d.file = j["file"].strOr("");
    if (d.file.empty()) { err = "models.json: " + d.id + " has no file"; return false; }
    for (size_t i = 0; i < j["extra"].size(); i++) d.extraFiles.push_back(j["extra"][i].strOr(""));
    d.scale = (float)j["scale"].numOr(1); d.height = (float)j["height"].numOr(0);
    d.rotationDeg = vec3(j["rotation"], Vec3()); d.offset = vec3(j["offset"], Vec3());
    d.groundAlign = j["groundAlign"].boolOr(true); d.animSpeed = (float)j["animSpeed"].numOr(1);
    d.fallback = j["fallback"].strOr("");
    if (!look(j, d.look, d.id, err)) return false;
    for (auto& v : j["variants"].obj) { Look l = d.look; if (!look(v.second, l, d.id + "." + v.first, err)) return false; d.variants[v.first] = l; }
    for (auto& c : j["clips"].obj) d.clips[c.first] = c.second.strOr("");
    out.models.push_back(d);
  }
  return true;
}
}  // namespace mdl
}  // namespace tz
