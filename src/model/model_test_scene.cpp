#include "model_test_scene.h"
#include <cstdio>
#include "../ui/ui_draw.h"
namespace tz {
namespace {
const float kClipSecs = 3.2f, kCamSecs = 8.0f;
struct CamPreset { const char* name; Vec3 target; float dist, height, yaw; bool orbit, followDino; };
const CamPreset kCams[ModelTestScene::kCamPresets] = {
    {"overview", Vec3(0, 7, -4), 86, 36, 0, true, false},
    {"Tommyzilla close-up", Vec3(0, 6, 0), 24, 7, 0.5f, false, true},
    {"people", Vec3(5, 1.2f, 12), 11, 3.2f, 0.15f, false, false},
    {"vehicles", Vec3(-2, 1.5f, 22), 24, 8, 0.35f, false, false},
    {"heroes + The Bulk", Vec3(-4, 9, -10), 72, 18, 0.05f, false, false}};
}

void ModelTestScene::init() {
  actors_.clear(); loaded_ = 0;
  auto add = [&](const std::string& id, Vec3 p, float yawDeg, bool cycle = false, int motion = 0, const std::string& variant = "", const std::string& label = "") {
    Actor a; a.id = id; a.variant = variant; a.label = label.empty() ? id : label; a.pos = p; a.yaw = yawDeg * PI / 180; a.cycle = cycle; a.motion = motion;
    a.asset = lib_.get(id);
    a.phase = (float)actors_.size() * 0.37f;
    actors_.push_back(std::move(a));
  };
  // heroes + boss
  add("tommyzilla", Vec3(0, 0, 0), 0, true, 1, "", "Tommyzilla (Dino)");
  add("mossback", Vec3(-24, 0, -8), 25, true, 0, "", "Mossback (Yeti)");
  add("sparkleraptor", Vec3(-42, 3, -4), 35, true, 0, "", "Sparkleraptor (Dragon Evolved)");
  add("mega_mech", Vec3(48, 0, -20), -40, true, 0, "", "Mega Mech (Mech Bee)");
  add("civilian_male_a", Vec3(-8, 0, 14), 10, true, 0, "", "Civilian");
  add("civilian_female_c", Vec3(-10.5f, 0, 14), -10, true, 0, "", "-");
  add("the_bulk", Vec3(30, 0, -16), -30, true, 0, "", "The Bulk (Blue Demon, purple)");
  // people
  add("cop", Vec3(0, 0, 12), 20, true, 0, "", "Cop (SWAT)");
  add("soldier", Vec3(3.5f, 0, 12), 0, true, 0, "", "Soldier");
  add("soldier", Vec3(7, 0, 12), 0, true, 0, "tier1", "Soldier (red)");
  add("soldier", Vec3(10.5f, 0, 12), 0, true, 0, "tier2", "Soldier (gold)");
  // vehicles
  add("police_car", Vec3(-30, 0, 22), 90, false, 2, "", "Police car");
  add("ambulance", Vec3(-4, 0, 22), 90, false, 0, "", "Ambulance");
  add("fire_truck", Vec3(8, 0, 22), 90, false, 0, "", "Fire truck (FBX)");
  add("tank", Vec3(24, 0, 10), -30, false, 0, "police", "Tank (police)");
  add("tank", Vec3(34, 0, 12), -40, false, 0, "army", "Tank (army)");
  add("police_heli", Vec3(-26, 14, 6), 0, false, 3, "", "Helicopter");
  // city
  add("shop_a", Vec3(-30, 0, -36), 0, false, 0, "", "Shop");
  add("tower_a", Vec3(-10, 0, -40), 0, false, 0, "", "Tower");
  add("house_a", Vec3(10, 0, -34), 0, false, 0, "", "House");
  add("factory_a", Vec3(32, 0, -36), 0, false, 0, "", "Factory");
  for (int i = -6; i <= 6; i++) { add("road_straight", Vec3(i * 8.f, 0.02f, 22), 90, false, 0, "", "-"); }
  add("street_light", Vec3(-16, 0, 27), 0, false, 0, "", "Street light");
  add("street_light", Vec3(16, 0, 27), 0, false, 0, "", "-");
  add("barrel", Vec3(18, 0, 14), 0, false, 0, "", "Barrel");
  add("barrel", Vec3(19.3f, 0, 14.6f), 0, false, 0, "", "-");
  add("barrier", Vec3(-18, 0, 16), 0, false, 0, "", "Barrier");
  add("container", Vec3(40, 0, -14), 30, false, 0, "", "Container");
  add("missing_model_demo", Vec3(44, 0, 2), 0, false, 0, "", "missing model -> primitive fallback");
  for (auto& a : actors_) {
    if (a.asset) loaded_++;
    if (a.asset) { a.anim.bind(&a.asset->model); if (a.cycle) startClip(a); else if (!a.asset->model.clips.empty()) a.anim.play(a.asset->clip("idle") >= 0 ? a.asset->clip("idle") : 0); }
  }
}

void ModelTestScene::startClip(Actor& a) {
  const char* logical = mdl::kLogicalClips[a.clipSlot % 5];
  int ci = a.asset->clip(logical);
  bool loop = a.clipSlot % 5 != 4;  // death plays once and holds
  a.anim.play(ci, loop, 0.35f, a.asset->desc.animSpeed);
  a.clipName = std::string(logical) + " -> " + (ci >= 0 ? a.asset->model.clips[(size_t)ci].name : "(none)");
}

void ModelTestScene::update(float dt) {
  t_ += dt; camYaw += camSpeed * dt;
  if (dt > 0) fps_ = fps_ <= 0 ? 1 / dt : lerpf(fps_, 1 / dt, 0.05f);
  for (auto& a : actors_) {
    if (a.cycle && a.asset) {
      a.clipT += dt;
      if (a.clipT > kClipSecs) { a.clipT = 0; a.clipSlot = (a.clipSlot + 1) % 5; startClip(a); }
    }
    if (a.motion == 1) {  // circle walker: moves only while walking / running
      int s = a.clipSlot % 5; float speed = s == 1 ? 3.5f : s == 2 ? 8.f : 0.f, R = 9;
      a.phase += speed / R * dt;
      a.pos = Vec3(std::cos(a.phase) * R, 0, std::sin(a.phase) * R);
      a.yaw = std::atan2(-std::sin(a.phase), std::cos(a.phase));
    } else if (a.motion == 2) {  // drive along the road and wrap
      a.pos.x += 9 * dt; if (a.pos.x > 48) a.pos.x = -48;
    } else if (a.motion == 3) {  // hover + slow turn
      a.yaw += 0.4f * dt; a.pos.y = 14 + std::sin(t_ * 1.3f) * 1.2f;
    }
    if (a.asset) a.anim.update(dt);
  }
}

void ModelTestScene::render(RenderList& rl, ModelDrawList& ml, int w, int h) {
  rl.clear(); ml.clear(); rl.width = w; rl.height = h;
  rl.sky = hexColor(0x9fd4ff); rl.fog = hexColor(0xbfe3ff); rl.fogNear = 140; rl.fogFar = 320;
  const int cam = camLock >= 0 ? camLock % kCamPresets : (int)(t_ / kCamSecs) % kCamPresets;
  const CamPreset& cp = kCams[cam];
  const float yaw = cp.orbit ? camYaw : cp.yaw + 0.25f * std::sin(t_ * 0.15f);
  Vec3 target = cp.target;
  if (cp.followDino && !actors_.empty()) target = actors_[0].pos + cp.target;
  Vec3 eye = target + Vec3(std::sin(yaw) * cp.dist, cp.height, std::cos(yaw) * cp.dist);
  rl.eye = eye;
  rl.view = Mat4::lookAtRH(eye, target, Vec3(0, 1, 0));
  rl.proj = Mat4::perspectiveRH(50 * PI / 180, (float)w / (float)std::max(1, h), 0.5f, 600);
  // ground + pavement
  rl.add(MESH_BOX, Mat4::scale(220, 0.2f, 220) * Mat4::translation(0, -0.1f, 0), hexColor(0x6fae5a));
  rl.add(MESH_BOX, Mat4::scale(110, 0.06f, 6) * Mat4::translation(0, 0.0f, 28.5f), hexColor(0xbdbdbd));
  rl.add(MESH_BOX, Mat4::scale(110, 0.06f, 30) * Mat4::translation(0, 0.0f, -37), hexColor(0xa9a9a9));
  for (auto& a : actors_) {
    Mat4 ent = Mat4::rotY(a.yaw) * Mat4::translation(a.pos.x, a.pos.y, a.pos.z);
    if (a.asset) {
      ml.draws.push_back(a.asset->makeDraw(ent, &a.anim.pose(), a.variant));
    } else {  // primitive fallback (what the game keeps doing when a model is missing)
      Color c = hexColor(0xff5fc8);
      rl.add(MESH_BOX, Mat4::scale(2.4f, 3.0f, 1.6f) * Mat4::translation(0, 1.5f, 0) * ent, c);
      rl.add(MESH_SPHERE, Mat4::scale(1.6f, 1.6f, 1.6f) * Mat4::translation(0, 3.8f, 0) * ent, c);
      rl.add(MESH_CONE, Mat4::scale(0.6f, 1.0f, 0.6f) * Mat4::translation(0, 5.0f, 0) * ent, hexColor(0xffe14d));
    }
  }
  // labels + HUD
  UIDraw ui(rl);
  for (auto& a : actors_) {
    if (a.label == "-") continue;
    if (cam != 0 && (a.pos - target).x * (a.pos - target).x + (a.pos - target).z * (a.pos - target).z > kCams[cam].dist * kCams[cam].dist) continue;  // declutter close-ups
    float top = a.asset ? (a.asset->model.boundsMax.y - a.asset->model.boundsMin.y) * a.asset->scale : 6;
    float sx, sy;
    if (!ui.project(a.pos + Vec3(0, top + 1.2f, 0), sx, sy)) continue;
    ui.textC(sx, sy - ui.lineH(0.8f), a.label, 0.8f, hexColor(0xffffff));
    if (a.cycle && a.asset) ui.textC(sx, sy, a.clipName, 0.65f, hexColor(0xffe680));
  }
  char b[160];
  std::snprintf(b, sizeof b, "MODEL TEST  -  %d / %zu actors from data/models.json (glTF/GLB + FBX)   camera: %s   %.0f fps", loaded_, actors_.size(), kCams[cam].name, wallFps >= 0 ? wallFps : fps_);
  ui.rect(0, 0, (float)w, ui.lineH(1.0f) * 2.2f, hexColor(0x000000, 0.45f));
  ui.text(12, 6, b, 1.0f, hexColor(0xffffff));
  ui.text(12, 6 + ui.lineH(1.0f), "Characters cycle idle / walk / run / attack / death with 0.35s crossfades. Esc quits.", 0.8f, hexColor(0xd0e8ff));
}
}  // namespace tz
