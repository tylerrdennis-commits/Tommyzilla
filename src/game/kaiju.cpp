// The player monster: movement, third-person camera, the 25-move roster, projectiles, grab & throw.
#include <algorithm>
#include "game.h"
namespace tz {
namespace {
// mouth / hand / head-height anchors per body plan (local space, scale 1, +z = forward)
struct Anchors { Vec3 mouth, hand; float camY; };
Anchors anchorsFor(BodyPlan p) {
  switch (p) {
    case BodyPlan::Biped: return {Vec3(0, 12.6f, 7.4f), Vec3(3.0f, 8.6f, 5.0f), 10};
    case BodyPlan::Quad: return {Vec3(0, 12.5f, 10.5f), Vec3(0, 11.5f, 10.0f), 9};
    case BodyPlan::Blob: return {Vec3(0, 8.5f, 5.6f), Vec3(4.5f, 6.5f, 3.5f), 8};
    case BodyPlan::Crab: return {Vec3(0, 6.5f, 5.5f), Vec3(5.5f, 4.5f, 8.0f), 7};
    case BodyPlan::Bird: return {Vec3(0, 11.0f, 5.5f), Vec3(2.5f, 5.0f, 3.0f), 9};
    case BodyPlan::Robot: return {Vec3(0, 13.2f, 3.2f), Vec3(4.6f, 7.0f, 3.5f), 10};
    case BodyPlan::Serpent: return {Vec3(0, 9.0f, 9.0f), Vec3(0, 8.0f, 9.5f), 8};
  }
  return {Vec3(0, 12, 6), Vec3(3, 8, 5), 10};
}
Vec3 rotY(const Vec3& v, float a) { float c = std::cos(a), s = std::sin(a); return Vec3(v.x * c + v.z * s, v.y, -v.x * s + v.z * c); }
enum { PROPS_NONE, PROPS_SHATTER, PROPS_FLING };
}  // namespace

const MonsterDef& Kaiju::def() const { return T.monsters[(size_t)std::max(0, std::min(monster, (int)T.monsters.size() - 1))]; }
// size: species scale x rarity size bonus x growth (hatchling ~0.42 -> 1, capped); legacy / un-rolled monsters are full size
float Kaiju::scale() const {
  float s = (!G || !G->hasLook(def().id)) ? def().scale : def().scale * G->rarityOf(def().id).size * eggs::growthScale(G->kaijuXp());
  if (G && G->hasInfinity("power")) s *= 1.2f;
  return s;
}
float Kaiju::radius() const { return C.kaijuRadius * scale(); }
static float vmult(const Kaiju& k, vpow::Stat st) { return G ? G->vault.mult(k.def().id, st) : 1.f; }  // Wren: Vault kaiju powers
float Kaiju::maxHp() const { return C.kaijuMaxHp * def().hp * (G && G->hasLook(def().id) ? G->rarityOf(def().id).hp : 1.f) * vmult(*this, vpow::ST_HP); }
Vec3 Kaiju::mouth() const { Anchors a = anchorsFor(def().plan); Vec3 m = rotY(a.mouth * scale(), facing + twist); return pos + m + Vec3(0, lift + (inWater ? -2.f : 0.f), 0); }
Vec3 Kaiju::hand() const { Anchors a = anchorsFor(def().plan); Vec3 m = rotY(a.hand * scale(), facing + twist); return pos + m + Vec3(0, lift + (inWater ? -2.f : 0.f), 0); }
float Kaiju::moveRadius(const MoveDef& m) const { return m.radius * prog::rangeMult(G->P().rangeLvl) * vmult(*this, vpow::ST_RANGE) * (G ? G->stoneRangeMul((int)m.kind) : 1.f); }
float Kaiju::moveBricks(const MoveDef& m) const {
  float baby = G->hasLook(def().id) ? lerpf(E.babyPower, 1.f, G->growthFrac()) : 1.f;  // small hatchlings hit harder for their size (fair early game)
  return m.bricks * prog::damageMult(G->P().dmgLvl) * (m.kind == MoveKind::Melee ? def().melee : def().power) * baby * vmult(*this, vpow::ST_DAMAGE) * (G ? G->stoneBrickMul((int)m.kind) : 1.f);
}

void Kaiju::respawn(float x, float z, float f) {
  pos = Vec3(x, 0, z); facing = f; cam.yaw = facing + PI; cam.pitch = 0.32f; hp = maxHp(); at = Attack(); charging = false;
  cooldowns.assign(T.moves.size(), 0.f); projectiles.clear(); bolts.clear(); eats.clear(); stompAim = false; stompPendingT = 0; stompFlashT = 0; gloveDist = 0; gloveLast = pos;
  if (held >= 0) { G->bodies.list[held].held = false; held = -1; }
  G->world.updateStreaming(pos.x, pos.z, true);
  updateCamera(1, true);
}
void Kaiju::damage(float n) {
  if (G->mode != Mode::Play || n <= 0 || !G->menus.empty()) return;
  n *= G->enemyDamageMult();  // Crazy difficulty: enemies (and THE BULK) hit twice as hard
  if (G->zoo.active || G->deevoT >= 0) return;                // the zoo opening / De-Evolution cinematic can't hurt you
  if (G->hasLook(def().id)) {
    n *= lerpf(E.babyDamageTaken, 1.f, G->growthFrac());     // hatchlings take less damage while they grow
    if (G->lookOf(def().id).has("tough")) n *= 0.8f;          // Tough Scales passive
  }
  if (hurtT <= 0) hitSeq++;  // model: one hit-react per burst of damage
  hp = std::max(0.f, hp - n); hurtT = 0.3f;
  G->onKaijuHit();
  if (frand() < 0.1f) G->play(SND_hurt, 0.6f);
  if (hp <= 0 && !G->coopSwapped) G->gameOver("ko");  // Titan: player 2 naps instead
}
float Kaiju::stompStunRadius() const { return C.stompStunRadius * std::max(1.f, scale()); }
float Kaiju::eatRange() const { return C.eatRange * std::max(1.f, scale()) * (G && G->hasLook(def().id) && G->lookOf(def().id).has("bigbelly") ? 1.35f : 1.f); }
int Kaiju::projectilesOf(int mi) const {
  int n = 0;
  for (auto& p : projectiles) n += p.move == mi;
  for (auto& b : bolts) n += b.move == mi;
  return n;
}
bool Kaiju::tryStart(int mi) {
  if (mi < 0 || mi >= (int)T.moves.size()) return false;
  const MoveDef& m = T.moves[mi];
  if (m.rapid) {  // quick particle / projectile weapons: no cooldown, every press fires (up to the on-screen cap)
    if (charging) return false;
    if (projectilesOf(mi) >= prog::projectileCap()) return false;
    if (at.move < 0 || T.moves[at.move].rapid) { at = Attack(); at.move = mi; at.dur = m.duration; at.power = 1; at.hitDone = true; attackSeq++; }  // short mouth / arm pose
    G->play(soundByName(m.sound, SND_claw), m.shape == MoveShape::Blaster ? 0.8f : 0.6f, 0.95f + frand() * 0.15f);
    G->tutorialUsed(m.id);
    if (m.shape == MoveShape::Blaster) { fireBlaster(mi); return true; }
    // Glow Breath puff: flies from the mouth down to the ground about `reach` metres ahead
    Vec3 f = fwd(), mo = mouth();
    float reach = m.reach * prog::rangeMult(G->P().rangeLvl) * vmult(*this, vpow::ST_RANGE) * std::max(1.f, scale()) * (G ? G->stoneRangeMul((int)m.kind) : 1.f);
    Vec3 tgt = pos + f * reach; tgt.y = 0;
    Vec3 dir = tgt - mo; float l = length(dir); dir = dir * (1.f / std::max(0.01f, l));
    Projectile p; p.move = mi; p.pos = mo + f * 1.5f; p.t = 0; p.lob = false;
    p.vel = dir * m.speed; p.life = l / std::max(1.f, m.speed) + 0.4f;
    projectiles.push_back(p);
    return true;
  }
  if (at.move >= 0 && !T.moves[at.move].rapid) return false;
  if (charging) return false;
  if (cooldowns[mi] > 0) return false;
  if (m.kind == MoveKind::Super && !G->spendEat(m, 1)) return false;  // supers are fuelled by eaten civilians (no cooldown)
  at = Attack(); at.move = mi; at.dur = m.duration; at.power = 1; attackSeq++;
  if (m.kind == MoveKind::Super) G->onSuperFired();
  cooldowns[mi] = m.cooldown;  // supers have cooldown 0 in moves.json (the glove keeps its own)
  G->play(soundByName(m.sound, SND_claw), 1, 0.95f + frand() * 0.1f);
  G->tutorialUsed(m.id);
  if (m.shape == MoveShape::Projectile || m.shape == MoveShape::Lob) {
    Vec3 f = fwd(), mo = mouth();
    Projectile p; p.move = mi; p.pos = mo + f * 2; p.t = 0; p.lob = m.shape == MoveShape::Lob;
    p.vel = p.lob ? f * m.speed + Vec3(0, 16, 0) : f * m.speed + Vec3(0, -3, 0);
    p.life = m.reach * (G ? G->stoneRangeMul((int)m.kind) : 1.f) / std::max(1.f, m.speed) + 1.0f;
    projectiles.push_back(p);
  }
  return true;
}
void Kaiju::fireBlaster(int mi) {
  const MoveDef& m = T.moves[mi];
  Vec3 aim; G->aimTarget(aim);
  Vec3 h = hand();
  Vec3 flat(aim.x - pos.x, 0, aim.z - pos.z);
  if (length(flat) > 2) facing = std::atan2(flat.x, flat.z);  // turn to face the shot
  h = hand();
  Vec3 dir = aim - h; float l = length(dir); dir = l > 1e-3f ? dir * (1.f / l) : fwd();
  Bolt b; b.move = mi; b.pos = h; b.vel = dir * m.speed; b.t = 0;
  b.life = (m.reach * prog::rangeMult(G->P().rangeLvl) * vmult(*this, vpow::ST_RANGE) * (G ? G->stoneRangeMul((int)m.kind) : 1.f) + 40) / std::max(1.f, m.speed);
  bolts.push_back(b);
  aimFlash = 0.12f;
  for (int k = 0; k < 6; k++) G->fx->particle(h, dir * 8 + Vec3((frand() - 0.5f) * 4, (frand() - 0.5f) * 4, (frand() - 0.5f) * 4), 0.3f, 0.5f, hexColor(k % 2 ? 0xffe03a : 0xff6bd5));
}
void Kaiju::updateBolts(float dt) {
  aimFlash = std::max(0.f, aimFlash - dt);
  for (int i = (int)bolts.size() - 1; i >= 0; i--) {
    Bolt& b = bolts[i]; const MoveDef& m = T.moves[b.move];
    b.t += dt;
    bool done = b.t > b.life; Vec3 impact = b.pos; bool boom = false;
    int steps = std::max(1, (int)std::ceil(length(b.vel) * dt / 1.5f));
    for (int st = 0; st < steps && !done; st++) {
      Vec3 a = b.pos; b.pos += b.vel * (dt / steps);
      // civilians: segment vs. person sphere -> that person becomes a rainbow (counts like smashing)
      Vec3 seg = b.pos - a; float sl = length(seg); Vec3 sd = sl > 1e-4f ? seg * (1.f / sl) : Vec3(0, 0, 1);
      float tp = 0; int pi = G->people.pickRay(a, sd, 0, sl, C.blasterAimRadius, &tp);
      if (pi >= 0) { G->people.popOne(pi); G->onPeoplePopped(1); done = true; impact = a + sd * tp; G->fx->rainbowBurst(impact, 1.2f); break; }
      if (b.pos.y <= 0.3f) { done = true; boom = true; impact = b.pos; impact.y = 0.5f; break; }
      bool hitB = false;
      G->world.buildingsNear(b.pos.x, b.pos.z, 2, [&](Building& bd) { if (!hitB && bd.health() > 0.05f && bd.intersects(b.pos.x, b.pos.y, b.pos.z, 0.6f)) hitB = true; });
      if (hitB) { done = true; boom = true; impact = b.pos; break; }
      for (auto& u : G->enemies.units) if (!u.wreck && length(u.pos - b.pos) < (u.air() ? 7.f : 4.f)) { done = true; boom = true; impact = b.pos; break; }
      if (G->bossHitTest(b.pos, 0.5f)) { done = true; impact = b.pos; G->hitBossArea(b.move, b.pos.x, b.pos.y, b.pos.z, 1, moveBricks(m) * 2); break; }
    }
    if (frand() < 0.8f) { static const uint32_t rb[] = {0xff9a1f, 0xffe03a, 0x4cd964, 0x3ad7ff, 0x5a6bff, 0xb06bff}; G->fx->particle(b.pos, Vec3((frand() - 0.5f) * 2, (frand() - 0.5f) * 2, (frand() - 0.5f) * 2), 0.35f, 0.45f, hexColor(rb[(int)(frand() * 6) % 6])); }
    if (!done) continue;
    int mv = b.move; bolts.erase(bolts.begin() + i);
    if (boom) {
      float r = moveRadius(m);
      hit(mv, impact.x, std::max(1.f, impact.y), impact.z, r, PROPS_SHATTER);
      G->fx->ring(impact.x, impact.z, r * 1.3f, 0.4f, m.color);
      G->fx->rainbowBurst(impact, 0.8f);
    }
  }
}
bool Kaiju::eat() {
  int i = G->people.nearestInRange(pos.x, pos.z, eatRange());
  if (i < 0) return false;  // nobody close enough: nothing happens
  Person& p = G->people.list[i];
  EatTween e; e.from = Vec3(p.x, p.y, p.z); e.t = 0; e.look = p.look; eats.push_back(e);
  p.active = false;
  Vec3 flat(p.x - pos.x, 0, p.z - pos.z); if (length(flat) > 1) facing = std::atan2(flat.x, flat.z);
  G->P().eaten++; G->P().eatenTotal++;  // eaten = super-attack fuel
  G->onPeoplePopped(1);  // counts as a rainbow (tutorial, coins) exactly like smashing / shooting
  G->healKaiju(E.eatHeal);  // a snack heals a little (at full HP it becomes XP)
  { Vec3 at = pos + Vec3(0, 12 * scale(), 0); G->addXp(E.xpFor("eat"), &at); }
  G->play(SND_gulp, 0.9f, 0.95f + frand() * 0.15f);
  G->tutorialUsed("eat");
  return true;
}
void Kaiju::updateEats(float dt) {
  for (int i = (int)eats.size() - 1; i >= 0; i--) {
    EatTween& e = eats[i]; e.t += dt;
    if (e.t >= C.eatTweenTime) { G->fx->rainbowBurst(mouth(), 1.0f); eats.erase(eats.begin() + i); }
  }
}

HitResult Kaiju::hit(int mi, float cx, float cy, float cz, float r, int props, float maxBricks) {
  const MoveDef& m = T.moves[mi];
  float bricks = maxBricks > 0 ? maxBricks : moveBricks(m);
  float imp = m.impulse * (1 + 0.08f * (G->P().dmgLvl - 1));
  HitResult res = G->world.damageSphere(cx, cy, cz, r, bricks, pos.x, pos.z, imp);
  if (res.bricks > 0) {
    int coins = res.bricks * (int)(m.kind == MoveKind::Melee ? C.coinsPerBrick : C.coinsPerBrickOther);
    G->awardCoins(coins, Vec3(cx, cy + 4, cz));
    cam.shake = std::min(1.2f, cam.shake + 0.25f + res.bricks * 0.01f);
    G->vibrate(std::min(1.f, 0.3f + res.bricks * 0.02f), 0.14f);
    if (m.kind == MoveKind::Melee) G->healKaiju(res.bricks * E.lifesteal);  // melee lifesteal: +1 HP per block (overflow -> XP)
    { Vec3 at(cx, cy + 6, cz); G->addXp(res.bricks * E.xpFor("brick"), &at); }
  }
  G->people.pop(cx, cz, r);
  G->people.knock(cx, cz, r + 3, imp * 0.4f * (m.id == "honk" ? 2.5f : 1.f));
  G->enemies.hitArea(cx, cz, r, mi, false);
  if (m.slows) G->enemies.slowArea(cx, cz, r, 4);
  G->world.pushFerry(cx, cz, r, imp * 0.6f);
  G->bodies.inRadius(cx, cz, r, [&](int i, float) {
    Body& b = G->bodies.list[i];
    if (b.kind == BODY_ROCK && props == PROPS_SHATTER) { G->bodies.shatter(i); return; }
    float dx = b.pos.x - pos.x, dz = b.pos.z - pos.z, l = std::hypot(dx, dz); if (l < 1e-3f) l = 1;
    float f = props == PROPS_FLING ? imp * 1.6f : imp * 0.6f;
    G->bodies.fling(i, dx / l * f, 8 + f * 0.4f, dz / l * f, props == PROPS_FLING);
  });
  if (m.glow) G->hitEgg(mi, cx, cz, r);
  G->hitBossArea(mi, cx, cy, cz, r, bricks);
  return res;
}

void Kaiju::grabOrThrow() {
  Vec3 f = fwd();
  if (held >= 0) {
    Body& b = G->bodies.list[held]; held = -1; b.held = false;
    b.pos.y = std::max(b.pos.y, 8.f);
    G->bodies.fling((int)(&b - &G->bodies.list[0]), f.x * C.throwSpeed, 12, f.z * C.throwSpeed, true);
    b.lying = false; G->play(SND_throwWhoosh);
    at = Attack(); at.throwAnim = true; at.dur = 0.3f; at.hitDone = true;
    return;
  }
  int i = G->bodies.nearest(pos.x + f.x * 3 * scale(), pos.z + f.z * 3 * scale(), C.treePickRange * std::max(1.f, scale()), true);
  if (i < 0) { G->toast("Walk up to a tree or rock to pick it up"); return; }
  Body& b = G->bodies.list[i];
  b.held = true; b.resting = true; b.thrown = false; b.lying = false; b.rx = 0; b.rz = 0;
  held = i; G->tutorialUsed("grab"); G->play(SND_ui);
}

void Kaiju::update(float dt, const InputState& in) {
  if (cooldowns.size() != T.moves.size()) cooldowns.assign(T.moves.size(), 0.f);
  for (auto& c : cooldowns) c = std::max(0.f, c - dt);
  hurtT = std::max(0.f, hurtT - dt);
  bool calm = G->enemies.stars() == 0 && G->enemies.units.empty();
  if (G->boss.active && G->boss.staged && G->boss.stage >= 2 && !G->boss.dead) calm = false;  // no free regen in the real BULK fight
  if (calm || G->save.settings.kidMode) hp = std::min(maxHp(), hp + C.kaijuRegen * (calm ? 1.f : 0.35f) * dt);  // natural regen only refills missing HP (never converts to XP)
  if (G->hasLook(def().id) && G->lookOf(def().id).has("regen")) hp = std::min(maxHp(), hp + 1.5f * dt);  // Healing Hugs passive
  popT = std::max(0.f, popT - dt);
  Profile& P = G->P(); const Settings& S = G->save.settings;
  // ---- camera input (mouse look behaves like the right stick)
  bool lookActive = std::fabs(in.lookX) + std::fabs(in.lookY) > 0 || in.mouseDX != 0 || in.mouseDY != 0;
  float inv = S.invertY ? -1.f : 1.f, ms = C.camMouseSpeed * S.mouseSens;
  cam.yaw -= in.lookX * C.camStickSpeed * dt + in.mouseDX * ms;
  cam.pitch += (in.lookY * C.camStickSpeed * 0.6f * dt + in.mouseDY * ms) * inv;
  cam.pitch = clampf(cam.pitch, -0.15f, 1.2f);
  cam.dist = clampf(cam.dist - in.wheel * 3, 18, 70);
  cam.idle = lookActive ? 0 : cam.idle + dt;
  if (in.pressed[A_CAMRESET]) { cam.yaw = facing + PI; cam.pitch = 0.32f; }
  // ---- movement relative to the camera
  float fx = -std::sin(cam.yaw), fz = -std::cos(cam.yaw), rx = -fz, rz = fx;
  float mx = fx * in.moveY + rx * in.moveX, mz = fz * in.moveY + rz * in.moveX;
  float mag = std::min(1.f, std::hypot(mx, mz));
  const MoveDef* am = at.move >= 0 ? &T.moves[at.move] : nullptr;
  float speed = C.kaijuSpeed * prog::speedMult(P.speedLvl) * def().speed * vmult(*this, vpow::ST_SPEED) * (in.held[A_SPRINT] ? C.kaijuSprint : 1.f);
  if (G->hasInfinity("space")) speed *= 1.35f;
  inWater = G->world.isWater(pos.x, pos.z);
  if (inWater) speed *= C.waterSlow;
  if (am) speed *= am->shape == MoveShape::Tornado ? 1.2f : am->shape == MoveShape::Spin ? 0.7f : am->shape == MoveShape::Dash ? 0.f : 0.3f;
  if (at.throwAnim) speed *= 0.6f;
  if (charging) speed *= 0.15f;
  if (mag > 0.05f) {
    float target = std::atan2(mx, mz);
    if (!(am && am->shape == MoveShape::Beam)) facing += wrapAngle(target - facing) * std::min(1.f, C.kaijuTurn * dt);
    pos.x += mx / mag * mag * speed * dt; pos.z += mz / mag * mag * speed * dt;
    if (cam.idle > C.camAutoFollowDelay && in.moveY > -0.3f) cam.yaw += wrapAngle((facing + PI) - cam.yaw) * std::min(1.f, C.camAutoFollowRate * dt);
  }
  speedNow = mag * speed;
  sprinting = in.held[A_SPRINT] && mag > 0.05f;   // the model's run clip follows the Shift / L3 intent (not a speed threshold)
  if (G->gloveEquipped() && G->mode == Mode::Play) {  // secret glove: one pulse per gloveStep metres walked, nothing while standing still
    float moved = dist2D(pos.x, pos.z, gloveLast.x, gloveLast.z);
    if (moved < 20) gloveDist += moved;  // ignore teleports / respawns
    if (gloveDist >= C.gloveStep) { gloveDist = std::fmod(gloveDist, std::max(0.5f, C.gloveStep)); G->glovePulse(); }
  } else gloveDist = 0;
  gloveLast = pos;
  if (mag > 0.05f) G->tutorialUsed("move");
  G->world.resolveCircle(pos, radius());
  if (mag > 0.05f) for (auto& u : G->enemies.units) {  // push through crowds of small ground units (cops / soldiers) instead of getting pinned
    if (u.wreck || u.air() || u.tank()) continue;
    Vec3 d(u.pos.x - pos.x, 0, u.pos.z - pos.z); float l = length(d), minD = radius() + 2.5f;
    if (l < minD) { if (l < 1e-3f) d = Vec3(1, 0, 0), l = 1; u.pos += d * ((minD - l) / l); }
  }
  if (G->boss.active && !G->boss.dead) {  // can't walk through THE BULK (his hitbox grows with him)
    Vec3 d(pos.x - G->boss.pos.x, 0, pos.z - G->boss.pos.z); float l = length(d), minD = G->boss.radius() + radius() * 0.8f;
    if (l < minD) { if (l < 1e-3f) d = Vec3(0, 0, 1), l = 1; pos += d * ((minD - l) / l); }
  }
  if (mag > 0.05f) G->bodies.inRadius(pos.x, pos.z, radius(), [&](int i, float) {
    Body& b = G->bodies.list[i]; if (!b.resting) return;
    float dx = b.pos.x - pos.x, dz = b.pos.z - pos.z, l = std::hypot(dx, dz); if (l < 1e-3f) l = 1;
    G->bodies.fling(i, dx / l * 10, 5, dz / l * 10, false);
  });
  if (inWater && mag > 0.1f && frand() < dt * 20) G->fx->particle(pos + Vec3((frand() - 0.5f) * 6, 1, (frand() - 0.5f) * 6), Vec3((frand() - 0.5f) * 4, 5 + frand() * 4, (frand() - 0.5f) * 4), 0.6f, 0.8f, hexColor(0xbfe8ff));
  // ---- melee: X/B/A (J/K/L), alt set with LB held (or U/I/O)
  const Action meleeBtn[3] = {A_CLAW, A_TAIL, A_STOMP}, altBtn[3] = {A_ALT1, A_ALT2, A_ALT3};
  for (int b = 0; b < 3; b++) {
    int bind = -1;
    if (in.pressed[meleeBtn[b]]) bind = in.held[A_ALT] ? b + 3 : b;
    if (in.pressed[altBtn[b]]) bind = b + 3;
    if (bind < 0) continue;
    const std::string& id = P.melee[bind];
    if (id.empty()) { G->toast("No move on this button yet - set it in Pause > Melee loadout"); continue; }
    if (id == "stomp") { stompAim = true; stompAct = in.pressed[meleeBtn[b]] ? meleeBtn[b] : altBtn[b]; continue; }  // hold = preview ring, release = stomp
    tryStart(T.moveIndex(id));
  }
  if (stompAim && !in.held[stompAct]) { stompAim = false; stompPendingT = 0.5f; }
  if (stompPendingT > 0) {  // released: stomp right away (or as soon as the current move ends)
    stompPendingT -= dt;
    if (tryStart(T.moveIndex("stomp"))) stompPendingT = 0;
  }
  stompFlashT = std::max(0.f, stompFlashT - dt);
  if (in.pressed[A_GRAB]) grabOrThrow();  // Game::oneButtonEat already cleared A_GRAB if this press ate (re-checking here missed it: the target was just eaten)
  // ---- power slots: slot 1 = RT / F, slot 2 = LT / Q
  for (int i = 0; i < 2; i++) {
    Action act = i == 0 ? A_SLOT1 : A_SLOT2;
    if (!in.pressed[act]) continue;
    if (i >= P.slotCount || P.slots[i].empty()) { G->toast("Empty skill slot - pick a skill with " + G->btn(i == 0 ? "D-pad left" : "D-pad right", i == 0 ? "1" : "2") + " or the skills list"); continue; }
    int mi = T.moveIndex(P.slots[i]); if (mi < 0) continue;
    const MoveDef& m = T.moves[mi];
    if (m.chargeTime > 0) {
      if (at.move < 0 && !charging && cooldowns[mi] <= 0) {
        int minCost = m.kind == MoveKind::Super ? G->superEatCost(m, 0) : 0;
        if (G->P().eaten < minCost) { G->spendEat(m, 0); continue; }  // not even a small roar's worth: shows 'Need N eaten'
        charging = true; charge = 0; chargeSlot = mi; chargeBtn = act; G->play(SND_roarCharge);
      }
    } else tryStart(mi);
  }
  if (charging) {
    const MoveDef& m = T.moves[chargeSlot];
    charge = std::min(1.f, charge + dt / m.chargeTime);  // charge time is never scaled (Crazy scales the eat cost instead)
    cam.shake = std::max(cam.shake, charge * 0.25f);
    if (frand() < dt * 30) { float a = frand() * 6.28f; G->fx->particle(pos + Vec3(std::cos(a) * 9, 0.5f + frand() * 12, std::sin(a) * 9), Vec3(-std::cos(a) * 9, 0, -std::sin(a) * 9), 0.9f, 0.6f, def().accent); }
    if (!in.held[chargeBtn]) {  // hold to build up (stays at full while held), release to fire; no cooldown afterwards
      charging = false; float power = 0.35f + 0.65f * charge;
      if (m.kind != MoveKind::Super || G->spendEat(m, charge)) {  // cost checked + spent on release (scales with charge)
        at = Attack(); at.move = chargeSlot; at.dur = m.duration; at.power = power; cooldowns[chargeSlot] = 0;
        if (m.kind == MoveKind::Super) G->onSuperFired();
        G->play(soundByName(m.sound, SND_roar), 0.5f + 0.5f * power); G->tutorialUsed(m.id);
      }
    }
  }
  updateAttack(dt);
  updateProjectiles(dt);
  updateBolts(dt);
  updateEats(dt);
  if (held >= 0) {
    Body& b = G->bodies.list[held];
    b.pos = hand();
    if (b.kind == BODY_TREE) { b.rx = -0.5f; b.ry = facing + twist; b.rz = 0; } else b.pos.y += 0.5f;
  }
  walkPhase += dt * (2 + speedNow * 0.35f / std::max(0.6f, scale()));
  updateCamera(dt, false);
}

void Kaiju::updateAttack(float dt) {
  twist = 0; lift = 0;
  if (at.throwAnim) { at.t += dt; if (at.t >= at.dur) at = Attack(); return; }
  if (at.move < 0) return;
  const MoveDef& m = T.moves[at.move]; int mi = at.move;
  at.t += dt; float k = std::min(1.f, at.t / at.dur), sc = scale();
  Vec3 f = fwd(), p = pos;
  float r = moveRadius(m) * std::max(1.f, sc * 0.9f), reach = m.reach * prog::rangeMult(G->P().rangeLvl) * vmult(*this, vpow::ST_RANGE) * sc * (G ? G->stoneRangeMul((int)m.kind) : 1.f);
  auto once = [&](float when, auto fn) { if (!at.hitDone && k >= when) { at.hitDone = true; fn(); } };
  auto twice = [&](float when, auto fn) { if (!at.hit2 && k >= when) { at.hit2 = true; fn(); } };
  auto ticks = [&](float every, auto fn) { at.tick -= dt; if (at.tick <= 0) { at.tick += every; fn(); } };
  auto stompFx = [&](float R, float power) {
    G->fx->ring(p.x, p.z, R * 1.4f, 0.6f, m.color); G->fx->dust(p.x, p.z, 24, 4);
    G->debris->kick(p.x, p.z, R * 1.8f, power); G->people.knock(p.x, p.z, R * 1.8f, 8);
    cam.shake = std::min(1.5f, cam.shake + 0.6f); G->vibrate(0.8f, 0.25f);
  };
  switch (m.shape) {
    case MoveShape::Front: {
      twist = std::sin(k * PI) * -0.5f;
      auto front = [&]() { hit(mi, p.x + f.x * reach, 7 * sc, p.z + f.z * reach, r, PROPS_SHATTER); };
      if (m.id == "doubleslap") { once(0.3f, front); twice(0.7f, front); }
      else once(0.45f, [&]() {
        front();
        if (m.id == "uppercut") { G->debris->kick(p.x + f.x * reach, p.z + f.z * reach, r * 1.5f, 26); G->fx->ring(p.x + f.x * reach, p.z + f.z * reach, r, 0.4f, m.color); }
      });
      break;
    }
    case MoveShape::Rear: {
      twist = std::sin(k * PI) * 2.2f;
      once(0.5f, [&]() {
        const float angs[3] = {-1.0f, 0, 1.0f};
        for (float a0 : angs) { float a = facing + PI + a0, d = reach + 2; hit(mi, p.x + std::sin(a) * d, 3.5f, p.z + std::cos(a) * d, r, PROPS_FLING, moveBricks(m) / 2); }
      });
      break;
    }
    case MoveShape::Around: {
      float hmax = m.id == "poundhop" ? 9.f : m.id == "belly" ? 1.5f : 3.5f, when = m.id == "belly" ? 0.5f : 0.75f;
      lift = k < when - 0.15f ? std::sin(k / (when - 0.15f) * PI / 2) * hmax : hmax * (1 - (k - (when - 0.15f)) / 0.15f);
      if (lift < 0) lift = 0;
      lift *= sc;
      once(when, [&]() {
        hit(mi, p.x, 2, p.z, r, PROPS_SHATTER); stompFx(r, m.id == "poundhop" ? 22.f : 14.f);
        if (m.id == "stomp") {  // stun everyone left standing inside the stomp ring (same radius as the preview)
          float R = stompStunRadius();
          if (G->people.stun(p.x, p.z, R, C.stompStunTime) > 0) G->play(SND_stun, 0.7f);
          stompFlashT = 0.35f; G->fx->ring(p.x, p.z, R, 0.35f, hexColor(0xffe03a));
        }
      });
      break;
    }
    case MoveShape::Spin: {
      twist = k * PI * 4;
      ticks(0.15f, [&]() { hit(mi, p.x, 4, p.z, r, PROPS_FLING); });
      if (frand() < 0.5f) G->fx->dust(p.x, p.z, 2, r * 0.8f);
      break;
    }
    case MoveShape::Dash: {
      pos += f * (reach / at.dur) * dt; G->world.resolveCircle(pos, radius() * 0.5f);
      lift = 0; twist = 0;
      ticks(0.1f, [&]() { hit(mi, pos.x + f.x * 3 * sc, 3, pos.z + f.z * 3 * sc, r, PROPS_FLING); G->fx->dust(pos.x, pos.z, 3, 2); });
      break;
    }
    case MoveShape::Cone: {
      Vec3 mo = mouth();
      for (int i = 0; i < 4; i++) {
        float s = 22 + frand() * 10, a = facing + (frand() - 0.5f) * (m.id == "honk" ? 0.9f : 0.35f);
        Color c = m.id == "zap" ? (frand() < 0.5f ? hexColor(0xffffff) : m.color) : (frand() < 0.3f ? hexColor(0xffffff) : m.color);
        G->fx->particle(mo, Vec3(std::sin(a) * s, -3 + (frand() - 0.5f) * 6, std::cos(a) * s), 0.9f, 0.9f + frand() * 0.8f, c);
      }
      ticks(0.12f, [&]() {
        for (int i = 1; i <= 4; i++) {
          float d = reach * i / 4, rr = r * (0.6f + i * 0.25f);
          hit(mi, mo.x + f.x * d, std::max(2.f, mo.y - d * 0.2f), mo.z + f.z * d, rr, m.id == "honk" ? PROPS_FLING : PROPS_NONE);
        }
      });
      break;
    }
    case MoveShape::Beam: {
      Vec3 mo = mouth(); float len = reach * std::min(1.f, k * 4);
      ticks(0.1f, [&]() { for (float d = 4; d < len; d += 5) hit(mi, mo.x + f.x * d, std::max(2.f, mo.y - d * 0.06f), mo.z + f.z * d, r, PROPS_NONE); });
      break;
    }
    case MoveShape::Projectile: case MoveShape::Lob: twist = std::sin(k * PI) * -0.2f; break;
    case MoveShape::Roar: {
      once(0.1f, [&]() {
        float R = r * at.power;
        G->fx->shell(Vec3(p.x, 8, p.z), R, 0.8f, at.power > 0.95f ? hexColor(0xffffff) : m.color); G->fx->ring(p.x, p.z, R, 0.9f, m.color);
        if (at.power > 0.95f) { G->fx->ring(p.x, p.z, R * 1.25f, 1.1f, hexColor(0xffe03a)); G->fx->rainbowBurst(Vec3(p.x, 10, p.z), 4); }
        const float rings[3] = {0.35f, 0.7f, 1.0f};
        for (float q : rings) hit(mi, p.x, 6, p.z, R * q, PROPS_FLING, moveBricks(m) * at.power / 3);
        G->debris->kick(p.x, p.z, R * 1.3f, 22 * at.power); G->people.knock(p.x, p.z, R * 1.4f, 10);
        cam.shake = 1.6f * at.power; G->vibrate(1, 0.4f);
        G->toast(at.power > 0.95f ? "MEGA ROAR!!" : "ROAR!");
      });
      break;
    }
    case MoveShape::Quake: {
      ticks(0.3f, [&]() {
        float R = r * (0.3f + 0.7f * k);
        hit(mi, p.x, 3, p.z, R, PROPS_FLING, moveBricks(m) / 5);
        G->fx->ring(p.x, p.z, R, 0.5f, m.color); G->debris->kick(p.x, p.z, R, 12); G->people.knock(p.x, p.z, R, 6);
        cam.shake = std::min(1.6f, cam.shake + 0.5f); G->vibrate(0.9f, 0.3f);
      });
      break;
    }
    case MoveShape::Meteor: {
      ticks(0.22f, [&]() {
        float a = frand() * 6.283f, d = 8 + frand() * reach;
        Projectile pr; pr.move = mi; pr.pos = Vec3(p.x + std::cos(a) * d, 70, p.z + std::sin(a) * d); pr.vel = Vec3((frand() - 0.5f) * 6, -55, (frand() - 0.5f) * 6);
        pr.t = 0; pr.life = 4; pr.lob = true; projectiles.push_back(pr);
      });
      break;
    }
    case MoveShape::Nova: {
      once(0.3f, [&]() {
        G->fx->shell(Vec3(p.x, 8, p.z), r, 1.0f, m.color); G->fx->ring(p.x, p.z, r, 1.0f, hexColor(0x3ad7ff));
        hit(mi, p.x, 6, p.z, r * 0.5f, PROPS_FLING, moveBricks(m) / 2); hit(mi, p.x, 6, p.z, r, PROPS_FLING, moveBricks(m) / 2);
        G->debris->kick(p.x, p.z, r * 1.3f, 26); cam.shake = 1.5f; G->vibrate(1, 0.5f);
      });
      break;
    }
    case MoveShape::Blaster: break;  // bolts are fired in tryStart
    case MoveShape::Glove:  // pressing the slot just flexes the glove; the real power comes from walking around
      once(0.05f, [&]() { G->fx->rainbowBurst(hand(), 1.2f); G->toast("Keep moving - the Glove flattens the city as you walk!"); });
      break;
    case MoveShape::Tornado: {
      twist = at.t * 18;
      ticks(0.2f, [&]() { hit(mi, pos.x, 4, pos.z, r, PROPS_FLING); });
      for (int i = 0; i < 3; i++) { float a = frand() * 6.283f, h = frand() * 22; G->fx->particle(pos + Vec3(std::cos(a) * (3 + h * 0.4f), h, std::sin(a) * (3 + h * 0.4f)), Vec3(-std::sin(a) * 12, 4, std::cos(a) * 12), 0.6f, 0.9f, m.color); }
      break;
    }
  }
  if (at.t >= at.dur) at = Attack();
}

void Kaiju::updateProjectiles(float dt) {
  for (int i = (int)projectiles.size() - 1; i >= 0; i--) {
    Projectile& p = projectiles[i];
    const MoveDef& m = T.moves[p.move];
    p.t += dt; if (p.lob) p.vel.y -= C.gravity * dt * (m.shape == MoveShape::Meteor ? 0.3f : 1.f);
    p.pos += p.vel * dt;
    if (frand() < 0.7f) G->fx->particle(p.pos, Vec3((frand() - 0.5f) * 3, (frand() - 0.5f) * 3, (frand() - 0.5f) * 3), 0.4f, 0.6f, m.color);
    bool boom = p.pos.y <= 0.5f || p.t > p.life;
    if (!boom) G->world.buildingsNear(p.pos.x, p.pos.z, 3, [&](Building& b) { if (!boom && b.health() > 0.05f && b.intersects(p.pos.x, p.pos.y, p.pos.z, 1.5f)) boom = true; });
    if (!boom) for (auto& u : G->enemies.units) if (!u.wreck && length(u.pos - p.pos) < (u.air() ? 7.f : 4.f)) { boom = true; break; }
    if (!boom && G->egg.active && G->egg.state == 0 && dist2D(G->egg.pos.x, G->egg.pos.z, p.pos.x, p.pos.z) < 6 && p.pos.y < 16) boom = true;
    if (!boom && G->bossHitTest(p.pos, 1.5f)) boom = true;
    if (!boom) continue;
    Vec3 at0 = p.pos; at0.y = std::max(1.f, at0.y);
    float r = moveRadius(m); int mv = p.move;
    projectiles.erase(projectiles.begin() + i);  // erase first: hit() may spawn effects but never projectiles
    hit(mv, at0.x, at0.y, at0.z, r, PROPS_SHATTER, m.shape == MoveShape::Meteor ? moveBricks(m) : -1);
    G->fx->ring(at0.x, at0.z, r * 1.2f, 0.5f, m.color);
    for (int k = 0; k < 10; k++) G->fx->particle(at0, Vec3((frand() - 0.5f) * 14, frand() * 10, (frand() - 0.5f) * 14), 0.6f, 1.0f, k % 2 ? m.color : hexColor(0xffffff));
    G->playAt(m.id == "bubble" ? SND_pop : SND_crumble, at0, 0.8f);
    cam.shake = std::min(1.2f, cam.shake + 0.2f);
  }
}

void Kaiju::updateCamera(float dt, bool snap) {
  // the camera follows a smoothed size so it pulls back gently as the kaiju grows
  float wantSc = scale();
  if (snap || camScale <= 0) camScale = wantSc; else camScale += (wantSc - camScale) * std::min(1.f, dt * E.camSmooth);
  float sc = std::max(0.8f * clampf(camScale / std::max(0.01f, def().scale), 0.5f, 1.f), camScale);
  Vec3 target(pos.x, anchorsFor(def().plan).camY * sc + (inWater ? -2.f : 0.f), pos.z);
  float cp = std::cos(cam.pitch), dist = cam.dist * sc;
  Vec3 want(target.x + std::sin(cam.yaw) * cp * dist, target.y + std::sin(cam.pitch) * dist + C.camHeight * 0.3f, target.z + std::cos(cam.yaw) * cp * dist);
  want.y = std::max(2.5f, want.y);
  cam.pos = snap ? want : cam.pos + (want - cam.pos) * std::min(1.f, dt * 10);
  cam.shake = std::max(0.f, cam.shake - dt * 2.5f);
  cam.target = target + Vec3(0, 2, 0);
}

void Kaiju::render(RenderList& rl, float time) {
  const MonsterDef& d = def();
  RigPose pose; pose.time = time;
  pose.walk = std::min(1.f, speedNow / C.kaijuSpeed); pose.walkPhase = walkPhase; pose.twist = twist;
  pose.sink = inWater ? -2.f : 0.f; pose.lift = lift + std::fabs(std::cos(walkPhase)) * 0.4f * pose.walk;
  float sw = std::sin(walkPhase) * 0.55f * pose.walk;
  pose.armR = -sw * 0.6f; pose.armL = sw * 0.6f;
  const MoveDef* m = at.move >= 0 ? &T.moves[at.move] : nullptr;
  float k = at.dur > 0 ? at.t / at.dur : 0;
  if (m && m->shape == MoveShape::Front) { pose.armR = k < 0.4f ? -1.8f * (k / 0.4f) : -1.8f + 3.2f * std::min(1.f, (k - 0.4f) / 0.3f); pose.armL = m->id == "doubleslap" ? pose.armR * -0.8f : -0.6f; }
  if (at.throwAnim) pose.armR = -2.2f + 3 * (at.t / at.dur);
  if (held >= 0) pose.armR = std::min(pose.armR, -1.2f);
  pose.jaw = 0.05f + std::max(0.f, std::sin(time * 0.7f)) * 0.05f;
  bool breathy = m && (m->id == "breath" || m->shape == MoveShape::Cone || m->shape == MoveShape::Beam || m->shape == MoveShape::Projectile || m->shape == MoveShape::Lob);
  if (breathy) { pose.jaw = 0.55f; pose.headX = 0.1f; }
  if (charging) { pose.headX = -0.4f * charge; pose.jaw = 0.2f; }
  if (m && (m->shape == MoveShape::Roar || m->shape == MoveShape::Nova)) { pose.jaw = 0.8f; pose.headX = -0.3f; }
  if (m && m->shape == MoveShape::Rear) pose.tailCurl = std::sin(k * PI);
  if (m && m->id == "belly") pose.squash = 1 + std::sin(k * PI) * 0.25f;
  pose.glow = 0.75f + std::sin(time * 3) * 0.15f;
  if (charging) pose.glow = 1 + charge * 0.8f + std::sin(time * 30) * 0.2f;
  if (m && (breathy || m->kind == MoveKind::Super)) pose.glow = 1.6f;
  if (!eats.empty()) { pose.jaw = 0.8f; pose.armR = -2.0f; }
  if (m && m->shape == MoveShape::Blaster) { pose.armR = -1.5f; pose.jaw = 0.1f; }
  if (hurtT > 0) pose.glow = 0.3f;
  if (!G->kaijuModel(rl, time)) {
    FinishMarks fm = finishMarks(rl);
    drawMonster(rl, G->lookedDef(monster), Vec3(pos.x, 0, pos.z), facing + twist, scale() * shrinkK * (1 + 0.25f * std::sin(std::min(1.f, popT / 0.6f) * PI)), pose);
    applyFinish(rl, fm, G->lookOf(d.id).finish, time);
  }
  // blob shadow
  float sh = 6.5f * scale() * (1 - std::min(0.6f, lift * 0.06f));
  rl.alpha[MESH_DISC].push_back(makeInstance(Mat4::scale(2 * sh, 1, 2 * sh) * Mat4::translation(pos.x, 0.09f, pos.z), Color{0, 0, 0, 0.35f}, 1));
  // beams
  if (m && m->shape == MoveShape::Beam) {
    Vec3 mo = mouth(); float len = m->reach * prog::rangeMult(G->P().rangeLvl) * scale() * std::min(1.f, k * 4);
    float wob = 1 + std::sin(at.t * 40) * 0.15f;
    Color c = m->color;
    if (m->id == "rainbow") { static const uint32_t rb[] = {0xff9a1f, 0xffe03a, 0x4cd964, 0x3ad7ff, 0x5a6bff, 0xb06bff}; c = hexColor(rb[(int)(time * 12) % 6]); }
    c.a = 0.75f;
    int beams = m->id == "peepers" ? 2 : 1;
    for (int bI = 0; bI < beams; bI++) {
      float side = beams == 2 ? (bI == 0 ? -0.9f : 0.9f) * scale() : 0;
      Vec3 o = mo + Vec3(std::cos(facing) * side, 0.6f * (beams - 1), -std::sin(facing) * side);
      float w = (m->id == "peepers" ? 1.2f : m->id == "rainbow" ? 3.6f : 3.0f) * wob;
      Mat4 base = Mat4::rotX(PI / 2 + 0.06f) * Mat4::rotY(facing) * Mat4::translation(o.x, o.y, o.z);
      rl.alpha[MESH_CYL].push_back(makeInstance(Mat4::translation(0, 0.5f, 0) * Mat4::scale(w, len, w) * base, c, 1));
      rl.opaque[MESH_CYL].push_back(makeInstance(Mat4::translation(0, 0.5f, 0) * Mat4::scale(w * 0.35f, len, w * 0.35f) * base, hexColor(0xffffff), 1));
    }
  }
  // Rainbow Blaster: a little rainbow pistol in the hand + bolts as short rainbow trails
  if (G->P().slots[1] == "blaster" || G->P().slots[0] == "blaster") {
    Vec3 h = hand(); Mat4 base = Mat4::rotY(facing) * Mat4::translation(h.x, h.y, h.z);
    rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(1.0f, 1.0f, 2.6f) * Mat4::translation(0, 0, 0.8f) * base, hexColor(0xff6bd5), 0.3f));
    rl.opaque[MESH_CYL].push_back(makeInstance(Mat4::scale(0.7f, 1.2f, 0.7f) * Mat4::rotX(PI / 2) * Mat4::translation(0, 0, 2.4f) * base, hexColor(0xffe03a), aimFlash > 0 ? 1.f : 0.5f));
  }
  static const uint32_t RB[] = {0xff9a1f, 0xffe03a, 0x4cd964, 0x3ad7ff, 0x5a6bff, 0xb06bff};
  for (auto& b : bolts) {
    Vec3 d = b.vel; float l = length(d); d = l > 1e-3f ? d * (1.f / l) : Vec3(0, 0, 1);
    for (int k = 0; k < 6; k++) { Vec3 q = b.pos - d * (k * 0.9f); float sz = 1.3f - k * 0.12f; rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(sz, sz, sz) * Mat4::translation(q.x, q.y, q.z), hexColor(RB[k]), 1)); }
  }
  // gulped civilians fly up into the mouth, shrinking (cartoon, no gore)
  for (auto& e : eats) {
    float k = clampf(e.t / std::max(0.05f, C.eatTweenTime), 0, 1), ease = k * k * (3 - 2 * k);
    Vec3 q = e.from + (mouth() - e.from) * ease + Vec3(0, std::sin(k * PI) * 4, 0); float sz = 1 - 0.8f * k;
    static const uint32_t SH[] = {0xff9a1f, 0xffd36b, 0x3ad7ff, 0x7aff8a, 0xff8ad8, 0xffffff, 0xffa040, 0x9a7aff};
    rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(0.5f * sz, 1.4f * sz, 0.5f * sz) * Mat4::rotZ(k * 8) * Mat4::translation(q.x, q.y, q.z), hexColor(SH[e.look % 8]), 1));
    rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(0.7f * sz, 0.7f * sz, 0.7f * sz) * Mat4::translation(q.x, q.y + 1.0f * sz, q.z), hexColor(0xf5d0b0), 1));
  }
  // stomp: hold shows the exact stun radius as a flat translucent ring; it pulses on impact, then hides
  if (stompAim || stompFlashT > 0) {
    float R = stompStunRadius(), pulse = stompFlashT > 0 ? 1 + (0.35f - stompFlashT) * 0.4f : 1;
    Color rc = hexColor(0xffe03a); rc.a = stompFlashT > 0 ? 0.95f * (stompFlashT / 0.35f) : 0.55f + 0.15f * std::sin(time * 8);
    rl.alpha[MESH_RING].push_back(makeInstance(Mat4::scale(2 * R * pulse, 2 * R, 2 * R * pulse) * Mat4::translation(pos.x, 0.15f, pos.z), rc, 1));
    Color dc = hexColor(0xffe03a); dc.a = stompFlashT > 0 ? 0.25f * (stompFlashT / 0.35f) : 0.12f;
    rl.alpha[MESH_DISC].push_back(makeInstance(Mat4::scale(2 * R * pulse, 1, 2 * R * pulse) * Mat4::translation(pos.x, 0.12f, pos.z), dc, 1));
  }
  for (auto& p : projectiles) {
    const MoveDef& pm = T.moves[p.move];
    if (pm.id == "breath") { Color c = pm.color; c.a = 0.55f; rl.alpha[MESH_SPHERE].push_back(makeInstance(Mat4::scale(3.6f, 3.6f, 3.6f) * Mat4::translation(p.pos.x, p.pos.y, p.pos.z), c, 1)); rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(1.4f, 1.4f, 1.4f) * Mat4::translation(p.pos.x, p.pos.y, p.pos.z), hexColor(0xffffff), 1)); }
    else if (pm.id == "bubble") { Color c = hexColor(0x9ae8ff, 0.45f); rl.alpha[MESH_SPHERE].push_back(makeInstance(Mat4::scale(5, 5, 5) * Mat4::translation(p.pos.x, p.pos.y, p.pos.z), c, 1)); }
    else if (pm.id == "boulder") rl.opaque[MESH_ROCK].push_back(makeInstance(Mat4::scale(5, 5, 5) * Mat4::rotX(p.t * 6) * Mat4::translation(p.pos.x, p.pos.y, p.pos.z), hexColor(0x8a8f99)));
    else rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(2.4f, 2.4f, 2.4f) * Mat4::translation(p.pos.x, p.pos.y, p.pos.z), pm.color, 1));
  }
  if (charging) { Color c = d.accent; c.a = 0.25f * charge; float R = 4 + charge * 6; rl.alpha[MESH_SPHERE].push_back(makeInstance(Mat4::scale(2 * R, 2 * R, 2 * R) * Mat4::translation(pos.x, 8 * scale(), pos.z), c, 1)); }
}
}  // namespace tz
