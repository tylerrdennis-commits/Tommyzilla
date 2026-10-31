// The Bulk's gauntlet arc (L12 slot, L13 Zookeeper's box, L14-19 stones, L20 pedestals), the three-stage Bulk fight
// with the snap, the Future-Sandbox rainbow wave, and the ending: credits -> THE END -> after-credits video.
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <cmath>
#include "game.h"
#include "../ui/ui_draw.h"
namespace tz {
static int popcount6(int m) { int n = 0; for (int i = 0; i < 6; i++) n += (m >> i) & 1; return n; }
bool Game::gauntletReady() const { const Profile& p = const_cast<Game*>(this)->P(); return p.gauntlet && p.gauntletOn && p.gauntletSlot; }
int Game::stoneCount() const { return (int)const_cast<Game*>(this)->P().stones.size(); }
void Game::equipGauntlet(bool on) { Profile& p = P(); if (!p.gauntlet) return; p.gauntletOn = on; persist(); }

void Game::setupArcLevel(const LevelDef& L) {
  world.noRebuild = L.noRebuild;
  if (L.obj == ObjType::Box) {  // L13: the zookeeper's box hides in one building of the district
    world.updateStreaming(L.ax, L.az, true);
    std::vector<int> ids;
    for (auto& b : world.buildings) if (!b.school && dist2D(b.x, b.z, L.ax, L.az) < L.ar) ids.push_back(b.id);
    level.arenaTotal = (int)ids.size(); level.arenaLeft = level.arenaTotal;
    Rng r((uint64_t)std::chrono::steady_clock::now().time_since_epoch().count() ^ AP().campaignSeed);
    level.boxBuilding = ids.empty() ? -1 : r.pick(ids);
  }
  if (L.pedestals) {  // L20: six stone pedestals around the gauntlet pedestal
    Vec3 c(L.bossX, 0, L.bossZ + 30);
    level.gauntletPedestal = c;
    for (int i = 0; i < 6; i++) { float a = i * (PI / 3) + PI / 6; level.pedestal[i] = c + Vec3(std::cos(a) * 26, 0, std::sin(a) * 26); }
    level.placed = 0; level.fightStarted = false;
    toast("Walk up to each pedestal and press " + btn("Y", "E") + " to place its stone (gauntlet on!)");
  }
  if (L.obj == ObjType::Stone) level.stoneTaken = false;
  if (L.obj == ObjType::Stone && world.procedural) toast("A gauntlet stone is hidden in one of the PURPLE buildings - smash them to find it!");  // Atlas
}
void Game::repelKaiju(const Vec3& from) {
  Vec3 d(kaiju.pos.x - from.x, 0, kaiju.pos.z - from.z); float l = length(d); d = l > 0.01f ? d * (1.f / l) : Vec3(0, 0, 1);
  kaiju.pos += d * (E.repelPush * 0.35f); world.resolveCircle(kaiju.pos, kaiju.radius());
  level.repelT = 0.6f;
  kaiju.cam.shake = 1.2f; vibrate(0.8f, 0.3f); play(SND_bonk, 1, 0.6f);
  fx->ring(from.x, from.z, 30, 0.6f, hexColor(0xffffff)); fx->rainbowBurst(from + Vec3(0, 4, 0), 1.5f);
  toast("Too much power! You need something to hold it...");
}
bool Game::tryPickStone() {
  const LevelDef& L = T.levels[level.idx];
  if (L.obj != ObjType::Stone || level.stoneTaken || L.stone < 0 || L.stone >= (int)E.stones.size()) return false;
  if (!stoneRevealed(L.stone)) return false;  // Atlas
  GateRun* g = nullptr; for (auto& q : gates) if (q.stone == L.stone) g = &q;
  if (!g || !g->open) return false;
  level.stonePos = g->pos + Vec3(0, 0, -8);
  if (dist2D(kaiju.pos.x, kaiju.pos.z, level.stonePos.x, level.stonePos.z) > 5 + kaiju.radius()) return false;
  if (!gauntletReady()) { if (level.repelT <= 0) repelKaiju(level.stonePos); return false; }
  const StoneDef& s = E.stones[(size_t)L.stone];
  Profile& p = AP();
  if (std::find(p.stones.begin(), p.stones.end(), s.id) == p.stones.end()) p.stones.push_back(s.id);
  level.stoneTaken = true;
  bigMessage("THE " + s.name + "!", std::to_string(p.stones.size()) + " / 6 in the gauntlet");
  play(SND_fanfare); flashT = 0.3f; fx->shell(level.stonePos + Vec3(0, 4, 0), 18, 1, s.color); fx->rainbowBurst(kaiju.hand(), 2);
  persist();
  return true;
}
bool Game::tryPlaceStone() {
  const LevelDef& L = T.levels[level.idx];
  if (!level.active || !L.pedestals || level.fightStarted) return false;
  for (int i = 0; i < 6; i++) {
    if (level.placed & (1 << i)) continue;
    if (dist2D(kaiju.pos.x, kaiju.pos.z, level.pedestal[i].x, level.pedestal[i].z) > 8 + kaiju.radius()) continue;
    const std::string& id = E.stones[(size_t)i].id;
    bool have = std::find(P().stones.begin(), P().stones.end(), id) != P().stones.end();
    if (!gauntletReady()) { toast("Put the gauntlet on first (Skills > Gauntlet)"); play(SND_error); return true; }
    if (!have) { toast("You don't have the " + E.stones[(size_t)i].name + " yet!"); play(SND_error); return true; }
    level.placed |= 1 << i; play(SND_unlock); fx->shell(level.pedestal[i] + Vec3(0, 6, 0), 10, 0.7f, E.stones[(size_t)i].color);
    toast(E.stones[(size_t)i].name + " placed! (" + std::to_string(popcount6(level.placed)) + "/6)");
    return true;
  }
  return false;
}
void Game::startBulkFight() {
  const LevelDef& L = T.levels[level.idx];
  level.fightStarted = true;
  skyDarkTarget = 1; skyDarkEvents++;
  kaiju.cam.shake = 1.5f; vibrate(1, 1.2f); play(SND_quake, 1, 0.6f);
  for (int i = 0; i < 6; i++) fx->shell(level.pedestal[i] + Vec3(0, 6, 0), 14, 1.2f, E.stones[(size_t)i].color);  // the stones fly home into the gauntlet
  startBoss(L);
  boss.staged = true; boss.stage = 1;
  toast("The sky goes dark... THE BULK is here! Hit him with the gauntlet!");
}
bool Game::bulkGauntletHit() {
  if (!boss.staged || boss.stage != 1 || boss.angerT > 0) return false;
  if (!gauntletReady() || stoneCount() < 6) return false;
  boss.hp = 1; boss.angerT = E.bulkAngerTime; boss.pulse = 0.6f;
  bigMessage("ONE PUNCH!", "THE BULK is ANGRY!"); play(SND_roar, 1, 0.45f); flashT = 0.4f;
  kaiju.cam.shake = 1.5f; vibrate(1, 0.5f);
  return true;
}
bool Game::tryGulpBoxKeeper() {
  if (!level.boxKeeper || zoo.active) return false;
  if (dist2D(kaiju.pos.x, kaiju.pos.z, level.boxKeeperPos.x, level.boxKeeperPos.z) > kaiju.eatRange()) return false;
  level.boxKeeper = false;
  EatTween e; e.from = level.boxKeeperPos; e.t = 0; e.look = 7; kaiju.eats.push_back(e); kaiju.attackSeq++;
  P().eaten++; P().eatenTotal++; onPeoplePopped(1); play(SND_gulp, 1, 0.9f); fx->rainbowBurst(kaiju.mouth(), 2);
  grantZookeeperKey();  // silent, like in the zoo
  return true;
}
bool Game::snapReady() const {
  return boss.active && boss.staged && boss.stage == 3 && !boss.dead && boss.transformT <= 0 && boss.hp <= boss.maxHp * E.snapThreshold + 1e-4f &&
         gauntletReady() && stoneCount() >= 6 && snapT < 0;
}
bool Game::doSnap() {
  if (!snapReady()) return false;
  snapT = 0; flashT = 0.8f; play(SND_roarCharge, 1, 0.5f); kaiju.cam.shake = 1.5f; vibrate(1, 0.8f);
  bigMessage("SNAP!", "");
  return true;
}
void Game::updateSnap(float dt) {
  if (snapT < 0) return;
  float prev = snapT; snapT += dt;
  auto at = [&](float k) { return prev < k && snapT >= k; };
  if (at(1.2f)) {
    boss.dead = true; boss.deadT = 0; boss.hp = 0; boss.rocks.clear();
    fx->shell(boss.pos + Vec3(0, boss.height() * 0.5f, 0), boss.radius() * 4, 2, hexColor(0xffffff)); fx->rainbowBurst(boss.pos + Vec3(0, boss.height() * 0.6f, 0), 8);
    play(SND_boom); play(SND_fanfare); flashT = 1.0f; skyDarkTarget = 0;
    bigMessage("PEACE TO THE LAND", "THE BULK is gone - Victoria is calm again");
  }
  for (int i = 0; i < 6; i++) if (at(2.0f + i * 0.4f)) {  // the stones shatter one by one
    bool survives = crazy && i == 5;
    Vec3 h = kaiju.hand();
    for (int k = 0; k < 14; k++) fx->particle(h, Vec3((frand() - 0.5f) * 14, frand() * 12, (frand() - 0.5f) * 14), 1.0f, 0.5f, E.stones[(size_t)i].color);
    play(survives ? SND_unlock : SND_crumble, 0.8f, 1.4f);
  }
  if (at(4.6f)) {
    if (crazy) { bigMessage("...WAIT!", "One stone still glows - the gauntlet re-forms!"); fx->rainbowBurst(kaiju.hand(), 3); }
    else { for (int k = 0; k < 40; k++) fx->particle(kaiju.hand(), Vec3((frand() - 0.5f) * 8, frand() * 6, (frand() - 0.5f) * 8), 1.4f, 0.4f, hexColor(k % 2 ? 0xffd36b : 0xd8c8ff)); bigMessage("SPARKLE...", "The gauntlet crumbles into stardust"); }
  }
  if (snapT >= 5.6f) finishSnap();
}
void Game::finishSnap() {
  snapT = -1;
  if (crazy) {  // the twist: the fully powered gauntlet is kept and goes to this slot's Sandbox ("Future Sandbox")
    save.crazy.endingDone = true;
    Profile& s = save.sandbox; s.gauntletSlot = true; s.gauntlet = true; s.gauntletOn = true; s.gauntletCracked = false;
    s.stones.clear(); for (auto& st : E.stones) s.stones.push_back(st.id);
  } else {
    Profile& a = save.adventure; a.stones.clear(); a.gauntlet = false; a.gauntletOn = false; a.gauntletCracked = true; a.endingDone = true;
  }
  persist();
}
void Game::restartBulkStage2() {
  const LevelDef& L = T.levels[level.idx];
  enemies.reset(); mode = Mode::Play; menus.clear();
  level.active = true; level.failed = false; level.fightStarted = true; level.placed = 63;
  kaiju.respawn(L.startX, L.startZ, PI);
  startBoss(L); boss.introT = 0.5f; boss.staged = true; boss.stage = 2;
  boss.scale = C.bossBaseScale * E.bulkStage2Scale; boss.maxHp = boss.hp = E.bulkStage2Hp * (crazy ? C.crazyBossHp : 1.f);
  skyDark = skyDarkTarget = 1;
  toast("Round 2! Dodge his big attacks (watch for the red warning rings)");
}
bool Game::sandboxUltimate() {
  if (adventure || !gauntletReady() || stoneCount() < 6 || ultCdT > 0 || enemies.nukeActive) return false;
  ultCdT = E.sandboxUltCooldown; sandboxUlts++;
  int n = 0;
  for (auto& b : world.buildings) {
    if (!b.generated || b.smashed || b.school || !world.chunks[b.chunk].active) continue;
    if (dist2D(b.x, b.z, kaiju.pos.x, kaiju.pos.z) > E.sandboxUltRadius) continue;
    world.hitBuilding(b, b.x, b.maxY * 0.5f, b.z, 1000, 100000, kaiju.pos.x, kaiju.pos.z, 10);
    if (b.smashed && !b.collapsed) world.collapse(b);
    n++;
  }
  people.pop(kaiju.pos.x, kaiju.pos.z, E.sandboxUltRadius * 0.6f);
  static const uint32_t bands[6] = {0xff3b3b, 0xff9a2a, 0xffe03a, 0x4bd66b, 0x3a8dff, 0xb06bff};
  for (int k = 0; k < 6; k++) fx->ring(kaiju.pos.x, kaiju.pos.z, E.sandboxUltRadius * (0.5f + 0.1f * k), 1.0f + 0.1f * k, hexColor(bands[k]));
  flashT = 0.5f; play(SND_boom, 1, 1.1f); kaiju.cam.shake = 1.5f; vibrate(1, 0.6f);
  bigMessage("GAUNTLET ULTIMATE!", "Rainbow wave!");
  return n > 0 || true;
}
void Game::updateArc(float dt) {
  ultCdT = std::max(0.f, ultCdT - dt); level.repelT = std::max(0.f, level.repelT - dt);
  skyDark += (skyDarkTarget - skyDark) * std::min(1.f, dt * 0.8f);
  updateSnap(dt);
  if (!adventure || !(level.active || level.won)) return;
  const LevelDef& L = T.levels[level.idx];
  if (L.obj == ObjType::Box && !level.boxOpen) {
    int left = 0;
    for (auto& b : world.buildings) if (!b.school && dist2D(b.x, b.z, L.ax, L.az) < L.ar && !b.smashed) left++;
    level.arenaLeft = left;
    if (left == 0 && !level.boxShown && level.boxBuilding >= 0) {
      const Building& b = world.buildings[(size_t)level.boxBuilding];
      level.boxShown = true; level.boxPos = openSpot(b.x, b.z, 3);
      bigMessage("WHAT'S THAT?", "A box in the rubble..."); play(SND_unlock); fx->rainbowBurst(level.boxPos + Vec3(0, 3, 0), 2);
    }
    if (level.boxShown && !P().zooKey && !(save.zooCompleted || !save.spinSlot) && !level.boxKeeper) {  // no key: its owner wanders over
      level.boxKeeper = true; level.boxKeeperPos = openSpot(level.boxPos.x + 30, level.boxPos.z + 10, 1);
    }
    if (level.boxKeeper) {
      Vec3 to = level.boxPos - level.boxKeeperPos; to.y = 0; float d = length(to);
      if (d > 4) { Vec3 dir = to * (1.f / d); level.boxKeeperPos += dir * std::min(d - 4, 5.f * dt); level.boxKeeperDir = std::atan2(dir.x, dir.z); }
    }
    if (level.boxShown && dist2D(kaiju.pos.x, kaiju.pos.z, level.boxPos.x, level.boxPos.z) < 6 + kaiju.radius()) {
      if (!P().zooKey && (save.zooCompleted || !save.spinSlot)) P().zooKey = true;  // retroactive key (zoo done before, or a pre-zoo save)
      if (P().zooKey) {
        level.boxOpen = true;
        Profile& p = AP(); p.gauntlet = true; p.gauntletSlot = true; p.gauntletOn = true; p.gauntletCracked = false;
        bigMessage("THE BULK'S GAUNTLET!", "It fits right in the gauntlet slot");
        play(SND_fanfare); flashT = 0.4f; fx->rainbowBurst(kaiju.hand(), 3);
        persist();
      }
    }
  }
  if (L.obj == ObjType::Stone) tryPickStone();
  if (L.pedestals && level.active && !level.fightStarted && popcount6(level.placed) == 6 && gauntletReady()) startBulkFight();
}
void Game::renderArc(RenderList& rl) {
  if (!adventure || !(level.active || level.won)) return;
  if (level.boxKeeper) drawZookeeper(rl, level.boxKeeperPos, level.boxKeeperDir, true);
  const LevelDef& L = T.levels[level.idx];
  if (L.obj == ObjType::Box && level.boxShown && !level.boxOpen) {
    Mat4 root = Mat4::rotY(0.3f) * Mat4::translation(level.boxPos.x, 0, level.boxPos.z);
    rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(4, 3, 4) * Mat4::translation(0, 1.5f, 0) * root, hexColor(0x2f8a3a)));
    rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(1.2f, 1.2f, 0.4f) * Mat4::translation(0, 2, 2.1f) * root, hexColor(0xffd36b), 0.5f));
    rl.opaque[MESH_RING].push_back(makeInstance(Mat4::scale(0.8f, 0.8f, 0.8f) * Mat4::rotX(PI / 2) * Mat4::translation(0, 2.8f, 2.1f) * root, hexColor(0xc8ccd4), 1));
  }
  if (L.obj == ObjType::Stone && !level.stoneTaken && L.stone >= 0) {
    for (auto& g : gates) if (g.stone == L.stone) {
      Vec3 p = g.pos + Vec3(0, 0, -8); float bob = 3 + std::sin(time * 2) * 0.5f; Color c = E.stones[(size_t)L.stone].color;
      rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(1.6f, 1.6f, 1.6f) * Mat4::translation(p.x, bob, p.z), c, 1, 0.6f));
      Color h = c; h.a = 0.22f; rl.alpha[MESH_SPHERE].push_back(makeInstance(Mat4::scale(5, 5, 5) * Mat4::translation(p.x, bob, p.z), h, 1));
    }
  }
  if (L.pedestals && !level.fightStarted) {
    Vec3 c = level.gauntletPedestal;
    rl.opaque[MESH_CYL].push_back(makeInstance(Mat4::translation(0, 0.5f, 0) * Mat4::scale(8, 3, 8) * Mat4::translation(c.x, 0, c.z), hexColor(0xd8d0e8)));
    if (gauntletReady()) drawGlove(rl, Mat4::scale(1.2f, 1.2f, 1.2f) * Mat4::rotY(time) * Mat4::translation(c.x, 6, c.z), time);
    for (int i = 0; i < 6; i++) {
      Vec3 p = level.pedestal[i]; Color sc = E.stones[(size_t)i].color;
      rl.opaque[MESH_CYL].push_back(makeInstance(Mat4::translation(0, 0.5f, 0) * Mat4::scale(5, 4, 5) * Mat4::translation(p.x, 0, p.z), mixColor(hexColor(0xb8b0c8), sc, 0.25f)));
      if (level.placed & (1 << i)) rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(1.8f, 1.8f, 1.8f) * Mat4::translation(p.x, 5.5f + std::sin(time * 3 + i) * 0.3f, p.z), sc, 1, 0.7f));
    }
  }
  if (snapT >= 0 && snapT < 1.6f && boss.active) {  // six coloured beams from the gauntlet to THE BULK
    Vec3 h = kaiju.hand(), b = boss.pos + Vec3(0, boss.height() * 0.6f, 0);
    for (int i = 0; i < 6; i++) {
      Vec3 off(std::cos(i * PI / 3) * 1.5f, std::sin(i * PI / 3) * 1.5f, 0); Vec3 a = h + off, d = b - a; float len = length(d);
      float yaw = std::atan2(d.x, d.z), pitch = std::asin(clampf(d.y / std::max(0.01f, len), -1, 1));
      Color c = E.stones[(size_t)i].color; c.a = 0.75f;
      rl.alpha[MESH_CYL].push_back(makeInstance(Mat4::translation(0, 0.5f, 0) * Mat4::scale(1.6f, len, 1.6f) * Mat4::rotX(PI / 2 - pitch) * Mat4::rotY(yaw) * Mat4::translation(a.x, a.y, a.z), c, 1));
    }
  }
}

