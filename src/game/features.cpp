// Egg spinner + rolled looks, growth / XP (heal overflow -> XP), De-Evolution Ray, the Kaiju Zoo opening,
// the hidden Zookeeper's Key and the Zoo Gates. Tables: data/eggs.json (EggTables E).
#include "kaiju_gen.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include "game.h"
#include "../ui/ui_draw.h"
namespace tz {
// ---------------------------------------------------------------- looks
KaijuLook Game::lookOf(const std::string& id) const {
  auto it = save.looks.find(id);
  if (it != save.looks.end()) return it->second;
  int mi = std::max(0, T.monsterIndex(id));
  return eggs::defaultLook(T.monsters[(size_t)mi]);
}
MonsterDef Game::lookedDef(int idx) const {
  MonsterDef d = T.monsters[(size_t)std::max(0, std::min(idx, (int)T.monsters.size() - 1))];
  if (!hasLook(d.id)) return d;
  KaijuLook l = lookOf(d.id);
  d.body = l.primary; d.belly = l.secondary; d.accent = l.accent;
  if (l.genSeed && kgen::usesProcedural(d.id)) { d.genSeed = l.genSeed; d.plan = kgen::planOf(kgen::generate(l.genSeed, l).body); }  // Rex
  return d;
}
Color Game::uiAccent() const { return lookOf(kaiju.def().id).accent; }
const RarityDef& Game::rarityOf(const std::string& id) const { return E.rarity(E.rarityOf(id)); }

// primitive finish fallback: params.z = finish id, params.w = time (both shaders read it)
FinishMarks finishMarks(const RenderList& rl) { FinishMarks m; for (int i = 0; i < MESH_COUNT; i++) m.n[i] = rl.opaque[i].size(); return m; }
void applyFinish(RenderList& rl, const FinishMarks& m, int finish, float time) {
  if (finish <= 0) return;
  for (int i = 0; i < MESH_COUNT; i++)
    for (size_t k = m.n[i]; k < rl.opaque[i].size(); k++) { Instance& in = rl.opaque[i][k]; if (in.params[0] > 0.5f) continue; in.params[2] = (float)finish; in.params[3] = time; }
}

// ---------------------------------------------------------------- growth / XP / heal
float Game::kaijuXp() const {
  const Profile& p = const_cast<Game*>(this)->P();
  auto it = p.xp.find(kaiju.def().id);
  return it == p.xp.end() ? E.growthFullXp : it->second;
}
float Game::growthFrac() const { return eggs::growthFrac(kaijuXp()); }
void Game::addXp(float xp, const Vec3* at) {
  if (xp <= 0 || E.rarities.empty()) return;
  Profile& p = P(); const std::string& id = kaiju.def().id;
  auto it = p.xp.find(id);
  float before = it == p.xp.end() ? E.growthFullXp : it->second;
  p.xp[id] = before + xp;
  xpPopAcc += xp;
  if (at && xpPopT <= 0 && xpPopAcc >= 1) {
    floats.push_back({"+" + std::to_string((int)std::lround(xpPopAcc)) + " XP", *at + Vec3(2, 3, 0), 1.0f, mixColor(uiAccent(), hexColor(0xffffff), 0.3f)});
    xpPopAcc = 0; xpPopT = 0.45f;
  }
  if (eggs::growthFrac(before) < 1 && eggs::growthFrac(before + xp) >= 1) { bigMessage(kaiju.def().name, "FULLY GROWN!"); play(SND_fanfare, 0.7f); }
}
float Game::healKaiju(float amount) {
  if (amount <= 0) return 0;
  float room = std::max(0.f, kaiju.maxHp() - kaiju.hp), h = std::min(room, amount), over = amount - h;
  kaiju.hp += h;
  heartPopAcc += h;
  if (heartPopT <= 0 && heartPopAcc >= 1) {
    floats.push_back({"+" + std::to_string((int)std::lround(heartPopAcc)) + " <3", kaiju.pos + Vec3(-3, 10 * kaiju.scale(), 0), 0.8f, hexColor(0xff6b9a)});
    heartPopAcc = 0; heartPopT = 0.5f;
  }
  float xp = over * E.healToXp;
  if (xp > 0) { Vec3 at = kaiju.pos + Vec3(0, 12 * kaiju.scale(), 0); addXp(xp, &at); }
  return xp;
}
void Game::tickPops(float dt) { xpPopT = std::max(0.f, xpPopT - dt); heartPopT = std::max(0.f, heartPopT - dt); }

// ---------------------------------------------------------------- spinner
KaijuLook Game::decideSpin(int reason) {
  Rng r(spinSeed ? spinSeed : (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count());
  spinSeed = 0;
  for (int k = 0; k < 3; k++) r.nextU64();
  (void)reason;  // every species is a playable kaiju: the first spin can never be a dud (and is always "new" - the roster is empty)
  std::set<std::string> owned; for (auto& kv : save.looks) owned.insert(kv.first);
  return kgen::rollEgg(r, T, owned);  // Rex: fewer duplicates, procedural seed, Tommyzilla legendary elements
}
bool Game::beginSpin(int reason) {
  if (reason == SPIN_TOKEN) { if (P().spins <= 0) { toast("No egg spins left - earn more by winning levels!"); play(SND_error); return false; } P().spins--; }
  KaijuLook l = decideSpin(reason);
  Rng r((uint64_t)std::chrono::steady_clock::now().time_since_epoch().count() ^ 0x5bd1e995ull);
  spinner.start(r, l, T, reason); spinReason = reason; spinNote.clear(); spinDup = false;
  menus.clear(); mode = Mode::Spin;
  play(SND_roarCharge, 0.5f, 1.6f);
  persist();
  return true;
}
bool Game::applySpinResult(const KaijuLook& l, int reason) {
  const RarityDef& rd = E.rarity(l.rarity);
  if (reason == SPIN_DEEVO) {  // the current kaiju is replaced: its look, rarity and growth are gone
    std::string old = save.monster;
    if (old != l.monster) { save.monsters.erase(old); save.looks.erase(old); for (Profile* p : {&save.sandbox, &save.adventure, &save.crazy}) p->xp.erase(old); }
    save.monsters.insert(l.monster); save.looks[l.monster] = l;
    P().xp[l.monster] = 0;
    for (Profile* p : {&save.sandbox, &save.adventure, &save.crazy}) if (p != &P() && !p->xp.count(l.monster)) p->xp[l.monster] = 0;
    if (!AP().lockedMonster.empty() && (AP().lockedMonster == old || adventure)) AP().lockedMonster = l.monster;  // new locked champion
    if (!save.crazy.lockedMonster.empty() && save.crazy.lockedMonster == old) save.crazy.lockedMonster = l.monster;
    save.monster = l.monster; kaiju.monster = std::max(0, T.monsterIndex(l.monster)); kaiju.hp = kaiju.maxHp();
    spinNote = "Your new kaiju: " + T.monsters[(size_t)kaiju.monster].name + "!";
    kaiju.camScale = -1; grantBonusSuper();
    return true;
  }
  bool owned = save.monsters.count(l.monster) > 0;
  if (reason == SPIN_CREATE || !owned) {
    if (reason == SPIN_CREATE) { save.monsters.clear(); save.looks.clear(); save.created = true; save.spinSlot = true; }
    save.monsters.insert(l.monster); save.looks[l.monster] = l;
    for (Profile* p : {&save.sandbox, &save.adventure, &save.crazy}) p->xp[l.monster] = 0;  // a hatchling in every mode
    if (reason == SPIN_CREATE) { save.monster = l.monster; kaiju.monster = std::max(0, T.monsterIndex(l.monster)); kaiju.hp = kaiju.maxHp(); kaiju.camScale = -1; grantBonusSuper(); }
    spinNote = (reason == SPIN_CREATE ? "Meet your kaiju: " : "New kaiju: ") + T.monsters[(size_t)std::max(0, T.monsterIndex(l.monster))].name + "!";
    return true;
  }
  // duplicate: coins by rarity + XP for that monster in this mode; a rarer finish upgrades the owned look
  Profile& p = P(); p.coins += rd.dupCoins; p.totalCoins += rd.dupCoins;
  auto it = p.xp.find(l.monster); if (it != p.xp.end()) it->second += rd.dupXp;
  KaijuLook& own = save.looks[l.monster]; if (own.monster.empty()) own = lookOf(l.monster);
  bool upgraded = l.finish > own.finish; if (upgraded) own.finish = l.finish;
  spinNote = "Duplicate! +" + std::to_string(rd.dupCoins) + " coins, +" + std::to_string((int)rd.dupXp) + " XP" + (upgraded ? " and a shiny " + E.finishes[(size_t)l.finish - 1].name + " finish!" : "");
  spinDup = true;
  return false;
}
void Game::grantBonusSuper() {
  if (!hasLook(kaiju.def().id)) return;
  KaijuLook l = lookOf(kaiju.def().id);
  if (l.bonusSuper.empty() || P().moves.count(l.bonusSuper)) return;
  int mi = T.moveIndex(l.bonusSuper); if (mi < 0) return;
  P().moves.insert(l.bonusSuper);
  toast(E.rarity(l.rarity).name + " bonus: " + T.moves[(size_t)mi].name + " unlocked!");
}
void Game::updateSpin(float dt, const InputState& in) {
  bool stopKey = in.pressed[A_CONFIRM] || in.click || in.pressed[A_CLAW] || in.pressed[A_SLOT1] || in.pressed[A_EAT] || in.pressed[A_STOMP] || in.pressed[A_TAIL];
  if (spinner.phase == Spinner::Spinning && stopKey) { spinner.stop(); play(SND_ui); }
  int before = spinner.cellUnderMarker();
  spinner.update(dt);
  if (spinner.cellUnderMarker() != before) play(SND_ui, 0.25f, 1.6f);  // reel tick
  if (spinner.phase == Spinner::Landed && spinner.landedT == 0) {
    bool isNew = applySpinResult(spinner.result, spinReason); spinner.isNew = isNew;
    play(SND_fanfare, 0.8f); flashT = 0.25f; persist();
  }
  if (spinner.phase == Spinner::Landed && spinner.landedT > 1.8f) {  // reveal shown: hatch the egg in 3D
    egg = Egg(); egg.active = true; egg.state = 1; egg.t = 0; egg.monster = spinner.result.monster; egg.spin = spinReason; egg.look = spinner.result; egg.hasLook = true;
    Vec3 f = kaiju.fwd(); egg.pos = kaiju.pos + f * 22; egg.pos.y = 0;
    if (spinReason == SPIN_CREATE) egg.pos = Vec3(E.zooX, 0, E.zooZ);
    mode = Mode::Reveal; revealT = 0; play(SND_hatch);
    spinner.phase = Spinner::Idle;
  }
}
void Game::drawSpinner(RenderList& rl) {
  UIDraw u(rl); float s = u.s;
  u.rect(0, 0, u.W, u.H, Color{0.06f, 0.03f, 0.1f, 0.82f});
  static const uint32_t rb[] = {0xff3b3b, 0xff9a2a, 0xffe03a, 0x4bd66b, 0x3a8dff, 0xb06bff};
  for (int k = 0; k < 6; k++) u.rect(0, u.H * 0.18f + k * 3 * s, u.W, 3 * s, hexColor(rb[k], 0.35f));  // rainbow band
  u.textC(u.W / 2, u.H * 0.08f, spinReason == SPIN_CREATE ? "HATCH YOUR KAIJU!" : spinReason == SPIN_DEEVO ? "A BRAND NEW EGG!" : "EGG SPIN!", s * 2.0f, hexColor(0xffd36b));
  float cw = 64 * s, ch = 78 * s, cy = u.H * 0.42f - ch / 2, mx = u.W / 2;
  int first = (int)std::floor(spinner.pos) - (int)(u.W / cw) / 2 - 2, last = first + (int)(u.W / cw) + 5;
  for (int i = first; i <= last; i++) {
    if (i < 0 || i >= (int)spinner.reel.size()) continue;
    float x = mx + (i - spinner.pos) * cw - cw / 2 + 3 * s;
    const RarityDef& rd = E.rarity(spinner.reelRarity[(size_t)i]);
    int mi = std::max(0, T.monsterIndex(spinner.reel[(size_t)i]));
    bool win = spinner.phase >= Spinner::Landed && i == spinner.stopIndex;
    Color bg = mixColor(Color{0.12f, 0.1f, 0.18f, 1}, rd.color, win ? 0.45f : 0.18f);
    u.rect(x, cy, cw - 6 * s, ch, bg);
    u.rect(x, cy + ch - 6 * s, cw - 6 * s, 6 * s, rd.color);  // rarity stripe
    // egg: shell in the monster's classic accent, rarity speckles
    const MonsterDef& md = T.monsters[(size_t)mi];
    float ex = x + (cw - 6 * s) / 2, ey = cy + ch * 0.45f;
    for (int r = 0; r < 9; r++) { float t = r / 8.f, w = (cw * 0.42f) * std::sqrt(std::max(0.f, 1 - (t - 0.55f) * (t - 0.55f) / 0.36f)); u.rect(ex - w / 2, ey - ch * 0.3f + t * ch * 0.55f, w, ch * 0.065f, mixColor(hexColor(0xfff3d0), md.accent, 0.25f)); }
    u.rect(ex - 5 * s, ey - 4 * s, 4 * s, 4 * s, rd.color); u.rect(ex + 3 * s, ey + 6 * s, 4 * s, 4 * s, rd.color);
  }
  // centre marker
  u.rect(mx - 1.5f * s, cy - 10 * s, 3 * s, ch + 20 * s, hexColor(0xffe03a));
  u.star(mx, cy - 12 * s, 6 * s, hexColor(0xffe03a));
  if (spinner.phase == Spinner::Spinning) {
    u.textC(mx, cy + ch + 18 * s, "Press " + btn("A", "Space / Enter / click") + " to STOP!", s * 1.2f, hexColor(0xffffff));
  } else if (spinner.phase == Spinner::Landed) {
    const KaijuLook& l = spinner.result; const RarityDef& rd = E.rarity(l.rarity);
    float glow = 0.5f + 0.5f * std::sin(realTime * 8);
    u.frame(mx - cw / 2 - 4 * s, cy - 4 * s, cw + 2 * s, ch + 8 * s, mixColor(rd.color, hexColor(0xffffff), glow * 0.4f), 3 * s);
    std::string nm = T.monsters[(size_t)std::max(0, T.monsterIndex(l.monster))].name;
    u.textC(mx, cy + ch + 14 * s, rd.name + "!", s * 2.2f, rd.color);
    u.textC(mx, cy + ch + 14 * s + u.lineH(s * 2.2f), nm + (l.finish ? "  -  " + E.finishes[(size_t)l.finish - 1].name + " FINISH!" : ""), s * 1.3f, l.finish ? E.finishes[(size_t)l.finish - 1].color : hexColor(0xffffff));
    if (!spinNote.empty()) u.textC(mx, cy + ch + 14 * s + u.lineH(s * 2.2f) + u.lineH(s * 1.3f) + 4 * s, spinNote, s, hexColor(0xd8e0ff));
    float sw = 26 * s; for (int k = 0; k < 4; k++) { Color c = k == 0 ? l.primary : k == 1 ? l.secondary : k == 2 ? l.accent : l.eyes; u.rect(mx - 2 * sw + k * sw + 2 * s, u.H - 40 * s, sw - 4 * s, 14 * s, c); }
  }
}

// ---------------------------------------------------------------- De-Evolution Ray (store item)
bool Game::canDeevo(std::string* why) {
  if (P().coins < E.deevoPrice) { if (why) *why = "Need " + std::to_string(E.deevoPrice) + " coins"; return false; }
  if (boss.active || level.bossPhase) { if (why) *why = "Not during a boss fight!"; return false; }
  if (deevoT >= 0 || zoo.active || mode != Mode::Play) { if (why) *why = "Not right now"; return false; }
  return true;
}
void Game::openDeevoConfirm() {
  std::string why;
  if (!canDeevo(&why)) { toast(why); play(SND_error); return; }
  MenuItem no; no.label = "No, keep my kaiju"; no.act = [this]() { popMenu(); };
  MenuItem yes; yes.label = "Yes - zap it! (" + std::to_string(E.deevoPrice) + " coins)"; yes.act = [this]() { menus.clear(); buyDeevo(); };
  prompt("De-Evolution Ray", "Your kaiju will turn back into an egg. Are you sure?", {no, yes}, false);
}
bool Game::buyDeevo() {
  if (!canDeevo()) return false;
  P().coins -= E.deevoPrice;
  deevoT = 0; deevoPos = kaiju.pos;
  flashT = 0.35f; play(SND_roarCharge, 1, 0.5f); kaiju.cam.shake = 0.8f;
  persist();
  return true;
}
void Game::updateDeevo(float dt) {
  if (deevoT < 0) return;
  float prev = deevoT; deevoT += dt;
  kaiju.shrinkK = clampf(1 - (deevoT - 0.8f) / 1.6f, 0.05f, 1);
  if (prev < 0.8f && deevoT >= 0.8f) { fx->ring(deevoPos.x, deevoPos.z, 60, 1.0f, hexColor(0x9ae8ff)); fx->ring(deevoPos.x, deevoPos.z, 40, 0.8f, hexColor(0xff6bd5)); flashT = 0.6f; play(SND_boom, 0.9f, 1.4f); vibrate(0.8f, 0.4f); }
  if (frand() < dt * 30) fx->particle(deevoPos + Vec3((frand() - 0.5f) * 10, frand() * 40, (frand() - 0.5f) * 10), Vec3(0, -20, 0), 0.6f, 0.6f, hexColor(frand() < 0.5f ? 0x9ae8ff : 0xffffff));
  if (deevoT >= 3.0f) { deevoT = -1; kaiju.shrinkK = 1; beginSpin(SPIN_DEEVO); }
}
void Game::renderDeevo(RenderList& rl) {
  if (deevoT < 0) return;
  float k = clampf(deevoT / 0.5f, 0, 1), w = 6 + 3 * std::sin(realTime * 20);
  Color beam = hexColor(0x9ae8ff); beam.a = 0.55f * k;
  rl.alpha[MESH_CYL].push_back(makeInstance(Mat4::translation(0, 0.5f, 0) * Mat4::scale(w * 2, 300, w * 2) * Mat4::translation(deevoPos.x, 0, deevoPos.z), beam, 1));
  rl.opaque[MESH_CYL].push_back(makeInstance(Mat4::translation(0, 0.5f, 0) * Mat4::scale(w * 0.5f, 300, w * 0.5f) * Mat4::translation(deevoPos.x, 0, deevoPos.z), hexColor(0xffffff), 1));
  if (kaiju.shrinkK < 0.6f) {  // the egg it turns back into
    float es = (0.6f - kaiju.shrinkK) * 1.6f;
    rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(7 * es, 9.5f * es, 7 * es) * Mat4::translation(deevoPos.x, 4.6f * es, deevoPos.z), mixColor(hexColor(0xfff3d0), uiAccent(), 0.2f)));
  }
}

