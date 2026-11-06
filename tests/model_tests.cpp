// Linux tests for the model module: loads every model mapped in data/models.json, checks mesh / material /
// skeleton / clip counts and texture decoding, the animator (sampling, crossfade, one-shot clips), looks/recolours,
// the manifest and the missing-file fallback. Prints the clip names found per model.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include "core/json.h"
#include "model/animator.h"
#include "model/model_library.h"
#include "model/model_loader.h"
#include "model/model_test_scene.h"
#include "model/game_models.h"
#include "game/game.h"
#include <chrono>
#include <memory>
using namespace tz;
using namespace tz::mdl;

static int gPass = 0, gFail = 0;
#define CHECK(c) do { if (c) gPass++; else { gFail++; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define SECTION(n) std::printf("[%s]\n", n)
static const std::string kRoot = std::string(TZ_SOURCE_DIR) + "/";

static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) <= e; }
static bool matNear(const Mat4& a, const Mat4& b, float e = 1e-4f) { for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) if (!near(a.m[i][j], b.m[i][j], e)) return false; return true; }

static void testMath() {
  SECTION("math: quaternions, TRS, inverse");
  float a = 0.7f;
  Quat qy = {0, std::sin(a / 2), 0, std::cos(a / 2)};
  CHECK(matNear(quatToMat4(qy), Mat4::rotY(a)));
  Quat qx = {std::sin(a / 2), 0, 0, std::cos(a / 2)};
  CHECK(matNear(quatToMat4(qx), Mat4::rotX(a)));
  Transform t; t.t = Vec3(1, 2, 3); t.r = normalizeQ({0.2f, 0.4f, 0.1f, 0.9f}); t.s = Vec3(2, 2, 2);
  Mat4 m = toMat4(t), inv;
  CHECK(invertAffine(m, inv));
  CHECK(matNear(m * inv, Mat4::identity(), 1e-4f));
  Quat h = slerpQ(Quat(), qy, 0.5f);
  CHECK(matNear(quatToMat4(h), Mat4::rotY(a / 2), 1e-4f));
  CHECK(near(linearToSrgb(srgbToLinear(0.4f)), 0.4f, 1e-4f));
  Color p = hsvRecolor(Color{0.17f, 0.34f, 0.43f, 1}, hexColor(0x8a3fd6));
  CHECK(p.r > p.g && p.b > p.g);  // purple
}

