// Atlas: procedural adventure-mode city ("Zillaport"). An invented town with made-up landmarks, five districts with
// their own architecture, huge downtown skyscrapers, a river with bridges, a lake and a bay. Deterministic per seed.
// Sandbox mode keeps the Victoria map (world.cpp). Landmark ids (parliament, museum, empress, ferry, school) are kept
// so level objectives and the school unlock work unchanged; only the names/looks are invented.
#include <algorithm>
#include "world_layout.h"
namespace tz {
namespace {
const float kRoadPitch = 50, kRoadHalf = 4;
const float kBridgeX[] = {-300, 50, 250};
bool nearRoadLine(float v) { float m = std::fmod(std::fabs(v), kRoadPitch); return m < kRoadHalf + 1 || m > kRoadPitch - kRoadHalf - 1; }
enum District { D_DOWNTOWN, D_OLDQUARTER, D_PASTEL, D_DOCKS, D_SHORE };
District districtAt(float x, float z) {
  if (z < -185) return D_OLDQUARTER;
  if (z > 100) return x < -150 ? D_PASTEL : D_SHORE;
  if (x < -150) return D_PASTEL;
  if (x > 150) return D_DOCKS;
  return D_DOWNTOWN;
}
}  // namespace

void buildProceduralCity(Layout& L, World& w, uint64_t seed) {
  L.r = Rng(seed);
  const float H = C.worldHalf, B = C.brick;
  // --- water: river (split at bridges so bridges are dry land), harbour inlet, lake, bay ---
  w.water.clear(); w.stoneBuildings.clear(); w.decoyBuildings.clear();
  { float x0 = -H - 50; for (float bx : kBridgeX) { w.water.push_back({x0, bx - 6, -185, -160}); x0 = bx + 6; } w.water.push_back({x0, H + 50, -185, -160}); }
  w.water.push_back({-205, -145, -160, -60});   // Bubblewharf harbour
  w.water.push_back({-340, -220, 195, 290});    // Puddlemoon Lake
  w.water.push_back({-H - 50, H + 50, 345, H + 50});  // Sparkle Bay
  w.ferry.home = w.ferry.pos = Vec3(-175, -0.8f, -105);
  // --- keep level start spots, the kaiju spawn and the boss spot clear ---
  const float clear[][2] = {{-20, 20}, {-20, 40}, {-60, 140}, {-55, 135}, {0, 150}, {110, -40}, {120, -40}, {150, 200}, {160, 210}, {160, 265}, {40, 140}, {60, -40}, {160, 195}};
  for (auto& c : clear) { float r = c[1] == -40 ? 12.f : 22.f; L.reserve(c[0] - r, c[1] - r, c[0] + r, c[1] + r); }  // L13 arena (110,-40) needs buildings close by
  // --- invented landmarks ---
  {
    Building& b = L.add("Crown Hall", -75, -25, 13, 8, 9, 0xe9dcc0, {0xffd36b, 0x3ad7ff}); b.landmark = "parliament";
    b.decor = {DecorDef{Vec3(-75, 18, -25), 6, {gcyl(0xe9dcc0, 5, 5.5f, 5, M(0, 2.5f, 0)), ghemi(0xd65db1, 5, M(0, 5, 0)), part(MESH_SPHERE, hexColor(0xffd36b), Mat4::scale(1.6f, 1.6f, 1.6f) * M(0, 10.5f, 0), 1)}}};
    L.label("Crown Hall", -75, 34, -25, 0);
  }
  {
    Building& b = L.add("Dino Bone Museum", 75, 25, 14, 9, 5, 0xc9b28a, {0x3ad7ff, 0x2a2f3a}); b.landmark = "museum";
    b.decor = {DecorDef{Vec3(75, 10, 25), 5, {gsph(0xf2efe6, 2.2f, M(-6, 2, 0)), gbox(0xf2efe6, 10, 1, 1.2f, M(0, 1.5f, 0)), gbox(0xf2efe6, 1, 5, 1, M(6, 3, 0)), gsph(0xf2efe6, 1.5f, M(6, 6, 0))}}};
    L.label("Dino Bone Museum", 75, 24, 25, 0);
  }
  {
    Building& b = L.add("Marmalade Grand Hotel", 125, 75, 12, 10, 14, 0xe08a3c, {0xffd36b, 0x3ad7ff, 0x2a2228}); b.landmark = "empress";
    b.decor = {DecorDef{Vec3(125, 28, 75), 6, {pyramidRoof(25, 21, 8, 0x8a3a2a), gcone(0xffd36b, 1.4f, 5, M(0, 9, 0))}}};
    L.label("Marmalade Grand Hotel", 125, 46, 75, 0);
  }
  {
    Building& b = L.add("Bubblewharf Ferry Terminal", -125, -110, 8, 10, 4, 0xbfe6f0, {0x3ad7ff, 0x1a1d26}); b.landmark = "ferry";
    b.decor = {DecorDef{Vec3(-125, 8, -110), 4, {gbox(0x2b5d9c, 18, 0.8f, 22, M(0, 0.4f, 0))}}};
    L.label("Bubblewharf Ferry", -125, 18, -110, 0);
  }
  {
    Building& b = L.add("Sunnyside Elementary", -275, 125, 14, 7, 3, 0xe8b04a, {0x3ad7ff, 0xffffff}); b.landmark = "school"; b.school = true;
    b.decor = {DecorDef{Vec3(-275, 6, 125), 4, {gbox(0x2b6f9c, 29, 0.8f, 15, M(0, 0.4f, 0))}}};
    Building& g = L.add("Sunnyside Gym", -247, 127, 7, 8, 4, 0x3a8fcf, {0xffffff, 0x3ad7ff}); g.school = true;
    L.label("Sunnyside Elementary", -265, 22, 125, 0);
  }
  { // the huge skyscrapers
    Building& s = L.add("Zillatron Spire", 25, -75, 9, 9, 70, 0x6fb7d8, {0x9ae8ff, 0x3ad7ff, 0x1a1d26});
    s.decor = {DecorDef{Vec3(25, 140, -75), 4, {gcyl(0xc8ccd4, 0.6f, 0.6f, 18, M(0, 9, 0)), part(MESH_SPHERE, hexColor(0xff5a5a), Mat4::scale(1.5f, 1.5f, 1.5f) * M(0, 18.5f, 0), 1)}}};
    L.label("Zillatron Spire", 25, 165, -75, 0);
    L.add("Noodle Tower East", -25, -125, 8, 8, 55, 0xf2c94c, {0x3ad7ff, 0x1a1d26});
    L.add("Noodle Tower West", -75, -125, 8, 8, 55, 0xf2c94c, {0x3ad7ff, 0x1a1d26});
    L.label("Twin Noodle Towers", -50, 120, -125, 0);
    Building& p = L.add("Pickle Plaza Tower", 125, -125, 9, 7, 62, 0x5fae5a, {0x9ae8ff, 0x1a1d26});
    p.decor = {DecorDef{Vec3(125, 124, -125), 4, {part(MESH_PYRAMID, hexColor(0x2f7a44), Mat4::scale(19, 10, 15) * M(0, 5, 0))}}};
    L.label("Pickle Plaza Tower", 125, 140, -125, 0);
  }
  // --- parks ---
  struct Park { float x0, z0, x1, z1; const char* name; };
  const Park parks[] = {{-96, 104, -4, 146, "Wobblewood Park"}, {154, 204, 196, 296, "Rainbow Hill"}, {-346, -46, -254, -4, "Gumdrop Gardens"}};
  for (auto& p : parks) {
    L.reserve(p.x0, p.z0, p.x1, p.z1);
    for (int i = 0; i < 14; i++) L.props.push_back({0, L.rr(p.x0 + 3, p.x1 - 3), L.rr(p.z0 + 3, p.z1 - 3), L.rr(0.8f, 1.3f)});
    L.label(p.name, (p.x0 + p.x1) / 2, 12, (p.z0 + p.z1) / 2, 1);
  }
  // --- district fill: one block = the square between streets ---
  for (float cx = -H + kRoadPitch / 2; cx < H; cx += kRoadPitch) for (float cz = -H + kRoadPitch / 2; cz < H; cz += kRoadPitch) {
    District d = districtAt(cx, cz);
    bool arena = std::hypot(cx - 110, cz + 40) < 80;  // L13 "flatten the district" arena: low, smashable market stalls only
    int tries = arena ? 10 : d == D_DOWNTOWN ? 4 : (d == D_DOCKS ? 2 : 5);
    for (int t = 0; t < tries; t++) {
      float x = cx + L.rr(-12, 12), z = cz + L.rr(-12, 12);
      int bw, bd, bh; uint32_t col; std::vector<uint32_t> wins; const char* name; bool roof = false, crown = false, chimney = false;
      switch (d) {
        case D_DOWNTOWN: bw = L.ri(5, 8); bd = L.ri(5, 8); bh = L.ri(16, 45); name = "Skyscraper";
          col = L.pick({0x5f86a8, 0x8fb2c8, 0x4a6680, 0x7fd0e0, 0xb0a0e0, 0x3a4a6a}); wins = {0x9ae8ff, 0x3ad7ff, 0xffd36b, 0x1a1d26}; break;
        case D_OLDQUARTER: bw = L.ri(4, 6); bd = L.ri(4, 6); bh = L.ri(3, 7); name = "Cobblestone shop";
          col = L.pick({0xb5654a, 0x8f4f3f, 0xc98b5a, 0xa0745a, 0x7a3a2a}); wins = {0xffd36b, 0x2a2228}; chimney = true; break;
        case D_PASTEL: bw = L.ri(4, 5); bd = L.ri(4, 5); bh = L.ri(2, 3); name = "Candy house";
          col = L.pick({0xf7b8d0, 0xb8e8d0, 0xfff0a0, 0xc8b8f0, 0xffc8a0, 0xa0d8ff}); wins = {0xffd36b, 0x3ad7ff}; roof = true; break;
        case D_DOCKS: bw = L.ri(8, 11); bd = L.ri(6, 9); bh = L.ri(4, 7); name = "Warehouse";
          col = L.pick({0x8a8f99, 0xd2733a, 0x6a7a5a, 0xa8a49a}); wins = {0x1a1d26, 0x3ad7ff}; chimney = true; break;
        default: bw = L.ri(5, 7); bd = L.ri(5, 7); bh = L.ri(6, 13); name = "Seaside deco block";
          col = L.pick({0xf2e6c8, 0x9ad8c8, 0xf0b8a0, 0xe8d8f0}); wins = {0x3ad7ff, 0xffd36b, 0x2a2a33}; crown = true; break;
      }
      if (arena) { bw = L.ri(3, 5); bd = L.ri(3, 5); bh = L.ri(3, 6); name = "Zoo Market stall"; col = L.pick({0xf2c94c, 0xff8a5a, 0x5ad8a0, 0x5aa8ff}); roof = true; crown = chimney = false; }
      float hw = bw * B / 2, hd = bd * B / 2;
      if (nearRoadLine(x - hw) || nearRoadLine(x + hw) || nearRoadLine(z - hd) || nearRoadLine(z + hd)) continue;
      if (std::floor((x - hw + H) / kRoadPitch) != std::floor((x + hw + H) / kRoadPitch) || std::floor((z - hd + H) / kRoadPitch) != std::floor((z + hd + H) / kRoadPitch)) continue;
      if (L.occupiedAt(x - hw, z - hd, x + hw, z + hd, 2)) continue;
      Building& b = L.add(name, x, z, bw, bd, bh, col, wins);
      b.oldtown = d == D_OLDQUARTER && !arena;  // L7 "Old Town Overload" counts only filter=="oldtown" buildings
      float top = bh * B;
      if (roof) b.decor = {DecorDef{Vec3(x, top, z), 3, {pyramidRoof(bw * B + 1.5f, bd * B + 1.5f, 4, L.pick({0xd65db1, 0x5a8adf, 0xff7a5a, 0x7a5adf}))}}};
      else if (crown) b.decor = {DecorDef{Vec3(x, top, z), 3, {gbox(col, bw * B * 0.7f, 2, bd * B * 0.7f, M(0, 1, 0)), gbox(col, bw * B * 0.4f, 2, bd * B * 0.4f, M(0, 3, 0))}}};
      else if (chimney) b.decor = {DecorDef{Vec3(x, top, z), 2, {gcyl(0x6a4a3a, 0.6f, 0.7f, 4, M(hw * 0.5f, 2, hd * 0.4f))}}};
      else if (bh > 30) b.decor = {DecorDef{Vec3(x, top, z), 2, {gcyl(0xc8ccd4, 0.3f, 0.3f, 8, M(0, 4, 0))}}};
    }
    if (d != D_DOWNTOWN && L.r.next() < 0.6f) { float x = cx + L.rr(-18, 18), z = cz + L.rr(-18, 18); if (!L.occupiedAt(x - 2, z - 2, x + 2, z + 2, 1)) L.props.push_back({0, x, z, L.rr(0.8f, 1.2f)}); }
  }
  { // gauntlet stones: 6 hiding buildings + 20 decoys, all painted the one stone colour, spread out, outside the L13 arena
    std::vector<int> cand;
    for (auto& b : w.buildings) if (b.landmark.empty() && !b.school && b.h <= 14 && std::hypot(b.x - 110, b.z + 40) > 85) cand.push_back(b.id);
    for (size_t i = cand.size(); i > 1; i--) std::swap(cand[i - 1], cand[(size_t)L.ri(0, (int)i - 1)]);
    std::vector<int> pickd;
    for (float spacing : {60.f, 35.f, 0.f}) for (int id : cand) {
      if (pickd.size() >= 26) break;
      if (std::find(pickd.begin(), pickd.end(), id) != pickd.end()) continue;
      bool ok = true; for (int o : pickd) if (std::hypot(w.buildings[id].x - w.buildings[o].x, w.buildings[id].z - w.buildings[o].z) < spacing) { ok = false; break; }
      if (ok) pickd.push_back(id);
    }
    w.stoneBuildings.assign(pickd.begin(), pickd.begin() + std::min<size_t>(6, pickd.size()));
    w.decoyBuildings.assign(pickd.begin() + w.stoneBuildings.size(), pickd.end());
    for (int id : pickd) { w.buildings[id].color = hexColor(w.stoneColor); w.buildings[id].decor.clear(); }
  }
  for (int i = 0; i < 30; i++) L.props.push_back({1, L.rr(-380, 380), L.rr(336, 343), L.rr(1, 2)});
  L.label("Skyspire Center", 0, 40, 0, 0); L.label("Old Town", 0, 30, -280, 0); L.label("Candy Hills", -300, 24, 40, 0);
  L.label("Sprocket Docks", 280, 28, -40, 0); L.label("Sunny Shore", 100, 28, 300, 0);
  L.label("Glimmer River", 150, 6, -172, 2); L.label("Puddlemoon Lake", -280, 6, 242, 2); L.label("Sparkle Bay", 0, 6, 370, 2);
  // --- ground ---
  auto flat = [&](float x0, float z0, float x1, float z1, uint32_t c, float y, int surf, int lane = 0) {
    w.ground.push_back(gbox(c, x1 - x0, 0.04f, z1 - z0, M((x0 + x1) / 2, y, (z0 + z1) / 2)));
    w.groundQuads.push_back(GroundQuad{x0, z0, x1, z1, y + 0.02f, hexColor(c), surf, lane, lane == 1 ? (z0 + z1) / 2 : lane == 2 ? (x0 + x1) / 2 : 0.f});
  };
  w.ground.push_back(gbox(0x3b3f4a, 2 * H + 600, 1, 2 * H + 600, M(0, -0.5f, 0)));
  w.groundQuads.push_back(GroundQuad{-(H + 300), -(H + 300), H + 300, H + 300, 0.f, hexColor(0x3b3f4a), GS_CONCRETE});
  for (auto& p : parks) flat(p.x0, p.z0, p.x1, p.z1, 0x4f9a4a, 0.03f, GS_GRASS);
  flat(-H, 330, H, 345, 0xd8c890, 0.03f, GS_PAVING);  // beach promenade
  size_t street0 = w.groundQuads.size();
  for (float v = -H + kRoadPitch; v < H; v += kRoadPitch) {
    flat(v - kRoadHalf, -H, v + kRoadHalf, H - 70, 0x2b2e36, 0.05f, GS_ASPHALT, 2);   // north-south (water quads draw over the gaps)
    flat(-H, v - kRoadHalf, H, v + kRoadHalf, 0x2b2e36, 0.05f, GS_ASPHALT, 1);         // east-west
  }
  {
    std::vector<GroundQuad> patches;
    for (size_t a = street0; a < w.groundQuads.size(); a++) for (size_t b = street0; b < w.groundQuads.size(); b++) {
      const GroundQuad &A = w.groundQuads[a], &Bq = w.groundQuads[b];
      if (A.lane != 1 || Bq.lane != 2) continue;
      float x0 = std::max(A.x0, Bq.x0), x1 = std::min(A.x1, Bq.x1), z0 = std::max(A.z0, Bq.z0), z1 = std::min(A.z1, Bq.z1);
      if (x1 - x0 > 0.5f && z1 - z0 > 0.5f) patches.push_back(GroundQuad{x0, z0, x1, z1, A.y + 0.01f, A.color, GS_ASPHALT});
    }
    w.groundQuads.insert(w.groundQuads.end(), patches.begin(), patches.end());
  }
  for (auto& r : w.water) flat(r.x0, r.z0, r.x1, r.z1, 0x1f5d8c, 0.12f, GS_WATER);
  for (float bx : kBridgeX) {  // bridge decks + rails over the river gaps
    flat(bx - 6, -185, bx + 6, -160, 0x8a8478, 0.14f, GS_PAVING);
    w.statics.push_back({gbox(0xd65db1, 0.6f, 1.2f, 25, M(bx - 5.7f, 0.7f, -172.5f)), w.chunkIndex(bx, -172.5f)});
    w.statics.push_back({gbox(0xd65db1, 0.6f, 1.2f, 25, M(bx + 5.7f, 0.7f, -172.5f)), w.chunkIndex(bx, -172.5f)});
  }
  flat(-3 * H, H, 3 * H, 3 * H, 0x1f5d8c, 0.1f, GS_WATER); flat(-3 * H, -3 * H, 3 * H, -H, 0x1f5d8c, 0.1f, GS_WATER);
  flat(-3 * H, -H, -H, H, 0x1f5d8c, 0.1f, GS_WATER); flat(H, -H, 3 * H, H, 0x1f5d8c, 0.1f, GS_WATER);
}
}  // namespace tz