// ---------------------------------------------------------------- Kaiju Zoo opening
bool Game::zooEligible() const { return save.spinSlot && !const_cast<Game*>(this)->P().zooDone; }
Vec3 Game::openSpot(float x, float z, float r) {
  for (int ring = 0; ring < 14; ring++) for (int a = 0; a < (ring == 0 ? 1 : 12); a++) {
    float ang = a * (PI / 6) + ring * 0.37f, d = ring * 6.f;
    float px = x + std::cos(ang) * d, pz = z + std::sin(ang) * d; bool ok = true;
    for (int k = 0; k < 8 && ok; k++) { float kk = k * PI / 4; if (world.blockedForPerson(px + std::cos(kk) * r, pz + std::sin(kk) * r)) ok = false; }
    if (ok && !world.blockedForPerson(px, pz) && !world.isWater(px, pz)) return Vec3(px, 0, pz);
  }
  return Vec3(x, 0, z);
}
void Game::startZoo(bool thenLevel) {
  zoo = ZooRun(); zoo.active = true; zoo.thenLevel = thenLevel;
  world.updateStreaming(E.zooX, E.zooZ, true);
  zoo.center = openSpot(E.zooX, E.zooZ, E.zooCageR + 2);
  enemies.reset(); stopLevel(); level = LevelRun();
  kaiju.respawn(zoo.center.x, zoo.center.z, PI);
  // onlookers just outside the fence (out of gulp range from inside the cage)
  int n = 0;
  for (auto& p : people.list) {
    if (n >= E.zooOnlookers) break;
    float a = n * (2 * PI / E.zooOnlookers) + 0.3f, d = E.zooCageR + kaiju.eatRange() + 10;
    p = Person(); p.active = true; p.look = p.look + n * 3; p.x = zoo.center.x + std::cos(a) * d; p.z = zoo.center.z + std::sin(a) * d; p.dir = std::atan2(zoo.center.x - p.x, zoo.center.z - p.z); p.idle = true;
    n++;
  }
  people.activeCount = std::max(people.activeCount, n);
  float a0 = 0.6f; zoo.keeper = zoo.center + Vec3(std::cos(a0), 0, std::sin(a0)) * (E.zooCageR + 26); zoo.keeperDir = std::atan2(zoo.center.x - zoo.keeper.x, zoo.center.z - zoo.keeper.z);
  bigMessage("KAIJU ZOO", "Your hatchling is in a cage...");
  mode = Mode::Play; menus.clear();
}
void Game::grantZookeeperKey() { P().zooKey = true; save.zooCompleted = true; }  // silent on purpose: no toast, no message
void Game::skipZoo() {
  if (!zoo.active) return;
  if (save.zooCompleted) grantZookeeperKey();  // skipped, but finished before in this slot: the key still comes along
  finishZoo();
}
void Game::finishZoo() {
  bool lvl = zoo.thenLevel;
  zoo.active = false; P().zooDone = true;
  persist();
  if (lvl && adventure) startLevel(AP().level);
  else toast("Welcome to Victoria, " + kaiju.def().name + "! Smash buildings to earn coins");
}
void Game::updateZoo(float dt, const InputState& in) {
  if (!zoo.active) return;
  zoo.t += dt; zoo.stateT += dt;
  if (in.pressed[A_SKILLS] && zoo.state < 2) { skipZoo(); return; }  // skip key (Tab / D-pad down)
  // keep the hatchling inside the cage until the bars break
  if (!zoo.barsBroken) {
    float r = E.zooCageR - 2;
    auto cage = [&](Vec3& p) { float d = dist2D(p.x, p.z, zoo.center.x, zoo.center.z); if (d > r) { Vec3 v(p.x - zoo.center.x, 0, p.z - zoo.center.z); v = v * (r / std::max(0.01f, d)); p.x = zoo.center.x + v.x; p.z = zoo.center.z + v.z; } };
    cage(kaiju.pos); if (coopOn() && coopJoined) cage(kaiju2.pos);  // co-op: player 2 is locked in too
  }
  kaiju.hp = kaiju.maxHp();  // the player can't fail here
  float kd = dist2D(kaiju.pos.x, kaiju.pos.z, zoo.keeper.x, zoo.keeper.z);
  zoo.inRange = !zoo.keeperEaten && kd <= kaiju.eatRange() * E.zooEatReach;  // Rex: generous reach in the opening
  if (zoo.state == 0) {  // the zookeeper strolls up to the bars, much too close
    Vec3 to = zoo.center - zoo.keeper; to.y = 0; float d = length(to);
    float stop = E.zooKeeperStop;
    if (d > stop) { Vec3 dir = to * (1.f / d); zoo.keeper += dir * std::min(d - stop, 7.f * dt); zoo.keeperDir = std::atan2(dir.x, dir.z); }
    else { zoo.state = 1; zoo.stateT = 0; zoo.idleT = 0; }
    if (zoo.state == 0 && zoo.inRange && zooGulpPressed(in)) { zoo.state = 1; zoo.stateT = 0; zoo.idleT = 0; }  // in reach while he strolls up: gulp now (the prompt already shows)
  }
  if (zoo.state == 1) {
    zoo.idleT += dt;
    zoo.keeperDir = std::atan2(kaiju.pos.x - zoo.keeper.x, kaiju.pos.z - zoo.keeper.z);
    if (zooGulpPressed(in) && zoo.inRange) {  // GULP (Rex: any action button works here)
      zoo.keeperEaten = true; zoo.state = 2; zoo.stateT = 0;
      EatTween e; e.from = zoo.keeper; e.t = 0; e.look = 7; kaiju.eats.push_back(e); kaiju.attackSeq++;
      kaiju.facing = std::atan2(zoo.keeper.x - kaiju.pos.x, zoo.keeper.z - kaiju.pos.z);
      P().eaten++; P().eatenTotal++; onPeoplePopped(1);
      play(SND_gulp, 1, 0.9f); fx->rainbowBurst(kaiju.mouth(), 2.2f);
      grantZookeeperKey();
      Vec3 at = kaiju.pos + Vec3(0, 10, 0); addXp(E.xpFor("zoo"), &at);
      kaiju.popT = 0.6f;
      bigMessage("GULP!", "Your kaiju grew!"); play(SND_roar, 0.9f, 1.3f);
    }
  } else if (zoo.state == 2 && zoo.stateT > 1.2f) {  // bars shatter
    zoo.state = 3; zoo.stateT = 0; zoo.barsBroken = true;
    for (int b = 0; b < 28; b++) {
      float a = b * (2 * PI / 28); Vec3 p = zoo.center + Vec3(std::cos(a) * E.zooCageR, 3 + frand() * 6, std::sin(a) * E.zooCageR);
      for (int k = 0; k < 3; k++) debris->spawn(p, Vec3(std::cos(a) * (10 + frand() * 10), 6 + frand() * 10, std::sin(a) * (10 + frand() * 10)), hexColor(k % 2 ? 0x9aa0aa : 0x5a6070), 0.5f + frand() * 0.5f);
    }
    fx->ring(zoo.center.x, zoo.center.z, E.zooCageR * 2.5f, 0.8f, hexColor(0xffd36b));
    kaiju.cam.shake = 1.4f; vibrate(1, 0.4f); play(SND_collapse, 1, 1.2f);
    bigMessage("BREAKOUT!", "Go smash the city!");
  } else if (zoo.state == 3 && zoo.stateT > 1.6f) finishZoo();
}
void Game::drawZookeeper(RenderList& rl, const Vec3& at, float dir, bool walking) {  // a civilian with a green cap and a key ring
  Person kp; kp.active = true; kp.x = at.x; kp.z = at.z; kp.dir = dir; kp.look = 7; kp.idle = !walking; kp.vx = walking ? 3.f : 0.f;
  Mat4 root = Mat4::rotY(dir) * Mat4::translation(at.x, 0, at.z);
  if (!personModel(rl, kp, 900, lastEye)) {
    rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(1.5f, 2.0f, 1.0f) * Mat4::translation(0, 1.6f, 0) * root, hexColor(0x4a7a3a)));
    rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(1.2f, 1.2f, 1.2f) * Mat4::translation(0, 3.3f, 0) * root, hexColor(0xf1c9a0)));
  }
  rl.opaque[MESH_CYL].push_back(makeInstance(Mat4::scale(1.3f, 0.45f, 1.3f) * Mat4::translation(0, 3.85f, 0) * root, hexColor(0x2f8a3a)));     // cap
  rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(1.1f, 0.15f, 0.7f) * Mat4::translation(0, 3.7f, 0.7f) * root, hexColor(0x2f8a3a)));   // brim
  rl.opaque[MESH_RING].push_back(makeInstance(Mat4::scale(0.35f, 0.35f, 0.35f) * Mat4::translation(0.8f, 1.6f, 0.2f) * root, hexColor(0xffd36b), 1));  // key ring
}
void Game::renderZoo(RenderList& rl) {
  bool closing = closingShotVisible();
  if (!zoo.active && !closing) return;
  Vec3 c = closing ? Vec3(E.zooX, 0, E.zooZ) : zoo.center; float R = E.zooCageR;
  if (!zoo.barsBroken || closing) {
    int N = 28;
    for (int b = 0; b < N; b++) { float a = b * (2 * PI / N); rl.opaque[MESH_CYL].push_back(makeInstance(Mat4::translation(0, 0.5f, 0) * Mat4::scale(0.5f, 12, 0.5f) * Mat4::translation(c.x + std::cos(a) * R, 0, c.z + std::sin(a) * R), hexColor(0x6a7080))); }
    for (float y : {1.f, 11.5f}) for (int b = 0; b < N; b++) {
      float a0 = b * (2 * PI / N), a1 = (b + 1) * (2 * PI / N); Vec3 p0 = c + Vec3(std::cos(a0) * R, y, std::sin(a0) * R), p1 = c + Vec3(std::cos(a1) * R, y, std::sin(a1) * R);
      Vec3 m = (p0 + p1) * 0.5f; float len = length(p1 - p0), yaw = std::atan2(p1.x - p0.x, p1.z - p0.z);
      rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(0.5f, 0.5f, len) * Mat4::rotY(yaw) * Mat4::translation(m.x, m.y, m.z), hexColor(0x5a6070)));
    }
  }
  // "KAIJU ZOO" sign on two posts (the text is drawn by the HUD at the projected sign position)
  Vec3 sp = c + Vec3(0, 0, -(R + 5));
  for (float sx : {-4.f, 4.f}) rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(0.5f, 9, 0.5f) * Mat4::translation(sp.x + sx, 4.5f, sp.z), hexColor(0x8a5a2a)));
  rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(10, 3.2f, 0.4f) * Mat4::translation(sp.x, 8, sp.z), hexColor(closing ? 0xfff3d0 : 0x2fa89a)));
  if (closing) return;
  if (!zoo.keeperEaten) drawZookeeper(rl, zoo.keeper, zoo.keeperDir, zoo.state == 0);
}

