// Props (trees, rocks, fallen roofs), brick debris pool, particles/rings/shells, rainbow paint splats.
#include <algorithm>
#include "game.h"
namespace tz {
// ---------------------------------------------------------------- bodies
int Bodies::add(const Body& b) {
  for (int i = 0; i < (int)list.size(); i++) if (!list[i].used) { list[i] = b; list[i].used = true; return i; }
  if (list.size() >= list.capacity()) return -1;  // capacity is reserved up front so references stay valid mid-update
  list.push_back(b); list.back().used = true; return (int)list.size() - 1;
}
void Bodies::fling(int i, float vx, float vy, float vz, bool thrown) {
  if (i < 0 || i >= (int)list.size()) return;
  Body& b = list[i]; if (!b.used || !b.alive || b.held) return;
  b.vel = Vec3(vx, vy, vz); b.resting = false; b.thrown = thrown;
  b.ang = Vec3((frand() - 0.5f) * 8, (frand() - 0.5f) * 4, (frand() - 0.5f) * 8);
}
int Bodies::nearest(float x, float z, float r, bool pickable) {
  int best = -1; float bd = r;
  for (int i = 0; i < (int)list.size(); i++) {
    Body& b = list[i];
    if (!b.used || !b.alive || b.held || !G->world.isActiveAt(b.pos.x, b.pos.z)) continue;
    if (pickable && b.kind == BODY_DECOR) continue;
    float d = dist2D(b.pos.x, b.pos.z, x, z);
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}
void Bodies::shatter(int i) {
  Body& b = list[i]; if (!b.alive) return;
  for (int k = 0; k < 9; k++)
    G->debris->spawn(b.pos + Vec3((frand() - 0.5f) * b.radius, frand() * b.radius, (frand() - 0.5f) * b.radius),
                     Vec3((frand() - 0.5f) * 14, 6 + frand() * 10, (frand() - 0.5f) * 14), b.color, 0.5f + frand() * 0.9f);
  G->playAt(SND_crumble, b.pos, 0.7f, 1.4f);
  b.alive = false; b.respawnAt = G->time + C.rockRespawn;
  if (b.kind == BODY_DECOR) b.used = false;
}
void Bodies::update(float dt) {
  float g = C.gravity, H = C.worldHalf - 2;
  for (int i = 0; i < (int)list.size(); i++) {
    Body& b = list[i];
    if (!b.used) continue;
    if (!b.alive) {
      if (b.respawnAt > 0 && G->time > b.respawnAt) { b.alive = true; b.resting = true; b.thrown = false; b.lying = false; b.vel = Vec3(); b.pos = b.homePos; b.rx = b.homeRx; b.ry = b.homeRy; b.rz = b.homeRz; }
      continue;
    }
    if (b.held || b.resting) continue;
    b.vel.y -= g * dt; b.pos += b.vel * dt;
    b.rx += b.ang.x * dt; b.ry += b.ang.y * dt; b.rz += b.ang.z * dt;
    float speed = length(b.vel);
    if (b.thrown && speed > 8) {  // thrown objects smash buildings and bump other props
      HitResult res = G->world.damageSphere(b.pos.x, b.pos.y, b.pos.z, b.radius + 2.5f, 10 * b.mass, b.pos.x - b.vel.x, b.pos.z - b.vel.z, speed * 0.5f);
      if (res.bricks > 0) { b.vel *= 0.55f; G->awardCoins(res.bricks * (int)C.coinsPerBrickOther, b.pos); }
      if (res.blocked) { b.vel.x *= -0.3f; b.vel.z *= -0.3f; }
      for (int j = 0; j < (int)list.size(); j++) {
        Body& o = list[j];
        if (j == i || !o.used || !o.alive || o.held) continue;
        if (length(o.pos - b.pos) < b.radius + o.radius + 0.5f) {
          if (o.kind == BODY_ROCK && speed > 20) shatter(j); else fling(j, b.vel.x * 0.7f, 8 + speed * 0.15f, b.vel.z * 0.7f, true);
          b.vel *= 0.6f;
        }
      }
      G->people.knock(b.pos.x, b.pos.z, b.radius + 2, 8);
      G->enemies.hitArea(b.pos.x, b.pos.z, b.radius + 2, -1, true);
      G->hitEgg(-1, b.pos.x, b.pos.z, 0);
    }
    float gy = b.groundOffset();
    if (b.pos.y < gy) {
      b.pos.y = gy;
      if (b.vel.y < -6) { b.vel.y *= -0.25f; b.vel.x *= 0.6f; b.vel.z *= 0.6f; }
      else {
        b.vel = Vec3(); b.resting = true; b.thrown = false;
        if (b.kind == BODY_TREE) { b.lying = true; b.rx = PI / 2; b.rz = 0; b.pos.y = 0.6f * b.scale; }
      }
    }
    b.pos.x = clampf(b.pos.x, -H, H); b.pos.z = clampf(b.pos.z, -H, H);
  }
}
void Bodies::render(RenderList& rl) {
  for (auto& b : list) {
    if (!b.used || !b.alive) continue;
    if (!b.held && !G->world.isActiveAt(b.pos.x, b.pos.z)) continue;
    // tree/rock bodies rotate in three.js 'YXZ'-ish order: yaw first, then pitch/roll
    Mat4 W = Mat4::scale(b.scale, b.scale, b.scale) * Mat4::rotZ(b.rz) * Mat4::rotX(b.rx) * Mat4::rotY(b.ry) * Mat4::translation(b.pos.x, b.pos.y, b.pos.z);
    if (b.kind == BODY_ROCK) W = Mat4::scale(b.scale, b.scale, b.scale) * Mat4::rotY(b.ry) * Mat4::rotX(b.rx) * Mat4::translation(b.pos.x, b.pos.y, b.pos.z);
    if (G->bodyModel(rl, b, (int)(&b - list.data()), W)) continue;  // loaded model (trees, street props, parked vehicles)
    for (auto& p : b.parts) rl.opaque[p.mesh].push_back(makeInstance(p.local * W, p.color, p.unlit));
  }
}

// ---------------------------------------------------------------- debris
void Debris::spawn(const Vec3& p, const Vec3& v, Color c, float s) {
  int i = next; next = (next + 1) % N;
  pos[i] = p; vel[i] = v; rot[i] = Vec3(frand() * 3, frand() * 3, 0);
  ang[i] = Vec3((frand() - 0.5f) * 10, (frand() - 0.5f) * 10, (frand() - 0.5f) * 10);
  life[i] = C.debrisLife * (0.8f + frand() * 0.4f); size[i] = s; col[i] = c; alive[i] = true; sleep[i] = false;
}
void Debris::kick(float x, float z, float r, float power) {
  for (int i = 0; i < N; i++) {
    if (!alive[i]) continue;
    float dx = pos[i].x - x, dz = pos[i].z - z, d = std::hypot(dx, dz);
    if (d < r && d > 0.01f) { float f = power * (1 - d / r); vel[i].x += dx / d * f; vel[i].z += dz / d * f; vel[i].y += f * 0.8f; sleep[i] = false; life[i] = std::max(life[i], 2.5f); }
  }
}
void Debris::update(float dt) {
  float g = C.gravity; live = 0;
  for (int i = 0; i < N; i++) {
    if (!alive[i]) continue;
    live++;
    life[i] -= dt; if (life[i] <= 0) { alive[i] = false; continue; }
    if (sleep[i]) continue;
    float half = size[i] * 0.5f;
    vel[i].y -= g * dt; pos[i] += vel[i] * dt; rot[i] += ang[i] * dt;
    if (pos[i].y < half) {
      pos[i].y = half;
      if (vel[i].y < -3) vel[i].y = -vel[i].y * C.debrisBounce; else vel[i].y = 0;
      float f = std::pow(1 - C.debrisFriction, dt * 4); vel[i].x *= f; vel[i].z *= f; ang[i] *= 0.8f;
      if (std::fabs(vel[i].x) + std::fabs(vel[i].y) + std::fabs(vel[i].z) < 0.4f) {
        sleep[i] = true;
        rot[i].x = std::round(rot[i].x / 1.5708f) * 1.5708f; rot[i].z = std::round(rot[i].z / 1.5708f) * 1.5708f;
      }
    }
  }
}
void Debris::render(RenderList& rl, const Vec3& eye) {
  float maxD2 = (C.fogFar + 20) * (C.fogFar + 20);
  for (int i = 0; i < N; i++) {
    if (!alive[i]) continue;
    Vec3 d = pos[i] - eye; if (dot(d, d) > maxD2) continue;
    float s = size[i], y = pos[i].y;
    if (life[i] < 1.2f) { s *= std::max(0.01f, life[i] / 1.2f); y = std::max(s * 0.5f, y - (1.2f - life[i])); }
    rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(s, s, s) * Mat4::rotZ(rot[i].z) * Mat4::rotY(rot[i].y) * Mat4::rotX(rot[i].x) * Mat4::translation(pos[i].x, y, pos[i].z), col[i]));
  }
}

