// People: kid-friendly stick figures that wander, flee from the monster and tumble. Never hurt: a direct hit
// pops them into a rainbow paint splat with confetti (no bodies, never red).
#include "zombies.h"
#include "game.h"
namespace tz {
static const uint32_t SHIRTS[] = {0xff9a1f, 0xffd36b, 0x3ad7ff, 0x7aff8a, 0xff8ad8, 0xffffff, 0xffa040, 0x9a7aff};
static const uint32_t SKIN[] = {0xf5d0b0, 0xd9a07a, 0x8a5a3a, 0xf0c090};
void People::init() { list.assign((size_t)C.maxPeople, Person()); for (size_t i = 0; i < list.size(); i++) { list[i].look = (int)i; list[i].phase = frand() * 6; } activeCount = 0; }
void People::spawnChunk(const Chunk& ch) {
  int n = (int)C.peoplePerChunk; float S = C.chunkSize;
  for (auto& p : list) {
    if (n <= 0) break; if (p.active) continue;
    bool ok = false; float x = 0, z = 0;
    for (int t = 0; t < 12 && !ok; t++) { x = ch.x0 + 4 + frand() * (S - 8); z = ch.z0 + 4 + frand() * (S - 8); ok = !G->world.blockedForPerson(x, z); }
    n--;
    if (!ok) continue;
    p.active = true; p.x = x; p.y = 0; p.z = z; p.vx = p.vy = p.vz = 0; p.dir = frand() * 6.28f; p.state = P_WANDER; p.t = frand() * 3; p.tilt = 0; p.spin = 0;
  }
}
void People::despawnInactive() { for (auto& p : list) if (p.active && !G->world.isActiveAt(p.x, p.z)) p.active = false; }
int People::pop(float x, float z, float r) {
  int n = 0;
  for (int i = 0; i < (int)list.size(); i++) {
    Person& p = list[i];
    if (!p.active || p.y > 6) continue;
    if (dist2D(p.x, p.z, x, z) < r) { popOne(i); n++; }
  }
  if (n) G->onPeoplePopped(n);
  return n;
}
void People::popOne(int i) {
  Person& p = list[i]; if (!p.active) return;
  p.active = false; G->splats.spawn(p.x, p.z);
  G->fx->rainbowBurst(Vec3(p.x, 1.5f, p.z), 1.0f);
}
int People::nearestInRange(float x, float z, float r) const {
  int best = -1; float bd = r;
  for (int i = 0; i < (int)list.size(); i++) {
    const Person& p = list[i];
    if (!p.active || p.y > 6) continue;
    float d = dist2D(p.x, p.z, x, z);
    if (d <= bd) { bd = d; best = i; }
  }
  return best;
}
int People::stun(float x, float z, float r, float secs) {
  int n = 0;
  for (auto& p : list) {
    if (!p.active || p.y > 6 || dist2D(p.x, p.z, x, z) >= r) continue;
    p.state = P_STUN; p.t = secs; p.y = 0; p.vx = p.vy = p.vz = 0; p.tilt = PI / 2; p.spin = 0; n++;
  }
  return n;
}
int People::pickRay(const Vec3& o, const Vec3& d, float tmin, float tmax, float radius, float* tHit) const {
  int best = -1; float bt = tmax;
  for (int i = 0; i < (int)list.size(); i++) {
    const Person& p = list[i]; if (!p.active) continue;
    Vec3 c = center(p), oc = c - o; float t = dot(oc, d);
    if (t < tmin || t > bt) continue;
    Vec3 q = o + d * t - c;
    float rr = radius + (p.state == P_STUN ? 0.4f : 0.f);
    if (dot(q, q) <= rr * rr) { bt = t; best = i; }
  }
  if (best >= 0 && tHit) *tHit = bt;
  return best;
}
void People::spawnOne(float kx, float kz) {
  Person* p = nullptr; for (auto& q : list) if (!q.active) { p = &q; break; }
  if (!p) return;
  std::vector<const Chunk*> act; for (auto& c : G->world.chunks) if (c.active) act.push_back(&c);
  if (act.empty()) return;
  float S = C.chunkSize;
  for (int t = 0; t < 8; t++) {
    const Chunk* ch = act[(size_t)(frand() * act.size()) % act.size()];
    float x = ch->x0 + 4 + frand() * (S - 8), z = ch->z0 + 4 + frand() * (S - 8);
    if (dist2D(x, z, kx, kz) < (G->tutorialActive() ? C.tutorialSpawnMinDist : 60.f) || G->world.blockedForPerson(x, z)) continue;
    p->active = true; p->x = x; p->y = 0; p->z = z; p->vx = p->vy = p->vz = 0; p->dir = frand() * 6.28f; p->state = P_WANDER; p->t = 2; p->tilt = 0; p->spin = 0;
    return;
  }
}
void People::knock(float x, float z, float r, float power) {
  for (auto& p : list) {
    if (!p.active || p.state == P_TUMBLE || p.state == P_STUN) continue;
    float dx = p.x - x, dz = p.z - z, d = std::hypot(dx, dz);
    if (d < r) {
      float f = power * (0.6f + 0.4f * (1 - d / r)) + 3, nx = d > 0.01f ? dx / d : frand() - 0.5f, nz = d > 0.01f ? dz / d : frand() - 0.5f;
      p.state = P_TUMBLE; p.vx = nx * f; p.vz = nz * f; p.vy = 6 + frand() * 4 + power * 0.2f; p.spin = (frand() < 0.5f ? -1 : 1) * (8 + frand() * 6);
      p.dir = std::atan2(nx, nz);
    }
  }
}
void People::update(float dt, float kx, float kz, float kspeed, float kr) {
  respawnT -= dt;
  if (respawnT <= 0) {
    bool tut = G->mode == Mode::Play && G->tutorialActive();  // the tutorial always keeps plenty of people to rainbow
    bool play = G->mode == Mode::Play;  // eat-to-fuel: every level (and the sandbox) keeps enough people around to gulp
    respawnT = tut ? C.tutorialRespawnInterval : play ? std::min(C.peopleRespawnInterval, C.levelRespawnInterval) : C.peopleRespawnInterval;
    int target = std::min((int)C.maxPeople, G->world.activeChunkCount() * (int)C.peoplePerChunk);
    if (tut) target = std::max(target, std::min((int)C.maxPeople, (int)C.tutorialMinCivilians));
    if (play) target = std::max(target, std::min((int)C.maxPeople, (int)C.levelMinCivilians));
    if (activeCount < target) spawnOne(kx, kz);
  }
  int count = 0;
  for (auto& p : list) {
    if (!p.active) continue;
    count++;
    float dx = p.x - kx, dz = p.z - kz, dist = std::hypot(dx, dz);
    p.t -= dt;
    float moveSpeed = 0;
    if (p.state == P_TUMBLE) {
      p.vy -= C.gravity * 0.7f * dt;
      p.x += p.vx * dt; p.z += p.vz * dt; p.y += p.vy * dt; p.tilt += p.spin * dt;
      if (p.y < 20 && G->world.blockedForPerson(p.x, p.z)) { p.vx *= -0.5f; p.vz *= -0.5f; p.x += p.vx * dt * 2; p.z += p.vz * dt * 2; }
      if (p.y <= 0) { p.y = 0; p.vy = 0; p.state = P_DIZZY; p.t = 0.7f; p.tilt = PI / 2; }
    } else if (p.state == P_STUN) {  // flat on the ground seeing stars: no moving, no fleeing
      p.tilt = PI / 2; p.vx = 0;
      if (p.t <= 0) { p.tilt = 0; p.state = P_FLEE; p.t = 3; }
      continue;
    } else if (p.state == P_DIZZY) {
      p.tilt = PI / 2 * std::min(1.f, p.t / 0.35f);
      if (frand() < dt * 6) G->fx->particle(Vec3(p.x, 1.5f, p.z), Vec3(std::cos(G->time * 9) * 2, 1, std::sin(G->time * 9) * 2), 0.4f, 0.35f, hexColor(0xffd36b));
      if (p.t <= 0) { p.tilt = 0; p.state = P_FLEE; p.t = 4; }
    } else if (zombie::on()) {
      moveSpeed = zombie::step(p, dt, kx, kz, dist, kr);  // Rex: crazy mode - shamble toward the kaiju and nibble
      if (dist < kr * 0.8f && kspeed > 1) { pop(kx, kz, kr * 0.8f); continue; }
      float nx = p.x + std::sin(p.dir) * moveSpeed * dt, nz = p.z + std::cos(p.dir) * moveSpeed * dt;
      if (G->world.blockedForPerson(nx, nz)) p.dir += PI / 2 + frand(); else { p.x = nx; p.z = nz; }
    } else {
      if (dist < kr * 0.8f && kspeed > 1) { pop(kx, kz, kr * 0.8f); continue; }  // stepped on: pop!
      if (dist < kr + 2.5f) knock(kx, kz, kr + 2.5f, 6);
      if (dist < C.screamRadius && frand() < dt * 0.6f) G->scream(Vec3(p.x, 1, p.z));
      if (dist < C.fleeRadius && p.state != P_FLEE) { p.state = P_FLEE; p.t = 3 + frand() * 2; }
      if (p.state == P_FLEE) {
        float away = std::atan2(dx, dz), diff = wrapAngle(away - p.dir);
        p.dir += diff * std::min(1.f, dt * 6) + (frand() - 0.5f) * dt * 2;
        moveSpeed = C.runSpeed;
        if (p.t <= 0 && dist > C.fleeRadius * 1.3f) { p.state = P_WANDER; p.t = 2; }
      } else {
        moveSpeed = C.walkSpeed;
        if (p.t <= 0) { p.t = 2 + frand() * 4; p.dir += (frand() - 0.5f) * 2.5f; p.idle = frand() < 0.25f; }
        if (p.idle) moveSpeed = 0;
      }
      float nx = p.x + std::sin(p.dir) * moveSpeed * dt, nz = p.z + std::cos(p.dir) * moveSpeed * dt;
      if (G->world.blockedForPerson(nx, nz)) p.dir += PI / 2 + frand(); else { p.x = nx; p.z = nz; }
    }
    p.vx = moveSpeed;  // remembered for the pose
    if (p.state != P_TUMBLE) p.phase += dt * moveSpeed * 2.2f;
  }
  activeCount = count;
}
// stick figures: head sphere, thin "line" boxes for torso and limbs
void People::render(RenderList& rl, const Vec3& eye) {
  const float k = 1.0f;  // person height 2.4
  float maxD2 = (C.fogFar) * (C.fogFar);
  for (auto& p : list) {
    if (!p.active) continue;
    float ex = p.x - eye.x, ez = p.z - eye.z; if (ex * ex + ez * ez > maxD2) continue;
    const bool model = G->personModel(rl, p, (int)(&p - list.data()), eye);  // loaded model (near civilians) instead of the stick figure
    Mat4 root = Mat4::rotX(-p.tilt) * Mat4::rotY(p.dir) * Mat4::translation(p.x, p.y + (p.tilt != 0 ? 0.25f : 0), p.z);
    Color shirt = hexColor(SHIRTS[p.look % 8]), skin = hexColor(SKIN[p.look % 4]), pants = hexColor(0x2a2f3a);
    if (zombie::on()) { skin = zombie::skin(p.look); shirt = scaleColor(shirt, 0.6f); }  // Rex
    float speed = p.state == P_TUMBLE || p.state == P_DIZZY ? 0 : p.vx;
    float sw = speed > 0 ? std::sin(p.phase) * (speed > 4 ? 0.9f : 0.5f) : 0;
    bool arms = p.state == P_FLEE || zombie::on();
    if (!model) rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(0.22f * k, 0.9f * k, 0.22f * k) * Mat4::translation(0, 1.25f * k, 0) * root, shirt, 1));
    if (p.state == P_STUN) {  // ring of cartoon stars circling above the head (head lies at +2 m along the facing)
      float hd = model ? 0.9f : 2.0f;  // the model lies down via its "stun" clip, the stick figure tips forward
      Vec3 head(p.x + std::sin(p.dir) * hd, model ? 1.0f : 1.2f, p.z + std::cos(p.dir) * hd);
      for (int st = 0; st < 5; st++) {
        float a = G->time * 5 + st * 1.2566f;
        Mat4 sm = Mat4::scale(0.35f, 0.35f, 0.12f) * Mat4::rotZ(a * 2) * Mat4::rotY(a) * Mat4::translation(head.x + std::cos(a) * 0.9f, head.y + 0.25f * std::sin(a * 2), head.z + std::sin(a) * 0.9f);
        rl.opaque[MESH_PYRAMID].push_back(makeInstance(sm, hexColor(st % 2 ? 0xffe03a : 0xffffff), 1));
      }
    }
    if (G->kaiju.stompAim && dist2D(p.x, p.z, G->kaiju.pos.x, G->kaiju.pos.z) < G->kaiju.stompStunRadius()) {  // inside the stomp ring
      rl.alpha[MESH_DISC].push_back(makeInstance(Mat4::scale(1.6f, 1, 1.6f) * Mat4::translation(p.x, 0.14f, p.z), Color{1, 0.88f, 0.23f, 0.6f}, 1));
    }
    if (model) continue;
    rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(0.64f * k, 0.64f * k, 0.64f * k) * Mat4::translation(0, 2.0f * k, 0) * root, skin, 1));
    float wave = arms ? PI * 0.85f + std::sin(G->time * 14 + p.look) * 0.35f : 0;
    const float L[4][3] = {{-0.22f, 1.65f, arms ? wave : sw}, {0.22f, 1.65f, arms ? wave : -sw}, {-0.1f, 0.8f, -sw}, {0.1f, 0.8f, sw}};
    for (int l = 0; l < 4; l++) {
      Mat4 limb = Mat4::scale(0.13f * k, 0.8f * k, 0.13f * k) * Mat4::translation(0, -0.4f * k, 0) * Mat4::rotZ(l < 2 ? (l == 0 ? 0.15f : -0.15f) : 0) * Mat4::rotX(L[l][2]) * Mat4::translation(L[l][0] * k, L[l][1] * k, 0) * root;
      rl.opaque[MESH_BOX].push_back(makeInstance(limb, l < 2 ? shirt : pants, 1));
    }
  }
}
}  // namespace tz
