// World: stylized Victoria BC layout, destructible brick buildings, streaming chunks, ferry, ground.
#include <algorithm>
#include "game.h"
#include "world_layout.h"
namespace tz {
Mat4 partM(float x, float y, float z, float rx, float ry, float rz, float sx, float sy, float sz) {
  return Mat4::scale(sx, sy, sz) * Mat4::rotZ(rz) * Mat4::rotY(ry) * Mat4::rotX(rx) * Mat4::translation(x, y, z);
}

bool Building::intersects(float cx, float cy, float cz, float r) const {
  float x = clampf(cx, minX, maxX), y = clampf(cy, 0, maxY), z = clampf(cz, minZ, maxZ);
  return (x - cx) * (x - cx) + (y - cy) * (y - cy) + (z - cz) * (z - cz) < r * r;
}
bool World::isWater(float x, float z) const { for (auto& w : water) if (x > w.x0 && x < w.x1 && z > w.z0 && z < w.z1) return true; return false; }
int World::chunkIndex(float x, float z) const {
  float H = C.worldHalf, S = C.chunkSize;
  int cx = std::max(0, std::min(nch - 1, (int)std::floor((x + H) / S))), cz = std::max(0, std::min(nch - 1, (int)std::floor((z + H) / S)));
  return cx * nch + cz;
}
int World::activeChunkCount() const { int n = 0; for (auto& c : chunks) if (c.active) n++; return n; }
bool World::schoolLocked() const { return !G->P().schoolUnlocked; }
Building* World::landmark(const std::string& id) { for (auto& b : buildings) if (b.landmark == id) return &b; return nullptr; }

static void makeTree(Layout& L, World& w, float x, float z, float s, int chunk) {
  (void)w; (void)chunk;
  Body b; b.kind = BODY_TREE; b.scale = s; b.radius = 1.5f * s; b.mass = 1.5f;
  uint32_t leaf = L.pick({0x3f8f4a, 0x2f7a44, 0x4fa858, 0x2a6a3a});
  b.parts.push_back(gcyl(0x6b4a32, 0.45f, 0.6f, 4, M(0, 2, 0)));
  if (L.r.next() < 0.5f) { b.parts.push_back(gcone(leaf, 2.6f, 5, M(0, 5.5f, 0))); b.parts.push_back(gcone(leaf, 2.0f, 4, M(0, 8, 0))); }
  else { b.parts.push_back(gsph(leaf, 2.6f, M(0, 6, 0))); b.parts.push_back(gsph(leaf, 1.8f, M(1.2f, 7.5f, 0.5f))); }
  b.pos = Vec3(x, 0, z); b.ry = L.r.next() * 6.28f;
  b.homePos = b.pos; b.homeRy = b.ry;
  G->bodies.add(b);
}
static void makeRock(Layout& L, float x, float z, float s) {
  Body b; b.kind = BODY_ROCK; b.scale = s; b.radius = 1.3f * s; b.mass = 2;
  uint32_t c = L.pick({0x7a808a, 0x8a8f99, 0x6a6f78}); b.color = hexColor(c);
  b.parts.push_back(part(MESH_ROCK, b.color, Mat4::scale(2.8f, 2.8f, 2.8f)));
  b.pos = Vec3(x, 1.0f * s, z); b.rx = L.r.next() * 3; b.ry = L.r.next() * 3;
  b.homePos = b.pos; b.homeRx = b.rx; b.homeRy = b.ry;
  G->bodies.add(b);
}

// street props + parked vehicles (BODY_PROP): knocked flying / picked up and thrown like trees. Their parts are the
// primitive look; with 3D models on they draw as kPropIds models. Placed with their own RNG after everything else,
// so the building / tree / rock layout is unchanged.
static void makeProp(int type, float x, float z, float yaw) {
  Body b; b.kind = BODY_PROP; b.prop = type; b.scale = 1; b.pos = Vec3(x, 0, z); b.ry = yaw; b.homePos = b.pos; b.homeRy = yaw;
  auto car = [&](uint32_t body, uint32_t top, float len, float h) {
    b.radius = len * 0.45f; b.mass = 3; b.color = hexColor(body);
    b.parts = {gbox(body, 3.2f, h * 0.45f, len, M(0, h * 0.35f, 0)), gbox(top, 2.9f, h * 0.35f, len * 0.5f, M(0, h * 0.75f, -len * 0.08f))};
    for (int w = 0; w < 4; w++) b.parts.push_back(part(MESH_CYL, hexColor(0x12141a), Mat4::scale(1.1f, 0.5f, 1.1f) * Mat4::rotZ(PI / 2) * M(w % 2 ? 1.5f : -1.5f, 0.55f, w < 2 ? len * 0.32f : -len * 0.32f)));
  };
  switch (type) {
    case PROP_LAMP: b.radius = 0.7f; b.mass = 1; b.color = hexColor(0x3a3f4a);
      b.parts = {gcyl(0x3a3f4a, 0.12f, 0.16f, 5.6f, M(0, 2.8f, 0)), gbox(0x3a3f4a, 0.22f, 0.22f, 1.8f, M(0, 5.55f, 0.8f)), part(MESH_BOX, hexColor(0xffe9a0), Mat4::scale(0.45f, 0.18f, 0.7f) * M(0, 5.4f, 1.5f), 1)}; break;
    case PROP_BARREL: b.radius = 0.6f; b.mass = 1; b.color = hexColor(0xc0392b); b.parts = {gcyl(0xc0392b, 0.38f, 0.38f, 1.3f, M(0, 0.65f, 0)), gcyl(0xffd36b, 0.4f, 0.4f, 0.12f, M(0, 0.9f, 0))}; break;
    case PROP_BARRIER: b.radius = 1.0f; b.mass = 1; b.color = hexColor(0xf2f2f2); b.parts = {gbox(0xf2f2f2, 2.0f, 0.5f, 0.3f, M(0, 0.85f, 0)), gbox(0xe04a2a, 0.3f, 1.1f, 0.4f, M(-0.8f, 0.55f, 0)), gbox(0xe04a2a, 0.3f, 1.1f, 0.4f, M(0.8f, 0.55f, 0))}; break;
    case PROP_CONTAINER: b.radius = 1.6f; b.mass = 4; b.color = hexColor(0x3a7ac0); b.parts = {gbox(0x3a7ac0, 2.4f, 2.5f, 2.4f, M(0, 1.25f, 0))}; break;
    case PROP_AMBULANCE: car(0xf2f2f2, 0xe04a4a, 7.0f, 3.8f); break;
    case PROP_FIRETRUCK: car(0xd03030, 0xf2f2f2, 7.4f, 3.6f); b.mass = 5; break;
    case PROP_SEDAN: car(0x3a6ad0, 0x9ae8ff, 5.4f, 2.7f); break;
    case PROP_TAXI: car(0xffd23a, 0x2a2a33, 5.8f, 3.1f); break;
    default: car(0xe8e8e8, 0x9ae8ff, 5.8f, 2.8f); break;  // PROP_VAN
  }
  G->bodies.add(b);
}
static void addStreetProps(Layout& L) {
  Rng r(0x51EE7u);
  auto freeAt = [&](float x, float z, float m) { return !L.occupiedAt(x - m, z - m, x + m, z + m, 0.5f); };
  // street lights along the two avenues (z 100..110 and x -20..0) and the Old Town grid
  for (float x = -380; x <= 380; x += 34) for (float side : {98.6f, 111.4f}) if (freeAt(x, side, 0.6f)) makeProp(PROP_LAMP, x, side, side < 105 ? PI : 0);
  for (float z = -50; z <= 320; z += 34) for (float side : {-21.4f, 1.4f}) if (freeAt(side, z, 0.6f)) makeProp(PROP_LAMP, side, z, side < -10 ? -PI / 2 : PI / 2);
  for (int i = 0; i <= 6; i++) for (float z = -300; z <= -60; z += 40) { float x = -5.f + i * 40 + 5.6f; if (freeAt(x, z, 0.6f)) makeProp(PROP_LAMP, x, z, PI / 2); }
  // parked vehicles on the avenues (some emergency vehicles among them)
  const int kinds[] = {PROP_SEDAN, PROP_TAXI, PROP_VAN, PROP_SEDAN, PROP_AMBULANCE, PROP_FIRETRUCK, PROP_TAXI, PROP_SEDAN};
  for (int i = 0; i < 26; i++) {
    bool av = r.next() < 0.6f; float x, z, yaw;
    if (av) { x = r.range(-370, 370); z = r.next() < 0.5f ? 101.8f : 108.2f; yaw = z < 105 ? PI / 2 : -PI / 2; }
    else { x = r.next() < 0.5f ? -17.8f : -2.2f; z = r.range(-40, 290); yaw = x < -10 ? 0 : PI; }
    if (L.w->isWater(x, z) || !freeAt(x, z, 3.5f)) continue;
    makeProp(kinds[r.irange(0, 7)], x, z, yaw);
  }
  // waterfront: shipping containers + barrels on the Dallas Road promenade; barriers at a few Old Town corners
  for (int i = 0; i < 18; i++) { float x = r.range(-360, 360), z = r.range(304, 326); if (freeAt(x, z, 1.8f)) makeProp(r.next() < 0.5f ? PROP_CONTAINER : PROP_BARREL, x, z, r.next() * 6.28f); }
  for (int i = 0; i < 8; i++) { float x = -5.f + r.irange(0, 6) * 40, z = -62.f - r.irange(0, 6) * 40;
    for (int k = 0; k < 3; k++) { float bx = x + 6.5f + k * 2.1f, bz = z + 6.2f; if (freeAt(bx, bz, 1.0f)) makeProp(PROP_BARRIER, bx, bz, 0); } }
}

void World::init() {
  float H = C.worldHalf, S = C.chunkSize;
  nch = (int)std::ceil(2 * H / S); gn = (int)std::ceil(2 * H / cell);
  buildings.clear(); stoneBuildings.clear(); decoyBuildings.clear(); stonesFound = 0; chunks.clear(); labels.clear(); statics.clear(); ground.clear(); groundQuads.clear();
  water = {{-160, -20, -70, 40}, {-400, -160, -50, 25}, {-92, -55, -260, -70}, {-400, 400, 330, 400}, {150, 185, 200, 225}};
  for (int cx = 0; cx < nch; cx++) for (int cz = 0; cz < nch; cz++) { Chunk c; c.cx = cx; c.cz = cz; c.x0 = -H + cx * S; c.z0 = -H + cz * S; chunks.push_back(c); }
  buildLayout();
  hash.assign(gn * gn, {});
  for (auto& b : buildings) {
    b.chunk = chunkIndex(b.x, b.z); chunks[b.chunk].buildings.push_back(b.id);
    int a0 = std::max(0, (int)std::floor((b.minX + H) / cell)), a1 = std::min(gn - 1, (int)std::floor((b.maxX + H) / cell));
    int b0 = std::max(0, (int)std::floor((b.minZ + H) / cell)), b1 = std::min(gn - 1, (int)std::floor((b.maxZ + H) / cell));
    for (int a = a0; a <= a1; a++) for (int c = b0; c <= b1; c++) hash[a * gn + c].push_back(b.id);
  }
  lastChunk = -1;
}

void World::buildLayout() {
  Layout L; L.w = this;
  if (procedural) {  // Atlas: invented adventure city
    buildProceduralCity(L, *this, citySeed);
    for (auto& p : L.props) { if (p.type == 0) makeTree(L, *this, p.x, p.z, p.s, 0); else makeRock(L, p.x, p.z, p.s); }
    return;
  }
  const uint32_t COPPER = 0x5fae9a, STONE = 0xd9cfb6;
  float H = C.worldHalf, S = C.chunkSize, B = C.brick;
  L.reserve(-80, 92, -30, 160);  // kaiju spawn area behind the legislature
  // --- Parliament Buildings ---
  L.reserve(-100, 42, -20, 72);
  {
    Building& b = L.add("Parliament Buildings", -60, 80, 26, 8, 8, STONE, {0xffd36b, 0x3ad7ff, 0x5a4a3a}); b.landmark = "parliament";
    DecorDef d1{Vec3(-60, 16, 80), 7, {gcyl(STONE, 6, 6.5f, 6, M(0, 3, 0)), ghemi(COPPER, 6, M(0, 6, 0)), gcyl(STONE, 1.2f, 1.2f, 3, M(0, 12.5f, 0)), gcone(COPPER, 1.4f, 3, M(0, 15.5f, 0)), part(MESH_SPHERE, hexColor(0xffd36b), Mat4::scale(1.4f, 1.4f, 1.4f) * M(0, 17.5f, 0), 1)}};
    DecorDef d2{Vec3(-84, 16, 80), 3, {gbox(STONE, 5, 4, 5, M(0, 2, 0)), ghemi(COPPER, 2.7f, M(0, 4, 0))}};
    DecorDef d3{Vec3(-36, 16, 80), 3, {gbox(STONE, 5, 4, 5, M(0, 2, 0)), ghemi(COPPER, 2.7f, M(0, 4, 0))}};
    b.decor = {d1, d2, d3};
  }
  L.label("Parliament Buildings", -60, 40, 80, 0);
  // --- Royal BC Museum ---
  {
    Building& b = L.add("Royal BC Museum", 22, 84, 14, 9, 5, 0xa8a49a, {0x3ad7ff, 0x2a2f3a, 0x2a2f3a, 0xffd36b}); b.landmark = "museum";
    b.decor = {DecorDef{Vec3(22, 10, 84), 5, {gbox(0x6f6b62, 31, 1, 21, M(0, 0.5f, 0))}},
               DecorDef{Vec3(32, 10, 78), 2.5f, {gbox(0xd9cfb6, 4, 12, 4, M(0, 6, 0)), part(MESH_PYRAMID, hexColor(0x5fae9a), Mat4::scale(4.2f, 3, 4.2f) * M(0, 13.5f, 0))}}};
  }
  L.label("Royal BC Museum", 22, 30, 84, 0);
  // --- Empress Hotel ---
  L.reserve(-18, -40, 0, 10);
  {
    Building& b = L.add("Empress Hotel", 20, -15, 16, 10, 10, 0x9a5444, {0xffd36b, 0xffd36b, 0x3ad7ff, 0x2a2228}); b.landmark = "empress";
    b.decor = {DecorDef{Vec3(20, 20, -15), 6, {pyramidRoof(31, 19, 9, 0x3f6b5a), gbox(0x9a5444, 1.2f, 4, 1.2f, M(-6, 8, 0)), gbox(0x9a5444, 1.2f, 4, 1.2f, M(6, 8, 0))}},
               DecorDef{Vec3(5, 20, -24), 2.5f, {gcyl(0x9a5444, 2.2f, 2.2f, 4, M(0, 2, 0)), gcone(0x3f6b5a, 2.8f, 5, M(0, 6.5f, 0))}},
               DecorDef{Vec3(35, 20, -24), 2.5f, {gcyl(0x9a5444, 2.2f, 2.2f, 4, M(0, 2, 0)), gcone(0x3f6b5a, 2.8f, 5, M(0, 6.5f, 0))}}};
  }
  L.label("Empress Hotel", 20, 44, -15, 0);
  L.label("Inner Harbour", -90, 6, -15, 2);
  // --- Ferry terminal + Coho ferry ---
  {
    Building& b = L.add("Ferry Terminal", -130, 64, 10, 6, 4, 0xc8ccd4, {0x3ad7ff, 0x3ad7ff, 0x1a1d26}); b.landmark = "ferry";
    b.decor = {DecorDef{Vec3(-130, 8, 64), 4, {gbox(0x2b5d9c, 22, 0.8f, 14, M(0, 0.4f, 0)), gcyl(0xc8ccd4, 0.5f, 0.5f, 6, M(8, 3.4f, 0))}}};
  }
  L.reserve(-138, 40, -118, 56);
  statics.push_back({gbox(0x6b5a4a, 6, 1.2f, 26, M(-128, 0.6f, 34)), chunkIndex(-128, 34)});
  ferry.home = ferry.pos = Vec3(-110, -0.8f, 10);
  ferry.parts = {gbox(0xf2f2f2, 30, 3, 10, M(0, 1.5f, 0)), gbox(0x2b5d9c, 30.2f, 0.8f, 10.2f, M(0, 0.6f, 0)), gbox(0xffffff, 18, 3, 8, M(-2, 4.5f, 0)),
                 gbox(0x3ad7ff, 18.2f, 0.8f, 8.2f, M(-2, 5.2f, 0)), gbox(0xe8e8e8, 8, 2, 6, M(-3, 7, 0)), gcyl(0x2b5d9c, 1.2f, 1.4f, 4, M(-6, 9, 0)),
                 gcyl(0x12141a, 1.25f, 1.25f, 1, M(-6, 11.2f, 0)), part(MESH_CONE, hexColor(0xf2f2f2), Mat4::scale(10, 6, 10) * M(17, 1.5f, 0, 0, 0, -PI / 2, 1, 1, 1.9f))};
  L.label("Coho Ferry", -110, 16, 10, 1);
  L.label("Ferry Terminal", -130, 18, 64, 0);
  // --- Ruth King Elementary ---
  {
    Building& b = L.add("Ruth King Elementary", -280, 170, 14, 7, 3, 0xe8b04a, {0x3ad7ff, 0xffffff}); b.landmark = "school"; b.school = true;
    b.decor = {DecorDef{Vec3(-280, 6, 170), 4, {gbox(0x2b6f9c, 29, 0.8f, 15, M(0, 0.4f, 0)), gbox(0xffffff, 6, 3, 0.4f, M(0, 2.2f, -7.2f))}}};
    Building& g = L.add("Ruth King Gym", -252, 172, 7, 8, 4, 0x3a8fcf, {0xffffff, 0x3ad7ff}); g.school = true;
  }
  L.reserve(-330, 185, -225, 260);
  {
    float x = -275, z = 215; int ch = chunkIndex(x, z);
    std::vector<Part> pg = {gcyl(0xff5a5a, 0.3f, 0.3f, 7, M(-4, 3, 0, 0, 0, 0.3f)), gcyl(0xff5a5a, 0.3f, 0.3f, 7, M(-4, 3, 0, 0, 0, -0.3f)),
                            gcyl(0xff5a5a, 0.3f, 0.3f, 7, M(4, 3, 0, 0, 0, 0.3f)), gcyl(0xff5a5a, 0.3f, 0.3f, 7, M(4, 3, 0, 0, 0, -0.3f)),
                            gcyl(0xffd36b, 0.3f, 0.3f, 9, M(0, 6.3f, 0, 0, 0, PI / 2)), gbox(0x3ad7ff, 1.6f, 0.3f, 0.8f, M(-2, 2, 0)), gbox(0x3ad7ff, 1.6f, 0.3f, 0.8f, M(2, 2, 0)),
                            gbox(0xffd36b, 3, 0.4f, 9, M(14, 2.5f, 0, 0.5f, 0, 0)), gbox(0x3a8fcf, 3, 5, 3, M(14, 2.5f, -5)), gbox(0xe8d8a0, 8, 0.6f, 8, M(-16, 0.3f, 4))};
    for (auto& p : pg) { p.local = p.local * Mat4::translation(x, 0, z); statics.push_back({p, ch}); }
  }
  L.label("Ruth King Elementary", -270, 22, 170, 0);
  // --- Beacon Hill Park ---
  L.reserve(60, 120, 260, 300);
  for (int i = 0; i < 70; i++) {
    float x = L.rr(65, 255), z = L.rr(125, 295);
    if (isWater(x, z) || (std::fabs(x - 200) < 8 && std::fabs(z - 150) < 8)) continue;
    L.props.push_back({0, x, z, L.rr(0.8f, 1.3f)});
  }
  for (int i = 0; i < 12; i++) L.props.push_back({1, L.rr(70, 250), L.rr(130, 290), L.rr(0.8f, 1.6f)});
  {
    const uint32_t cs[] = {0xc0392b, 0xf2c94c, 0x2a1424, 0x3ad7ff, 0xc0392b, 0x2f7a44, 0xf2c94c};
    int ch = chunkIndex(200, 150);
    for (int i = 0; i < 7; i++) statics.push_back({gcyl(cs[i], 1.4f, 1.5f, 3, M(200, 1.5f + i * 3, 150)), ch});
    statics.push_back({gbox(0x2a1424, 8, 1, 1.5f, M(200, 19, 150)), ch});
  }
  L.label("Beacon Hill Park", 160, 14, 180, 0);
  L.label("Totem Pole", 200, 26, 150, 1);
  // --- Old Town grid ---
  for (int bx = 0; bx < 6; bx++) for (int bz = 0; bz < 6; bz++) {
    float cx = 20.f + bx * 40, cz = -82.f - bz * 40;
    if (bx == 1 && bz == 4) {
      Building& b = L.add("Chinatown Gate", cx, cz, 9, 2, 6, 0xc0392b); b.landmark = "chinatown"; b.windows = false; b.mask = 1;
      b.decor = {DecorDef{Vec3(cx, 12, cz), 4, {gbox(0x2f7a44, 22, 1, 6, M(0, 0.5f, 0)), gbox(0xf2c94c, 17, 1, 4, M(0, 1.5f, 0)), gbox(0xf2c94c, 6, 2, 0.4f, M(0, -1.6f, 2.3f))}}};
      L.reserve(cx - 12, cz - 12, cx + 12, cz + 12); L.label("Chinatown Gate", cx, 20, cz, 1); continue;
    }
    int n = L.ri(2, 4);
    float slots[4][2] = {{-7.5f, -7.5f}, {7.5f, -7.5f}, {-7.5f, 7.5f}, {7.5f, 7.5f}};
    int ns = n == 2 ? 2 : (n == 3 ? 3 : 4);
    for (int s = 0; s < ns; s++) {
      int w = L.ri(5, 7), d = L.ri(5, 7);
      Building& b = L.add("Old Town shop", cx + slots[s][0], cz + slots[s][1], w, d, L.ri(3, 8), L.pick({0xb5654a, 0xc98b5a, 0xd8c3a0, 0x8f4f3f, 0x6f8fa8, 0xc8ccd4, 0xa0745a}));
      b.oldtown = true;
    }
    if (L.r.next() < 0.5f) L.props.push_back({0, cx + L.rr(-17, 17), cz + 17, 0.8f});
  }
  L.label("Old Town", 120, 34, -170, 0);
  // --- downtown towers ---
  for (int i = 0; i < 40 && buildings.size() < 220; i++) {
    float x = L.rr(45, 170), z = L.rr(-50, 105); int w = L.ri(5, 7), d = L.ri(5, 7);
    if (L.occupiedAt(x - w, z - d, x + w, z + d, 5)) continue;
    L.add("Office tower", x, z, w, d, L.ri(8, 15), L.pick({0x5f86a8, 0x8fb2c8, 0xc8ccd4, 0x4a6680, 0x7a8a9a}), {0x3ad7ff, 0xffd36b, 0x1a1d26, 0x9ae8ff});
  }
  // --- residential neighbourhoods ---
  for (int cx = 0; cx < nch; cx++) for (int cz = 0; cz < nch; cz++) {
    float x0 = -H + cx * S, z0 = -H + cz * S;
    for (int t = 0; t < 6; t++) {
      int w = L.ri(4, 6), d = L.ri(4, 6), h = L.ri(2, 4); float x = L.rr(x0 + 10, x0 + S - 10), z = L.rr(z0 + 10, z0 + S - 10);
      if (L.occupiedAt(x - w, z - d, x + w, z + d, 6)) continue;
      Building& b = L.add("House", x, z, w, d, h, L.pick({0xf2c6a0, 0xa8d8c8, 0xf0e0a0, 0xd8a8c8, 0xc8ccd4, 0xb8c8f0, 0xf09a7a}), {0xffd36b, 0x3ad7ff, 0x2a2a33});
      b.decor = {DecorDef{Vec3(x, h * B, z), 3, {pyramidRoof(w * B + 1.5f, d * B + 1.5f, 4, L.pick({0x5a3a3a, 0x3a4a5a, 0x2a1424, 0x6a4a2a}))}}};
    }
    for (int t = 0; t < 4; t++) {
      float x = L.rr(x0 + 5, x0 + S - 5), z = L.rr(z0 + 5, z0 + S - 5);
      if (!L.occupiedAt(x - 2, z - 2, x + 2, z + 2, 2)) L.props.push_back({0, x, z, L.rr(0.8f, 1.2f)});
    }
  }
  for (int i = 0; i < 26; i++) L.props.push_back({1, L.rr(-380, 380), L.rr(318, 328), L.rr(1, 2)});
  for (int i = 0; i < 10; i++) { float x = L.rr(-155, -25), z = L.rr(43, 47); if (!L.occupiedAt(x - 1, z - 1, x + 1, z + 1, 0)) L.props.push_back({1, x, z, L.rr(0.7f, 1.3f)}); }
  L.label("Dallas Road Waterfront", 60, 6, 340, 2);
  L.label("Strait of Juan de Fuca", -250, 6, 370, 2);
  // props become physics bodies right away (they are cheap); they are only simulated/drawn in active chunks
  for (auto& p : L.props) { if (p.type == 0) makeTree(L, *this, p.x, p.z, p.s, 0); else makeRock(L, p.x, p.z, p.s); }
  addStreetProps(L);
  // --- ground ---
  // Each surface is kept twice: a flat-colour box (the fallback look) and a textured top quad (GroundQuad, see ground.h).
  // Streets carry a dashed centre line (lane 1 = along X, 2 = along Z); crossings get a plain asphalt patch on top.
  auto flat = [&](float x0, float z0, float x1, float z1, uint32_t c, float y, int surf, int lane = 0) {
    ground.push_back(gbox(c, x1 - x0, 0.04f, z1 - z0, M((x0 + x1) / 2, y, (z0 + z1) / 2)));
    GroundQuad q{x0, z0, x1, z1, y + 0.02f, hexColor(c), surf, lane, lane == 1 ? (z0 + z1) / 2 : lane == 2 ? (x0 + x1) / 2 : 0.f};
    groundQuads.push_back(q);
  };
  ground.push_back(gbox(0x3b3f4a, 2 * H + 600, 1, 2 * H + 600, M(0, -0.5f, 0)));
  groundQuads.push_back(GroundQuad{-(H + 300), -(H + 300), H + 300, H + 300, 0.f, hexColor(0x3b3f4a), GS_CONCRETE});
  flat(60, 120, 260, 300, 0x4f9a4a, 0.03f, GS_GRASS); flat(-100, 42, -20, 72, 0x4f9a4a, 0.03f, GS_GRASS); flat(-330, 185, -225, 260, 0x5fae5a, 0.03f, GS_GRASS);
  size_t street0 = groundQuads.size();
  for (int i = 0; i <= 6; i++) { flat(-5.f + i * 40 - 5, -310, -5.f + i * 40 + 5, -55, 0x2b2e36, 0.05f, GS_ASPHALT, 2); flat(-10, -62.f - i * 40 - 5, 245, -62.f - i * 40 + 5, 0x2b2e36, 0.05f, GS_ASPHALT, 1); }
  flat(-400, 300, 400, 330, 0x8a8478, 0.04f, GS_PAVING); flat(-20, -55, 0, 330, 0x2b2e36, 0.05f, GS_ASPHALT, 2); flat(-400, 100, 400, 110, 0x2b2e36, 0.05f, GS_ASPHALT, 1);
  { // intersections: no centre line through the crossing (a slightly raised plain patch covers the overlap)
    std::vector<GroundQuad> patches;
    for (size_t a = street0; a < groundQuads.size(); a++) for (size_t b = street0; b < groundQuads.size(); b++) {
      const GroundQuad &A = groundQuads[a], &B = groundQuads[b];
      if (A.surface != GS_ASPHALT || B.surface != GS_ASPHALT || A.lane != 1 || B.lane != 2) continue;
      float x0 = std::max(A.x0, B.x0), x1 = std::min(A.x1, B.x1), z0 = std::max(A.z0, B.z0), z1 = std::min(A.z1, B.z1);
      if (x1 - x0 > 0.5f && z1 - z0 > 0.5f) patches.push_back(GroundQuad{x0, z0, x1, z1, A.y + 0.01f, A.color, GS_ASPHALT});
    }
    groundQuads.insert(groundQuads.end(), patches.begin(), patches.end());
  }
  for (auto& w : water) flat(w.x0, w.z0, w.x1, w.z1, 0x1f5d8c, 0.12f, GS_WATER);
  // ocean beyond the map edge
  flat(-3 * H, H, 3 * H, 3 * H, 0x1f5d8c, 0.1f, GS_WATER); flat(-3 * H, -3 * H, 3 * H, -H, 0x1f5d8c, 0.1f, GS_WATER);
  flat(-3 * H, -H, -H, H, 0x1f5d8c, 0.1f, GS_WATER); flat(H, -H, 3 * H, H, 0x1f5d8c, 0.1f, GS_WATER);
}

void World::generate(Building& b) {
  if (b.generated) return;
  int w = b.w, d = b.d, h = b.h; float B = C.brick;
  Rng r(0xB00Cull + (uint64_t)b.id * 7919ull);
  b.grid.assign(w * h * d, -1); b.bricks.clear();
  for (int j = 0; j < h; j++) for (int k = 0; k < d; k++) for (int i = 0; i < w; i++) {
    bool shell = i == 0 || i == w - 1 || k == 0 || k == d - 1, top = j == h - 1;
    if (!shell && !top) continue;
    if (b.mask == 1 && !(i <= 1 || i >= 7 || j >= 4)) continue;  // Chinatown gate opening
    b.grid[(j * d + k) * w + i] = (int)b.bricks.size();
    Brick br; br.i = (uint8_t)i; br.j = (uint8_t)j; br.k = (uint8_t)k; br.interior = shell ? 0 : 1;
    br.x = b.minX + (i + 0.5f) * B; br.y = (j + 0.5f) * B; br.z = b.minZ + (k + 0.5f) * B;
    bool edgeI = i == 0 || i == w - 1, edgeK = k == 0 || k == d - 1, corner = edgeI && edgeK;
    bool isWin = !br.interior && !corner && j > 0 && j < h - 1 && (j % 2 == 1) && (((edgeI ? k : i) % 2) == 1) && b.windows;
    if (isWin) br.c = hexColor(b.windowCols[r.irange(0, (int)b.windowCols.size() - 1)]);
    else { float v = 0.86f + r.next() * 0.2f; br.c = scaleColor(b.color, j == h - 1 ? 0.8f * v : v); }
    b.bricks.push_back(br);
  }
  b.alive.assign(b.bricks.size(), 1); b.aliveCount = (int)b.bricks.size();
  b.generated = true;
}

void World::updateStreaming(float x, float z, bool force, const Vec3* second) {
  int k = chunkIndex(x, z), k2 = second ? chunkIndex(second->x, second->z) : k;
  int key = k * nch * nch + k2;
  if (key == lastChunk && !force) return;
  lastChunk = key;
  int kx = k / nch, kz = k % nch, kx2 = k2 / nch, kz2 = k2 % nch, R = (int)C.activeRadius;
  for (auto& ch : chunks) {
    bool on = (std::abs(ch.cx - kx) <= R && std::abs(ch.cz - kz) <= R) || (std::abs(ch.cx - kx2) <= R && std::abs(ch.cz - kz2) <= R);
    if (on == ch.active) continue;
    ch.active = on;
    if (on) {
      if (!ch.generated) { for (int id : ch.buildings) generate(buildings[id]); ch.generated = true; }
      G->people.spawnChunk(ch);
    }
  }
  if (!force) G->people.despawnInactive();
}

void World::detach(Building& b, int i, float vx, float vy, float vz) {
  if (!b.alive[i]) return;
  b.alive[i] = 0; b.aliveCount--;
  const Brick& br = b.bricks[i];
  G->debris->spawn(Vec3(br.x, br.y, br.z), Vec3(vx, vy, vz), br.c, C.brick * 0.9f);
}

int World::hitBuilding(Building& b, float cx, float cy, float cz, float r, float maxN, float ox, float oz, float impulse) {
  float r2 = r * r;
  std::vector<std::pair<float, int>> cand;
  for (int i = 0; i < (int)b.bricks.size(); i++) {
    if (!b.alive[i]) continue;
    const Brick& br = b.bricks[i]; float dx = br.x - cx, dy = br.y - cy, dz = br.z - cz, d2 = dx * dx + dy * dy + dz * dz;
    if (d2 < r2) cand.push_back({d2, i});
  }
  if (cand.empty()) return 0;
  std::sort(cand.begin(), cand.end());
  int n = std::min((int)cand.size(), std::max(1, (int)std::lround(maxN)));
  std::vector<int> cols;
  for (int q = 0; q < n; q++) {
    int i = cand[q].second; const Brick& br = b.bricks[i];
    float dx = br.x - ox, dz = br.z - oz, l = std::hypot(dx, dz); if (l < 1e-3f) l = 1; dx /= l; dz /= l;
    float f = impulse * (0.6f + frand() * 0.6f);
    detach(b, i, dx * f + (frand() - 0.5f) * 4, 3 + frand() * impulse * 0.5f, dz * f + (frand() - 0.5f) * 4);
    cols.push_back(br.i * 256 + br.k);
  }
  std::sort(cols.begin(), cols.end()); cols.erase(std::unique(cols.begin(), cols.end()), cols.end());
  int fell = 0;
  for (int key : cols) {  // bricks with nothing underneath fall down (column collapse)
    int i = key / 256, k = key % 256; bool gap = false;
    for (int j = 0; j < b.h; j++) {
      int id = b.grid[(j * b.d + k) * b.w + i]; if (id < 0) continue;
      if (!b.alive[id]) { gap = true; continue; }
      if (gap) { detach(b, id, (frand() - 0.5f) * 3, -1, (frand() - 0.5f) * 3); fell++; }
    }
  }
  if (b.health() < C.roofFallHealth)
    for (int i = 0; i < (int)b.bricks.size(); i++) if (b.alive[i] && b.bricks[i].interior) { detach(b, i, (frand() - 0.5f) * 2, -2, (frand() - 0.5f) * 2); fell++; }
  int total = n + fell;
  b.lastHit = G->time; b.rebuilding = false;
  G->playAt(SND_crumble, Vec3(b.x, 5, b.z), 0.8f, 0.9f + frand() * 0.2f);
  G->onBricks(total);
  afterDamage(b);
  return total;
}

void World::afterDamage(Building& b) {
  float h = b.health();
  if (!b.decorDown && h < C.decorFallHealth && !b.decor.empty()) {
    b.decorDown = true; b.decorBodies.clear();
    for (auto& d : b.decor) {
      Body body; body.kind = BODY_DECOR; body.parts = d.parts; body.pos = d.pos; body.radius = d.radius; body.mass = 2;
      body.homePos = d.pos; body.color = d.parts.empty() ? b.color : d.parts[0].color;
      int id = G->bodies.add(body);
      if (id < 0) continue;
      G->bodies.fling(id, (frand() - 0.5f) * 10, 4 + frand() * 4, (frand() - 0.5f) * 10, false);
      b.decorBodies.push_back(id);
    }
  }
  if (!b.landmark.empty() && !b.landmarkSmashed && h < C.landmarkSmashHealth) { b.landmarkSmashed = true; G->onLandmarkSmashed(b); }
  if (!b.smashed && h < C.smashedHealth) {
    b.smashed = true;
    if (frand() < C.collapseChance) collapse(b); else addRubble(b);
    G->onBuildingSmashed(b);
  }
}
void World::collapse(Building& b) {
  b.collapsed = true;
  for (int i = 0; i < (int)b.bricks.size(); i++) if (b.alive[i]) {
    const Brick& br = b.bricks[i]; float dx = br.x - b.x, dz = br.z - b.z, l = std::hypot(dx, dz); if (l < 1e-3f) l = 1;
    detach(b, i, dx / l * (2 + frand() * 5), frand() * 3, dz / l * (2 + frand() * 5));
  }
  G->playAt(SND_collapse, Vec3(b.x, 5, b.z));
  G->onCollapse(b);
}
void World::addRubble(Building& b) {
  if (!b.rubble.empty()) return;
  Color c = scaleColor(b.color, 0.75f);
  float W = b.maxX - b.minX, D = b.maxZ - b.minZ;
  for (int i = 0; i < 10; i++) {
    float along = frand(); int side = (int)(frand() * 4); float out = 1 + frand() * 3;
    float x = side < 2 ? b.minX + along * W : (side == 2 ? b.minX - out : b.maxX + out);
    float z = side >= 2 ? b.minZ + along * D : (side == 0 ? b.minZ - out : b.maxZ + out);
    float s = 1.5f + frand() * 2.5f;
    b.rubble.push_back(part(MESH_BOX, i % 3 ? c : hexColor(0x8a8f99), Mat4::scale(s * 1.6f, s, s * 1.3f) * M(x, s * 0.4f, z, 0, frand() * 3, frand() * 0.4f)));
  }
}
void World::finishRebuild(Building& b, bool announce) {
  b.rebuilding = false; b.smashed = false; b.landmarkSmashed = false; b.collapsed = false; b.rubble.clear();
  G->splats.fadeNear(b.x, b.z, C.splatFadeRadius);  // a rebuild clears nearby paint splats
  if (b.decorDown) { for (int id : b.decorBodies) G->bodies.remove(id); b.decorBodies.clear(); b.decorDown = false; }
  if (announce && chunks[b.chunk].active) G->toast(b.name + " has been rebuilt!");
}
void World::restoreAll() {
  for (auto& b : buildings) {
    if (!b.generated) continue;
    if (b.aliveCount >= (int)b.bricks.size() && !b.decorDown && b.rubble.empty()) continue;
    std::fill(b.alive.begin(), b.alive.end(), 1); b.aliveCount = (int)b.bricks.size(); b.lastHit = -1e9f;
    finishRebuild(b, false);
  }
  G->splats.fadeAll();
}
void World::update(float dt) {
  for (auto& b : buildings) {
    if (!b.generated || b.aliveCount >= (int)b.bricks.size()) continue;
    if (noRebuild || G->time - b.lastHit < C.rebuildDelay) continue;
    if (!b.rebuilding) { b.rebuilding = true; b.ptr = 0; b.acc = 0; }
    b.acc += dt;
    while (b.acc > C.rebuildBrickInterval && b.aliveCount < (int)b.bricks.size()) {
      b.acc -= C.rebuildBrickInterval;
      while (b.ptr < (int)b.bricks.size() && b.alive[b.ptr]) b.ptr++;
      if (b.ptr >= (int)b.bricks.size()) break;
      b.alive[b.ptr] = 1; b.aliveCount++;
      if (chunks[b.chunk].active && frand() < 0.25f) { const Brick& br = b.bricks[b.ptr]; G->fx->particle(Vec3(br.x, br.y, br.z), Vec3(0, 3, 0), 0.6f, 1.2f, hexColor(0x3ad7ff)); }
    }
    if (b.aliveCount >= (int)b.bricks.size()) finishRebuild(b, true);
  }
  // ferry bobs and drifts back home when pushed
  Ferry& f = ferry;
  f.pos += f.vel * dt; f.vel *= std::pow(0.4f, dt);
  f.vel.x += (f.home.x - f.pos.x) * 0.5f * dt; f.vel.z += (f.home.z - f.pos.z) * 0.5f * dt;
  f.yaw += f.spin * dt; f.spin *= std::pow(0.5f, dt); f.yaw *= std::pow(0.8f, dt);
  f.pos.y = f.home.y + std::sin(G->time * 1.3f) * 0.35f;
  schoolMsgT = std::max(0.f, schoolMsgT - dt);
}
HitResult World::damageSphere(float cx, float cy, float cz, float r, float maxN, float ox, float oz, float impulse) {
  HitResult res;
  buildingsNear(cx, cz, r, [&](Building& b) {
    if (!b.intersects(cx, cy, cz, r)) return;
    if (b.school && schoolLocked()) { res.blocked = true; G->schoolLockedHit(); return; }
    int n = hitBuilding(b, cx, cy, cz, r, maxN, ox, oz, impulse);
    if (n > 0) { res.bricks += n; res.buildings++; }
    else if (b.health() > 0.3f) res.blocked = true;
  });
  return res;
}
void World::resolveCircle(Vec3& p, float r) {
  buildingsNear(p.x, p.z, r + 2, [&](Building& b) {
    if (b.health() < C.blockKaijuHealth) return;
    float cx = clampf(p.x, b.minX, b.maxX), cz = clampf(p.z, b.minZ, b.maxZ), dx = p.x - cx, dz = p.z - cz, d = std::hypot(dx, dz);
    if (d < r) {
      if (d < 0.001f) {
        float l = p.x - b.minX, rg = b.maxX - p.x, t = p.z - b.minZ, bt = b.maxZ - p.z, m = std::min(std::min(l, rg), std::min(t, bt));
        if (m == l) p.x = b.minX - r; else if (m == rg) p.x = b.maxX + r; else if (m == t) p.z = b.minZ - r; else p.z = b.maxZ + r;
      } else { p.x = cx + dx / d * r; p.z = cz + dz / d * r; }
    }
  });
  float L = C.worldHalf - 5; p.x = clampf(p.x, -L, L); p.z = clampf(p.z, -L, L);
}
bool World::rayHit(const Vec3& o, const Vec3& d, float tmin, float tmax, float& tHit) {
  for (float t = std::max(0.f, tmin); t <= tmax; t += 1.0f) {
    Vec3 p = o + d * t;
    if (p.y <= 0) { tHit = d.y < -1e-4f ? std::max(tmin, -o.y / d.y) : t; return true; }
    bool hit = false;
    buildingsNear(p.x, p.z, 1, [&](Building& b) { if (!hit && b.health() > 0.05f && b.intersects(p.x, p.y, p.z, 0.3f)) hit = true; });
    if (hit) { tHit = t; return true; }
  }
  return false;
}
bool World::blockedForPerson(float x, float z) {
  float H = C.worldHalf;
  if (isWater(x, z) || std::fabs(x) > H - 4 || std::fabs(z) > H - 4) return true;
  bool hit = false;
  buildingsNear(x, z, 1, [&](Building& b) { if (!hit && b.health() > 0.3f && x > b.minX - 0.6f && x < b.maxX + 0.6f && z > b.minZ - 0.6f && z < b.maxZ + 0.6f) hit = true; });
  return hit;
}
void World::pushFerry(float x, float z, float r, float power) {
  float dx = ferry.pos.x - x, dz = ferry.pos.z - z, d = std::hypot(dx, dz);
  if (d < r + 14 && d > 0.01f) { ferry.vel.x += dx / d * power; ferry.vel.z += dz / d * power; ferry.spin += (frand() - 0.5f) * power * 0.05f; }
}
void World::render(RenderList& rl, const Frustum& fr, const Vec3& eye) {
  float maxD = C.fogFar + 40;
  if (G->groundTextured) rl.ground.insert(rl.ground.end(), groundQuads.begin(), groundQuads.end());  // textured (renderer has the textures)
  else for (auto& g : ground) rl.opaque[g.mesh].push_back(makeInstance(g.local, g.color));    // fallback: flat-colour boxes
  for (auto& s : statics) if (chunks[s.second].active) rl.opaque[s.first.mesh].push_back(makeInstance(s.first.local, s.first.color, s.first.unlit));
  { Mat4 fm = Mat4::rotZ(std::sin(G->time * 0.9f) * 0.03f) * Mat4::rotY(ferry.yaw) * Mat4::translation(ferry.pos.x, ferry.pos.y, ferry.pos.z);
    if (isActiveAt(ferry.pos.x, ferry.pos.z)) for (auto& p : ferry.parts) rl.opaque[p.mesh].push_back(makeInstance(p.local * fm, p.color)); }
  float bs = C.brick * 0.97f;
  for (auto& ch : chunks) {
    if (!ch.active) continue;
    for (int id : ch.buildings) {
      Building& b = buildings[id];
      Vec3 c(b.x, b.maxY * 0.5f, b.z);
      if (length(c - eye) > maxD + b.radius || !fr.sphere(c, b.radius + 8)) continue;
      if (G->buildingModel(rl, b)) continue;  // intact building drawn as a Kenney model (bricks take over once it is hit)
      auto& out = rl.opaque[MESH_BOX];
      for (size_t i = 0; i < b.bricks.size(); i++) if (b.alive[i]) { const Brick& br = b.bricks[i]; out.push_back(makeInstance(brickMat(br.x, br.y, br.z, bs), br.c)); }
      if (!b.decorDown) for (auto& d : b.decor) { Mat4 T = Mat4::translation(d.pos.x, d.pos.y, d.pos.z); for (auto& p : d.parts) rl.opaque[p.mesh].push_back(makeInstance(p.local * T, p.color, p.unlit)); }
      for (auto& p : b.rubble) rl.opaque[p.mesh].push_back(makeInstance(p.local, p.color));
    }
  }
}
}  // namespace tz