// ---------------------------------------------------------------- Zoo Gates (secret; Zookeeper's Key)
void Game::setupGates() {
  gates.clear();
  int lv = adventure ? (level.active || level.won ? level.idx + 1 : -1) : 0;
  if (lv < 0) return;
  for (auto& g : E.gates) {
    if (g.level != lv) continue;
    GateRun r; r.id = g.id; r.pos = openSpot(g.x, g.z, 6); r.reward = g.reward; r.amount = g.amount; r.open = P().gates.count(g.id) > 0; r.openT = r.open ? 1 : 0;
    gates.push_back(r);
  }
  for (size_t i = 0; i < E.stones.size(); i++) {
    const StoneDef& s = E.stones[i];
    if (!adventure || s.level != lv) continue;
    GateRun r; r.id = "stone_" + s.id; r.pos = world.procedural && i < world.stoneBuildings.size() ? openSpot(world.buildings[(size_t)world.stoneBuildings[i]].x, world.buildings[(size_t)world.stoneBuildings[i]].z, 24) : openSpot(s.x, s.z, 8);  // Atlas: room for the gate pen + stone
    r.stone = (int)i; r.reward = "stone";
    gates.push_back(r);
  }
}
void Game::openGate(GateRun& g) {
  g.open = true; play(SND_unlock, 0.9f, 1.1f); fx->rainbowBurst(g.pos + Vec3(0, 5, 0), 1.6f);
  if (g.stone >= 0) return;  // stone gates: the stone itself is the reward (picked up separately)
  if (P().gates.count(g.id)) return;  // each reward only once
  P().gates.insert(g.id);
  Vec3 at = g.pos + Vec3(0, 8, 0);
  if (g.reward == "coins") { awardCoins(g.amount, at); }
  else if (g.reward == "spin") { P().spins += g.amount; floats.push_back({"+" + std::to_string(g.amount) + " EGG SPIN", at, 1.6f, hexColor(0xffd36b)}); }
  else if (g.reward == "finish") {
    Rng r((uint64_t)std::chrono::steady_clock::now().time_since_epoch().count());
    KaijuLook l = lookOf(kaiju.def().id); int f = 1 + (int)(r.next() * 3) % 3;  // a guaranteed finish roll
    if (f > l.finish) { l.finish = f; save.looks[l.monster] = l; }
    floats.push_back({E.finishes[(size_t)std::max(1, l.finish) - 1].name + " FINISH!", at, 1.6f, E.finishes[(size_t)std::max(1, l.finish) - 1].color});
  }
  persist();
}
void Game::updateGates(float dt) {
  for (auto& g : gates) {
    if (g.stone >= 0 && !stoneRevealed(g.stone)) continue;  // Atlas: still hidden inside its building
    if (g.open) { g.openT = std::min(1.f, g.openT + dt); continue; }
    if (P().zooKey && dist2D(kaiju.pos.x, kaiju.pos.z, g.pos.x, g.pos.z) < 7 + kaiju.radius()) openGate(g);
  }
}
void Game::renderGates(RenderList& rl) {
  for (auto& g : gates) {
    // Adventure L14-19: the stone gate is always on the map (like Victoria's fixed gates) with a beacon so it can be
    // found from afar; it stays locked until its purple building next to it is smashed (stoneRevealed).
    bool hidden = g.stone >= 0 && !stoneRevealed(g.stone);
    if (g.stone >= 0 && !level.stoneTaken) {
      Color c = E.stones[(size_t)g.stone].color; c.a = 0.28f;
      rl.alpha[MESH_CYL].push_back(makeInstance(Mat4::translation(0, 0.5f, 0) * Mat4::scale(2.2f, 120, 2.2f) * Mat4::translation(g.pos.x, 0, g.pos.z - 8), c, 1));
    }
    Mat4 root = Mat4::translation(g.pos.x, 0, g.pos.z);
    // fenced pen behind the gate
    for (int b = 0; b < 16; b++) { float a = b * (2 * PI / 16); if (b == 4) continue; rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(0.4f, 5, 0.4f) * Mat4::translation(std::cos(a) * 7, 2.5f, std::sin(a) * 7 - 8) * root, hexColor(0x8a6a4a))); }
    for (float sx : {-3.5f, 3.5f}) rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(0.8f, 7, 0.8f) * Mat4::translation(sx, 3.5f, 0) * root, hexColor(0x6a4a2a)));
    float lift = g.openT * 6.5f;
    for (int bar = 0; bar < 6; bar++) rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(0.3f, 6, 0.3f) * Mat4::translation(-2.5f + bar, 3 + lift, 0) * root, hexColor(0x5a6070)));
    (void)hidden;
    if (!g.open) {  // padlock sign
      rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(1.4f, 1.4f, 0.4f) * Mat4::translation(0, 3.2f, 0.4f) * root, hexColor(0xffd36b), 0.5f));
      rl.opaque[MESH_RING].push_back(makeInstance(Mat4::scale(0.9f, 0.9f, 0.9f) * Mat4::rotX(PI / 2) * Mat4::translation(0, 4.1f, 0.4f) * root, hexColor(0xc8ccd4), 1));
    } else if (g.stone < 0 && !P().gates.count(g.id)) {}
  }
}
}  // namespace tz