// ---------------------------------------------------------------- effects
void Effects::particle(const Vec3& p, const Vec3& v, float life, float size, Color c) {
  int i = pNext; pNext = (pNext + 1) % NP;
  pp[i] = p; pv[i] = v; pl[i] = life; pml[i] = life; ps[i] = size; pc[i] = c;
}
void Effects::ring(float x, float z, float maxR, float dur, Color c) { items.push_back({0, Vec3(x, 0.6f, z), 0, dur, maxR, c}); }
void Effects::shell(const Vec3& p, float maxR, float dur, Color c) { items.push_back({1, p, 0, dur, maxR, c}); }
void Effects::dust(float x, float z, int n, float r) {
  for (int i = 0; i < n; i++) {
    float a = frand() * 6.283f, s = 4 + frand() * 8;
    particle(Vec3(x + std::cos(a) * r, 0.5f, z + std::sin(a) * r), Vec3(std::cos(a) * s, 2 + frand() * 3, std::sin(a) * s), 0.7f, 1.4f, hexColor(0x9aa0aa));
  }
}
void Effects::rainbowBurst(const Vec3& p, float scale) {
  static const uint32_t RB[] = {0xff4d4d, 0xff9a1f, 0xffe03a, 0x4cd964, 0x3ad7ff, 0x5a6bff, 0xb06bff};  // a cartoon rainbow arc
  float yaw = frand() * 6.283f, cx = std::cos(yaw), cz = std::sin(yaw);
  for (int band = 0; band < 7; band++) {
    float R = (2.2f - band * 0.22f) * scale;
    for (int i = 0; i <= 8; i++) {
      float a = PI * i / 8, x = std::cos(a) * R, y = std::sin(a) * R;
      particle(p + Vec3(cx * x, y + 0.5f * scale, cz * x), Vec3(0, 1.5f, 0), 1.0f, 0.38f * scale, hexColor(RB[band]));
    }
  }
  for (int i = 0; i < 6; i++) particle(p, Vec3((frand() - 0.5f) * 8, 4 + frand() * 6, (frand() - 0.5f) * 8), 0.8f, 0.3f * scale, hexColor(0xffffff));
}
void Effects::update(float dt) {
  for (int k = (int)items.size() - 1; k >= 0; k--) { items[k].t += dt; if (items[k].t >= items[k].dur) items.erase(items.begin() + k); }
  for (int i = 0; i < NP; i++) {
    if (pl[i] <= 0) continue;
    pl[i] -= dt; pp[i] += pv[i] * dt; pv[i] *= std::pow(0.96f, dt * 60);
  }
}
void Effects::render(RenderList& rl) {
  for (auto& it : items) {
    float f = it.t / it.dur, r = 0.5f + it.maxR * (1 - (1 - f) * (1 - f));
    Color c = it.c;
    if (it.type == 0) { c.a = 0.9f * (1 - f); rl.alpha[MESH_RING].push_back(makeInstance(Mat4::scale(2 * r, 2 * r, 2 * r) * Mat4::translation(it.pos.x, it.pos.y, it.pos.z), c, 1)); }
    else { c.a = 0.35f * (1 - f); rl.alpha[MESH_SPHERE].push_back(makeInstance(Mat4::scale(2 * r, 2 * r, 2 * r) * Mat4::translation(it.pos.x, it.pos.y, it.pos.z), c, 1)); }
  }
  for (int i = 0; i < NP; i++) {
    if (pl[i] <= 0) continue;
    float s = ps[i] * (0.4f + 0.6f * pl[i] / pml[i]);
    rl.opaque[MESH_ROCK].push_back(makeInstance(Mat4::scale(s, s, s) * Mat4::translation(pp[i].x, pp[i].y, pp[i].z), pc[i], 1));
  }
}

