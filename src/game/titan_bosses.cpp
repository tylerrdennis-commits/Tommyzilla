// Titan: varied end-of-level bosses (Unit::boss 3..5). 1 = Giant Super-Tank, 2 = Mega Mech live in enemies.cpp.
//   3 THUNDER MOTH  - flutters around you, dives, drops a ring of zap bombs
//   4 ROCK CRAB     - scuttles sideways, snaps a pincer shockwave, burrows (can't be hit) and pops up under you
//   5 BLIMP KING    - a giant balloon that floats over and drops a line of confetti bombs; swoops low to be smashed
#include <algorithm>
#include "game.h"
namespace tz {
int bossIdFromName(const std::string& n) { return n == "mech" ? 2 : n == "moth" ? 3 : n == "crab" ? 4 : n == "blimp" ? 5 : 1; }
const char* bossDisplayName(int b) { static const char* N[6] = {"Giant Super-Tank", "Giant Super-Tank", "Mega Mech", "Thunder Moth", "Rock Crab", "Blimp King"}; return N[std::max(0, std::min(5, b))]; }
float bossSpawnHeight(int b) { return b == 3 ? 14.f : b == 5 ? 26.f : 0.f; }
bool bossFlies(int b) { return b == 3 || b == 5; }

static void bombAt(std::vector<Shot>& shots, const Vec3& from, const Vec3& vel, float dmg) { Shot s; s.pos = from; s.vel = vel; s.dmg = dmg; s.bomb = true; shots.push_back(s); }

bool Enemies::updateVariedBoss(Unit& u, float dt, float slow, bool frozen) {
  if (u.boss < 3) return false;
  Kaiju& K = G->kaiju; u.t += dt; u.reload -= dt; u.moving = true;
  Vec3 toK(K.pos.x - u.pos.x, 0, K.pos.z - u.pos.z); float d = length(toK); Vec3 f = d > 0.01f ? toK * (1.f / d) : Vec3(0, 0, 1);
  if (u.maxHp > 1 && u.hp <= u.maxHp * 0.5f) { u.reload -= dt * 0.5f; u.enraged = true; }  // enrage: faster bombs
  float dmg = C.mbossShotDamage * u.dmgK * C.bossHardness * (u.enraged ? 1.25f : 1.f);
  if (u.boss == 3) {
    u.ang += dt * 0.7f * slow;
    float dive = std::fmod(u.t, 6.f) > 4.5f ? 1.f : 0.f;
    Vec3 tgt(K.pos.x + std::cos(u.ang) * (40 - 26 * dive), 16 - 10 * dive, K.pos.z + std::sin(u.ang) * (40 - 26 * dive));
    u.pos = u.pos + (tgt - u.pos) * std::min(1.f, dt * 1.2f * slow * u.speedK);
    if (u.reload <= 0 && !frozen) {
      u.reload = C.mbossReload * 0.8f; u.fireT = G->time;
      for (int k = 0; k < 6; k++) { float a = k * 1.047f; bombAt(shots, u.pos, Vec3(std::cos(a) * 14, -10, std::sin(a) * 14) + f * 20, dmg * 0.5f); }
      G->playAt(SND_throwWhoosh, u.pos, 1.0f, 1.4f);
    }
  } else if (u.boss == 4) {
    float cyc = std::fmod(u.t, 9.f); bool under = cyc > 6.5f && cyc < 8.f;   // burrow 1.5 s
    u.bombT = under ? 1.f : 0.f;                                             // bombT = burrowed flag (render + hits)
    if (under) { u.hitCd = 0.2f; u.pos.x += f.x * 28 * dt * slow; u.pos.z += f.z * 28 * dt * slow; if (frand() < dt * 20) G->fx->dust(u.pos.x, u.pos.z, 2, 4); }
    else {
      Vec3 side(f.z, 0, -f.x); float want = d > 30 ? 1.f : d < 18 ? -1.f : 0.f;  // keeps a mid range, scuttles sideways
      u.pos += (side * std::sin(u.t * 0.8f) * 12 + f * want * 10) * (dt * slow * u.speedK);
      u.dir = std::atan2(f.x, f.z);
    }
    if (!under && cyc > 8.f && cyc - dt <= 8.f && !frozen) {  // pops up: ring shockwave
      G->fx->ring(u.pos.x, u.pos.z, 20, 0.6f, hexColor(0xff8a3a)); G->fx->dust(u.pos.x, u.pos.z, 20, 8); G->playAt(SND_quake, u.pos, 0.8f);
      if (d < 20 + K.radius()) K.damage(dmg * 1.2f);
    }
    if (!under && u.reload <= 0 && d < 40 && !frozen) {  // pincer snap: a short cone of three rock bombs
      u.reload = C.mbossReload; u.fireT = G->time;
      for (int k = -1; k <= 1; k++) { float a = k * 0.3f, ca = std::cos(a), sa = std::sin(a); Vec3 dir(f.x * ca + f.z * sa, 0, -f.x * sa + f.z * ca); bombAt(shots, u.pos + Vec3(0, 5, 0), dir * 24 + Vec3(0, 8, 0), dmg * 0.6f); }
      G->playAt(SND_bonk, u.pos, 1.0f, 0.6f);
    }
  } else {  // 5 BLIMP KING
    float low = std::fmod(u.t, 10.f) > 7.f ? 1.f : 0.f;  // every 10 s it sinks low for 3 s - smash it!
    Vec3 tgt(K.pos.x - f.x * (14 - 6 * low), 26 - 18 * low, K.pos.z - f.z * (14 - 6 * low));
    u.pos = u.pos + (tgt - u.pos) * std::min(1.f, dt * 0.6f * slow * u.speedK);
    u.dir = std::atan2(f.x, f.z);
    if (u.reload <= 0 && !frozen && low == 0) {
      u.reload = C.mbossReload * 1.2f; u.fireT = G->time;
      for (int k = 0; k < 5; k++) bombAt(shots, u.pos + f * (k * 6.f - 12), Vec3(0, -6, 0), dmg * 0.5f);
      G->playAt(SND_throwWhoosh, u.pos, 0.8f, 0.8f);
    }
  }
  return true;
}

static void Pz(RenderList& rl, MeshId m, uint32_t c, const Mat4& local, const Mat4& root, float unlit = 0) { rl.opaque[m].push_back(makeInstance(local * root, hexColor(c), unlit)); }
bool drawVariedBoss(RenderList& rl, const Unit& u, const Mat4& root) {
  if (u.type != U_MBOSS || u.boss < 3) return false;
  if (u.boss == 3) {
    float fl = std::sin(u.t * 14) * 0.7f;
    Pz(rl, MESH_SPHERE, 0xffd23a, Mat4::scale(3, 3, 6), root); Pz(rl, MESH_SPHERE, 0x3a2a10, Mat4::scale(2.2f, 2.2f, 2.2f) * Mat4::translation(0, 0.4f, 4), root);
    for (int s = -1; s <= 1; s += 2) for (int w = 0; w < 2; w++)
      Pz(rl, MESH_BOX, w ? 0x6ad0ff : 0x3a7aff, Mat4::translation(5 * s, 0, 0) * Mat4::scale(10, 0.3f, w ? 4.f : 6.f) * Mat4::rotZ(fl * s) * Mat4::translation(1.2f * s, 1.2f, w ? -2.5f : 1.f), root);
    for (int s = -1; s <= 1; s += 2) Pz(rl, MESH_SPHERE, 0xfff6a0, Mat4::scale(0.6f, 0.6f, 0.6f) * Mat4::translation(0.9f * s, 3.2f, 6), root, 1);
  } else if (u.boss == 4) {
    float sink = u.bombT > 0 ? -5.f : 0.f, snap = std::sin(u.t * 6) * 0.3f;
    Pz(rl, MESH_SPHERE, 0xd8502a, Mat4::scale(12, 4, 9) * Mat4::translation(0, 3 + sink, 0), root);
    for (int s = -1; s <= 1; s += 2) {
      for (int l = 0; l < 3; l++) Pz(rl, MESH_CYL, 0xa83a1a, Mat4::scale(0.8f, 5, 0.8f) * Mat4::rotZ(0.9f * s) * Mat4::translation(6.5f * s, 1.5f + sink, -3 + l * 3), root);
      Pz(rl, MESH_SPHERE, 0xff7040, Mat4::scale(3.5f, 2.2f, 4.5f) * Mat4::rotY(snap * s) * Mat4::translation(5 * s, 4 + sink, 7), root);
      Pz(rl, MESH_SPHERE, 0x101010, Mat4::scale(0.8f, 0.8f, 0.8f) * Mat4::translation(1.5f * s, 7 + sink, 4), root, 1);
    }
  } else {
    float bob = std::sin(u.t * 1.5f) * 0.6f;
    Pz(rl, MESH_SPHERE, 0xff5aa0, Mat4::scale(16, 9, 9) * Mat4::rotY(PI / 2) * Mat4::translation(0, bob, 0), root);
    Pz(rl, MESH_BOX, 0xfff0a0, Mat4::scale(4, 2, 2.5f) * Mat4::translation(0, -6 + bob, 0), root);
    for (int k = 0; k < 4; k++) Pz(rl, MESH_SPHERE, k % 2 ? 0x6ad0ff : 0xffe03a, Mat4::scale(1.6f, 1.6f, 1.6f) * Mat4::translation(std::sin(k * 1.57f) * 5, 7 + bob, std::cos(k * 1.57f) * 5), root, 1);  // crown jewels
  }
  return true;
}
}  // namespace tz
