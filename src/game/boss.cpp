// THE BULK - the level 15 final boss. An original purple, hunched brute with huge fists, cream horns, back
// spikes and glowing amber eyes. Every hit he takes makes him grow (config bossGrowPerHit, capped at bossMaxScale);
// as he grows he unlocks harder attacks: ground-pound shockwaves, a charging rush, then thrown debris.
#include <algorithm>
#include "game.h"
namespace tz {
float Boss::growth() const { return scale / std::max(0.01f, C.bossBaseScale); }
int Boss::phase() const { float g = growth(); return g < 1.45f ? 0 : g < 2.0f ? 1 : 2; }
float Boss::radius() const { return C.bossRadius * scale; }
float Boss::height() const { return 15.f * scale; }

void Game::startBoss(const LevelDef& L) {
  boss = Boss(); boss.active = true;
  boss.pos = Vec3(L.bossX, 0, L.bossZ);
  boss.scale = C.bossBaseScale;
  boss.maxHp = boss.hp = C.bossHp * (crazy ? C.crazyBossHp : 1.f) * diff().bossHp;
  boss.introT = C.bossIntroTime; boss.nextAttack = 2.5f;
  boss.facing = std::atan2(kaiju.pos.x - boss.pos.x, kaiju.pos.z - boss.pos.z);
  kaiju.facing = std::atan2(boss.pos.x - kaiju.pos.x, boss.pos.z - kaiju.pos.z); kaiju.cam.yaw = kaiju.facing + PI;
  bigMessage("FINAL BOSS", "THE BULK"); play(SND_roar, 1, 0.6f); play(SND_quake, 0.8f);
  toast("THE BULK grows every time you hit him - keep smashing!");
}
bool Game::bossHitTest(const Vec3& p, float r) const {
  if (!boss.active || boss.dead || boss.intro()) return false;
  return dist2D(p.x, p.z, boss.pos.x, boss.pos.z) < boss.radius() + r && p.y < boss.height() + r;
}
bool Game::hitBossArea(int move, float x, float y, float z, float r, float bricks) {
  (void)move;
  if (!boss.active || boss.dead || boss.intro() || boss.hitCd > 0) return false;
  if (dist2D(x, z, boss.pos.x, boss.pos.z) > r + boss.radius() || y > boss.height() + r) return false;
  if (boss.angerT > 0 || boss.transformT > 0 || snapT >= 0) return true;  // anger / transformation beats: invulnerable
  boss.hitCd = C.bossHitGap; boss.hits++;
  if (boss.staged && boss.stage == 1) {  // L20 stage 1: the gauntlet one-shot leaves him on exactly 1 HP
    if (!bulkGauntletHit()) boss.hp = std::max(1.f, boss.hp - std::max(1.f, bricks * C.bossDamagePerBrick));
    playAt(SND_bonk, boss.pos, 1, 0.6f); boss.pulse = 0.3f;
    return true;
  }
  float dmg = std::max(1.f, bricks * C.bossDamagePerBrick);
  if (boss.staged && boss.stage == 3) boss.hp = std::max(1.f, boss.hp - dmg);  // final form: only the snap can finish him
  else boss.hp = std::max(0.f, boss.hp - dmg);
  if (boss.staged && boss.stage == 2 && boss.hp <= 0) {  // stage 2 beaten -> FINAL FORM
    boss.stage = 3; boss.transformT = E.bulkTransformTime; boss.maxHp = boss.hp = E.bulkStage3Hp * (crazy ? C.crazyBossHp : 1.f); boss.attack = 0; boss.rocks.clear();
    bigMessage("FINAL FORM!", "THE BULK transforms!"); play(SND_roar, 1, 0.4f); play(SND_quake, 1, 0.5f); flashT = 0.5f;
    kaiju.cam.shake = 1.5f; vibrate(1, 0.8f);
    fx->shell(boss.pos + Vec3(0, boss.height() * 0.5f, 0), boss.radius() * 3, 1.5f, hexColor(0xff3b3b));
    return true;
  }
  float cap = boss.staged ? C.bossBaseScale * E.bulkStage2Scale * 1.25f : C.bossBaseScale * C.bossMaxScale;
  boss.scale = std::min(cap, boss.scale * (1 + C.bossGrowPerHit));  // grows with every hit, never past the cap
  float frac = 1 - boss.hp / std::max(1.f, boss.maxHp);              // ...and at least in step with the damage dealt,
  if (!boss.staged) boss.scale = std::min(cap, std::max(boss.scale, C.bossBaseScale + (cap - C.bossBaseScale) * frac));  // so he is near max size when he falls
  boss.pulse = 0.3f; vibrate(0.6f, 0.15f); kaiju.cam.shake = std::min(1.5f, kaiju.cam.shake + 0.35f);
  playAt(SND_bonk, boss.pos, 0.9f, 0.7f);
  for (int k = 0; k < 10; k++) fx->particle(Vec3(x, std::max(2.f, y), z), Vec3((frand() - 0.5f) * 14, frand() * 10, (frand() - 0.5f) * 14), 0.5f, 0.7f, hexColor(k % 2 ? 0xb06bff : 0xffe03a));
  if (boss.hp <= 0) {
    boss.dead = true; boss.deadT = 0; boss.rocks.clear();
    bigMessage("YOU DID IT!", "THE BULK is beaten!"); play(SND_boom, 0.8f); play(SND_fanfare);
    fx->shell(boss.pos + Vec3(0, boss.height() * 0.5f, 0), boss.radius() * 3, 1.5f, hexColor(0xb06bff)); fx->rainbowBurst(boss.pos + Vec3(0, boss.height() * 0.6f, 0), 6);
  }
  return true;
}
void Game::updateBoss(float dt) {  // Wren: co-op - the boss fights whichever player is nearest
  if (!boss.active) return;
  bool sw = coopFocusBegin(boss.pos); updateBossFocused(dt); coopFocusEnd(sw);
}
void Game::updateBossFocused(float dt) {
  Boss& b = boss;
  b.t += dt; b.pulse = std::max(0.f, b.pulse - dt); b.hitCd = std::max(0.f, b.hitCd - dt);
  if (snapT >= 0 && !b.dead) { b.pulse = 0.2f; b.lift = std::sin(b.t * 40) * 0.3f; return; }  // frozen by the snap
  if (b.angerT > 0) {  // stage 1 -> 2: furious roar, grows huge, HP refills
    b.angerT -= dt; b.lift = std::sin(b.t * 30) * 0.4f; kaiju.cam.shake = std::max(kaiju.cam.shake, 0.6f);
    float k = 1 - std::max(0.f, b.angerT) / std::max(0.1f, E.bulkAngerTime);
    b.scale = lerpf(C.bossBaseScale, C.bossBaseScale * E.bulkStage2Scale, k);
    if (b.angerT <= 0) { b.stage = 2; b.angerT = 0; b.scale = C.bossBaseScale * E.bulkStage2Scale; b.maxHp = b.hp = E.bulkStage2Hp * (crazy ? C.crazyBossHp : 1.f); b.nextAttack = 1.5f; play(SND_roar, 1, 0.4f);
                         bigMessage("STAGE 2", "Watch out for his big attacks!"); }
    kaiju.cam.dist = std::max(kaiju.cam.dist, std::min(90.f, 26 + b.height() * 0.6f));
    return;
  }
  if (b.transformT > 0) {  // stage 2 -> 3: transformation (aura, roar, invulnerable)
    b.transformT -= dt; b.pulse = 0.2f; b.lift = std::sin(b.t * 20) * 0.6f;
    if (frand() < dt * 30) fx->particle(b.pos + Vec3((frand() - 0.5f) * b.radius() * 2, frand() * b.height(), (frand() - 0.5f) * b.radius() * 2), Vec3(0, 8, 0), 0.8f, 1.2f, hexColor(frand() < 0.5f ? 0xff3b3b : 0xb06bff));
    if (b.transformT <= 0) { b.transformT = 0; b.nextAttack = 1.5f; toast("Get him to half health, then press " + btn("RB", "R") + " to DO THE SNAP!"); }
    return;
  }
  if (b.dead) { b.deadT += dt; b.scale = std::max(0.2f, b.scale - dt * 0.6f); if (frand() < dt * 20) fx->rainbowBurst(b.pos + Vec3((frand() - 0.5f) * 10, frand() * b.height(), (frand() - 0.5f) * 10), 1.5f); return; }
  // keep both monsters in frame: pull the camera back as he grows
  kaiju.cam.dist = std::max(kaiju.cam.dist, std::min(70.f, 26 + b.height() * 0.55f));
  if (b.intro()) {  // rises out of the ground roaring
    b.introT -= dt; b.lift = -b.height() * std::max(0.f, b.introT / C.bossIntroTime - 0.3f);
    if (frand() < dt * 30) fx->dust(b.pos.x, b.pos.z, 3, b.radius());
    kaiju.cam.shake = std::max(kaiju.cam.shake, 0.4f);
    if (b.introT <= 0) { b.lift = 0; play(SND_roar, 1, 0.55f); toast("THE BULK: \"RAAAH! BULK SMASH TINY LIZARD!\""); }
    return;
  }
  Vec3 to(kaiju.pos.x - b.pos.x, 0, kaiju.pos.z - b.pos.z); float d = length(to);
  float speedK = 1 + 0.25f * b.phase();
  if (b.attack == 0) {
    b.facing += wrapAngle(std::atan2(to.x, to.z) - b.facing) * std::min(1.f, dt * 3);
    if (d > b.radius() + kaiju.radius() + 3) { b.pos += Vec3(std::sin(b.facing), 0, std::cos(b.facing)) * (C.bossSpeed * speedK * dt); }
    b.lift = 0; b.armSwing = std::sin(b.t * 4) * 0.3f;
    b.nextAttack -= dt;
    if (b.nextAttack <= 0) {
      int ph = b.phase(), pick = ph == 0 ? 1 : 1 + (int)(frand() * (ph + 1)) % (ph + 1);
      if (pick == 3 && d < 20) pick = 1;
      b.attack = pick; b.attackT = 0; b.attackHit = false; b.attackSeq++;
      b.rushDir = d > 0.01f ? to * (1.f / d) : Vec3(0, 0, 1);
      b.nextAttack = C.bossAttackGap * 0.8f / speedK * (b.staged && b.stage >= 2 ? 0.75f : 1.f);  // stages 2-3 attack more often
      if (pick == 2) toast("THE BULK is charging!");
    }
  } else {
    b.attackT += dt;
    if (b.staged && b.stage >= 2 && b.attackT <= dt + 1e-4f) {  // telegraph: a red warning ring where the attack will land
      float R = b.attack == 1 ? C.bossPoundRadius * b.growth() : b.radius() + 6;
      Vec3 at = b.attack == 3 ? kaiju.pos : b.pos;
      fx->ring(at.x, at.z, R, 0.8f, hexColor(0xff2020)); fx->ring(at.x, at.z, R * 0.5f, 0.8f, hexColor(0xff6060));
    }
    if (b.attack == 1) {  // GROUND POUND: fists up, slam, shockwave ring
      b.armSwing = b.attackT < 0.8f ? -2.4f * (b.attackT / 0.8f) : 0.8f;
      b.lift = b.attackT < 0.8f ? 3 * std::sin(b.attackT / 0.8f * PI / 2) : 0;
      if (!b.attackHit && b.attackT >= 0.8f) {
        b.attackHit = true;
        float R = C.bossPoundRadius * b.growth();
        fx->ring(b.pos.x, b.pos.z, R, 0.7f, hexColor(0xb06bff)); fx->ring(b.pos.x, b.pos.z, R * 0.6f, 0.5f, hexColor(0xffe03a)); fx->dust(b.pos.x, b.pos.z, 30, b.radius());
        debris->kick(b.pos.x, b.pos.z, R, 18); people.knock(b.pos.x, b.pos.z, R, 10);
        world.damageSphere(b.pos.x, 2, b.pos.z, b.radius() * 1.3f, 30, b.pos.x, b.pos.z, 14);
        playAt(SND_quake, b.pos, 1); kaiju.cam.shake = 1.4f; vibrate(1, 0.4f);
        if (dist2D(kaiju.pos.x, kaiju.pos.z, b.pos.x, b.pos.z) < R) kaiju.damage(C.bossHardness * C.bossPoundDamage * (b.staged && b.stage >= 2 ? E.bulkStage2Damage : 1.f));
      }
      if (b.attackT > 1.4f) b.attack = 0;
    } else if (b.attack == 2) {  // CHARGING RUSH: crouch, then barrel forward smashing buildings
      if (b.attackT < 0.7f) { b.lift = -1; b.armSwing = 1.2f; if (frand() < dt * 20) fx->dust(b.pos.x, b.pos.z, 2, b.radius()); }
      else if (b.attackT < 1.7f) {
        b.lift = 0; b.facing = std::atan2(b.rushDir.x, b.rushDir.z);
        b.pos += b.rushDir * (C.bossRushSpeed * dt);
        world.damageSphere(b.pos.x + b.rushDir.x * b.radius(), 4, b.pos.z + b.rushDir.z * b.radius(), b.radius(), 20, b.pos.x, b.pos.z, 20);
        people.knock(b.pos.x, b.pos.z, b.radius() + 4, 12);
        if (frand() < dt * 30) fx->dust(b.pos.x, b.pos.z, 3, b.radius());
        if (!b.attackHit && dist2D(kaiju.pos.x, kaiju.pos.z, b.pos.x, b.pos.z) < b.radius() + kaiju.radius() + 2) {
          b.attackHit = true; kaiju.damage(C.bossHardness * C.bossRushDamage * (b.staged && b.stage >= 2 ? E.bulkStage2Damage : 1.f)); kaiju.pos += b.rushDir * 6; kaiju.cam.shake = 1.2f; vibrate(1, 0.3f); playAt(SND_bonk, kaiju.pos, 1, 0.6f);
        }
      } else b.attack = 0;
    } else {  // DEBRIS THROW: scoops a boulder and lobs it at you (it lands where you were standing)
      b.armSwing = b.attackT < 0.6f ? -2.6f : 1.0f;
      if (!b.attackHit && b.attackT >= 0.6f) {
        b.attackHit = true;
        int n = 1 + (b.phase() >= 2 ? 2 : 0);
        for (int k = 0; k < n; k++) {
          Vec3 tgt = kaiju.pos + Vec3((frand() - 0.5f) * 14 * k, 0, (frand() - 0.5f) * 14 * k);
          BossRock r; r.pos = b.pos + Vec3(0, b.height() * 0.9f, 0); float T0 = 1.6f;
          r.vel = Vec3((tgt.x - r.pos.x) / T0, (0 - r.pos.y) / T0 + 0.5f * C.gravity * T0, (tgt.z - r.pos.z) / T0);
          b.rocks.push_back(r);
        }
        playAt(SND_throwWhoosh, b.pos, 1, 0.7f);
      }
      if (b.attackT > 1.2f) b.attack = 0;
    }
  }
  for (int i = (int)b.rocks.size() - 1; i >= 0; i--) {
    BossRock& r = b.rocks[i]; r.t += dt; r.vel.y -= C.gravity * dt; r.pos += r.vel * dt;
    if (r.pos.y > 0.5f && r.t < 6) continue;
    fx->ring(r.pos.x, r.pos.z, 9, 0.5f, hexColor(0xffd36b)); fx->dust(r.pos.x, r.pos.z, 12, 2);
    for (int k = 0; k < 8; k++) debris->spawn(r.pos + Vec3(0, 1, 0), Vec3((frand() - 0.5f) * 16, 6 + frand() * 8, (frand() - 0.5f) * 16), hexColor(0x8a7aa0), 0.6f + frand() * 0.6f);
    playAt(SND_crumble, r.pos, 1, 0.8f);
    if (dist2D(r.pos.x, r.pos.z, kaiju.pos.x, kaiju.pos.z) < 8 + kaiju.radius()) kaiju.damage(C.bossHardness * C.bossThrowDamage * (b.staged && b.stage >= 2 ? E.bulkStage2Damage : 1.f));
    b.rocks.erase(b.rocks.begin() + i);
  }
  b.pos.x = clampf(b.pos.x, -C.worldHalf + 20, C.worldHalf - 20); b.pos.z = clampf(b.pos.z, -C.worldHalf + 20, C.worldHalf - 20);
}
void Game::bossVictory() {
  bool first = !save.crazyUnlocked;
  save.bulkDefeated = true; save.crazyUnlocked = true; persist();
  bool staged = boss.staged; bool cz = crazy;
  MenuItem ok; ok.label = staged ? "Roll the credits!" : "Yay!"; ok.act = [this, staged, cz]() { Game* self = this; bool st = staged, c = cz; self->popMenu(); if (st) self->startEnding(c); };
  prompt(crazy ? "CRAZY CHAMPION!" : "PEACE TO THE LAND!",
         std::string(crazy ? "You beat the CRAZY campaign. You are the ultimate kaiju!\n" : "THE BULK is gone and Victoria is calm again. The people cheer (in rainbow colours)!\n") +
             (crazy && staged ? "The Gauntlet is yours in Sandbox! (all six stones - press " + btn("RB", "R") + " there for the rainbow wave)\n" : "") +
             (first ? "NEW: CRAZY difficulty unlocked! Pick it from the main menu - all " + std::to_string(T.levels.size()) + " levels, twice as tough, and any hero kaiju you like.\n" : "") +
             "\n" + kGameCredits,
         {ok});
}

// --------------------------------------------------------------- rendering (own silhouette: hunched, huge fists, horns, spikes)
void drawBoss(RenderList& rl, const Boss& b, float time) {
  if (!b.active || (b.dead && b.deadT > 2.5f)) return;  // vanishes in a rainbow shower after his defeat
  if (G->bossModel(rl, b)) {  // loaded model (purple Blue Demon): keep the shadow + thrown rocks
    float sr = C.bossRadius * b.scale * 1.4f;
    rl.alpha[MESH_DISC].push_back(makeInstance(Mat4::scale(2 * sr, 1, 2 * sr) * Mat4::translation(b.pos.x, 0.1f, b.pos.z), Color{0, 0, 0, 0.35f}, 1));
    for (auto& r : b.rocks) rl.opaque[MESH_ROCK].push_back(makeInstance(Mat4::scale(4, 4, 4) * Mat4::rotX(r.t * 5) * Mat4::translation(r.pos.x, r.pos.y, r.pos.z), hexColor(0x8a7aa0)));
    return;
  }
  float s = b.scale * (1 + b.pulse * 0.25f);
  Color body = hexColor(0x7a3cc8), dark = hexColor(0x4a2080), plate = hexColor(0xe0a030), horn = hexColor(0xf0e0c0), eye = hexColor(0xffb000);
  if (b.pulse > 0.15f) body = mixColor(body, hexColor(0xffffff), 0.5f);
  if (b.dead) { body.a = dark.a = plate.a = horn.a = 1; }
  float bob = std::sin(time * 3) * 0.2f;
  Mat4 root = Mat4::scale(s, s, s) * Mat4::rotY(b.facing) * Mat4::translation(b.pos.x, b.lift + bob * s, b.pos.z);
  auto P = [&](MeshId m, Color c, const Mat4& l, float unlit = 0) { rl.opaque[m].push_back(makeInstance(l * root, c, unlit)); };
  float walk = std::sin(b.t * 5) * (b.attack == 0 ? 0.35f : 0.f);
  // legs (short, wide stance) with gold knee plates
  for (int side = -1; side <= 1; side += 2) {
    Mat4 hip = Mat4::rotX(walk * side) * Mat4::translation(1.6f * side, 4.2f, 0);
    P(MESH_CYL, dark, Mat4::scale(1.8f, 4.2f, 1.8f) * Mat4::translation(0, -2.1f, 0) * hip);
    P(MESH_BOX, plate, Mat4::scale(1.2f, 1.0f, 0.6f) * Mat4::translation(0, -2.2f, 0.9f) * hip);
    P(MESH_BOX, dark, Mat4::scale(2.2f, 0.9f, 3.0f) * Mat4::translation(0, -4.0f, 0.5f) * hip);
  }
  // hunched torso: big chest leaning forward, small belly plate
  Mat4 torso = Mat4::rotX(0.45f) * Mat4::translation(0, 7.4f, 0.4f);
  P(MESH_SPHERE, body, Mat4::scale(7.4f, 6.4f, 5.6f) * torso);
  P(MESH_SPHERE, dark, Mat4::scale(5.0f, 3.6f, 4.0f) * Mat4::translation(0, -2.4f, 0.4f) * torso);
  P(MESH_BOX, plate, Mat4::scale(3.2f, 2.0f, 0.6f) * Mat4::translation(0, -0.6f, 2.7f) * torso);
  // back spikes
  for (int i = 0; i < 5; i++) P(MESH_CONE, horn, Mat4::scale(1.0f, 2.2f - i * 0.2f, 1.0f) * Mat4::rotX(-0.6f) * Mat4::translation(0, 2.6f - i * 1.1f, -2.4f - i * 0.15f) * torso);
  // small head low and forward, two curled horns, glowing eyes, underbite tusks
  Mat4 head = Mat4::translation(0, 1.6f, 3.4f) * torso;
  P(MESH_SPHERE, body, Mat4::scale(3.0f, 2.6f, 2.8f) * head);
  P(MESH_BOX, dark, Mat4::scale(2.8f, 0.6f, 0.8f) * Mat4::translation(0, 0.7f, 1.1f) * head);  // heavy brow
  for (int side = -1; side <= 1; side += 2) {
    P(MESH_CONE, horn, Mat4::scale(0.7f, 2.4f, 0.7f) * Mat4::rotZ(-0.7f * side) * Mat4::translation(1.5f * side, 1.7f, 0.0f) * head);
    float glow = 0.8f + 0.2f * std::sin(time * 8);
    rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(0.55f, 0.4f, 0.3f) * Mat4::translation(0.7f * side, 0.35f, 1.35f) * head * root, eye, 1, glow));
    P(MESH_CONE, horn, Mat4::scale(0.35f, 0.7f, 0.35f) * Mat4::translation(0.6f * side, -0.5f, 1.3f) * head);
  }
  // long arms with HUGE fists (the silhouette's signature)
  for (int side = -1; side <= 1; side += 2) {
    float sw = b.armSwing + (b.attack == 0 ? -walk * side : 0);
    Mat4 sh = Mat4::rotX(sw) * Mat4::rotZ(0.25f * side) * Mat4::translation(4.4f * side, 9.6f, 1.0f);
    P(MESH_SPHERE, body, Mat4::scale(3.4f, 3.0f, 3.0f) * sh);
    P(MESH_CYL, body, Mat4::scale(2.2f, 5.6f, 2.2f) * Mat4::translation(0, -3.2f, 0) * sh);
    P(MESH_BOX, plate, Mat4::scale(2.4f, 1.0f, 2.4f) * Mat4::translation(0, -4.6f, 0) * sh);
    P(MESH_SPHERE, dark, Mat4::scale(4.2f, 3.8f, 4.2f) * Mat4::translation(0, -7.2f, 0.3f) * sh);
    for (int k = -1; k <= 1; k++) P(MESH_SPHERE, horn, Mat4::scale(0.8f, 0.8f, 0.8f) * Mat4::translation(k * 1.1f, -7.0f, 2.2f) * sh);
  }
  // shadow + thrown rocks
  float sr = C.bossRadius * b.scale * 1.4f;
  rl.alpha[MESH_DISC].push_back(makeInstance(Mat4::scale(2 * sr, 1, 2 * sr) * Mat4::translation(b.pos.x, 0.1f, b.pos.z), Color{0, 0, 0, 0.35f}, 1));
  for (auto& r : b.rocks) rl.opaque[MESH_ROCK].push_back(makeInstance(Mat4::scale(4, 4, 4) * Mat4::rotX(r.t * 5) * Mat4::translation(r.pos.x, r.pos.y, r.pos.z), hexColor(0x8a7aa0)));
}
}  // namespace tz