// ---------------------------------------------------------------- splats (never red, no bodies)
static const uint32_t SPLAT_COLORS[] = {0xff9a1f, 0xffe03a, 0x4cd964, 0x3ad7ff, 0x5a6bff, 0xb06bff, 0xff6bd5, 0xffffff};
void Splats::init() { list.assign((size_t)C.maxSplats, Splat()); next = 0; count = 0; }
void Splats::spawn(float x, float z) {
  if (list.empty()) init();
  Splat& s = list[next]; next = (next + 1) % (int)list.size();
  s.alive = true; s.x = x; s.z = z; s.born = G->time; s.fade = 0;
  for (int b = 0; b < 7; b++) {
    float a = frand() * 6.283f, d = b == 0 ? 0 : 0.6f + frand() * 1.6f, r = b == 0 ? 1.4f + frand() * 0.5f : 0.35f + frand() * 0.6f;
    s.blobs[b] = {x + std::cos(a) * d, z + std::sin(a) * d, r, 0.07f + b * 0.006f, hexColor(SPLAT_COLORS[(int)(frand() * 8) % 8])};
  }
  for (int i = 0; i < 16; i++) {  // confetti burst
    float a = frand() * 6.283f, sp = 4 + frand() * 7;
    G->fx->particle(Vec3(x, 1.5f, z), Vec3(std::cos(a) * sp, 6 + frand() * 8, std::sin(a) * sp), 0.9f, 0.35f, hexColor(SPLAT_COLORS[i % 8]));
  }
  G->playAt(SND_pop, Vec3(x, 1, z), 0.9f, 0.9f + frand() * 0.3f);
}
void Splats::fadeNear(float x, float z, float r) { for (auto& s : list) if (s.alive && s.fade <= 0 && dist2D(s.x, s.z, x, z) < r) s.fade = 1.5f; }
void Splats::update(float dt) {
  count = 0;
  for (auto& s : list) {
    if (!s.alive) continue;
    count++;
    if (s.fade <= 0 && G->time - s.born > C.splatMaxAge) s.fade = 1.5f;
    if (s.fade > 0) { s.fade -= dt; if (s.fade <= 0) s.alive = false; }
  }
}
void Splats::render(RenderList& rl) {
  for (auto& s : list) {
    if (!s.alive || !G->world.isActiveAt(s.x, s.z)) continue;
    float k = s.fade > 0 ? s.fade / 1.5f : 1;
    for (auto& b : s.blobs) rl.opaque[MESH_DISC].push_back(makeInstance(Mat4::scale(2 * b.r * k, 1, 2 * b.r * k * 0.85f) * Mat4::translation(b.x, b.y, b.z), b.c, 1));
  }
}
}  // namespace tz
