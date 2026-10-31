// Loaded 3D models in gameplay: picks the data/models.json id for each entity, maps game state to a logical clip
// (idle / walk / run / attack / death / hit / eat / stun / jump / cheer) and emits a ModelReq. Every function returns
// false when models are off or the entity has no model, and the caller then draws its primitive rig unchanged.
#include <cstdio>
#include "game.h"
namespace tz {
const char* const kModelClipNames[MC_COUNT] = {"idle", "walk", "run", "attack", "death", "hit", "eat", "stun", "jump", "cheer"};
const char* const kPropIds[PROP_COUNT] = {"street_light", "barrel", "barrier", "container", "ambulance", "fire_truck", "car_sedan", "car_taxi", "car_van"};

namespace {
const char* const kCivilians[12] = {"civilian_male_a", "civilian_female_a", "civilian_male_b", "civilian_female_b", "civilian_male_c", "civilian_female_c",
                                    "civilian_male_d", "civilian_female_d", "civilian_male_e", "civilian_female_e", "civilian_male_f", "civilian_female_f"};
const char* const kHouses[] = {"house_a", "house_b", "house_c", "house_d", "house_e", "house_f", "house_g", "house_h"};
const char* const kShops[] = {"shop_a", "shop_b", "shop_c", "shop_d", "shop_e", "shop_f", "shop_g", "shop_h"};
const char* const kOffices[] = {"office_a", "office_b", "office_c", "office_d", "office_e", "office_f"};
const char* const kTowers[] = {"tower_a", "tower_b", "tower_c", "tower_d", "tower_e"};
const char* const kFactories[] = {"factory_a", "factory_b", "factory_c", "factory_d"};
uint32_t hashId(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
}  // namespace

int Game::modelAsset(const std::string& id) {
  if (!modelsOn()) return -1;
  if (modelCache.owner != models) { modelCache = ModelCache(); modelCache.owner = models; }
  auto it = modelCache.ids.find(id);
  if (it != modelCache.ids.end()) return it->second;
  int a = models->find(id);
  modelCache.ids[id] = a;
  return a;
}
int Game::modelVariant(int asset, const std::string& name) {
  if (asset < 0 || name.empty() || !modelsOn()) return 0;
  auto key = std::make_pair(asset, name);
  auto it = modelCache.variants.find(key);
  if (it != modelCache.variants.end()) return it->second;
  int v = models->variant(asset, name);
  modelCache.variants[key] = v;
  return v;
}

bool Game::monsterModel(RenderList& rl, const MonsterDef& m, const Vec3& pos, float yaw, float scale, ModelReq r) {
  if (m.genSeed) return false;  // Rex: procedurally generated kaiju draw with the primitive rig (drawMonster)
  int a = modelAsset(m.id);
  if (a < 0) return false;
  r.asset = a;
  r.world = Mat4::scale(scale, scale, scale) * Mat4::rotY(yaw) * Mat4::translation(pos.x, pos.y, pos.z);
  if (hasLook(m.id) && r.tint.r > 0.5f) {  // rolled look (m is already recoloured by lookedDef); locked silhouettes stay dark
    KaijuLook l = lookOf(m.id);
    r.palette = true; r.primary = m.body; r.secondary = m.belly; r.accent = m.accent; r.eyes = l.eyes;
    r.finish = (uint8_t)l.finish; r.finishT = realTime;
  }
  rl.models.push_back(r);
  return true;
}

// ---------------------------------------------------------------- the player's kaiju
bool Game::kaijuModel(RenderList& rl, float time) {
  (void)time;
  if (!modelsOn()) return false;
  const Kaiju& k = kaiju; const MonsterDef& d = k.def();
  ModelReq r; r.key = modelKey(MK_KAIJU, (uint32_t)k.player);  // P1 / P2 animate independently
  const MoveDef* m = k.at.move >= 0 ? &T.moves[k.at.move] : nullptr;
  float walk = C.kaijuSpeed * d.speed;
  if (k.hp <= 0) { r.clip = MC_DEATH; r.loop = false; }
  else if (!k.eats.empty()) { r.clip = MC_EAT; r.loop = false; r.trigger = (uint16_t)k.attackSeq; r.fit = std::max(0.35f, C.eatTweenTime + 0.15f); }
  else if (m) {
    bool roar = m->shape == MoveShape::Roar || m->shape == MoveShape::Nova;
    bool stomp = m->id == "stomp" || m->shape == MoveShape::Quake || m->shape == MoveShape::Meteor;
    r.clip = roar ? MC_CHEER : stomp ? MC_JUMP : MC_ATTACK; r.loop = false; r.trigger = k.attackSeq; r.fit = std::max(0.25f, k.at.dur);
  } else if (k.hurtT > 0) { r.clip = MC_HIT; r.loop = false; r.trigger = k.hitSeq; }
  else if (k.speedNow > 1.5f) {
    bool run = k.speedNow > walk * 1.25f;
    r.clip = run ? MC_RUN : MC_WALK;
    r.speed = clampf(k.speedNow / (run ? walk * C.kaijuSprint : walk), 0.6f, 1.6f);
  } else r.clip = MC_IDLE;
  if (k.charging) r.glow = 0.25f + k.charge * 0.5f;
  else if (m && (m->kind == MoveKind::Super || m->shape == MoveShape::Beam)) r.glow = 0.3f;
  if (k.hurtT > 0) { r.mix = hexColor(0xff4040); r.mixK = 0.3f * clampf((k.hurtT - 0.15f) / 0.15f, 0, 1); }  // brief hit flash
  if (k.spawnGlow > 0) r.glow = std::max(r.glow, k.spawnGlow);
  float pop = 1 + 0.25f * std::sin(std::min(1.f, k.popT / 0.6f) * PI);
  return monsterModel(rl, lookedDef(k.monster), Vec3(k.pos.x, (k.inWater ? -2.f : 0.f) + k.lift, k.pos.z), k.facing + k.twist, k.scale() * k.shrinkK * pop, r);
}

// ---------------------------------------------------------------- THE BULK
bool Game::bossModel(RenderList& rl, const Boss& b) {
  int a = modelAsset("the_bulk");
  if (a < 0) return false;
  ModelReq r; r.asset = a; r.key = modelKey(MK_BOSS, 0);
  if (b.dead) { r.clip = MC_DEATH; r.loop = false; }
  else if (b.intro()) r.clip = MC_CHEER;
  else if (b.attack != 0) { r.clip = MC_ATTACK; r.loop = false; r.trigger = b.attackSeq; r.fit = 1.2f; }
  else { r.clip = MC_WALK; r.speed = 0.8f; }
  if (b.pulse > 0.15f) { r.mix = hexColor(0xffffff); r.mixK = 0.5f; }  // hit flash (he grows with each hit)
  float s = b.scale * (1 + b.pulse * 0.25f);
  r.world = Mat4::scale(s, s, s) * Mat4::rotY(b.facing) * Mat4::translation(b.pos.x, b.lift, b.pos.z);
  rl.models.push_back(r);
  return true;
}

// ---------------------------------------------------------------- police + military
bool Game::unitModel(RenderList& rl, const Unit& u, const Mat4& root) {
  if (!modelsOn()) return false;
  if (u.type == U_MBOSS && u.boss >= 3) return false;  // Titan: Thunder Moth is drawn from primitives
  const char* id = nullptr; std::string var; float extra = 1;
  switch (u.type) {
    case U_COP: id = "cop"; break;
    case U_SOLDIER: id = "soldier"; if (u.tier > 0) var = u.tier >= 2 ? "tier2" : "tier1"; break;
    case U_CAR: id = "police_car"; break;
    case U_JEEP: id = "car_suv"; var = "army"; break;
    case U_PHELI: id = "police_heli"; break;
    case U_AHELI: id = "police_heli"; var = "army"; break;
    case U_PTANK: id = "tank"; var = "police"; break;
    case U_ATANK: id = "tank"; var = "army"; break;
    case U_MBOSS: if (u.boss == 2) id = "mega_mech"; else { id = "tank"; var = "super"; extra = 2.2f; } break;
    case U_RIOT: id = "cop"; break;                                   // SWAT + riot shield (unitExtras)
    case U_FLAME: id = "soldier"; var = "tier1"; break;               // Soldier + flamethrower prop (unitExtras)
    case U_ARMOR: id = "tank"; var = "army"; extra = 1.25f; break;   // armoured: chrome metal finish
    default: return false;  // jets keep their primitive
  }
  int a = modelAsset(id);
  if (a < 0) return false;
  ModelReq r; r.asset = a; r.variant = modelVariant(a, var);
  r.world = Mat4::scale(extra, extra, extra) * root;
  if (u.type != U_MBOSS && u.type != U_SOLDIER && u.tier > 0) { r.mix = hexColor(tierTintHex(u.tier)); r.mixK = tierTintAmount(u.tier); }  // tier paint
  bool animated = u.infantry() || (u.type == U_MBOSS && u.boss == 2);
  if (animated) {
    r.key = modelKey(MK_UNIT, u.uid);
    if (u.wreck) { r.clip = MC_DEATH; r.loop = false; }
    else if (time - u.fireT < 0.5f) { r.clip = MC_ATTACK; r.loop = false; r.trigger = (uint16_t)(int)(u.fireT * 8); }
    else if (u.slowT > 0) { r.clip = MC_STUN; r.speed = 0.5f; }
    else if (u.moving) r.clip = u.type == U_MBOSS ? MC_WALK : MC_RUN;
    else r.clip = MC_IDLE;
  } else if (u.wreck) r.tint = Color{0.38f, 0.36f, 0.36f, 1};  // scorched wreck
  if (u.type == U_ARMOR) { r.finish = FIN_CHROME; r.finishT = time; r.mixK = 0; }
  rl.models.push_back(r);
  unitExtras(rl, u, root);
  if (u.type == U_PHELI && !u.wreck) rl.alpha[MESH_CONE].push_back(makeInstance(Mat4::scale(10, 26, 10) * Mat4::translation(0, -14, 0) * root, Color{1, 0.96f, 0.75f, 0.12f}, 1));  // searchlight
  return true;
}

// ---------------------------------------------------------------- civilians (crowd: shared poses, LOD to stick figures)
bool Game::personModel(RenderList& rl, const Person& p, int index, const Vec3& eye) {
  if (!modelsOn()) return false;
  float dx = p.x - eye.x, dz = p.z - eye.z;
  if (dx * dx + dz * dz > C.modelPeopleDist * C.modelPeopleDist) return false;
  int a = -1;
  for (int k = 0; k < 12 && a < 0; k++) a = modelAsset(kCivilians[(p.look * 5 + index + k) % 12]);
  if (a < 0) return false;
  ModelReq r; r.asset = a; r.key = modelKey(MK_PERSON, (uint32_t)index); r.shared = true; r.phase = (index % 3) / 3.f;
  Mat4 root = Mat4::rotY(p.dir) * Mat4::translation(p.x, p.y, p.z);
  switch (p.state) {
    case P_TUMBLE: root = Mat4::rotX(-p.tilt) * Mat4::rotY(p.dir) * Mat4::translation(p.x, p.y, p.z); r.clip = MC_HIT; break;
    case P_DIZZY: r.clip = MC_HIT; break;
    case P_STUN: root = Mat4::rotY(p.dir) * Mat4::translation(p.x, 0, p.z); r.clip = MC_STUN; r.loop = false; break;
    case P_FLEE: r.clip = MC_RUN; r.speed = clampf(p.vx / std::max(0.1f, C.runSpeed), 0.7f, 1.4f); break;
    default:
      if (p.idle || p.vx < 0.2f) r.clip = MC_IDLE;
      else { r.clip = MC_WALK; r.speed = clampf(p.vx / std::max(0.1f, C.walkSpeed), 0.6f, 1.6f); }
  }
  r.world = root;
  rl.models.push_back(r);
  return true;
}

// ---------------------------------------------------------------- buildings: Kenney city kits, fitted to the brick footprint
bool Game::buildingModel(RenderList& rl, const Building& b) {
  if (!modelsOn() || C.modelBuildings < 0.5f || !b.landmark.empty() || b.school) return false;
  if (C.modelBuildingDist > 0 && lastEye.x > -1e29f && dist2D(b.x, b.z, lastEye.x, lastEye.z) > C.modelBuildingDist) return false;  // far: bricks
  bool intact = !b.rebuilding && !b.collapsed && !b.smashed && !b.decorDown && b.rubble.empty() && (!b.generated || b.aliveCount == (int)b.bricks.size());
  if (!intact) return false;  // first hit: the destructible bricks take over (and come back as the model once rebuilt)
  modelAsset("");             // sync the cache with the current catalog
  if (modelCache.buildings.size() != world.buildings.size()) modelCache.buildings.assign(world.buildings.size(), -2);
  if (b.id < 0 || b.id >= (int)modelCache.buildings.size()) return false;
  int& a = modelCache.buildings[(size_t)b.id];
  uint32_t h = hashId((uint32_t)b.id * 2654435761u + 17);
  if (a == -2) {
    const char* const* pool = nullptr; int n = 0;
    if (b.name == "House") { if (h % 9 == 0) { pool = kFactories; n = 4; } else { pool = kHouses; n = 8; } }
    else if (b.name == "Old Town shop") { pool = kShops; n = 8; }
    else if (b.name == "Office tower") { if (b.h >= 11) { pool = kTowers; n = 5; } else { pool = kOffices; n = 6; } }
    a = -1;
    for (int k = 0; k < n && a < 0; k++) a = modelAsset(pool[(h / 7 + k) % (uint32_t)n]);
  }
  if (a < 0) return false;
  Vec3 lo, hi;
  if (!models->bounds(a, lo, hi)) return false;
  int rot = (int)((h >> 20) & 3);
  float W = b.maxX - b.minX, D = b.maxZ - b.minZ, H = b.maxY + (b.name == "House" ? 3.f : 0.f);  // houses: + their pyramid roof
  float ex = std::max(1e-3f, hi.x - lo.x), ey = std::max(1e-3f, hi.y - lo.y), ez = std::max(1e-3f, hi.z - lo.z);
  float sx = ((rot & 1) ? D : W) / ex, sz = ((rot & 1) ? W : D) / ez, sy = H / ey;
  ModelReq r; r.asset = a;
  r.world = Mat4::translation(-(lo.x + hi.x) * 0.5f, -lo.y, -(lo.z + hi.z) * 0.5f) * Mat4::scale(sx, sy, sz) * Mat4::rotY(rot * PI * 0.5f) * Mat4::translation(b.x, 0, b.z);
  r.mix = b.color; r.mixK = 0.12f;
  rl.models.push_back(r);
  return true;
}

// ---------------------------------------------------------------- props: trees, street props, parked vehicles
bool Game::bodyModel(RenderList& rl, const Body& b, int index, const Mat4& W) {
  if (!modelsOn()) return false;
  const char* id = nullptr;
  if (b.kind == BODY_TREE) id = index % 3 == 0 ? "tree_small" : "tree_large";
  else if (b.kind == BODY_PROP && b.prop >= 0 && b.prop < PROP_COUNT) id = kPropIds[b.prop];
  if (!id) return false;
  int a = modelAsset(id);
  if (a < 0) return false;
  ModelReq r; r.asset = a; r.world = W;
  rl.models.push_back(r);
  return true;
}
}  // namespace tz