static void testInlineGltf() {
  SECTION("glTF: embedded base64 buffer, matrix node, default material, computed normals");
  const char* gltf = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
    "nodes":[{"mesh":0,"matrix":[2,0,0,0, 0,2,0,0, 0,0,2,0, 5,0,0,1]}],
    "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],
    "buffers":[{"byteLength":36,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA"}],
    "bufferViews":[{"buffer":0,"byteLength":36}],
    "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}]})";
  std::string p = "/tmp/tz_model_test_triangle.gltf";
  CHECK(writeTextFile(p, gltf));
  Model m; std::string err;
  bool ok = loadModel(p, m, err);
  if (!ok) std::printf("  err: %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  CHECK(m.meshes.size() == 1 && m.meshes[0].vertices.size() == 3 && m.triangleCount() == 1);
  CHECK(m.materials.size() == 1 && m.materials[0].name == "default");
  CHECK(near(m.nodes[0].local.t.x, 5) && near(m.nodes[0].local.s.y, 2));
  CHECK(near(std::fabs(m.meshes[0].vertices[0].nrm[2]), 1));
  CHECK(near(m.boundsMin.x, 5) && near(m.boundsMax.x, 7) && near(m.boundsMax.y, 2));
  Model bad; CHECK(!loadModel("/tmp/does_not_exist.glb", bad, err) && err.find("not found") != std::string::npos);
  CHECK(writeTextFile("/tmp/tz_model_test_bad.glb", "this is not a glb"));
  CHECK(!loadModel("/tmp/tz_model_test_bad.glb", bad, err));
}

static void testTextureDecode() {
  SECTION("stb_image: Kenney colormap.png");
  Texture t; std::string err;
  bool ok = decodeImageFile(kRoot + "assets/kenney/kenney_car-kit/Models/GLB format/Textures/colormap.png", t, err);
  if (!ok) std::printf("  err: %s\n", err.c_str());
  CHECK(ok);
  CHECK(t.width > 0 && t.height > 0 && t.rgba.size() == (size_t)t.width * t.height * 4);
  bool anyColour = false; for (size_t i = 0; i + 3 < t.rgba.size(); i += 4) if (t.rgba[i] != t.rgba[i + 1]) { anyColour = true; break; }
  CHECK(anyColour);
}

struct Expect { const char* id; bool skinned; bool textured; bool animated; const char* format; };
static void testManifestModels(ModelLibrary& lib) {
  SECTION("manifest: every mapped model loads");
  const Expect ex[] = {
      {"tommyzilla", true, false, true, "glb"}, {"mossback", true, false, true, "glb"}, {"sparkleraptor", true, false, true, "glb"},
      {"the_bulk", true, false, true, "glb"},   {"cop", true, false, true, "glb"},       {"soldier", true, false, true, "glb"},
      {"police_car", false, true, false, "glb"}, {"ambulance", false, true, false, "glb"}, {"fire_truck", false, true, false, "fbx"},
      {"tank", false, false, false, "glb"},      {"police_heli", false, false, false, "glb"},
      {"shop_a", false, true, false, "glb"}, {"tower_a", false, true, false, "glb"}, {"house_a", false, true, false, "glb"},
      {"factory_a", false, true, false, "glb"}, {"office_a", false, true, false, "glb"}, {"mega_mech", true, false, true, "glb"},
      {"civilian_male_a", true, true, true, "glb"}, {"civilian_female_f", true, true, true, "glb"}, {"tree_large", false, true, false, "glb"}, {"road_straight", false, true, false, "glb"},
      {"street_light", false, false, false, "glb"}, {"barrel", false, false, false, "glb"}, {"barrier", false, false, false, "glb"}, {"container", false, false, false, "glb"}};
  std::set<std::string> covered;
  for (const Expect& e : ex) {
    covered.insert(e.id);
    const ModelAsset* a = lib.get(e.id);
    if (!a) { std::printf("  %s: NOT LOADED\n", e.id); CHECK(a != nullptr); continue; }
    const Model& m = a->model;
    std::printf("  %-16s %s\n", e.id, describeModel(m).c_str());
    CHECK(m.format == e.format);
    CHECK(!m.meshes.empty()); CHECK(!m.materials.empty()); CHECK(m.triangleCount() > 0);
    CHECK(m.boundsMax.y > m.boundsMin.y);
    CHECK(a->scale > 0 && std::isfinite(a->scale));
    if (e.skinned) {
      CHECK(!m.skins.empty()); CHECK(m.skinnedMeshCount() > 0);
      for (auto& sk : m.skins) CHECK(!sk.joints.empty() && sk.inverseBind.size() == sk.joints.size());
      bool weightsOk = true;
      for (auto& me : m.meshes) if (me.skinned) for (auto& v : me.vertices) { float s = v.weights[0] + v.weights[1] + v.weights[2] + v.weights[3]; if (!near(s, 1, 1e-3f)) weightsOk = false; }
      CHECK(weightsOk);
    }
    if (e.textured) {
      CHECK(!m.textures.empty());
      bool matTex = false; for (auto& mt : m.materials) matTex |= mt.texture >= 0;
      CHECK(matTex);
      for (auto& t : m.textures) CHECK(t.width > 0 && t.height > 0 && t.rgba.size() == (size_t)t.width * t.height * 4);
    }
    if (e.animated) {
      CHECK(!m.clips.empty());
      std::string names; for (auto& c : m.clips) { names += (names.empty() ? "" : ", ") + c.name; CHECK(c.duration > 0 && !c.channels.empty()); }
      std::printf("      clips (%zu): %s\n", m.clips.size(), names.c_str());
      std::string map;
      for (const char* lc : kLogicalClips) { int ci = a->clip(lc); CHECK(ci >= 0); map += std::string(lc) + "=" + (ci >= 0 ? m.clips[(size_t)ci].name : "-") + "  "; }
      std::printf("      mapped: %s\n", map.c_str());
    }
    for (auto& w : m.warnings) std::printf("      warning: %s\n", w.c_str());
  }
  for (auto& d : lib.manifest().models) if (!covered.count(d.id)) { std::printf("  manifest entry without expectations: %s\n", d.id.c_str()); CHECK(lib.get(d.id) != nullptr); }
}

static void testSkinning(ModelLibrary& lib) {
  SECTION("skinning: bind pose + animated palette");
  const ModelAsset* a = lib.get("tommyzilla");
  if (!a) { CHECK(false); return; }
  const Model& m = a->model;
  // In the rest pose the skinned bounds must match the mesh transformed by its node (inverse bind matrices are consistent).
  Pose rest; computePose(m, m.restLocals(), rest);
  Vec3 lo(1e9f, 1e9f, 1e9f), hi(-1e9f, -1e9f, -1e9f);
  for (size_t ni = 0; ni < m.nodes.size(); ni++) {
    if (m.nodes[ni].mesh < 0) continue;
    for (auto& v : m.meshes[(size_t)m.nodes[ni].mesh].vertices) {
      Vec3 w = rest.global[ni].transformPoint(Vec3(v.pos[0], v.pos[1], v.pos[2]));
      lo = Vec3(std::min(lo.x, w.x), std::min(lo.y, w.y), std::min(lo.z, w.z)); hi = Vec3(std::max(hi.x, w.x), std::max(hi.y, w.y), std::max(hi.z, w.z));
    }
  }
  float rawH = hi.y - lo.y, skinH = m.boundsMax.y - m.boundsMin.y;
  std::printf("  mesh-node rest height %.3f, skinned rest height %.3f\n", rawH, skinH);
  CHECK(skinH > 0.8f * rawH && skinH < 1.25f * rawH);
  Animator an; an.bind(&m);
  CHECK(an.play(a->clip("walk")));
  an.update(0.0f);
  std::vector<Mat4> p0 = an.pose().skinMats[0];
  an.update(0.4f);
  const std::vector<Mat4>& p1 = an.pose().skinMats[0];
  int moved = 0; for (size_t j = 0; j < p0.size(); j++) moved += !matNear(p0[j], p1[j], 1e-3f);
  std::printf("  walk: %d / %zu joints moved after 0.4s\n", moved, p0.size());
  CHECK(moved > 5);
  // CPU skinning (what the DX12 renderer uploads): rest palette reproduces the bind-pose bounds, animated palette moves verts
  {
    const Mesh* sm = nullptr; int skin = -1;
    for (auto& n : m.nodes) if (n.mesh >= 0 && m.meshes[(size_t)n.mesh].skinned) { sm = &m.meshes[(size_t)n.mesh]; skin = n.skin; break; }
    CHECK(sm && skin >= 0);
    if (sm && skin >= 0) {
      std::vector<Vertex> out(sm->vertices.size());
      skinMesh(*sm, rest.skinMats[(size_t)skin], out.data());
      float y0 = 1e9f, y1 = -1e9f; for (auto& v : out) { y0 = std::min(y0, v.pos[1]); y1 = std::max(y1, v.pos[1]); }
      CHECK(near(y0, m.boundsMin.y, 1e-3f) && near(y1, m.boundsMax.y, 1e-3f));
      std::vector<Vertex> out2(sm->vertices.size());
      skinMesh(*sm, an.pose().skinMats[(size_t)skin], out2.data());
      size_t diff = 0; for (size_t k = 0; k < out.size(); k++) diff += std::fabs(out[k].pos[1] - out2[k].pos[1]) > 1e-3f;
      CHECK(diff > out.size() / 10);
      bool finite = true; for (auto& v : out2) finite &= std::isfinite(v.pos[0]) && std::isfinite(v.nrm[1]);
      CHECK(finite);
    }
  }
  // crossfade
  CHECK(an.play(a->clip("run"), true, 0.5f));
  an.update(0.25f);
  CHECK(an.fadeWeight() > 0.4f && an.fadeWeight() < 0.6f);
  an.update(0.5f);
  CHECK(near(an.fadeWeight(), 1));
  CHECK(an.current() == a->clip("run"));
  // one-shot death holds the last frame
  int death = a->clip("death");
  CHECK(an.play(death, false, 0.1f));
  float d = m.clips[(size_t)death].duration;
  for (float t = 0; t < d + 0.5f; t += 1.f / 30) an.update(1.f / 30);
  CHECK(an.finished());
  CHECK(near(an.time(), d));
  // same clip again does not restart
  CHECK(an.play(death, false)); CHECK(an.finished());
  CHECK(!an.play(9999));
  CHECK(an.play("CharacterArmature|Idle"));
}

static void testLooks(ModelLibrary& lib) {
  SECTION("looks: tint / recolour / variants");
  const ModelAsset* bulk = lib.get("the_bulk");
  if (bulk) {
    const Model& m = bulk->model; const auto* look = bulk->look();
    CHECK(look && look->size() == m.materials.size());
    for (size_t i = 0; look && i < m.materials.size(); i++) {
      if (m.materials[i].name == "BlueDemon_Main") {
        Color b = m.materials[i].baseColor, c = (*look)[i];
        std::printf("  BlueDemon_Main %.2f %.2f %.2f -> %.2f %.2f %.2f\n", b.r, b.g, b.b, c.r, c.g, c.b);
        CHECK(c.r > c.g && c.b > c.g && c.r > b.r);  // purple, redder than the blue original
      }
      if (m.materials[i].name == "BlueDemon_Secondary") CHECK(near((*look)[i].r, hexColor(0xe0a030).r, 0.02f));  // gold horns/claws
    }
  } else CHECK(false);
  const ModelAsset* s = lib.get("soldier");
  if (s) {
    const auto *t1 = s->look("tier1"), *t3 = s->look("tier2"), *dflt = s->look("nonexistent");
    CHECK(t1 && t3 && dflt == s->look(""));
    for (size_t i = 0; t1 && t3 && i < s->model.materials.size(); i++)
      if (s->model.materials[i].name == "Character_Main") CHECK(!near((*t1)[i].g, (*t3)[i].g, 0.05f) && near((*t1)[i].r, hexColor(0x9e2a2a).r, 0.02f) && near((*t3)[i].r, hexColor(0xe0a830).r, 0.02f));
    // weapons: base shows only the AK, tier1 the shotgun, tier2 the rocket launcher; the body is always visible
    auto vis = [&](const char* variant, const char* node) {
      const auto* v = s->visible(variant); if (!v) return true;
      for (size_t i = 0; i < s->model.nodes.size(); i++) if (s->model.nodes[i].name == node) return (*v)[i] != 0;
      return false;
    };
    CHECK(vis("", "AK") && !vis("", "Shotgun") && !vis("", "Knife_1") && vis("", "Body") && vis("", "Head"));
    CHECK(vis("tier2", "RocketLauncher") && !vis("tier2", "AK") && vis("tier2", "Body"));
    CHECK(vis("tier1", "Shotgun") && !vis("tier1", "Sniper_2") && !vis("tier1", "AK"));
    RenderList rl; ModelDraw d = s->makeDraw(Mat4::identity(), nullptr, "tier2");
    CHECK(d.nodeVisible != nullptr && d.materialColors == s->look("tier2"));
  } else CHECK(false);
  const ModelAsset* tank = lib.get("tank");
  if (tank) { int changed = 0; const auto* p = tank->look("police"); for (size_t i = 0; p && i < p->size(); i++) changed += !near((*p)[i].b, tank->model.materials[i].baseColor.b); CHECK(changed == 2); } else CHECK(false);
}

static void testFallbackAndManifest() {
  SECTION("manifest parsing + missing-file fallback");
  ModelLibrary lib; std::string err;
  CHECK(lib.loadManifestText(R"({"models":{"ghost":{"file":"assets/nope/ghost.glb","height":3,"tint":"#ff0000"},
                                 "bad":{"file":"tests/model_tests.cpp"}}})", kRoot, err));
  CHECK(lib.get("ghost") == nullptr);
  CHECK(lib.get("bad") == nullptr);
  CHECK(lib.get("not_in_manifest") == nullptr);
  CHECK(lib.log().size() == 2);
  for (auto& l : lib.log()) std::printf("  log: %s\n", l.c_str());
  const ModelDesc* g = lib.manifest().find("ghost");
  CHECK(g && near(g->height, 3) && near(g->look.tint.r, 1) && near(g->look.tint.g, 0));
  CHECK(!lib.loadManifestText("{ not json", kRoot, err));
  CHECK(!lib.loadManifestText(R"({"models":{"x":{}}})", kRoot, err));
  Color c; CHECK(parseColor("#8a3fd6", c) && near(c.r, 0x8a / 255.f)); CHECK(!parseColor("#12", c));
}

static void testOptionalFbxCharacter() {
  SECTION("FBX skinned character (official Quaternius pack, optional)");
  const char* candidates[] = {"assets/quaternius_ultimate_monsters/Big/FBX/Dino.fbx", "assets/_downloads/quaternius_ultimate_monsters/Big/FBX/Dino.fbx"};
  for (const char* c : candidates) {
    std::string p = kRoot + c;
    if (!fileExists(p)) continue;
    Model m; std::string err;
    bool ok = loadModel(p, m, err);
    if (!ok) std::printf("  err: %s\n", err.c_str());
    CHECK(ok);
    if (!ok) return;
    std::printf("  %s\n", describeModel(m).c_str());
    std::string names; for (auto& cl : m.clips) names += (names.empty() ? "" : ", ") + cl.name;
    std::printf("  clips: %s\n", names.c_str());
    CHECK(!m.skins.empty() && !m.clips.empty() && m.skinnedMeshCount() > 0);
    return;
  }
  std::printf("  skipped: official FBX not downloaded (Google Drive quota) - FBX path covered by fire_truck\n");
}

static void testScene(ModelLibrary& lib) {
  SECTION("model test scene");
  ModelTestScene sc(lib); sc.init();
  CHECK(sc.loadedCount() == sc.actorCount() - 1);  // everything except the deliberate fallback demo
  RenderList rl; ModelDrawList ml;
  for (int i = 0; i < 200; i++) sc.update(1.f / 30);
  sc.render(rl, ml, 1280, 720);
  CHECK((int)ml.draws.size() == sc.loadedCount());
  CHECK(rl.instanceCount() > 3 && !rl.ui.empty());
  bool posesOk = true; for (auto& d : ml.draws) posesOk &= d.pose != nullptr && d.model != nullptr && d.pose->global.size() == d.model->nodes.size();
  CHECK(posesOk);
}


// ---- gameplay bridge: real Game + real library through GameModels
static std::string dataFile(const char* n) { std::string s; readTextFile(kRoot + "data/" + n, s); return s; }
static void testGameModels(ModelLibrary& lib) {
  SECTION("gameplay models (GameModels presenter)");
  std::string e = loadGameData(dataFile("config.json"), dataFile("moves.json"), dataFile("monsters.json"), dataFile("levels.json"));
  CHECK(e.empty()); if (!e.empty()) { std::printf("  data: %s\n", e.c_str()); return; }
  CHECK(lib.manifest().enabled);
  GameModels gm(lib);
  // monsters: mapped ones load, unmapped keep their primitive rig
  int mapped = 0; std::string none;
  for (auto& m : T.monsters) { if (gm.find(m.id) >= 0) mapped++; else none += " " + m.id; }
  std::printf("  monsters with models: %d/%zu (primitive:%s)\n", mapped, T.monsters.size(), none.c_str());
  CHECK(gm.find("tommyzilla") >= 0 && gm.find("mossback") >= 0 && gm.find("sparkleraptor") >= 0 && gm.find("clanksworth") >= 0 && gm.find("pinchington") >= 0 && gm.find("stompasaurus") >= 0 && mapped >= 16);
  { int bad = 0; for (auto& d : lib.manifest().models) { const ModelAsset* a = lib.get(d.id); if (!a) continue;
      for (auto& c : d.clips) if (a->clip(c.first) < 0) { bad++; std::printf("  %s: clip %s -> '%s' missing\n", d.id.c_str(), c.first.c_str(), c.second.c_str()); } }
    CHECK(bad == 0); }
  CHECK(gm.find("no_such_model") == -1 && gm.find("") == -1);
  int bulk = gm.find("the_bulk"); Vec3 lo, hi;
  CHECK(bulk >= 0 && gm.bounds(bulk, lo, hi) && std::fabs((hi.y - lo.y) - 15.f) < 0.2f && std::fabs(lo.y) < 0.05f);
  int sold = gm.find("soldier");
  CHECK(gm.variant(sold, "tier1") > 0 && gm.variant(sold, "tier2") > 0 && gm.variant(sold, "tier1") != gm.variant(sold, "tier2") && gm.variant(sold, "nope") == 0);
  // clip fallbacks: tank has none (rest pose), civilians stun with "die", blobs eat with their bite
  CHECK(gm.clipFor(gm.find("tank"), MC_WALK) == -1);
  int civ = gm.find("civilian_male_a");
  CHECK(civ >= 0 && gm.clipFor(civ, MC_STUN) >= 0 && gm.asset(civ)->model.clips[(size_t)gm.clipFor(civ, MC_STUN)].name == "die");
  CHECK(gm.clipFor(gm.find("gloopzilla"), MC_EAT) >= 0 && gm.clipFor(gm.find("tommyzilla"), MC_JUMP) >= 0);
  auto g = std::make_unique<Game>(); g->init(nullptr); g->save.tutorial.done = true; g->startMode(false);
  InputState none_; for (int i = 0; i < 20; i++) g->frame(1.f / 60, none_);
  for (int i = 0; i < 6 && !g->menus.empty(); i++) { InputState c; c.pressed[A_CONFIRM] = true; g->frame(1.f / 60, c); }
  g->models = &gm;
  g->enemies.addUnit(U_COP); g->enemies.addUnit(U_SOLDIER); g->enemies.addUnit(U_CAR); g->enemies.addUnit(U_PHELI);
  RenderList rl; ModelDrawList ml;
  g->render(rl, 1280, 720); gm.build(rl, ml);
  std::printf("  frame: %d requests -> %d draws (%d culled), solo %d shared %d, animators %d\n", gm.stats.requests, gm.stats.drawn, gm.stats.culled, gm.stats.solo, gm.stats.shared, gm.stats.animators);
  CHECK(gm.stats.requests > 50 && gm.stats.drawn > 20 && gm.stats.drawn + gm.stats.culled == gm.stats.requests && gm.stats.solo >= 1);
  bool ok = true; for (auto& d : ml.draws) ok &= d.model && d.pose && d.pose->global.size() == d.model->nodes.size();
  CHECK(ok && (int)ml.draws.size() == gm.stats.drawn);
  // a busy minute of play: animators stay bounded (crowds share), poses stay valid, the hero's clip follows input
  InputState walk; walk.moveY = 1;
  auto t0 = std::chrono::steady_clock::now(); int frames = 0, maxAnim = 0;
  for (int i = 0; i < 600; i++) {
    InputState in = i % 120 < 90 ? walk : InputState(); if (i % 60 == 30) in.pressed[A_CLAW] = in.held[A_CLAW] = true;
    g->frame(1.f / 60, in);
    rl = RenderList(); ml.clear(); g->render(rl, 1280, 720); rl.animDt = 1.f / 60; gm.build(rl, ml); frames++;
    maxAnim = std::max(maxAnim, gm.stats.animators);
  }
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / frames;
  int people = 0; for (auto& p : g->people.list) people += p.active;
  std::printf("  600 frames: %.2f ms/frame (sim + render list + animation, no GPU), max animators %d, %d people active\n", ms, maxAnim, people);
  CHECK(maxAnim < 120);
  ok = true; for (auto& d : ml.draws) ok &= d.pose != nullptr && d.pose->global.size() == d.model->nodes.size(); CHECK(ok);
  // solo cap: beyond soloMax keyed requests share poses
  gm.soloMax = 1; rl = RenderList(); ml.clear(); g->render(rl, 1280, 720); gm.build(rl, ml);
  CHECK(gm.stats.solo <= 2);
  gm.soloMax = 32;
  // rolled kaiju look: palette recolours *_Main / eye materials, finish id + time reach the draw; no palette = base colours
  {
    int tz = gm.find("tommyzilla"); CHECK(tz >= 0);
    rl = RenderList(); g->render(rl, 1280, 720);
    auto drawOf = [&](const ModelReq& q) { RenderList r2 = rl; r2.models.clear(); r2.models.push_back(q); ModelDrawList m2; gm.build(r2, m2); return m2.draws.empty() ? ModelDraw() : m2.draws[0]; };
    ModelReq q; q.asset = tz; q.world = Mat4::translation(g->kaiju.pos.x, 0, g->kaiju.pos.z);
    ModelDraw plain = drawOf(q);
    CHECK(plain.model != nullptr && plain.finish == 0);
    q.palette = true; q.primary = Color{1, 0, 0, 1}; q.secondary = Color{0, 1, 0, 1}; q.accent = Color{0, 0, 1, 1}; q.eyes = Color{1, 1, 0, 1}; q.finish = 3; q.finishT = 2.5f;
    ModelDraw looked = drawOf(q);
    CHECK(looked.model != nullptr && looked.finish == 3 && std::fabs(looked.finishT - 2.5f) < 1e-4f);
    CHECK(looked.materialColors != nullptr);
    bool changed = false, alphaKept = true;
    if (looked.materialColors) {
      const auto& mc = *looked.materialColors; const auto& mats = looked.model->materials;
      for (size_t i = 0; i < mc.size() && i < mats.size(); i++) {
        Color b = plain.materialColors && i < plain.materialColors->size() ? (*plain.materialColors)[i] : mats[i].baseColor;
        changed |= std::fabs(mc[i].r - b.r) + std::fabs(mc[i].g - b.g) + std::fabs(mc[i].b - b.b) > 0.05f;
        alphaKept &= std::fabs(mc[i].a - b.a) < 1e-4f;
      }
    }
    CHECK(changed && alphaKept);
    ModelDraw again = drawOf(q); CHECK(again.materialColors == looked.materialColors);  // cached, stable storage
    std::printf("  palette look: %zu material colours recoloured, finish %d\n", looked.materialColors ? looked.materialColors->size() : (size_t)0, looked.finish);
  }
  // 2P split screen: one draw list for both views, each kaiju its own animator, nothing drawn twice per key
  {
    g->menus.clear(); g->players = g->save.settings.players = 2;
    InputState j; j.p2Active = true; for (int i = 0; i < 6; i++) g->frame(1.f / 60, j);
    g->kaiju2.pos = g->kaiju.pos + Vec3(150, 0, 0); for (int i = 0; i < 30; i++) g->frame(1.f / 60, j);
    RenderList L, R; ModelDrawList both; CHECK(g->splitScreen() && g->render(L, 1280, 720, &R));
    auto has = [](const RenderList& r, uint32_t key) { for (auto& m : r.models) if (m.key == key) return true; return false; };
    uint32_t k1 = modelKey(MK_KAIJU, 0), k2 = modelKey(MK_KAIJU, 1);
    CHECK(has(L, k1) && has(R, k2) && k1 != k2);
    gm.build(L, R, both);
    CHECK(gm.stats.requests >= (int)L.models.size() && gm.stats.requests < (int)(L.models.size() + R.models.size() + 1));
    int ka = -1; for (auto& m : L.models) if (m.key == k1) ka = m.asset;
    int kd = 0; if (ka >= 0) for (auto& d : both.draws) kd += d.model == &gm.asset(ka)->model;
    CHECK(kd >= 2);  // both kaijus drawn (each from its own view at least)
    std::printf("  split: L %zu + R %zu requests -> %d merged, %d draws\n", L.models.size(), R.models.size(), gm.stats.requests, gm.stats.drawn);
    g->players = g->save.settings.players = 1; for (int i = 0; i < 3; i++) g->frame(1.f / 60, InputState());
  }
  // models.enabled off -> no requests at all (everything primitive)
  float was = C.modelsEnabled; C.modelsEnabled = 0; rl = RenderList(); g->render(rl, 1280, 720); CHECK(rl.models.empty()); C.modelsEnabled = was;
}

// The shipped ground textures decode with the same stb_image path the game uses, are seamless, and load as a set.
static void testGroundTextureFiles() {
  SECTION("ground textures: real PNG decode, tiling seams, set load + missing-file fallback");
  auto stb = [](const std::string& path, int& w, int& h, std::vector<uint8_t>& px, std::string& err) {
    Texture t; if (!decodeImageFile(path, t, err)) return false; w = t.width; h = t.height; px = std::move(t.rgba); return true; };
  GroundTextureSet set = loadGroundTextures(kRoot + "data/", stb);
  if (!set.ok) std::printf("  ground load error: %s\n", set.error.c_str());
  CHECK(set.ok && set.size == 256);
  for (int s = 0; s < GS_COUNT && set.ok; s++) {
    CHECK(set.mips[s].size() == 9 && set.mips[s].back().size() == 4);
    CHECK(set.meanLum[s] > 0.3f && set.meanLum[s] < 0.8f);  // mid-grey detail map: tinting keeps the palette
    const std::vector<uint8_t>& px = set.mips[s][0]; int n = set.size;
    auto L = [&](int x, int y) { return (float)px[((size_t)((y + n) % n) * n + (size_t)((x + n) % n)) * 4]; };
    double seam = 0, inner = 0, lo = 255, hi = 0;
    for (int y = 0; y < n; y++) { seam += std::fabs(L(n - 1, y) - L(0, y)) + std::fabs(L(y, n - 1) - L(y, 0)); inner += std::fabs(L(n / 2 - 1, y) - L(n / 2, y)) + std::fabs(L(y, n / 2 - 1) - L(y, n / 2)); }
    for (size_t i = 0; i < px.size(); i += 4) { lo = std::min<double>(lo, px[i]); hi = std::max<double>(hi, px[i]); }
    std::printf("  %s: mean %.3f, seam %.2f vs interior %.2f per px, range %.0f..%.0f\n", groundMaterial(s).name, set.meanLum[s], seam / (2 * n), inner / (2 * n), lo, hi);
    CHECK(seam <= inner * 1.6 + 2 * n);   // wrapping edge is no harsher than an interior step (tiles without a seam)
    CHECK(hi - lo > 20 && hi - lo < 120);  // visible detail, but not noisy or garish
  }
  // a missing file -> the whole set fails with a clear message (the game keeps the flat-colour ground)
  GroundTextureSet miss = loadGroundTextures(kRoot + "no_such_dir/", stb);
  CHECK(!miss.ok && !miss.error.empty());
}

// Bind pose of the Quaternius heroes is an A/T stance (arms out). A real walk cycle pulls the arms in.
static float skinnedSpanX(const Model& m, const Pose& p) {
  const Mesh* sm = nullptr; int skin = -1;
  for (auto& n : m.nodes) if (n.mesh >= 0 && m.meshes[(size_t)n.mesh].skinned) { sm = &m.meshes[(size_t)n.mesh]; skin = n.skin; break; }
  if (!sm || skin < 0 || skin >= (int)p.skinMats.size()) return -1.f;
  std::vector<Vertex> out(sm->vertices.size());
  skinMesh(*sm, p.skinMats[(size_t)skin], out.data());
  float lo = 1e9f, hi = -1e9f;
  for (auto& v : out) { lo = std::min(lo, v.pos[0]); hi = std::max(hi, v.pos[0]); }
  return hi - lo;
}
static void testLimbsNotTPose(ModelLibrary& lib) {
  SECTION("walk cycle, not the bind T-pose, in play and menus");
  const ModelAsset* hero = lib.get("tommyzilla");
  CHECK(hero != nullptr);
  if (!hero) return;
  const Model& m = hero->model;
  Pose bind; computePose(m, m.restLocals(), bind);
  const float bindSpan = skinnedSpanX(m, bind);
  std::printf("  tommyzilla bind spanX %.3f\n", bindSpan);
  CHECK(bindSpan > 2.f);
  auto g = std::make_unique<Game>();
  GameModels gm(lib);
  g->init(nullptr); g->save.tutorial.done = true; g->startMode(false); g->models = &gm;
  g->selectMonster(std::max(0, T.monsterIndex("tommyzilla")));
  InputState none;
  for (int i = 0; i < 20; i++) g->frame(1.f / 60, none);
  for (int i = 0; i < 6 && !g->menus.empty(); i++) { InputState c; c.pressed[A_CONFIRM] = true; g->frame(1.f / 60, c); }
  auto spanRange = [&](int frames) {
    float mn = 1e9f, mx = -1e9f; int seen = 0;
    for (int i = 0; i < frames; i++) {
      g->frame(1.f / 60, none);
      RenderList rl; ModelDrawList ml;
      g->render(rl, 1280, 720);
      gm.build(rl, ml);
      for (auto& d : ml.draws) if (d.model == &m && d.pose) { float s = skinnedSpanX(m, *d.pose); mn = std::min(mn, s); mx = std::max(mx, s); seen++; }
    }
    std::printf("  samples %d spanX %.3f..%.3f (bind %.3f)\n", seen, mn, mx, bindSpan);
    return std::make_tuple(seen, mn, mx);
  };
  auto stood = spanRange(90);  // standing still: walk cycle, not a frozen bind pose
  CHECK(std::get<0>(stood) > 0 && std::get<1>(stood) < bindSpan - 0.5f && std::get<2>(stood) - std::get<1>(stood) > 0.4f);
  g->openCharacterSelect(true);
  g->updateMenus(none, 1.f / 60);
  auto menu = spanRange(90);  // character menu: the cycle keeps playing
  CHECK(std::get<0>(menu) > 0 && std::get<1>(menu) < bindSpan - 0.5f && std::get<2>(menu) - std::get<1>(menu) > 0.4f);
  g->menus.clear();
}

int main() {
  testMath(); testInlineGltf(); testTextureDecode(); testFallbackAndManifest(); testGroundTextureFiles();
  ModelLibrary lib; std::string err;
  bool ok = lib.loadManifestFile(kRoot + "data/models.json", kRoot, err);
  if (!ok) std::printf("manifest error: %s\n", err.c_str());
  CHECK(ok);
  testManifestModels(lib); testSkinning(lib); testLooks(lib); testOptionalFbxCharacter(); testScene(lib); testGameModels(lib); testLimbsNotTPose(lib);
  std::printf("\n[library log]\n"); for (auto& l : lib.log()) std::printf("  %s\n", l.c_str());
  std::printf("\n%d passed, %d failed\n", gPass, gFail);
  return gFail ? 1 : 0;
}