// ---------------------------------------------------------------- ending: credits -> THE END -> after-credits video
void Game::loadCredits(const std::string& text) {
  credits.clear();
  Json j; if (!Json::parse(text.empty() ? embeddedJson("credits") : text, j)) return;
  CreditLine t; t.text = j["title"].strOr("TOMMYZILLA CITY SMASH"); t.head = true; credits.push_back(t);
  for (auto& sec : j["sections"].arr) {
    bool thanks = sec["thanks"].boolOr(false);
    if (!sec["head"].strOr("").empty()) { CreditLine h; h.text = sec["head"].strOr(""); h.head = true; h.thanks = thanks; credits.push_back(h); }
    for (auto& l : sec["lines"].arr) {
      CreditLine c; c.thanks = thanks;
      if (l.size() >= 2) { c.role = l[0].strOr(""); c.name = l[1].strOr(""); } else c.text = l[0].strOr("");
      credits.push_back(c);
    }
    credits.push_back(CreditLine());  // gap
  }
  const Json& a = j["afterCredits"];
  afterVideo = a["video"].strOr(afterVideo); afterSign = a["sign"].strOr(afterSign);
  endHold = (float)a["endHold"].numOr(2.5); endFallback = (float)a["fallbackSeconds"].numOr(12);
}
// duration of an MP4 from its moov/mvhd box (no decoder needed); <= 0 if unreadable
float mp4DurationSeconds(const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "rb"); if (!f) return -1;
  std::vector<unsigned char> d; unsigned char buf[65536]; size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
  std::fclose(f);
  auto be32 = [&](size_t o) { return (uint32_t)d[o] << 24 | (uint32_t)d[o + 1] << 16 | (uint32_t)d[o + 2] << 8 | d[o + 3]; };
  for (size_t i = 4; i + 28 < d.size(); i++) {
    if (d[i] != 'm' || d[i + 1] != 'v' || d[i + 2] != 'h' || d[i + 3] != 'd') continue;
    size_t o = i + 4; int ver = d[o];
    if (ver == 1) { if (o + 32 > d.size()) return -1; uint32_t ts = be32(o + 20); uint64_t du = (uint64_t)be32(o + 24) << 32 | be32(o + 28); return ts ? (float)((double)du / ts) : -1; }
    uint32_t ts = be32(o + 12), du = be32(o + 16); return ts ? (float)du / ts : -1;
  }
  return -1;
}
void Game::startEnding(bool crazyRun) {
  if (credits.empty()) loadCredits("");
  stopLevel(); boss = Boss(); enemies.reset();
  world.noRebuild = false; world.restoreAll();  // peace: the city is rebuilt
  skyDark = skyDarkTarget = 0; snapT = -1;
  ending = EndingRun(); ending.crazyEnding = crazyRun;
  { float d = mp4DurationSeconds(afterVideo); if (d > 0) endFallback = d; }  // the fallback shot lasts as long as the real video
  menus.clear(); mode = Mode::Ending;
  kaiju.respawn(E.zooX + 40, E.zooZ + 30, PI);
}
static float creditsLength(const std::vector<CreditLine>& c) { return 4.f + c.size() * 0.55f; }
void Game::updateEnding(float dt, const InputState& in) {
  ending.t += dt; ending.stageT += dt;
  bool anyKey = in.click; for (int a = 0; a < A_COUNT; a++) if (in.pressed[a]) anyKey = true;
  auto toStage = [&](int s) { ending.stage = s; ending.stageT = 0; };
  switch (ending.stage) {
    case 0:  // credits roll (skippable: lands on the after-credits scene)
      if (anyKey && ending.stageT > 0.3f) { toStage(2); }
      else if (ending.stageT >= creditsLength(credits)) toStage(1);
      break;
    case 1:  // fade to black, THE END
      if (ending.stageT >= endHold) toStage(2);
      break;
    case 2: break;
    case 3:  // video over: any button goes to the title
      if (anyKey) { mode = Mode::Title; menus.clear(); openMainMenu(); }
      return;
  }
  if (ending.stage == 2) {
    if (!ending.tried) {
      ending.tried = true;
      ending.usedVideo = video && video->start(afterVideo);
      ending.videoLen = ending.usedVideo ? std::max(1.f, video->duration()) : endFallback;
    }
    if (ending.usedVideo) video->update(dt);
    bool done = ending.usedVideo ? video->finished() : ending.stageT >= endFallback;
    if (done) { if (ending.usedVideo) video->stop(); toStage(3); }
    // all input is ignored while the after-credits scene plays
  }
}
void Game::renderClosingShot(RenderList& rl, float t) {
  // in-engine after-credits scene (fallback when the MP4 can't play): a new enclosure, a new zookeeper putting up
  // the sign, a new egg inside
  Vec3 c(E.zooX, 0, E.zooZ);
  renderZoo(rl);
  Egg e; e.active = true; e.pos = c; e.monster = T.monsters[0].id;
  float glow = 0.6f + 0.4f * std::sin(t * 3);
  rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(5, 6.8f, 5) * Mat4::translation(c.x, 3.4f, c.z), hexColor(0xfff3d0)));
  Color halo = hexColor(0xffd36b); halo.a = 0.15f * glow; rl.alpha[MESH_SPHERE].push_back(makeInstance(Mat4::scale(8, 10, 8) * Mat4::translation(c.x, 4, c.z), halo, 1));
  float walk = clampf(t / 5.f, 0, 1);
  Vec3 sp = c + Vec3(0, 0, -(E.zooCageR + 5)), kp = sp + Vec3(-30 + 26 * walk, 0, -3);
  Person kpp; kpp.active = true; kpp.x = kp.x; kpp.z = kp.z; kpp.dir = walk < 1 ? PI / 2 : 0; kpp.look = 7; kpp.vx = walk < 1 ? 3.f : 0.f; kpp.idle = walk >= 1;
  Mat4 root = Mat4::rotY(kpp.dir) * Mat4::translation(kp.x, 0, kp.z);
  if (!personModel(rl, kpp, 901, lastEye)) {
    rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(1.5f, 2.0f, 1.0f) * Mat4::translation(0, 1.6f, 0) * root, hexColor(0x4a7a3a)));
    rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(1.2f, 1.2f, 1.2f) * Mat4::translation(0, 3.3f, 0) * root, hexColor(0xf1c9a0)));
  }
  rl.opaque[MESH_CYL].push_back(makeInstance(Mat4::scale(1.3f, 0.45f, 1.3f) * Mat4::translation(0, 3.85f, 0) * root, hexColor(0x2f8a3a)));
}
// "Do not feed people." over after_credits.mp4 (10.625 s, 1280x720): the blank sign sits still centre-left
// (wooden board measured in the final clip: x 0.19-0.36, y 0.40-0.57 of the frame) until ~5.0 s; then the camera pushes in toward the egg (x 0.51, y 0.62)
// from 1.0x to 1.8x with a smoothstep ease until the end. The overlay follows that same zoom transform so it stays
// on the sign as it slides off to the left. The in-engine fallback shot has no such sign framing: there the text
// sits on the sign 0-5 s and then becomes a caption to the end.
static const float kSignX0 = 0.19f, kSignX1 = 0.36f, kSignY0 = 0.40f, kSignY1 = 0.57f, kZoomCX = 0.51f, kZoomCY = 0.62f, kZoomStart = 5.0f, kZoomMax = 1.8f;
float Game::endingSceneTime() const { return ending.stage == 2 ? ending.stageT : ending.videoLen; }
float Game::endingZoom() const {
  float len = std::max(kZoomStart + 0.1f, ending.videoLen), k = clampf((endingSceneTime() - kZoomStart) / (len - kZoomStart), 0, 1);
  return 1 + (kZoomMax - 1) * k * k * (3 - 2 * k);
}
bool Game::signRect(float& x0, float& y0, float& x1, float& y1) const {  // sign in normalised frame coords at the current zoom
  float z = endingZoom();
  x0 = kZoomCX + (kSignX0 - kZoomCX) * z; x1 = kZoomCX + (kSignX1 - kZoomCX) * z;
  y0 = kZoomCY + (kSignY0 - kZoomCY) * z; y1 = kZoomCY + (kSignY1 - kZoomCY) * z;
  return x1 > 0 && x0 < 1;
}
float Game::signOverlayAlpha() const {
  if (mode != Mode::Ending || ending.stage < 2) return 0;
  float t = endingSceneTime();
  if (ending.usedVideo) { float x0, y0, x1, y1; signRect(x0, y0, x1, y1); return clampf(x1 / 0.06f, 0, 1); }  // anchored to the sign the whole video
  return t >= 5.4f ? 0.f : 1 - clampf((t - 4.8f) / 0.6f, 0, 1);
}
float Game::signCaptionAlpha() const {
  if (mode != Mode::Ending || ending.stage < 2 || ending.usedVideo) return 0;
  return clampf((endingSceneTime() - 5.0f) / 0.5f, 0, 1);
}
void Game::drawEnding(RenderList& rl) {
  UIDraw u(rl); float s = u.s;
  if (ending.stage == 0) {
    u.rect(0, 0, u.W, u.H, Color{0.02f, 0.01f, 0.05f, 0.86f});
    float speed = 0.55f > 0 ? (u.lineH(s * 1.1f) + 6 * s) / 0.55f : 40;
    float y = u.H - ending.stageT * speed + u.H * 0.1f;
    for (auto& c : credits) {
      float lh = u.lineH(c.head ? s * 1.6f : s * 1.1f) + 6 * s;
      if (y > -lh && y < u.H + lh) {
        if (c.head) u.textC(u.W / 2, y, c.text, s * 1.6f, c.thanks ? hexColor(0xff8ad8) : hexColor(0xffd36b));
        else if (!c.role.empty()) {  // long names shrink to fit the right half of the screen
          float ns = s, room = u.W / 2 - 16 * s, w = u.textW(c.name, ns); if (w > room) ns *= room / w;
          u.textR(u.W / 2 - 8 * s, y, c.role, s * 1.0f, hexColor(0xa8c8ff)); u.text(u.W / 2 + 8 * s, y + (s - ns) * 6.5f, c.name, ns, hexColor(0xffffff));
        } else if (!c.text.empty()) {
          float ts = s * (c.thanks ? 0.85f : 1.2f), w = u.textW(c.text, ts); if (w > u.W * 0.95f) ts *= u.W * 0.95f / w;
          u.textC(u.W / 2, y, c.text, ts, hexColor(0xffffff));
        }
      }
      y += lh;
    }
    u.textR(u.W - 8, u.H - u.lineH(s * 0.75f) - 6, "press any button to skip", s * 0.75f, Color{0.7f, 0.7f, 0.8f, 0.7f});
  } else if (ending.stage == 1) {
    float a = clampf(ending.stageT / 0.8f, 0, 1);
    u.rect(0, 0, u.W, u.H, Color{0, 0, 0, a});
    u.textC(u.W / 2, u.H * 0.45f, "THE END", s * 3, Color{1, 1, 1, clampf((ending.stageT - 0.5f) / 0.6f, 0, 1)});
  } else {
    // the video (or the in-engine shot) is drawn by the renderer; the sign text is our overlay (the video can't render it)
    if (signOverlayAlpha() > 0) {
      float a = signOverlayAlpha(), x0, y0, x1, y1;
      if (ending.usedVideo) signRect(x0, y0, x1, y1);
      else {  // in-engine shot: project the blank board drawn by renderZoo (closing) so the text sits on it
        x0 = kSignX0; x1 = kSignX1; y0 = kSignY0; y1 = kSignY1;
        Vec3 sp = Vec3(E.zooX, 0, E.zooZ) + Vec3(0, 0, -(E.zooCageR + 5)); float ax, ay, bx, by;
        if (u.project(sp + Vec3(-5, 9.6f, 0), ax, ay) && u.project(sp + Vec3(5, 6.4f, 0), bx, by)) {
          x0 = std::min(ax, bx) / u.W; x1 = std::max(ax, bx) / u.W; y0 = std::min(ay, by) / u.H; y1 = std::max(ay, by) / u.H;
        }
      }
      // fit the two-line text inside the sign board (scales with the zoom); clipped naturally as the sign leaves frame
      float bw = (x1 - x0) * u.W, bh = (y1 - y0) * u.H, cx = (x0 + x1) * 0.5f * u.W, cy = (y0 + y1) * 0.5f * u.H;
      float ts = s * 1.2f * endingZoom(); float w1 = u.textW("Do not feed", ts); if (w1 > bw * 0.86f) ts *= bw * 0.86f / w1;
      size_t sp = afterSign.find(" feed ");
      std::string l1 = sp == std::string::npos ? afterSign : afterSign.substr(0, sp + 5), l2 = sp == std::string::npos ? "" : afterSign.substr(sp + 6);
      float lh = u.lineH(ts), top = cy - (l2.empty() ? lh * 0.5f : lh);
      (void)bh;
      Color ink{0.12f, 0.08f, 0.05f, a};
      u.textC(cx, top, l1, ts, ink, false); if (!l2.empty()) u.textC(cx, top + lh, l2, ts, ink, false);
    }
    if (signCaptionAlpha() > 0) {  // fallback shot: caption from 0:05 to the end
      float a = signCaptionAlpha(), tw = u.textW(afterSign, s * 1.3f) + 20 * s;
      u.rect(u.W / 2 - tw / 2, u.H * 0.86f - 4 * s, tw, u.lineH(s * 1.3f) + 8 * s, Color{0, 0, 0, 0.55f * a});
      u.textC(u.W / 2, u.H * 0.86f, afterSign, s * 1.3f, Color{1, 1, 1, a});
    }
    if (ending.stage == 3) u.textC(u.W / 2, u.H * 0.94f, "press any button", s * 0.8f, Color{0.8f, 0.8f, 0.9f, 0.6f + 0.4f * std::sin(realTime * 4)});
  }
}
}  // namespace tz
