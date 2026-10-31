// Vehicles + infantry: sandbox police (wanted stars: cars -> helicopter -> tanks -> nuke countdown) and the adventure
// roster (cops on foot + police cars, soldiers, jeeps, helicopters, tanks, jets, end-of-level bosses) share one unit
// system. Adventure levels choose a tier (0-2): tougher, repainted variants with multipliers from config.json.
#include <algorithm>
#include "game.h"
namespace tz {
bool Enemies::policeOn() const { return !G->adventure && (G->save.settings.police || forced); }
void Enemies::addHeat(float h) { if (!policeOn() || h <= 0) return; heat = std::min(5.99f, heat + h); calmT = 0; }
void Enemies::reset() { chase = PoliceChase(); cheese = CheeseMeter(); units.clear(); shots.clear(); heat = 0; calmT = 0; nukeActive = false; minHeat = 0; spawnT = 0; milSpawnT = 1; }
int Enemies::count(UnitType t) const { int n = 0; for (auto& u : units) if (!u.wreck && u.type == t) n++; return n; }
Vec3 Enemies::spawnPoint(float dist) {
  const Vec3& K = G->kaiju.pos;
  for (int t = 0; t < 12; t++) {
    float a = frand() * 6.283f, x = K.x + std::cos(a) * dist, z = K.z + std::sin(a) * dist;
    if (!G->world.blockedForPerson(x, z)) return Vec3(x, 0, z);
  }
  for (int t = 0; t < 24; t++) {  // Atlas: fallback ring, never on water
    float a = t * 0.2618f, x = clampf(K.x + std::cos(a) * dist, -C.worldHalf + 10, C.worldHalf - 10), z = clampf(K.z + std::sin(a) * dist, -C.worldHalf + 10, C.worldHalf - 10);
    if (!G->world.isWater(x, z)) return Vec3(x, 0, z);
  }
  return Vec3(K.x, 0, K.z);
}
float Enemies::tierHp(int t) { return t >= 2 ? C.tier2Hp : t == 1 ? C.tier1Hp : 1.f; }
float Enemies::tierDamage(int t) { return t >= 2 ? C.tier2Damage : t == 1 ? C.tier1Damage : 1.f; }
float Enemies::tierSpeed(int t) { return t >= 2 ? C.tier2Speed : t == 1 ? C.tier1Speed : 1.f; }
float Enemies::tierCount(int t) { return t >= 2 ? C.tier2Count : t == 1 ? C.tier1Count : 1.f; }
void Enemies::applyTier(Unit& u, int tier) {
  u.tier = tier; u.dmgK = tierDamage(tier); u.speedK = tierSpeed(tier);
  if (u.type != U_MBOSS && u.type != U_ARMOR) { u.hp = std::ceil(u.hp * tierHp(tier) - 0.01f); u.maxHp = u.hp; }  // bosses keep their own (kid-sized) HP
}
Unit* Enemies::miniBoss() { for (auto& u : units) if (u.type == U_MBOSS && !u.wreck) return &u; return nullptr; }
void Enemies::addUnit(UnitType t) {
  static uint32_t nextUid = 1;
  Unit u; u.type = t; u.pos = spawnPoint(C.spawnDist); u.uid = nextUid++;
  switch (t) {
    case U_COP: u.hp = C.copHp; u.reload = 1 + frand(); break;
    case U_SOLDIER: u.hp = C.soldierHp; u.reload = 1 + frand(); break;
    case U_RIOT: u.hp = C.copHp + 1; u.reload = 1.5f + frand(); break;
    case U_FLAME: u.hp = C.soldierHp; u.reload = 1 + frand(); break;
    case U_ARMOR: u.hp = 3; u.reload = C.tankReload * 1.4f; break;  // 3 super hits (HP is not scaled by tiers / Crazy)
    case U_MBOSS: u.hp = C.mbossHp; u.reload = 3; u.pos = spawnPoint(C.spawnDist * 0.8f); break;
    case U_CAR: u.hp = 1; break;
    case U_JEEP: u.hp = 1; break;
    case U_PHELI: u.hp = C.heliHp; u.pos.y = 30; u.ang = frand() * 6; G->toast("Police helicopter incoming!"); break;
    case U_AHELI: u.hp = C.heliHp + 1; u.pos.y = 32; u.ang = frand() * 6; u.reload = 3; break;
    case U_PTANK: u.hp = C.tankHp; u.reload = C.tankReload; break;
    case U_ATANK: u.hp = C.tankHp + 1; u.reload = C.tankReload; break;
    case U_JET: {
      u.hp = C.jetHp; u.pos = spawnPoint(230); u.pos.y = 45;
      Vec3 d = G->kaiju.pos - u.pos; d.y = 0; u.jetDir = normalize(d); u.dir = std::atan2(u.jetDir.x, u.jetDir.z);
      G->playAt(SND_jet, u.pos, 0.8f); break;
    }
    default: break;
  }
  u.dir = t == U_JET ? u.dir : std::atan2(G->kaiju.pos.x - u.pos.x, G->kaiju.pos.z - u.pos.z);
  if (t != U_ARMOR) u.hp *= G->enemyHpMult();  // Crazy difficulty: tougher vehicles
  u.maxHp = u.hp;
  units.push_back(u);
}
void Enemies::wreckUnit(Unit& u) {
  if (u.wreck) return;
  u.wreck = true; u.t = u.air() ? 3 : 2.5f;
  u.vel = u.type == U_JET ? u.jetDir * C.jetSpeed * 0.5f + Vec3(0, 5, 0) : Vec3((frand() - 0.5f) * 20, 14, (frand() - 0.5f) * 20);
  G->playAt(SND_wreck, u.pos);
  for (int k = 0; k < 10; k++) G->fx->particle(u.pos + Vec3(0, 2, 0), Vec3((frand() - 0.5f) * 10, frand() * 10, (frand() - 0.5f) * 10), 0.8f, 1.2f, hexColor(0x9aa0aa));
  destroyed[u.type]++;
  if (u.type == U_PHELI || u.type == U_AHELI) G->toast("Helicopter knocked away!");
  if (u.type == U_JET) G->toast("Jet knocked out of the sky!");
  if (u.type == U_MBOSS) {
    G->bigMessage(bossDisplayName(u.boss), "BOSS DOWN!"); G->play(SND_fanfare);
    G->fx->rainbowBurst(u.pos + Vec3(0, 8, 0), 4); G->fx->shell(u.pos + Vec3(0, 6, 0), 20, 1.0f, hexColor(0xffe03a));
  }
  addHeat(0.08f);
  G->onUnitDestroyed(u.type);
}
void Enemies::hitArea(float x, float z, float r, int moveIdx, bool thrown) {
  const MoveDef* m = moveIdx >= 0 ? &T.moves[moveIdx] : nullptr;
  bool canAir = thrown || (m && (m->kind != MoveKind::Melee || m->shape == MoveShape::Spin));
  bool heavy = m && (m->kind == MoveKind::Super || m->shape == MoveShape::Around);
  if (moveIdx >= 0 && moveIdx == T.moveIndex("stomp") && miniBoss()) cheese.onStomp();  // Titan anti-spam
  for (auto& u : units) {
    if (u.wreck || u.hitCd > 0) continue;
    float d = dist2D(u.pos.x, u.pos.z, x, z);
    if (u.air()) { if (!canAir || d > r + (u.type == U_JET ? 12 : 8)) continue; }
    else if (d > r + (u.type == U_MBOSS ? 8 : u.tank() ? 3 : 2)) continue;
    if (u.armored() && !(m && m->kind == MoveKind::Super)) {  // armour: normal hits just clang
      u.hitCd = 0.12f; clangs++;
      if (u.clangT <= 0) { u.clangT = 1.2f; G->floats.push_back({"CLANG! needs a SUPER!", u.pos + Vec3(0, 8, 0), 1.2f, hexColor(0xdfe6f0)}); G->playAt(SND_bonk, u.pos, 0.8f, 1.6f);
                           if (G->chestHintT <= 0) { G->toast("Metal tanks need a SUPER attack! (gulp people with " + G->btn("Y", "E") + " for fuel)"); G->chestHintT = 3; } }
      continue;
    }
    if (u.type == U_RIOT && !thrown && shieldBlocks(u, G->kaiju.pos, moveIdx)) {  // riot shield blocks ranged hits from the front
      u.hitCd = 0.12f; blocked++;
      if (u.clangT <= 0) { u.clangT = 0.8f; G->floats.push_back({"BLOCKED!", u.pos + Vec3(0, 6, 0), 0.9f, hexColor(0x9ae8ff)}); G->playAt(SND_bonk, u.pos, 0.5f, 2.0f); }
      continue;
    }
    u.hp -= heavy ? 2 : 1; u.hitCd = 0.12f;
    if (u.hp <= 0) wreckUnit(u);
  }
}
bool Enemies::shieldBlocks(const Unit& u, const Vec3& from, int moveIdx) {
  if (u.type != U_RIOT || moveIdx < 0) return false;
  const MoveDef& m = T.moves[(size_t)moveIdx];
  if (m.kind != MoveKind::Direct) return false;  // melee, stomps and supers get round the shield
  Vec3 f(std::sin(u.dir), 0, std::cos(u.dir)), to(from.x - u.pos.x, 0, from.z - u.pos.z); float l = length(to);
  if (l < 0.01f) return true;
  return dot(f, to * (1.f / l)) > 0.45f;  // attacker in front (about a 125-degree cone)
}
bool Enemies::flameHits(const Unit& u, const Vec3& target) {
  Vec3 f(std::sin(u.dir), 0, std::cos(u.dir)), to(target.x - u.pos.x, 0, target.z - u.pos.z); float l = length(to);
  float reach = C.flameRange + G->kaiju.radius();
  if (l > reach) return false;
  return l < 0.5f || dot(f, to * (1.f / l)) > std::cos(0.45f);  // ~26 degree half-angle cone
}
void Enemies::slowArea(float x, float z, float r, float secs) { for (auto& u : units) if (!u.wreck && dist2D(u.pos.x, u.pos.z, x, z) < r + 3) u.slowT = std::max(u.slowT, secs); }
bool Enemies::payNuke() {
  int cost = (int)C.nukeCancelCost;
  if (G->P().coins < cost) { G->toast("Not enough coins! Need " + std::to_string(cost)); G->play(SND_error); return false; }
  G->P().coins -= cost; nukeActive = false; heat = std::max(minHeat, std::min(heat, C.nukeCancelHeat));
  for (auto& u : units) if (u.tank() && u.type != U_MBOSS && !u.wreck) wreckUnit(u);
  G->toast("Nuke called off! (-" + std::to_string(cost) + " coins)"); G->bigMessage("PHEW!", "Nuke cancelled"); G->play(SND_unlock); G->persist();
  return true;
}
static float drive(Unit& u, float speed, float stopDist, float dt) {
  const Vec3& K = G->kaiju.pos; float dx = K.x - u.pos.x, dz = K.z - u.pos.z, d = std::hypot(dx, dz);
  float want = std::atan2(dx, dz);
  if (d > stopDist) {
    const float offs[] = {0, 0.9f, -0.9f, 1.8f, -1.8f};
    for (float off : offs) { float a = want + off, nx = u.pos.x + std::sin(a) * speed * dt * 4, nz = u.pos.z + std::cos(a) * speed * dt * 4; if (!G->world.blockedForPerson(nx, nz)) { want = a; break; } }
    u.dir += wrapAngle(want - u.dir) * std::min(1.f, dt * 4);
    { float nx = u.pos.x + std::sin(u.dir) * speed * dt, nz = u.pos.z + std::cos(u.dir) * speed * dt;  // Atlas: ground units never drive onto water
      if (!G->world.isWater(nx, nz)) { u.pos.x = nx; u.pos.z = nz; } else if (!G->world.isWater(nx, u.pos.z)) u.pos.x = nx; else if (!G->world.isWater(u.pos.x, nz)) u.pos.z = nz; }
  } else u.dir += wrapAngle(want - u.dir) * std::min(1.f, dt * 3);
  return d;
}
void Enemies::update(float dt, const InputState& in) {
  Kaiju& K = G->kaiju;
  bool police = policeOn(), military = G->adventure && level != nullptr;
  if (!police && !military) {
    if (!units.empty() || nukeActive) { units.erase(std::remove_if(units.begin(), units.end(), [](const Unit& u) { return !u.wreck; }), units.end()); nukeActive = false; }
    heat = 0;
  }
  if (police) {
    calmT += dt;
    if (calmT > C.heatDecayDelay && !nukeActive) heat = std::max(minHeat, heat - C.heatDecay * dt);
    int st = stars(); prog::PoliceTargets tg = prog::policeTargets(st);
    spawnT -= dt;
    if (spawnT <= 0) {
      spawnT = 1 / std::max(0.1f, G->diff().spawnRate);  // Titan
      if (count(U_CAR) < tg.cars) addUnit(U_CAR);
      if (tg.heli && count(U_PHELI) == 0) addUnit(U_PHELI);
      if (count(U_PTANK) < tg.tanks) addUnit(U_PTANK);
    }
    {  // Titan: small groups can be outrun; the search resumes when a building is smashed
      int n = 0; float nearest = 1e9f;
      for (auto& u : units) if (!u.wreck && u.police()) { n++; nearest = std::min(nearest, dist2D(u.pos.x, u.pos.z, K.pos.x, K.pos.z)); }
      if (chase.update(dt, n, nearest, K.speedNow)) {
        units.erase(std::remove_if(units.begin(), units.end(), [](const Unit& u) { return !u.wreck && u.police(); }), units.end());
        heat = std::min(heat, 0.99f); G->toast("You got away! Smash a building and they'll come looking...");
      }
      if (chase.searching) heat = std::min(heat, 0.99f);
    }
    if (st == 0 && (count(U_CAR) || count(U_PTANK) || count(U_PHELI))) {
      units.erase(std::remove_if(units.begin(), units.end(), [](const Unit& u) { return !u.wreck; }), units.end());
      G->toast("The police gave up the chase.");
    }
    if (tg.nuke && !nukeActive) { nukeActive = true; nukeT = C.nukeCountdown; G->toast("NUKE WARNING! Press " + G->btn("RB", "R") + " to pay " + std::to_string((int)C.nukeCancelCost) + " coins and call it off!"); }
    if (nukeActive) {
      nukeT -= dt;
      if (in.pressed[A_PAY]) payNuke();
      else if (nukeT <= 0) { nukeActive = false; G->gameOver("nuke"); }
    }
  }
  if (military) {
    milSpawnT -= dt;
    if (milSpawnT <= 0 && !G->level.bossPhase) {  // boss fight: no new reinforcements
      milSpawnT = level->interval / std::max(0.1f, G->diff().spawnRate);  // Titan: difficulty / players
      // one new unit per interval, rotating through the level's roster so every type shows up
      struct { UnitType t; int max; } want[] = {{U_COP, level->cops}, {U_CAR, level->cars}, {U_SOLDIER, level->soldiers}, {U_JEEP, level->jeeps},
                                                {U_AHELI, level->helis}, {U_ATANK, level->tanks}, {U_JET, level->jets},
                                                {U_RIOT, level->riots}, {U_FLAME, level->flamers}, {U_ARMOR, std::min(2, level->armored)}};
      const int NW = 10; float cm = tierCount(level->tier);
      for (int k = 0; k < NW; k++) {
        auto& w = want[(spawnRot + k) % NW];
        int mx = w.max > 0 ? std::max(1, (int)std::floor(w.max * cm + 0.5f)) : 0;
        if (w.t == U_ARMOR) mx = std::min(mx, 2);  // never a wall of armour: at most 2 alive, and they are never needed for a goal
        if (count(w.t) < mx) { addUnit(w.t); applyTier(units.back(), level->tier); spawnRot = (spawnRot + k + 1) % NW; break; }
      }
    }
  }
  float jetNear = 1e9f;
  for (auto& u : units) {
    u.hitCd = std::max(0.f, u.hitCd - dt); u.slowT = std::max(0.f, u.slowT - dt); if (u.type == U_RIOT) u.clangT = std::max(0.f, u.clangT - dt);
    if (u.wreck) {
      u.t -= dt; u.vel.y -= C.gravity * dt; u.pos += u.vel * dt; u.roll += dt * 5;
      if (u.pos.y < 0) { u.pos.y = 0; u.vel *= 0.3f; }
      continue;
    }
    float slow = u.slowT > 0 ? 0.3f : 1.f; bool frozen = u.slowT > 0;
    bool focusP2 = G->coopFocusBegin(u.pos);  // Wren: co-op - target the nearest player (K now refers to them)
    switch (u.type) {
      case U_COP: case U_SOLDIER: {  // on foot: walk up, then fire cartoon pea-shooters
        bool cop = u.type == U_COP;
        float range = cop ? C.copRange : C.soldierRange;
        float d = drive(u, (cop ? C.copSpeed : C.soldierSpeed) * slow * u.speedK, range, dt);
        u.t += dt * (d > range ? 1.f : 0.2f); u.reload -= dt; u.moving = d > range;
        if (d <= range + 8 && u.reload <= 0 && !frozen) {
          u.reload = (cop ? C.copReload : C.soldierReload) * (0.8f + frand() * 0.4f); u.fireT = G->time;
          Shot s; s.pos = u.pos + Vec3(std::sin(u.dir) * 1.2f, 2.4f, std::cos(u.dir) * 1.2f);
          s.vel = normalize(Vec3(K.pos.x, 7, K.pos.z) - s.pos) * 34; s.dmg = (cop ? C.copShotDamage : C.soldierShotDamage) * u.dmgK; s.police = cop; s.pea = true;
          shots.push_back(s);
          if (cop) G->playPolice(SND_shot, u.pos, 0.35f, 1.7f); else G->playAt(SND_shot, u.pos, 0.4f, 1.5f);
        }
        break;
      }
      case U_RIOT: {  // riot cop: slow, shield up front, the odd pea shot
        float d = drive(u, C.copSpeed * 0.75f * slow * u.speedK, C.copRange * 0.8f, dt);
        u.t += dt * (d > C.copRange * 0.8f ? 0.8f : 0.2f); u.reload -= dt; u.moving = d > C.copRange * 0.8f;
        if (d <= C.copRange + 6 && u.reload <= 0 && !frozen) {
          u.reload = C.copReload * 1.4f; u.fireT = G->time;
          Shot sh; sh.pos = u.pos + Vec3(std::sin(u.dir) * 1.6f, 2.4f, std::cos(u.dir) * 1.6f);
          sh.vel = normalize(Vec3(K.pos.x, 7, K.pos.z) - sh.pos) * 34; sh.dmg = C.copShotDamage * u.dmgK; sh.police = true; sh.pea = true; shots.push_back(sh);
          G->playPolice(SND_shot, u.pos, 0.3f, 1.8f);
        }
        break;
      }
      case U_FLAME: {  // flamethrower trooper: walks right up, short fire cone
        float d = drive(u, C.soldierSpeed * 1.1f * slow * u.speedK, C.flameRange * 0.7f, dt);
        u.t += dt * (d > C.flameRange ? 1.f : 0.2f); u.moving = d > C.flameRange * 0.7f;
        u.flameT = std::max(0.f, u.flameT - dt);
        if (!frozen && flameHits(u, K.pos)) {
          if (u.flameT <= 0) { u.flameT = 0.4f; u.fireT = G->time; G->playAt(SND_shot, u.pos, 0.25f, 0.6f); }
          K.damage(C.flameDps * u.dmgK * dt);
        }
        if (u.flameT > 0 && frand() < dt * 40) { Vec3 f(std::sin(u.dir), 0, std::cos(u.dir)); G->fx->particle(u.pos + f * 2 + Vec3(0, 2.4f, 0), f * (C.flameRange * 2.2f) + Vec3((frand() - 0.5f) * 5, frand() * 3, (frand() - 0.5f) * 5), 0.4f, 1.1f, hexColor(frand() < 0.5f ? 0xff9a2a : 0xffe03a)); }
        break;
      }
      case U_ARMOR: {  // armoured metal tank: slow, only supers dent it
        float d = drive(u, C.tankSpeed * 0.7f * slow * u.speedK, 40, dt);
        u.reload -= dt; u.clangT = std::max(0.f, u.clangT - dt);
        if (u.reload <= 0 && d < 70 && !frozen) {
          u.reload = C.tankReload * 1.4f; G->playAt(SND_cannon, u.pos, 0.8f, 0.8f);
          Shot sh; sh.pos = u.pos + Vec3(std::sin(u.dir) * 7, 3.6f, std::cos(u.dir) * 7);
          sh.vel = normalize(Vec3(K.pos.x - sh.pos.x, 8 - sh.pos.y, K.pos.z - sh.pos.z)) * 45; sh.dmg = C.tankShotDamage * 0.8f * u.dmgK; shots.push_back(sh);
        }
        break;
      }
      case U_MBOSS: {  // end-of-level boss: lumbers in and fires a three-shot cannon spread
        cheese.update(dt, dist2D(u.pos.x, u.pos.z, K.pos.x, K.pos.z), 9);
        if (!frozen && (cheese.punishClose() || cheese.punishStomp())) {  // Titan: no camping point-blank / spamming stomp
          bool st = cheese.punishStomp(); cheese.consume();
          Vec3 push(K.pos.x - u.pos.x, 0, K.pos.z - u.pos.z); float pl = length(push); push = pl > 0.01f ? push * (1.f / pl) : Vec3(0, 0, 1);
          K.pos += push * 14; K.damage(C.mbossShotDamage * u.dmgK * 1.5f); u.hitCd = 1.5f;
          G->fx->ring(u.pos.x, u.pos.z, 22, 0.6f, hexColor(st ? 0x40e0ff : 0xff5030)); G->kaiju.cam.shake = 1; G->play(SND_quake, 0.7f);
          G->toast(st ? "The boss jumped your stomp! Mix up your moves!" : "Too close! Back up and use other moves!");
        }
        if (updateVariedBoss(u, dt, slow, frozen)) break;  // Titan: moth / crab / blimp (titan_bosses.cpp)
        float d = drive(u, C.mbossSpeed * slow * u.speedK, 34, dt);
        u.t += dt; u.reload -= dt; u.moving = d > 34;
        if (u.reload <= 0 && d < 95 && !frozen) {
          u.reload = C.mbossReload; u.fireT = G->time;
          Vec3 f(std::sin(u.dir), 0, std::cos(u.dir));
          for (int k = -1; k <= 1; k++) {
            Shot s; s.pos = u.pos + f * 9 + Vec3(0, u.boss == 2 ? 13.f : 6.5f, 0);
            Vec3 dir = normalize(Vec3(K.pos.x, 8, K.pos.z) - s.pos); float a = k * 0.14f, ca = std::cos(a), sa = std::sin(a);
            dir = Vec3(dir.x * ca + dir.z * sa, dir.y, -dir.x * sa + dir.z * ca);
            s.vel = dir * 42; s.dmg = C.mbossShotDamage * u.dmgK; shots.push_back(s);
          }
          G->playAt(SND_cannon, u.pos, 1.0f, 0.7f); G->kaiju.cam.shake = std::min(1.f, G->kaiju.cam.shake + 0.2f);
        }
        break;
      }
      case U_CAR: case U_JEEP: {
        float range = u.type == U_CAR ? C.carRange : 20;
        float d = drive(u, (u.type == U_CAR ? C.carSpeed : C.jeepSpeed) * slow * u.speedK, range, dt);
        if (d <= range + 2 && !frozen) {
          K.damage((u.type == U_CAR ? C.carDps : C.jeepDps) * u.dmgK * dt);
          if (frand() < dt * 3) { if (u.type == U_CAR) G->playPolice(SND_shot, u.pos, 0.6f); else G->playAt(SND_shot, u.pos, 0.6f); G->fx->particle(u.pos + Vec3(0, 2.5f, 0), Vec3((K.pos.x - u.pos.x) * 1.5f, 4, (K.pos.z - u.pos.z) * 1.5f), 0.5f, 0.4f, hexColor(0xffd36b)); }
        }
        break;
      }
      case U_PHELI: case U_AHELI: {
        u.ang += dt * 0.5f * slow;
        HeliSwoop sw = heliSwoop(u.t + u.uid * 1.7f, u.type == U_PHELI ? 30.f : 32.f);  // Titan: swoops low + close to be smashable
        Vec3 tgt(K.pos.x + std::cos(u.ang) * sw.radius, sw.height, K.pos.z + std::sin(u.ang) * sw.radius);
        u.pos = u.pos + (tgt - u.pos) * std::min(1.f, dt * 1.5f * slow * u.speedK);
        u.dir = std::atan2(K.pos.x - u.pos.x, K.pos.z - u.pos.z); u.t += dt;
        if (!frozen) K.damage((u.type == U_PHELI ? C.heliDps : C.heliDps * 0.6f) * u.dmgK * dt);
        if (u.type == U_AHELI && !frozen) {
          u.reload -= dt;
          if (u.reload <= 0) { u.reload = 3; Shot s; s.pos = u.pos; s.vel = normalize(Vec3(K.pos.x, 8, K.pos.z) - u.pos) * 40; s.dmg = 3 * u.dmgK; shots.push_back(s); G->playAt(SND_shot, u.pos, 0.8f, 0.7f); }
        }
        break;
      }
      case U_PTANK: case U_ATANK: {
        float d = drive(u, C.tankSpeed * slow * u.speedK, 38, dt);
        u.reload -= dt;
        if (u.reload <= 0 && d < 70 && !frozen) {
          u.reload = C.tankReload; if (u.type == U_PTANK) G->playPolice(SND_cannon, u.pos); else G->playAt(SND_cannon, u.pos);
          Shot s; s.pos = u.pos + Vec3(std::sin(u.dir) * 6, 3.2f, std::cos(u.dir) * 6);
          s.vel = normalize(Vec3(K.pos.x - s.pos.x, 8 - s.pos.y, K.pos.z - s.pos.z)) * 45; s.dmg = C.tankShotDamage * (u.type == U_ATANK ? 1.2f : 1.f) * u.dmgK; s.police = u.type == U_PTANK;
          shots.push_back(s);
        }
        break;
      }
      case U_JET: {
        u.pos += u.jetDir * C.jetSpeed * slow * u.speedK * dt; u.bombT -= dt;
        Vec3 rel = K.pos - u.pos; rel.y = 0; float d = length(rel);
        jetNear = std::min(jetNear, d);
        if (d < 18 && u.bombT <= 0 && !frozen) {
          u.bombT = 2; Shot s; s.pos = u.pos; s.vel = u.jetDir * C.jetSpeed * 0.4f; s.bomb = true; s.dmg = C.jetBombDamage * u.dmgK; s.t = 5; shots.push_back(s);
          G->playAt(SND_bomb, u.pos, 0.8f);
        }
        if (d > 230 && dot(rel, u.jetDir) < 0) {  // turn around for another pass
          Vec3 to = normalize(rel + Vec3((frand() - 0.5f) * 60, 0, (frand() - 0.5f) * 60)); u.jetDir = to; u.dir = std::atan2(to.x, to.z);
          G->playAt(SND_jet, u.pos, 0.6f);
        }
        u.roll = std::sin(G->time * 2 + u.ang) * 0.3f;
        break;
      }
      default: break;
    }
    G->coopFocusEnd(focusP2);
  }
  units.erase(std::remove_if(units.begin(), units.end(), [](const Unit& u) { return u.wreck && u.t <= 0; }), units.end());
  for (int i = (int)shots.size() - 1; i >= 0; i--) {
    Shot& s = shots[i]; s.t -= dt;
    if (s.bomb) s.vel.y -= C.gravity * dt;
    s.pos += s.vel * dt;
    bool focusP2 = G->coopFocusBegin(s.pos);  // Wren: shots can hit either player (the nearer one)
    bool hitK = length(s.pos - Vec3(K.pos.x, 8, K.pos.z)) < 6;
    if (hitK || s.t <= 0 || s.pos.y < 0) {
      if (s.bomb && !hitK && s.pos.y < 0.5f) { if (dist2D(s.pos.x, s.pos.z, K.pos.x, K.pos.z) < 10) K.damage(s.dmg * 0.6f); G->world.damageSphere(s.pos.x, 1, s.pos.z, 4, 6, s.pos.x, s.pos.z, 10); G->fx->ring(s.pos.x, s.pos.z, 8, 0.5f, hexColor(0xffd36b)); }
      if (hitK && s.t > 0) K.damage(s.dmg);
      if (s.t > 0) for (int k = 0; k < 8; k++) G->fx->particle(s.pos, Vec3((frand() - 0.5f) * 12, frand() * 8, (frand() - 0.5f) * 12), 0.5f, 1.5f, hexColor(k % 2 ? 0xffd36b : 0xffffff));
      if (s.bomb || hitK) { if (s.police) G->playPolice(SND_bomb, s.pos, 0.5f, 1.3f); else G->playAt(SND_bomb, s.pos, 0.5f, 1.3f); }
      G->coopFocusEnd(focusP2); shots.erase(shots.begin() + i); continue;
    }
    G->coopFocusEnd(focusP2);
  }
  // siren only for a live police chase (sandbox police on, chasing units alive, not yet arrived / outrun). Adventure's
  // cops / cars / riot cops are the army roster: no chase ever ran there, so the siren used to play until every one was dead.
  G->loopVol[LOOP_SIREN] = police ? chase.sirenVolume(C.policeVolume, count(U_CAR) + count(U_COP) + count(U_RIOT)) : 0.f;  // Titan: quieter; stops once they arrive so the music plays  // police siren (cop audio multiplier)
  G->loopVol[LOOP_HELI] = count(U_AHELI) > 0 ? 1.f : count(U_PHELI) > 0 ? C.policeVolume : 0.f;  // army heli full volume, police heli scaled
  G->loopVol[LOOP_NUKE] = nukeActive ? 1.f : 0.f;
  G->loopVol[LOOP_JET] = jetNear < 1e8f ? clampf(1 - jetNear / 200, 0, 1) : 0.f;
}

// tier palette swap: tier 1 = crimson paint, tier 2 = gold paint (lights / unlit parts stay their own colour)
static Color gTint{1, 1, 1, 1}; static float gTintK = 0;
static void P(RenderList& rl, MeshId m, uint32_t c, const Mat4& local, const Mat4& root, float unlit = 0) {
  Color col = hexColor(c); if (gTintK > 0 && unlit <= 0) col = mixColor(col, gTint, gTintK);
  rl.opaque[m].push_back(makeInstance(local * root, col, unlit));
}
void unitExtras(RenderList& rl, const Unit& u, const Mat4& root) {
  if (u.wreck) return;
  if (u.type == U_RIOT) {  // big see-through riot shield held in front
    rl.alpha[MESH_BOX].push_back(makeInstance(Mat4::scale(2.6f, 3.4f, 0.25f) * Mat4::translation(0, 2.4f, 1.6f) * root, Color{0.6f, 0.85f, 1, 0.45f}, 1));
    rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(2.7f, 0.25f, 0.3f) * Mat4::translation(0, 4.1f, 1.6f) * root, hexColor(0x1a2a6a)));
    rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(1.2f, 0.4f, 0.3f) * Mat4::translation(0, 3.3f, 1.65f) * root, hexColor(0xf2f2f2), 1));  // POLICE stripe
  } else if (u.type == U_FLAME) {  // fuel tank backpack + nozzle + flame cone while firing
    for (float sx : {-0.45f, 0.45f}) rl.opaque[MESH_CYL].push_back(makeInstance(Mat4::scale(0.7f, 2.0f, 0.7f) * Mat4::translation(sx, 2.6f, -0.9f) * root, hexColor(0xd03030)));
    rl.opaque[MESH_CYL].push_back(makeInstance(Mat4::scale(0.3f, 2.2f, 0.3f) * Mat4::rotX(PI / 2) * Mat4::translation(0.7f, 2.4f, 1.6f) * root, hexColor(0x2a2f3a)));
    if (u.flameT > 0) { Color c = hexColor(0xff9a2a); c.a = 0.55f; rl.alpha[MESH_CONE].push_back(makeInstance(Mat4::scale(4, C.flameRange, 4) * Mat4::rotX(-PI / 2) * Mat4::translation(0.7f, 2.4f, 2.7f + C.flameRange * 0.5f) * root, c, 1)); }
  }
}
uint32_t tierTintHex(int tier) { return tier >= 2 ? 0xffb020 : tier == 1 ? 0xd03030 : 0xffffff; }
float tierTintAmount(int tier) { return tier >= 2 ? 0.4f : tier == 1 ? 0.35f : 0.f; }
static Mat4 S(float x, float y, float z, float px, float py, float pz) { return Mat4::scale(x, y, z) * Mat4::translation(px, py, pz); }
void Enemies::render(RenderList& rl) {
  for (auto& u : units) {
    Mat4 root = Mat4::rotZ(u.wreck ? u.roll * 0.6f : (u.type == U_JET ? u.roll : 0)) * Mat4::rotX(u.wreck ? u.roll : 0) * Mat4::rotY(u.dir) * Mat4::translation(u.pos.x, u.pos.y, u.pos.z);
    Mat4 wheelR = Mat4::rotZ(PI / 2);
    if (G->unitModel(rl, u, root)) continue;  // loaded model (cop / soldier / vehicles / minibosses)
    gTint = hexColor(tierTintHex(u.tier)); gTintK = u.type == U_MBOSS ? 0.f : tierTintAmount(u.tier);
    FinishMarks fm = finishMarks(rl);
    switch (u.type) {
      case U_COP: case U_SOLDIER: case U_RIOT: case U_FLAME: {  // cartoony ~4 m tall officer / soldier with a big green pea-shooter
        bool cop = u.type == U_COP || u.type == U_RIOT; float sw = std::sin(u.t * 10) * 0.5f;
        uint32_t suit = cop ? 0x2a4aa8 : 0x5a6a3a, dark = cop ? 0x16245a : 0x3a4528;
        for (int l = 0; l < 2; l++) P(rl, MESH_BOX, dark, Mat4::translation(0, -0.8f, 0) * Mat4::scale(0.55f, 1.6f, 0.6f) * Mat4::rotX(l ? sw : -sw) * Mat4::translation(l ? 0.4f : -0.4f, 1.6f, 0), root);
        P(rl, MESH_BOX, suit, S(1.6f, 1.6f, 1.0f, 0, 2.4f, 0), root);
        P(rl, MESH_SPHERE, 0xf1c9a0, S(1.25f, 1.25f, 1.25f, 0, 3.75f, 0), root);
        P(rl, MESH_SPHERE, 0x111111, S(0.18f, 0.25f, 0.1f, -0.25f, 3.85f, 0.6f), root); P(rl, MESH_SPHERE, 0x111111, S(0.18f, 0.25f, 0.1f, 0.25f, 3.85f, 0.6f), root);
        if (cop) { P(rl, MESH_CYL, 0x16245a, S(1.35f, 0.5f, 1.35f, 0, 4.35f, 0), root); P(rl, MESH_BOX, 0x16245a, S(1.2f, 0.12f, 0.6f, 0, 4.15f, 0.65f), root);
                   P(rl, MESH_SPHERE, 0xffd23a, S(0.3f, 0.3f, 0.1f, 0, 4.4f, 0.68f), root, 1); P(rl, MESH_SPHERE, 0xffd23a, S(0.25f, 0.25f, 0.1f, -0.45f, 2.8f, 0.52f), root, 1); }
        else { P(rl, MESH_SPHERE, 0x4a5a30, S(1.45f, 0.9f, 1.45f, 0, 4.2f, 0), root); }
        P(rl, MESH_BOX, suit, S(0.4f, 0.4f, 1.3f, 0.9f, 2.7f, 0.5f), root);                 // arm holding the shooter
        P(rl, MESH_CYL, 0x3ac24a, Mat4::scale(0.35f, 1.6f, 0.35f) * Mat4::rotX(PI / 2) * Mat4::translation(0.9f, 2.75f, 1.4f), root);  // pea-shooter
        P(rl, MESH_SPHERE, 0x2a9a3a, S(0.5f, 0.5f, 0.5f, 0.9f, 2.75f, 0.7f), root);
        break;
      }
      case U_MBOSS: {
        float sc = 3.0f;
        if (drawVariedBoss(rl, u, root)) break;  // Titan
        if (u.boss == 2) {  // Mega Mech: walking legs, boxy torso, arm cannons, red visor
          float sw = std::sin(u.t * 3) * 0.35f;
          for (int l = 0; l < 2; l++) P(rl, MESH_BOX, 0x4a4f5a, Mat4::translation(0, -3.5f, 0) * Mat4::scale(1.6f, 7, 1.8f) * Mat4::rotX(l ? sw : -sw) * Mat4::translation(l ? 2.2f : -2.2f, 7, 0), root);
          P(rl, MESH_BOX, 0xd0a020, S(7.5f, 5.5f, 5, 0, 10, 0), root); P(rl, MESH_BOX, 0x2a2f3a, S(4.5f, 2.5f, 4, 0, 14, 0.3f), root);
          P(rl, MESH_BOX, 0xff3030, S(3.6f, 0.7f, 0.3f, 0, 14.3f, 2.35f), root, 1);
          for (int a = 0; a < 2; a++) { P(rl, MESH_BOX, 0x4a4f5a, S(1.8f, 1.8f, 4, a ? 4.8f : -4.8f, 11, 0.5f), root);
                                        P(rl, MESH_CYL, 0x1a1d24, Mat4::scale(0.9f, 4, 0.9f) * Mat4::rotX(PI / 2) * Mat4::translation(a ? 4.8f : -4.8f, 11, 3.5f), root); }
          P(rl, MESH_SPHERE, 0xffe03a, S(1.2f, 1.2f, 0.3f, 0, 10.5f, 2.55f), root, 1);
        } else {  // Giant Super-Tank: huge gold-and-black tank, twin barrels
          P(rl, MESH_BOX, 0xd0a020, S(5 * sc, 2 * sc, 8 * sc, 0, 1.4f * sc, 0), root); P(rl, MESH_BOX, 0x1a1d24, S(5.6f * sc, 1.2f * sc, 8.4f * sc, 0, 0.6f * sc, 0), root);
          P(rl, MESH_BOX, 0x2a2f3a, S(3.4f * sc, 1.4f * sc, 3.6f * sc, 0, 3.1f * sc, 0), root);
          for (int b = 0; b < 2; b++) P(rl, MESH_CYL, 0x1a1d24, Mat4::scale(0.7f * sc, 5 * sc, 0.7f * sc) * Mat4::rotX(PI / 2) * Mat4::translation(b ? 1.1f * sc : -1.1f * sc, 3.2f * sc, 3.8f * sc), root);
          P(rl, MESH_SPHERE, 0xff3030, S(1.2f, 1.2f, 0.3f, 0, 3.4f * sc, 1.85f * sc), root, 1);
        }
        break;
      }
      case U_CAR: {
        P(rl, MESH_BOX, 0xf2f2f2, S(3.4f, 1.4f, 6.5f, 0, 1.1f, 0), root); P(rl, MESH_BOX, 0x1a2a6a, S(3.42f, 0.5f, 6.52f, 0, 1.2f, 0), root); P(rl, MESH_BOX, 0x12141a, S(3, 1.2f, 3.2f, 0, 2.3f, -0.3f), root);
        for (int w = 0; w < 4; w++) P(rl, MESH_CYL, 0x12141a, Mat4::scale(1.2f, 0.5f, 1.2f) * wheelR * Mat4::translation(w % 2 ? 1.6f : -1.6f, 0.6f, w < 2 ? 2.f : -2.f), root);
        bool on = ((int)(G->time * 6)) % 2;
        P(rl, MESH_BOX, on ? 0xff4040 : 0x401010, S(1.1f, 0.4f, 0.6f, -0.6f, 3.1f, -0.3f), root, 1); P(rl, MESH_BOX, on ? 0x102040 : 0x3060ff, S(1.1f, 0.4f, 0.6f, 0.6f, 3.1f, -0.3f), root, 1);
        break;
      }
      case U_JEEP: {
        P(rl, MESH_BOX, 0x5a6a3a, S(3.2f, 1.3f, 5.6f, 0, 1.2f, 0), root); P(rl, MESH_BOX, 0x4a5a30, S(3.0f, 0.8f, 1.6f, 0, 2.2f, 1.2f), root);
        P(rl, MESH_BOX, 0x2a2f2a, S(3.0f, 0.2f, 0.2f, 0, 3.2f, -0.6f), root); P(rl, MESH_BOX, 0x2a2f2a, S(0.2f, 1.4f, 0.2f, -1.4f, 2.5f, -0.6f), root); P(rl, MESH_BOX, 0x2a2f2a, S(0.2f, 1.4f, 0.2f, 1.4f, 2.5f, -0.6f), root);
        P(rl, MESH_SPHERE, 0xffffff, S(0.6f, 0.6f, 0.2f, 0, 1.9f, -2.8f), root); // spare tyre star
        for (int w = 0; w < 4; w++) P(rl, MESH_CYL, 0x12141a, Mat4::scale(1.3f, 0.6f, 1.3f) * wheelR * Mat4::translation(w % 2 ? 1.6f : -1.6f, 0.65f, w < 2 ? 1.8f : -1.8f), root);
        break;
      }
      case U_PHELI: case U_AHELI: {
        uint32_t body = u.type == U_PHELI ? 0x1a2a6a : 0x4a5a3a;
        P(rl, MESH_SPHERE, body, S(4.8f, 4.3f, 6.7f, 0, 0, 0), root); P(rl, MESH_SPHERE, 0x9ae8ff, S(3.2f, 3.2f, 3.2f, 0, 0.3f, 2.2f), root);
        P(rl, MESH_BOX, body, S(0.6f, 0.6f, 7, 0, 0.4f, -5), root); P(rl, MESH_BOX, 0xf2f2f2, S(0.2f, 2, 1, 0.4f, 1, -8.2f), root);
        P(rl, MESH_BOX, 0x12141a, S(0.3f, 0.3f, 5, -1.4f, -2.2f, 0), root); P(rl, MESH_BOX, 0x12141a, S(0.3f, 0.3f, 5, 1.4f, -2.2f, 0), root);
        float ra = G->time * 30;
        P(rl, MESH_BOX, 0x2a2f3a, Mat4::scale(14, 0.15f, 0.7f) * Mat4::rotY(ra) * Mat4::translation(0, 2.6f, 0), root);
        P(rl, MESH_BOX, 0x2a2f3a, Mat4::scale(14, 0.15f, 0.7f) * Mat4::rotY(ra + PI / 2) * Mat4::translation(0, 2.6f, 0), root);
        if (u.type == U_PHELI && !u.wreck) rl.alpha[MESH_CONE].push_back(makeInstance(Mat4::scale(10, 26, 10) * Mat4::translation(0, -14, 0) * root, Color{1, 0.96f, 0.75f, 0.12f}, 1));
        break;
      }
      case U_ARMOR: case U_PTANK: case U_ATANK: {
        uint32_t body = u.type == U_PTANK ? 0x4a5a3a : 0x3a4a2a, tur = u.type == U_PTANK ? 0x5a6a48 : 0x4a5a38;
        P(rl, MESH_BOX, body, S(5, 2, 8, 0, 1.4f, 0), root); P(rl, MESH_BOX, 0x2a2f2a, S(5.6f, 1.2f, 8.4f, 0, 0.6f, 0), root);
        P(rl, MESH_BOX, tur, S(3.4f, 1.4f, 3.6f, 0, 3.1f, 0), root); P(rl, MESH_CYL, 0x2a2f2a, Mat4::scale(0.7f, 5, 0.7f) * Mat4::rotX(PI / 2) * Mat4::translation(0, 3.2f, 3.8f), root);
        if (u.type == U_ATANK) P(rl, MESH_SPHERE, 0xffffff, S(0.9f, 0.9f, 0.1f, 0, 3.1f, -1.85f), root, 1);
        break;
      }
      case U_JET: {
        P(rl, MESH_CYL, 0x8a96a8, Mat4::scale(2.2f, 12, 2.2f) * Mat4::rotX(PI / 2) * Mat4::translation(0, 0, 0), root);
        P(rl, MESH_CONE, 0x6a7688, Mat4::scale(2.2f, 3.5f, 2.2f) * Mat4::rotX(PI / 2) * Mat4::translation(0, 0, 7.7f), root);
        P(rl, MESH_BOX, 0x7a8698, S(14, 0.3f, 3.5f, 0, 0, -1), root); P(rl, MESH_BOX, 0x7a8698, S(5, 0.25f, 1.6f, 0, 0.2f, -5.5f), root);
        P(rl, MESH_BOX, 0x7a8698, S(0.3f, 2.6f, 1.8f, 0, 1.4f, -5.3f), root); P(rl, MESH_SPHERE, 0x9ae8ff, S(1.2f, 1.0f, 2.4f, 0, 0.9f, 3), root);
        P(rl, MESH_SPHERE, 0xffa040, S(1.2f, 1.2f, 1.2f, 0, 0, -6.2f), root, 1);
        break;
      }
      default: break;
    }
    if (u.type == U_ARMOR) applyFinish(rl, fm, FIN_CHROME, G->time);  // armour: chrome metal finish
    unitExtras(rl, u, root);
  }
  gTintK = 0;
  for (auto& s : shots) {
    if (s.pea) { rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(0.8f, 0.8f, 0.8f) * Mat4::translation(s.pos.x, s.pos.y, s.pos.z), hexColor(0x5ae04a), 1.f)); continue; }
    rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(s.bomb ? 1.6f : 1.4f, s.bomb ? 1.6f : 1.4f, s.bomb ? 2.4f : 1.4f) * Mat4::translation(s.pos.x, s.pos.y, s.pos.z), hexColor(s.bomb ? 0x2a2f3a : 0xffd36b), s.bomb ? 0.f : 1.f));
  }
}
}  // namespace tz
