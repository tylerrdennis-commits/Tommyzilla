// Game flow: modes, events from the world, unlocks, tutorial, adventure levels, eggs, feedback.
#include <algorithm>
#include <chrono>
#include <ctime>
#include "game.h"
#include "../ui/ui_draw.h"
namespace tz {
static const uint32_t k_gloveStones[6] = {0xb06bff, 0x3a8dff, 0x4bd66b, 0xff3b3b, 0xff9a2a, 0xffe03a};  // purple, blue, green, red, orange, yellow
Game* G = nullptr;

std::string loadGameData(const std::string& cfg, const std::string& moves, const std::string& monsters, const std::string& levels, const std::string& eggsJson) {
  C = Config();
  std::string err;
  if (!cfg.empty() && !C.loadJson(cfg, &err)) return "config.json: " + err;
  if (!T.load(moves, monsters, levels, &err)) return "data tables: " + err;
  err = loadEggData(eggsJson);
  if (!err.empty()) return err;
  return "";
}

Game::Game() { G = this; debris = new Debris(); fx = new Effects(); }
Game::~Game() { delete debris; delete fx; if (G == this) G = nullptr; }

static const char* TUT_IDS[] = {"move", "claw", "tail", "stomp", "breath", "grab", "police"};
constexpr int TUT_N = 7;

bool Game::loadSave(const std::string* saveJson) {
  save.reset();
  bool ok = true;
  vault = vpow::State();  // Vault powers + cheats unlock live beside SaveData in the same file ("vault" key)
  if (saveJson && !saveJson->empty()) { Json j; ok = Json::parse(*saveJson, j) && save.fromJson(j); if (!ok) save.reset(); else vault.fromJson(j["vault"]); }  // damaged -> fresh game
  if (save.sandbox.moves.empty()) prog::initProfile(save.sandbox, T);
  if (save.adventure.moves.empty()) prog::initProfile(save.adventure, T);
  if (save.crazy.moves.empty()) prog::initProfile(save.crazy, T);
  prog::normalizeSlots(save.sandbox, T); prog::normalizeSlots(save.adventure, T); prog::normalizeSlots(save.crazy, T);  // older saves: grant the blaster, fix slot types
  int mi = T.monsterIndex(save.monster); kaiju.monster = mi < 0 ? 0 : mi;
  adventure = false; crazy = false; boss = Boss(); chests.clear(); egg = Egg(); level = LevelRun();
  return ok;
}
void Game::init(const std::string* saveJson) {
  G = this;
  loadSave(saveJson);
  if (credits.empty()) loadCredits("");  // data/credits.json (embedded copy)
  bodies.list.clear(); bodies.list.reserve(8192);
  world.init(); splats.init(); people.init(); enemies.reset();
  kaiju.respawn(-55, 135, PI);
  mode = Mode::Title; menus.clear();
  if (playIntro) { mode = Mode::Intro; introT = 0; introWarmup = 0; return; }
  if (slots && slot < 0) openSlotMenu(); else openMainMenu();
}
void Game::finishIntro() {
  if (mode != Mode::Intro) return;
  introT = INTRO_END; mode = Mode::Title; menus.clear();
  if (slots && slot < 0) openSlotMenu(); else openMainMenu();
}
void Game::updateIntro(float dt, const InputState& in) {
  if (introWarmup < 30) { introWarmup++; dt = 0; }
  if (introHold && realTime < 5) dt = 0;  // wait (up to 5 s) until our window is actually on screen  // first frames (window appearing, shader warm-up) don't eat into the studio card
  float t0 = introT; introT += dt;
  bool any = in.click; for (int i = 0; i < A_COUNT; i++) any = any || in.pressed[i];
  if (any && (t0 > 0.1f || introWarmup >= 30)) { finishIntro(); return; }  // any key / click / button skips straight to the menu
  auto crossed = [&](float t) { return t0 < t && introT >= t; };
  if (crossed(5.6f)) { play(SND_quake, 0.7f); vibrate(0.5f, 1.0f); }
  if (crossed(9.0f)) { play(SND_roar, 1.0f, 0.9f); vibrate(0.8f, 0.6f); }
  world.update(dt); people.update(dt, -40, 40, 0, 1); fx->update(dt);
  if (introT >= INTRO_END) finishIntro();
}
bool Game::selectSlot(int n) {
  if (!slots || n < 0 || n >= SAVE_SLOTS) return false;
  std::string text; bool have = slots->read(n, text);
  bool ok = loadSave(have ? &text : nullptr);
  if (have && !ok) { slots->backupCorrupt(n); toast("That save was damaged - starting a fresh game in slot " + std::to_string(n + 1)); }
  slot = n;
  const SlotStore* st = slots;
  saveText = [st, n](const std::string& t) { return st->write(n, t); };
  enemies.reset(); world.restoreAll(); kaiju.respawn(-55, 135, PI);
  if (!have || !ok) { save.created = false; save.spinSlot = true; }  // brand-new slot: character creation = the egg spinner
  persist();  // creates the file for a new slot
  menus.clear(); mode = Mode::Title;
  if (!save.created) beginSpin(SPIN_CREATE); else openMainMenu();
  return ok;
}
bool Game::deleteSlot(int n) {
  if (!slots || n < 0 || n >= SAVE_SLOTS) return false;
  bool r = slots->remove(n);
  if (n == slot) { slot = -1; saveText = nullptr; loadSave(nullptr); }
  return r;
}
void Game::persist() {
  std::time_t now = std::time(nullptr); char buf[32] = {}; std::tm* lt = std::localtime(&now);
  if (lt && std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", lt)) save.lastPlayed = buf;
  if (saveText) { Json j = save.toJson(); j.set("vault", vault.toJson()); saveText(j.dump(2)); }  // Wren: persist Vault powers/cheats
}

// ---------------------------------------------------------------- feedback
void Game::play(SoundId id, float vol, float pitch) {
  static float last[SND_COUNT] = {};
  static const struct { SoundId id; float gap; } gaps[] = {{SND_crumble, 0.12f}, {SND_coin, 0.08f}, {SND_pop, 0.05f}, {SND_hurt, 0.3f}, {SND_shot, 0.08f}, {SND_wreck, 0.1f}, {SND_collapse, 0.2f}, {SND_bomb, 0.1f}};
  float now = realTime;
  for (auto& g : gaps) if (g.id == id) { if (now - last[id] < g.gap && now >= last[id]) return; last[id] = now; }
  if (save.settings.muted || vol <= 0.01f) return;
  if (sounds.size() < 64) sounds.push_back({id, vol, pitch});
}
void Game::playAt(SoundId id, const Vec3& p, float vol, float pitch) {
  float d = length(p - kaiju.pos);
  float k = clampf(1 - d / 260, 0, 1);
  play(id, vol * (0.25f + 0.75f * k) * (k > 0 ? 1.f : 0.f), pitch);
}
bool Game::playPolice(SoundId id, const Vec3& p, float vol, float pitch) {
  policeVoiceEnds.erase(std::remove_if(policeVoiceEnds.begin(), policeVoiceEnds.end(), [this](float e) { return e <= realTime; }), policeVoiceEnds.end());
  if ((int)policeVoiceEnds.size() >= std::max(1, (int)C.policeMaxVoices)) return false;  // don't let many cops stack up
  float len = id == SND_cannon ? 0.6f : id == SND_bomb ? 0.9f : id == SND_wreck ? 0.5f : 0.2f;  // synth lengths (see audio_synth.cpp)
  size_t before = sounds.size();
  playAt(id, p, vol * C.policeVolume, pitch);
  if (sounds.size() == before) return false;  // rate-limited / muted / out of earshot
  policeVoiceEnds.push_back(realTime + len);
  return true;
}
void Game::scream(const Vec3& p) {
  if (time - lastScreamT < C.screamMinGap) return;  // rate-limited squeaky screams
  lastScreamT = time;
  playAt((SoundId)(SND_scream1 + rng.irange(0, 5)), p, 0.7f, 0.9f + rng.next() * 0.35f);
}
void Game::toast(const std::string& s) {
  for (auto& t : toasts) if (t.text == s) { t.t = 3.5f; return; }
  toasts.push_back({s, 3.5f}); if (toasts.size() > 6) toasts.erase(toasts.begin());
}
void Game::bigMessage(const std::string& top, const std::string& big) { bigTop = top; bigText = big; bigT = 3.0f; }
void Game::awardCoins(int n, const Vec3& at) {
  if (n <= 0) return;
  if (hasLook(kaiju.def().id) && lookOf(kaiju.def().id).has("magnet")) n = (int)std::ceil(n * 1.25f);  // Coin Magnet passive
  if (diff().coins != 1.f) n = (int)std::ceil(n * diff().coins);  // Titan: difficulty / player-count coin scale
  P().coins += n; P().totalCoins += n;
  floats.push_back({"+" + std::to_string(n), at, 1.2f, hexColor(0xffd36b)}); if (floats.size() > 40) floats.erase(floats.begin());
  play(SND_coin, 0.6f);
}

// ---------------------------------------------------------------- world events
void Game::onBricks(int n) { enemies.addHeat(n * C.heatPerBrick); }
void Game::onPeoplePopped(int n) {
  if (n <= 0) return;
  enemies.addHeat(n * C.heatPerPerson);
  Profile& p = P(); p.rainbows += n;
  int c = n * (int)C.coinsPerRainbow; if (c > 0) { p.coins += c; p.totalCoins += c; }
  if (tutorialActive()) {
    int before = save.tutorial.rainbows; save.tutorial.rainbows += n;
    int tgt = (int)C.tutorialRainbows;
    if (before < tgt && save.tutorial.rainbows >= tgt) { toast("Rainbow challenge done! " + std::to_string(tgt) + "/" + std::to_string(tgt) + " rainbows"); play(SND_unlock, 0.7f); }
  }
}
void Game::onKaijuHit() {
  if (time - kaiju.lastHitCountT < C.hitCountGap) return;  // continuous fire counts as one hit per gap
  kaiju.lastHitCountT = time;
  Profile& p = P(); p.hitsTaken++;
  int need = (int)C.roarUnlockHits, roar = T.moveIndex("roar");
  if (roar >= 0 && !p.moves.count("roar") && p.hitsTaken < need && p.hitsTaken % 5 == 0) toast("Tough monster! Hits survived " + std::to_string(p.hitsTaken) + "/" + std::to_string(need));
  checkUnlocks();
}
bool Game::tutorialGoalsMet() const {
  return save.tutorial.rainbows >= (int)C.tutorialRainbows && save.tutorial.buildings >= (int)C.tutorialBuildings;
}
void Game::onCollapse(Building& b) { if (world.chunks[b.chunk].active) awardCoins((int)C.collapseBonusCoins, Vec3(b.x, 12, b.z)); }
void Game::schoolLockedHit() {
  if (world.schoolMsgT > 0) return;
  toast("Smash " + std::to_string((int)C.schoolUnlockCount) + " landmarks to unlock Ruth King Elementary (" + std::to_string(prog::schoolProgress(P())) + "/" + std::to_string((int)C.schoolUnlockCount) + ")");
  world.schoolMsgT = 2.5f;
}
void Game::onBuildingSmashed(Building& b) {
  Profile& p = P();
  p.smashCount++;
  if (enemies.chase.searching) { enemies.chase.onBuildingSmashed(); toast("The police heard that! The search is back on!"); }  // Titan
  enemies.addHeat(C.heatPerBuilding);
  if (tutorialActive()) {
    int before = save.tutorial.buildings; save.tutorial.buildings++;
    int tgt = (int)C.tutorialBuildings;
    if (before < tgt && save.tutorial.buildings >= tgt) { toast("Building challenge done! " + std::to_string(tgt) + "/" + std::to_string(tgt) + " smashed"); play(SND_unlock, 0.7f); }
  }
  if (b.school) p.schoolSmashed = true;
  { Vec3 at(b.x, b.maxY + 4, b.z); addXp(E.xpFor("building"), &at); }
  if (!adventure && E.sandboxSpinEvery > 0 && p.smashCount - p.spinSmashMark >= E.sandboxSpinEvery) {
    p.spinSmashMark = p.smashCount; p.spins++; toast("+1 EGG SPIN! (Pause > Egg Spinner)"); play(SND_unlock, 0.6f);
  }
  if (level.active) {
    const LevelDef& L = T.levels[level.idx];
    if (L.obj == ObjType::Smash && (L.filter.empty() || (L.filter == "oldtown" && b.oldtown))) { level.count++; toast(L.name + ": " + std::to_string(level.count) + "/" + std::to_string(L.count)); }
    if (L.obj == ObjType::Landmarks) level.count++;
  }
  checkUnlocks(); checkAdventureGate(); persist();
}
void Game::onLandmarkSmashed(Building& b) {
  Profile& p = P();
  bool schoolLm = std::find(std::begin(C.schoolLandmarks), std::end(C.schoolLandmarks), b.landmark) != std::end(C.schoolLandmarks);
  bool first = !p.landmarks.count(b.landmark);
  p.landmarks.insert(b.landmark);
  { Vec3 at(b.x, b.maxY + 6, b.z); addXp(E.xpFor("landmark"), &at); }
  if (level.active) level.zoneT += 0;  // landmark objectives read the world state directly
  if (schoolLm && first) {
    int n = prog::schoolProgress(p);
    toast(b.name + " smashed! " + std::to_string(std::min(n, (int)C.schoolUnlockCount)) + "/" + std::to_string((int)C.schoolUnlockCount) + " landmarks");
    if (!p.schoolUnlocked && n >= (int)C.schoolUnlockCount) {
      p.schoolUnlocked = true; bigMessage("UNLOCKED!", "Ruth King Elementary"); play(SND_unlock);
      toast("Ruth King Elementary is now smashable - stomp out west!");
    }
  } else toast("You smashed the " + b.name + "!");
  checkUnlocks(); persist();
}
void Game::onUnitDestroyed(UnitType t) {
  { Vec3 at = kaiju.pos + Vec3(0, 12 * kaiju.scale(), 0); addXp(t == U_MBOSS ? E.xpFor("miniboss") : (t == U_ATANK || t == U_PTANK || t == U_ARMOR || t == U_JET || t == U_AHELI || t == U_PHELI) ? E.xpFor("bigEnemy") : E.xpFor("enemy"), &at); }
  if (!level.active) return;
  const LevelDef& L = T.levels[level.idx];
  if (t == U_MBOSS) { if (level.bossPhase) level.bossDown = true; return; }
  if (L.obj != ObjType::Destroy || level.bossPhase) return;
  bool match = (L.unit == "any" && t != U_ARMOR) || (L.unit == "tank" && (t == U_ATANK || t == U_PTANK)) || (L.unit == "jet" && t == U_JET) ||
               (L.unit == "heli" && (t == U_AHELI || t == U_PHELI)) || (L.unit == "jeep" && t == U_JEEP) ||
               (L.unit == "cop" && (t == U_COP || t == U_CAR || t == U_RIOT)) || (L.unit == "soldier" && (t == U_SOLDIER || t == U_FLAME));
  if (match) { level.destroyed++; toast(L.name + ": " + std::to_string(level.destroyed) + "/" + std::to_string(L.count)); }
}

// ---------------------------------------------------------------- unlocks
void Game::unlockMove(int idx, bool fromChest) {
  Profile& p = P(); const MoveDef& m = T.moves[idx];
  if (p.moves.count(m.id)) return;
  if (m.chest && !fromChest) {  // the skill arrives in a LOCKED chest near the monster
    if (std::find(p.chests.begin(), p.chests.end(), m.id) != p.chests.end()) return;
    p.chests.push_back(m.id); spawnChest(m.id);
    bigMessage("SKILL CHEST!", m.name + " chest appeared!"); play(SND_unlock);
    toast("A locked chest with " + m.name + " appeared nearby! Open it with a key (" + btn("Y", "E") + ")" + (p.keys > 0 ? "" : " - keys cost " + std::to_string((int)C.keyCost) + " coins in the store (" + btn("D-pad up", "B") + ")"));
    persist(); return;
  }
  p.moves.insert(m.id);
  const char* kind = m.kind == MoveKind::Melee ? "NEW MELEE MOVE!" : m.kind == MoveKind::Direct ? "NEW POWER!" : "NEW SUPER POWER!";
  if (m.id == "roar") { bigMessage("UNLOCKED!", "Charged Roar unlocked!"); play(SND_fanfare); toast("Charged Roar unlocked! Hold its slot button to charge, release to ROAR"); }
  else { bigMessage(kind, m.name); play(SND_unlock); toast("Unlocked: " + m.name + " - " + m.desc); }
  persist();
  if (m.kind == MoveKind::Melee) {
    for (int i = 0; i < MELEE_BINDS; i++) if (p.melee[i].empty()) {
      p.melee[i] = m.id;
      static const char* names[] = {"X / J", "B / K", "A / L", "LB+X / U", "LB+B / I", "LB+A / O"};
      toast(m.name + " set on " + names[i] + " (change it in Melee loadout)"); break;
    }
  } else offerEquip(m.id);
}
void Game::checkUnlocks() { for (int i : prog::pendingUnlocks(P(), T)) unlockMove(i); }
void Game::offerEquip(const std::string& id) {
  Profile& p = P(); int mi = T.moveIndex(id); if (mi < 0) return;
  if (p.slots[0] == id || p.slots[1] == id) return;
  int fs = T.moves[mi].slot - 1;  // each skill has one slot type: 1 close range, 2 ranged / super
  if (fs < 0 || fs >= p.slotCount) return;
  if (p.slots[fs].empty()) {
    prog::equip(p, id, fs); toast(T.moves[mi].name + " equipped in slot " + std::to_string(fs + 1));
    save.tutorial.pending.push_back({id, fs}); persist(); return;
  }
  // slot already filled: the new skill joins the collection (auto-equip only into an empty slot)
  toast(T.moves[mi].name + " added to your skills! Swap it into slot " + std::to_string(fs + 1) + " with " + (fs == 0 ? btn("D-pad left", "1") : btn("D-pad right", "2")) + " or the skills list (" + btn("D-pad down", "Tab") + ")");
}
void Game::checkAdventureGate() {
  if (save.adventureUnlocked || adventure) return;
  if (prog::adventureReady(save)) {
    save.adventureUnlocked = true; bigMessage("UNLOCKED!", "Adventure Mode"); play(SND_fanfare);
    toast("Adventure mode unlocked! Pick it from the main menu."); persist();
    if (mode == Mode::Play && !adventure) {  // a clear (one-press) choice; both options save first
      MenuItem keep; keep.label = "Keep playing Sandbox to unlock more powers"; keep.act = [this]() { persist(); popMenu(); };
      MenuItem go; go.label = "Go to Main Menu and start Adventure"; go.act = [this]() { persist(); stopLevel(); menus.clear(); mode = Mode::Title; openMainMenu(); if (!menus.empty()) menus.back().index = 1; };
      prompt("Adventure Mode unlocked!", "You can keep smashing in Sandbox, or head to the main menu and start the 15-level Adventure. Your progress is saved either way.", {keep, go});
    }
  }
}
void Game::tutorialUsed(const std::string& id) {
  TutorialState& t = save.tutorial;
  if (!t.done && !adventure && t.step < TUT_N && id == TUT_IDS[t.step]) { t.step++; play(SND_ui); persist(); }
  for (size_t i = 0; i < t.pending.size(); i++) if (t.pending[i].first == id) { t.pending.erase(t.pending.begin() + i); break; }
}
std::string Game::tutorialText(bool& isTut) {
  TutorialState& t = save.tutorial; isTut = false;
  if (!adventure && !t.done && t.step < TUT_N - 1) {
    isTut = true;
    switch (t.step) {
      case 0: return usingPad ? "Move with the L stick, look around with the R stick" : "Move with W A S D. Click the game to capture the mouse, then move it to look (Esc releases)";
      case 1: return usingPad ? "Claw Swipe: press X" : "Claw Swipe: press J or left-click";
      case 2: return usingPad ? "Tail Swipe: press B (flings rocks and trees)" : "Tail Swipe: press K or right-click (flings rocks and trees)";
      case 3: return usingPad ? "Stomp: press A" : "Stomp: press Space or L";
      case 4: return std::string("Power slot 1 - Glow Breath: tap ") + (usingPad ? "RT" : "F") + " as fast as you like!";
      case 5: return usingPad ? "Walk to a tree and press Y to pick it up, Y again to throw" : "Walk to a tree and press G to pick it up, G again to throw";
    }
  }
  if (tutorialActive() && t.step >= TUT_N - 1 && !tutorialGoalsMet()) {
    isTut = true;
    return std::string("Last challenge! Turn people into rainbows (stomp, smash, Rainbow Blaster ") + btn("LT", "Q") + ", gulp with " + btn("Y", "E") + ") and smash buildings!";
  }
  if (!t.pending.empty()) {
    int mi = T.moveIndex(t.pending[0].first);
    if (mi >= 0) return T.moves[mi].name + " is in slot " + std::to_string(t.pending[0].second + 1) + ": press " + (t.pending[0].second == 0 ? btn("RT", "F") : btn("LT", "Q")) + (T.moves[mi].chargeTime > 0 ? " (hold to charge, release to roar)" : "");
  }
  return "";
}

// ---------------------------------------------------------------- modes and levels
void Game::startMode(bool adv, bool crazyMode) {
  stopLevel(); persist();
  skyDark = skyDarkTarget = 0;  // never carry the L20 storm lighting (left mid-fight) into Sandbox / another run
  difficulty = save.settings.difficulty; players = save.settings.players;  // Titan: saved difficulty / co-op
  adventure = adv; crazy = adv && crazyMode; enemies.reset();
  if (world.procedural != adv) { world.procedural = adv; bodies.list.clear(); world.init(); people.init(); }  // Atlas: adventure = procedural city, sandbox = Victoria
  world.restoreAll(); egg = Egg(); level = LevelRun(); chests.clear(); tutFinaleT = -1;
  debris->clear(); fx->clear();
  kaiju.projectiles.clear();
  int mi = T.monsterIndex(save.monster); kaiju.monster = mi < 0 ? 0 : mi;
  menus.clear(); pendingOffers.clear(); mode = Mode::Play;
  if (adv) {
    Profile& p = AP();
    if (p.campaign.empty() || p.level >= (int)T.levels.size()) {
      p.campaignSeed = (uint64_t)std::chrono::system_clock::now().time_since_epoch().count();
      std::vector<std::string> order = prog::campaignOrder(p.campaignSeed, T, (int)T.levels.size());
      std::stable_partition(order.begin(), order.end(), [this](const std::string& id) { return !save.monsters.count(id); });  // new monsters first
      p.campaign = order; p.level = 0; p.levelsWon = 0;
      p.lockedMonster.clear();                    // new playthrough: pick a new level-6 champion
      p.stones.clear(); p.gauntlet = p.gauntletOn = false; p.gauntletSlot = false; p.gauntletCracked = false;  // the gauntlet arc starts over
    }
    grantBonusSuper();
    if (zooEligible() && p.level == 0 && p.levelsWon == 0) startZoo(true);
    else startLevel(p.level);
  } else {
    kaiju.respawn(-55, 135, PI);
    grantBonusSuper();
    if (zooEligible()) startZoo(false);
    else toast("Welcome to Victoria, " + kaiju.def().name + "! Smash buildings to earn coins");
    setupGates();
  }
  respawnPendingChests(); checkUnlocks();
  persist();
}
void Game::startLevel(int idx) {
  if (idx < 0 || idx >= (int)T.levels.size()) return;
  stopLevel();
  const LevelDef& L = T.levels[idx];
  enemies.reset(); world.restoreAll(); egg = Egg(); debris->clear();
  kaiju.respawn(L.startX, L.startZ, PI);
  chests.clear(); respawnPendingChests();
  level = LevelRun(); level.active = true; level.idx = idx; level.t = L.obj == ObjType::Survive ? L.time : L.time;
  enemies.level = &T.levels[idx]; enemies.milSpawnT = 2;
  AP().level = idx;
 skyDark = skyDarkTarget = 0; snapT = -1;
  if (L.obj == ObjType::Boss && !L.pedestals) startBoss(L);
  setupArcLevel(L); setupGates();
  bigMessage("LEVEL " + std::to_string(idx + 1), L.name); toast("Goal: " + L.desc);
  if (L.chestDrop) {  // level-5 capstone: every chest skill not yet owned drops from the sky - free to open, no key needed
    int n = 0;
    for (auto& m : T.moves) if (m.chest && !AP().moves.count(m.id)) { spawnChest(m.id, true); n++; }
    if (n > 0) { bigMessage("SUPPLY DROP!", std::to_string(n) + (n == 1 ? " skill chest" : " skill chests") + " - free to open!"); toast("Skill chests fell nearby - walk into them, no key needed!"); }
  }
  mode = Mode::Play;
  if (adventure && !crazy && idx >= (int)C.kaijuPickLevel - 1) {  // level 6+: pick a champion once, then it is locked in
    Profile& p = AP(); int lm = T.monsterIndex(p.lockedMonster);
    if (lm < 0) { p.lockedMonster.clear(); openKaijuPick(); }
    else if (kaiju.monster != lm) { save.monster = p.lockedMonster; kaiju.monster = lm; kaiju.hp = kaiju.maxHp(); }
  }
  persist();
}
void Game::stopLevel() {
  world.noRebuild = false; gates.clear(); snapT = -1;
  level.active = false; enemies.level = nullptr; boss = Boss();
  enemies.units.erase(std::remove_if(enemies.units.begin(), enemies.units.end(), [](const Unit& u) { return !u.wreck; }), enemies.units.end());
  enemies.shots.clear();
}
void Game::updateLevel(float dt) {
  if (!level.active) return;
  const LevelDef& L = T.levels[level.idx];
  level.elapsed += dt;
  bool win = false, fail = false;
  switch (L.obj) {
    case ObjType::Smash: win = level.count >= L.count; break;
    case ObjType::Survive: {
      bool inZone = !L.hasZone || dist2D(kaiju.pos.x, kaiju.pos.z, L.zx, L.zz) < L.zr;
      if (inZone) level.t -= dt;
      win = level.t <= 0; break;
    }
    case ObjType::Reach: win = dist2D(kaiju.pos.x, kaiju.pos.z, L.x, L.z) < L.r; break;
    case ObjType::Destroy: win = level.destroyed >= L.count; break;
    case ObjType::Landmarks: {
      bool all = true;
      for (auto& id : L.landmarks) { Building* b = world.landmark(id); if (!b || !b->landmarkSmashed) all = false; }
      win = all && level.count >= L.count; break;
    }
    case ObjType::Boss: win = boss.dead && boss.deadT > 1.5f && snapT < 0; break;
    case ObjType::Box: win = level.boxOpen; break;
    case ObjType::Stone: win = level.stoneTaken; break;
  }
  if (devSkipGoal && L.obj != ObjType::Boss && L.obj != ObjType::Box && L.obj != ObjType::Stone && level.elapsed > 3) win = true;
  if (!L.miniBoss.empty()) {  // levels 11-14: finishing the goal summons a boss; knocking it out wins the level
    if (level.bossPhase) win = level.bossDown;
    else if (win) {
      win = false; level.bossPhase = true;
      std::string bossId = prog::bossAfter(L.miniBoss, AP().lastBoss); AP().lastBoss = bossId;
      enemies.addUnit(U_MBOSS); Unit& u = enemies.units.back(); u.boss = bossIdFromName(bossId); u.pos.y = bossSpawnHeight(u.boss); enemies.applyTier(u, L.tier);
      std::string nm = bossDisplayName(u.boss);
      bigMessage("BOSS INCOMING!", nm); play(SND_roarCharge, 0.8f, 0.6f); play(SND_cannon, 1.f, 0.5f);
      toast("Goal done! Now knock out the " + nm + " (watch its health bar)");
    }
  }
  if (L.obj != ObjType::Survive && L.time > 0 && !level.bossPhase) { level.t -= dt; if (level.t <= 0 && !win) fail = true; }
  if (win) {
    level.active = false; level.won = true; enemies.level = nullptr;
    for (auto& u : enemies.units) if (!u.wreck) { u.wreck = true; u.t = 0; }
    Profile& p = AP(); p.levelsWon = std::max(p.levelsWon, level.idx + 1);
    awardCoins(L.reward, kaiju.pos);
    { Vec3 at = kaiju.pos + Vec3(0, 14, 0); addXp(E.xpFor("level"), &at); }
    if (L.unlockGauntletSlot) p.gauntletSlot = true;  // silent: an empty outline appears on the Skills screen, nothing else
    p.spins += E.spinsPerLevel;
    bigMessage("LEVEL COMPLETE!", L.name); play(SND_fanfare);
    if (L.obj == ObjType::Boss) { p.level = std::min((int)T.levels.size(), level.idx + 1); persist(); bossVictory(); return; }
    toast("The army retreats! A mystery egg appeared - hatch it with a glow move! (+" + std::to_string(E.spinsPerLevel) + " egg spin)");
    spawnEgg(); persist();
  } else if (fail) {
    level.active = false; level.failed = true; enemies.level = nullptr;
    bigMessage("TIME UP!", L.name); play(SND_error);
    MenuItem retry; retry.label = "Retry level"; retry.act = [this]() { popMenu(); startLevel(level.idx); };
    MenuItem fp; fp.label = "Free play (retry later from the pause menu)"; fp.act = [this]() { popMenu(); };
    prompt("Out of time!", "Free play continues - coins you earn are kept.", {retry, fp});
  }
}
void Game::spawnEgg() {
  egg = Egg(); egg.active = true; egg.state = 0;
  const Profile& p = AP();
  if (level.idx < (int)p.campaign.size()) egg.monster = p.campaign[level.idx];
  else { Rng r((uint64_t)std::chrono::steady_clock::now().time_since_epoch().count()); egg.monster = eggs::rollSpecies(r, T); }  // past the shuffled roster: a random egg
  Vec3 f = kaiju.fwd(); egg.pos = kaiju.pos + f * 22 * kaiju.scale();
  for (int t = 0; t < 12 && world.blockedForPerson(egg.pos.x, egg.pos.z); t++) { float a = t * 0.8f; egg.pos = kaiju.pos + Vec3(std::sin(a), 0, std::cos(a)) * 24; }
  egg.pos.y = 0;
  for (int k = 0; k < 30; k++) fx->particle(egg.pos + Vec3(0, 4, 0), Vec3((frand() - 0.5f) * 16, frand() * 14, (frand() - 0.5f) * 16), 1.0f, 0.8f, hexColor(0xffe03a));
}
void Game::hitEgg(int move, float x, float z, float r) {
  if (!egg.active || egg.state != 0 || move < 0) return;
  if (dist2D(x, z, egg.pos.x, egg.pos.z) > r + 5) return;
  if (!T.moves[move].glow) return;
  if (egg.hitCd > 0) return;
  egg.hitCd = 0.25f; egg.hits++; egg.wobble = 1; play(SND_eggCrack, 0.9f, 0.9f + egg.hits * 0.05f);
  tutorialUsed("egg");
  if (egg.hits >= (int)C.eggGlowHits) { egg.state = 1; egg.t = 0; mode = Mode::Reveal; revealT = 0; play(SND_hatch); }
}
void Game::updateEgg(float dt) {
  if (!egg.active) return;
  egg.hitCd = std::max(0.f, egg.hitCd - dt); egg.wobble = std::max(0.f, egg.wobble - dt * 2);
  if (egg.state == 0) {
    if (frand() < dt * 6) fx->particle(egg.pos + Vec3((frand() - 0.5f) * 6, 2 + frand() * 8, (frand() - 0.5f) * 6), Vec3(0, 3, 0), 0.8f, 0.4f, hexColor(0xffe03a));
    return;
  }
  egg.t += dt; revealT = egg.t;
  int mi = std::max(0, T.monsterIndex(egg.monster));
  const MonsterDef& m = T.monsters[mi];
  if (egg.state == 1 && egg.t > 0.8f) {  // shell bursts
    egg.state = 2;
    for (int k = 0; k < 40; k++) debris->spawn(egg.pos + Vec3((frand() - 0.5f) * 5, 2 + frand() * 8, (frand() - 0.5f) * 5), Vec3((frand() - 0.5f) * 20, 8 + frand() * 14, (frand() - 0.5f) * 20), k % 2 ? hexColor(0xfff3d0) : m.accent, 0.6f + frand() * 0.8f);
    fx->shell(egg.pos + Vec3(0, 6, 0), 20, 1.0f, m.accent); fx->ring(egg.pos.x, egg.pos.z, 26, 1.2f, m.accent);
    play(SND_eggCrack); play(SND_collapse, 0.6f, 1.4f);
  }
  if (egg.state == 2 && egg.t > 0.8f + 3.0f) {
    egg.state = 3; bigMessage("NEW MONSTER!", m.name); play(SND_fanfare); play(SND_roar, 0.8f, 1.2f);
  }
  if (egg.state == 3 && egg.t > C.revealTime + 2.2f && egg.spin) {  // hatched from the egg spinner
    int why = egg.spin; egg.active = false; mode = Mode::Play;
    if (why == SPIN_CREATE) { menus.clear(); mode = Mode::Title; openMainMenu(); toast(spinNote); persist(); return; }
    grantBonusSuper(); persist();
    MenuItem ok; ok.label = spinDup ? "Cool!" : (why == SPIN_DEEVO ? "Let's grow!" : "Play as " + m.name + " now");
    ok.act = [this, mi, why]() { popMenu(); if (why == SPIN_TOKEN && !spinDup) selectMonster(mi); };
    MenuItem keep; keep.label = "Keep playing as " + kaiju.def().name; keep.act = [this]() { popMenu(); };
    std::vector<MenuItem> items = {ok}; if (why == SPIN_TOKEN && !spinDup && !kaijuLocked() && m.id != kaiju.def().id) items.push_back(keep);
    prompt(spinDup ? "Duplicate: " + m.name : "Hatched: " + m.name, spinNote, items);
    return;
  }
  if (egg.state == 3 && egg.t > C.revealTime + 2.2f) {
    egg.active = false; mode = Mode::Play;
    bool had = save.monsters.count(m.id) > 0;
    if (!had) {  // campaign hatch: roll its colour look; it starts as a hatchling in every mode
      Rng r((uint64_t)std::chrono::steady_clock::now().time_since_epoch().count());
      save.looks[m.id] = eggs::rollLook(r, m.id, T);
      for (Profile* pp : {&save.sandbox, &save.adventure, &save.crazy}) if (!pp->xp.count(m.id)) pp->xp[m.id] = 0;
    }
    save.monsters.insert(m.id);
    Profile& p = AP(); p.level = std::min((int)T.levels.size(), level.idx + 1);
    persist();
    MenuItem playIt; playIt.label = "Play as " + m.name + " now"; playIt.act = [this, mi]() { popMenu(); selectMonster(mi); };
    MenuItem keep; keep.label = "Keep playing as " + kaiju.def().name; keep.act = [this]() { popMenu(); };
    if (kaijuLocked()) { playIt.label = "Cool! (" + kaiju.def().name + " is your champion this campaign)"; playIt.act = [this]() { popMenu(); }; }
    bool last = level.idx + 1 >= (int)T.levels.size();
    std::string body = (had ? m.name + " was already in your roster - a shiny duplicate!\n" : m.name + " joined your roster! (" + m.desc + ")\n") +
                       (last ? "That was the final level - CAMPAIGN COMPLETE! Start a new shuffled campaign from the main menu." : "Free play continues. Open the pause menu and choose Next Level when you are ready.");
    if (last) bigMessage("CAMPAIGN COMPLETE!", "You are a legend");
    if (kaijuLocked()) prompt(had ? "Hatched: " + m.name : "New playable monster: " + m.name, body, {playIt});
    else prompt(had ? "Hatched: " + m.name : "New playable monster: " + m.name, body, {playIt, keep});
  }
}
void Game::selectMonster(int idx) {
  if (idx < 0 || idx >= (int)T.monsters.size() || (!crazy && !save.monsters.count(T.monsters[idx].id))) return;  // Crazy: any hero
  if (kaijuLocked() && T.monsters[idx].id != AP().lockedMonster) {
    toast(T.monsters[AP().lockedMonster.empty() ? 0 : std::max(0, T.monsterIndex(AP().lockedMonster))].name + " is your locked-in champion for this campaign!"); play(SND_error); return;
  }
  save.monster = T.monsters[idx].id;
  float hpK = kaiju.hp / std::max(1.f, kaiju.maxHp());
  kaiju.monster = idx; kaiju.hp = kaiju.maxHp() * hpK; kaiju.camScale = -1;
  toast("Now playing as " + T.monsters[idx].name + "!"); play(SND_unlock); grantBonusSuper(); persist();
}
void Game::gameOver(const std::string& reason) {
  if (mode == Mode::GameOver) return;
  mode = Mode::GameOver; gameOverReason = reason; flashT = 1.2f;
  bool inLevel = level.active; int idx = level.idx;
  if (reason == "nuke") { play(SND_boom); fx->shell(kaiju.pos + Vec3(0, 10, 0), 120, 2.5f, hexColor(0xffffff)); } else play(SND_collapse);
  if (inLevel) { level.active = false; level.failed = true; enemies.level = nullptr; }
  std::vector<MenuItem> items;
  if (adventure && inLevel && boss.staged && boss.stage >= 2) { MenuItem r2; r2.label = "Retry from stage 2 (THE BULK)"; r2.act = [this]() { popMenu(); restartBulkStage2(); }; items.push_back(r2); }
  if (adventure && inLevel) { MenuItem r; r.label = "Retry level"; r.act = [this, idx]() { popMenu(); startLevel(idx); }; items.push_back(r); }
  MenuItem r; r.label = "Restart here (coins and upgrades are kept)"; r.act = [this]() { popMenu(); restartAfterGameOver(); }; items.push_back(r);
  MenuItem mm; mm.label = "Main menu"; mm.act = [this]() { menus.clear(); mode = Mode::Title; openMainMenu(); }; items.push_back(mm);
  std::string name = kaiju.def().name;
  prompt(reason == "nuke" ? "KA-BOOM! (cartoon style)" : name + " needs a nap!",
         reason == "nuke" ? "The nuke went off. " + name + " is covered in soot but totally fine!" : "Too many hits from the vehicles. Your coins and upgrades are kept.", items);
  persist();
}
void Game::restartAfterGameOver() {
  enemies.reset(); mode = Mode::Play;
  if (adventure) kaiju.respawn(kaiju.pos.x, kaiju.pos.z, kaiju.facing); else kaiju.respawn(-55, 135, PI);
}


// ---------------------------------------------------------------- secret glove
float Game::levelSmashFraction() const {
  int total = 0, smashed = 0;
  for (auto& b : world.buildings) {
    if (!b.generated || b.school || !world.chunks[b.chunk].active) continue;
    total++; smashed += b.smashed ? 1 : 0;
  }
  return total ? (float)smashed / total : 0.f;
}
bool Game::gloveEquipped() const { return gauntletReady(); }  // THE BULK'S GAUNTLET in the gauntlet slot
void Game::updateGlove(float dt) {
  glovePulseT = std::max(0.f, glovePulseT - dt);
  if (gloveDrop.active) {
    gloveDrop.t += dt;
    if (frand() < dt * 12) fx->particle(gloveDrop.pos + Vec3((frand() - 0.5f) * 4, 1 + frand() * 3, (frand() - 0.5f) * 4), Vec3(0, 3, 0), 0.8f, 0.4f, hexColor(k_gloveStones[(int)(frand() * 6) % 6]));
    if (dist2D(kaiju.pos.x, kaiju.pos.z, gloveDrop.pos.x, gloveDrop.pos.z) < C.glovePickupRange + kaiju.radius()) collectGlove();
    return;
  }
  return;  // the old random-level glove drop is gone: the gauntlet comes from the L13 box (see arc.cpp)
  Profile& p = AP();
  if (p.gloveDropped || p.moves.count("glove") || level.idx != p.gloveLevel) return;
  if (levelSmashFraction() < C.gloveDropFraction) return;
  Vec3 at = kaiju.pos + kaiju.fwd() * (kaiju.radius() + C.glovePickupRange + 6); at.y = 0;  // lands just out of reach, so the player sees it
  world.resolveCircle(at, 2);
  gloveDrop = GloveDrop(); gloveDrop.active = true; gloveDrop.pos = at;
  p.gloveDropped = true;  // one drop per playthrough
  fx->rainbowBurst(at + Vec3(0, 3, 0), 2); play(SND_unlock, 0.9f, 1.3f);
  toast("Something shiny tumbled out of the rubble...");
  persist();
}
void Game::collectGlove() {
  gloveDrop.active = false;
  Profile& p = P();
  if (p.moves.count("glove")) return;
  p.moves.insert("glove"); p.gloveDropped = true;
  if (p.slotCount >= 2) p.slots[1] = "glove";  // goes straight into SLOT 2 (ranged / super); the old skill stays in the collection
  bigMessage("SECRET FOUND!", "THE BULK'S GLOVE"); play(SND_fanfare); flashT = 0.3f;
  fx->rainbowBurst(kaiju.pos + Vec3(0, 8, 0), 3);
  toast("Equipped in SLOT 2 - keep moving to flatten the city!");
  persist();
}
int Game::glovePulse() {
  std::vector<std::pair<float, Building*>> c;
  for (auto& b : world.buildings) {
    if (!b.generated || b.smashed || b.rebuilding || b.school || !world.chunks[b.chunk].active) continue;
    c.push_back({dist2D(b.x, b.z, kaiju.pos.x, kaiju.pos.z), &b});
  }
  int targets = E.pulseTargets[(size_t)std::max(0, std::min(6, stoneCount()))];  // power grows with the stones: 0 stones = cosmetic
  StonePower pulse; bool pulseOn = stonePowerOn(0, pulse);
  if (pulseOn) targets += std::max(0, pulse.extra);
  int n = std::min((int)c.size(), std::max(0, targets));
  if (n <= 0 && !(pulseOn && (pulse.people || pulse.zap))) return 0;
  std::partial_sort(c.begin(), c.begin() + n, c.end(), [](const std::pair<float, Building*>& a, const std::pair<float, Building*>& b) { return a.first < b.first; });
  float far = 10;
  for (int i = 0; i < n; i++) {
    Building& b = *c[i].second; far = std::max(far, c[i].first);
    float h = b.maxY;
    world.hitBuilding(b, b.x, h * 0.5f, b.z, 1000, 100000, kaiju.pos.x, kaiju.pos.z, 10);  // counts like any smash (coins, goals)
    if (b.smashed && !b.collapsed) world.collapse(b);
  }
  static const uint32_t bands[6] = {0xff3b3b, 0xff9a2a, 0xffe03a, 0x4bd66b, 0x3a8dff, 0xb06bff};
  for (int k = 0; k < 6; k++) {
    Color col = hexColor(bands[k]);
    if (pulseOn) { const StoneDef* st = nullptr; for (auto& s : E.stones) if (s.id == P().stoneBind[0].substr(0, P().stoneBind[0].find(':'))) st = &s; if (st) col = mixColor(st->color, col, 0.35f); }
    fx->ring(kaiju.pos.x, kaiju.pos.z, far * (0.55f + 0.09f * k), 0.7f + 0.06f * k, col);
  }
  if (pulseOn && pulse.people) people.pop(kaiju.pos.x, kaiju.pos.z, 16);
  if (pulseOn && pulse.zap) enemies.hitArea(kaiju.pos.x, kaiju.pos.z, 18, -1, false);
  fx->rainbowBurst(kaiju.pos + Vec3(0, 6, 0), 2);
  kaiju.cam.shake = std::min(1.2f, kaiju.cam.shake + 0.5f); vibrate(0.7f, 0.25f); play(SND_boom, 0.6f, 1.2f);
  glovePulseT = 0.5f; glovePulses++;
  return n;
}
void Game::drawGlove(RenderList& rl, const Mat4& root, float t) {
  // ORIGINAL design: chunky purple fist with a gold cuff and knuckle plates; six stones sit in a hexagon ring
  // around a gold stud on the back of the hand, each pulsing on its own beat.
  const Color purple = hexColor(0x7a3cc8), deep = hexColor(0x4a2080), gold = hexColor(0xffc23a);
  auto box = [&](float sx, float sy, float sz, float x, float y, float z, Color c, float glow = 0) { rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(sx, sy, sz) * Mat4::translation(x, y, z) * root, c, glow > 0 ? 0.3f : 0, glow)); };
  box(2.6f, 1.4f, 2.4f, 0, -1.6f, 0, gold, 0.2f);          // cuff
  box(2.9f, 0.35f, 2.7f, 0, -0.85f, 0, deep);              // cuff rim
  box(2.8f, 2.2f, 2.2f, 0, 0.5f, 0, purple);               // back of the hand
  for (int f = 0; f < 4; f++) {                            // fingers curled into a fist + gold knuckle plates
    float x = -1.05f + f * 0.7f;
    box(0.62f, 0.9f, 1.2f, x, 1.95f, 0.25f, purple);
    box(0.66f, 0.3f, 0.5f, x, 2.45f, -0.25f, gold, 0.2f);
  }
  box(0.7f, 1.3f, 0.8f, 1.6f, 0.5f, 0.6f, purple);         // thumb
  box(0.7f, 0.7f, 0.25f, 0, 0.5f, -1.15f, gold, 0.3f);     // central gold stud
  for (int k = 0; k < 6; k++) {
    float a = k * (PI / 3), x = std::cos(a) * 0.95f, y = 0.5f + std::sin(a) * 0.8f;
    float pulse = 0.7f + 0.3f * std::sin(t * 4 + k * 1.1f);
    Color c = hexColor(k_gloveStones[k]);
    rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(0.42f, 0.42f, 0.42f) * Mat4::translation(x, y, -1.15f) * root, c, 1, pulse));
    Color g = c; g.a = 0.25f * pulse;
    rl.alpha[MESH_SPHERE].push_back(makeInstance(Mat4::scale(0.8f, 0.8f, 0.8f) * Mat4::translation(x, y, -1.2f) * root, g, 1));
  }
}

// ---------------------------------------------------------------- skill chests, eating, store keys, aiming
static std::string fmt1(float v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); return b; }
bool Game::kaijuLocked() const { return adventure && !crazy && !(crazy ? save.crazy : save.adventure).lockedMonster.empty(); }
void Game::openKaijuPick() {
  Menu m; m.title = "PICK YOUR CHAMPION"; m.kind = 3; m.noBack = true;
  m.index = std::max(0, T.monsterIndex(save.monster));
  m.build = [this](Menu& mm) {
    mm.body = "Level " + std::to_string((int)C.kaijuPickLevel) + "! Choose your favourite kaiju. It stays with you for the rest of this campaign.";
    for (int i = 0; i < (int)T.monsters.size(); i++) {
      const MonsterDef& md = T.monsters[i]; bool has = save.monsters.count(md.id) > 0;
      MenuItem it; it.label = has ? md.name : "???"; it.right = md.id == save.monster ? "CURRENT" : (has ? "" : "locked");
      it.desc = has ? md.desc + "\n" + "speed x" + fmt1(md.speed) + "  HP x" + fmt1(md.hp) + "  melee x" + fmt1(md.melee) + "  power x" + fmt1(md.power)
                    : std::string("Not hatched yet.");
      it.enabled = has;
      it.act = [this, i]() {
        AP().lockedMonster.clear(); selectMonster(i);
        AP().lockedMonster = T.monsters[i].id; kaiju.hp = kaiju.maxHp();
        popMenu(); bigMessage("CHAMPION!", T.monsters[i].name + " is locked in"); play(SND_fanfare); persist();
      };
      mm.items.push_back(it);
    }
  };
  pushMenu(m);
}
void Game::spawnChest(const std::string& moveId, bool free) {
  for (auto& c : chests) if (c.active && c.move == moveId) { c.free = c.free || free; return; }
  Chest c; c.active = true; c.move = moveId; c.state = 0; c.free = free;
  Vec3 f = kaiju.fwd(); float d = C.chestDist * std::max(1.f, kaiju.scale());
  int n = 0; for (auto& o : chests) if (o.active) n++;
  if (n > 0) { float a = kaiju.facing + n * 0.95f; f = Vec3(std::sin(a), 0, std::cos(a)); }  // spread several chests in a ring
  if (free) c.drop = 1.2f;  // falls from the sky
  c.pos = kaiju.pos + f * d;
  for (int t = 0; t < 16 && world.blockedForPerson(c.pos.x, c.pos.z); t++) { float a = kaiju.facing + t * 0.8f; c.pos = kaiju.pos + Vec3(std::sin(a), 0, std::cos(a)) * (d + (t / 4) * 6); }
  c.pos.y = 0;
  chests.push_back(c);
  for (int k = 0; k < 24; k++) fx->particle(c.pos + Vec3(0, 3, 0), Vec3((frand() - 0.5f) * 12, frand() * 12, (frand() - 0.5f) * 12), 0.9f, 0.6f, hexColor(0xffd36b));
}
void Game::respawnPendingChests() { for (auto& id : P().chests) spawnChest(id); }
Chest* Game::chestNear(float x, float z, float r) {
  Chest* best = nullptr; float bd = r;
  for (auto& c : chests) if (c.active && c.state == 0) { float d = dist2D(x, z, c.pos.x, c.pos.z); if (d < bd) { bd = d; best = &c; } }
  return best;
}
bool Game::tryOpenChest(Chest& c) {
  if (!c.active || c.state != 0) return false;
  Profile& p = P();
  if (!c.free && p.keys <= 0) {
    if (chestHintT <= 0) { toast("Locked - buy a key at the store, " + std::to_string((int)C.keyCost) + " coins (" + btn("D-pad up", "B") + ")"); play(SND_error, 0.4f); chestHintT = 2.5f; }
    return false;
  }
  if (!c.free) p.keys--;
  c.state = 1; c.t = 0;
  p.chests.erase(std::remove(p.chests.begin(), p.chests.end(), c.move), p.chests.end());
  fx->shell(c.pos + Vec3(0, 3, 0), 10, 0.8f, hexColor(0xffd36b)); fx->rainbowBurst(c.pos + Vec3(0, 4, 0), 2.5f);
  play(SND_hatch, 0.8f);
  int mi = T.moveIndex(c.move);
  if (mi >= 0) unlockMove(mi, true);
  { Vec3 at = c.pos + Vec3(0, 6, 0); addXp(E.xpFor("chest"), &at); }
  if (frand() < E.chestSpinChance) { p.spins++; floats.push_back({"+1 EGG SPIN", c.pos + Vec3(0, 9, 0), 1.6f, hexColor(0xffd36b)}); }
  persist();
  return true;
}
void Game::updateChests(float dt) {
  float reach = kaiju.radius() + 3;
  for (auto& c : chests) {
    if (!c.active) continue;
    c.t += dt; c.drop = std::max(0.f, c.drop - dt);
    if (c.state == 0) {
      if (frand() < dt * 5) fx->particle(c.pos + Vec3((frand() - 0.5f) * 5, 2 + frand() * 4, (frand() - 0.5f) * 5), Vec3(0, 3, 0), 0.7f, 0.35f, hexColor(0xffd36b));
      if (dist2D(kaiju.pos.x, kaiju.pos.z, c.pos.x, c.pos.z) < reach) tryOpenChest(c);  // walking into it
    } else if (c.t > 1.4f) c.active = false;
  }
  chests.erase(std::remove_if(chests.begin(), chests.end(), [](const Chest& c) { return !c.active; }), chests.end());
}
// ---------------------------------------------------------------- eat-to-fuel super attacks (Rampage style)
int Game::superEatCost(const MoveDef& m, float charge) const {
  if (m.kind != MoveKind::Super || m.id == "glove") return 0;
  int n = (int)T.levels.size(), idx = (adventure && level.active) ? level.idx : (adventure ? std::min(n - 1, std::max(0, (crazy ? save.crazy : save.adventure).level)) : 0);  // sandbox: level-1 cost
  float t = n > 1 ? clampf((float)idx / (float)(n - 1), 0, 1) : 0;
  float base = lerpf(C.eatCostStart, C.eatCostEnd, t);
  float k = m.chargeTime > 0 ? lerpf(C.eatCostMinCharge, C.eatCostFullCharge, clampf(charge, 0, 1)) : 1.f;
  int c = std::max(1, (int)std::lround(base * k * superCostMult()));
  return std::max(1, (int)std::ceil(c * vault.mult(kaiju.def().id, vpow::ST_THRIFT) - 1e-4f));  // Wren: Vault thrift power (round up, min 1)
}
bool Game::spendEat(const MoveDef& m, float charge) {
  int cost = superEatCost(m, charge);
  if (cost <= 0) return true;
  Profile& p = P();
  if (p.eaten < cost) {
    needEatN = cost; needEatT = 2.0f;
    toast("Need " + std::to_string(cost) + " eaten for " + m.name + " - gulp people with " + btn("Y", "E") + "!"); play(SND_error, 0.5f);
    return false;
  }
  p.eaten -= cost;
  return true;
}
bool Game::interact() {
  if (zoo.active) return false;  // the zoo opening handles E itself (only the zookeeper, only in range)
  if (adventure && tryPlaceStone()) return true;
  if (adventure && tryGulpBoxKeeper()) return true;
  Chest* c = chestNear(kaiju.pos.x, kaiju.pos.z, C.chestRange * std::max(1.f, kaiju.scale()) + kaiju.radius());
  if (c) { tryOpenChest(*c); return true; }
  return kaiju.eat();
}
bool Game::oneButtonEat(InputState& in) {
  if (!in.pressed[A_EAT]) return false;
  bool did = zoo.active ? zoo.inRange : interact();  // the zoo gulp itself runs in updateZoo
  if (did) { in.pressed[A_GRAB] = false; in.held[A_GRAB] = false; }  // one press, one action: never eat AND grab a tree
  return did;
}
int Game::buyKey() {
  int r = prog::buyKey(P());
  if (r == 0) { play(SND_coin); play(SND_unlock, 0.6f); toast("Bought a chest key! Keys: " + std::to_string(P().keys)); persist(); }
  else { play(SND_error); toast("A key costs " + std::to_string((int)C.keyCost) + " coins - smash buildings and make rainbows to earn more!"); }
  return r;
}
void Game::aimRay(Vec3& o, Vec3& d) const {
  // ray through the crosshair, which sits above the monster's head (AIM_SCREEN_Y) so shots are not hidden behind him
  o = kaiju.cam.pos; d = kaiju.cam.target - kaiju.cam.pos;
  float l = length(d); d = l > 1e-4f ? d * (1.f / l) : Vec3(0, 0, -1);
  Vec3 r = cross(d, Vec3(0, 1, 0)); float rl = length(r);
  if (rl > 1e-4f) {
    r = r * (1.f / rl); Vec3 up = cross(r, d);
    d = d + up * (std::tan(30.f * PI / 180.f) * (0.5f - AIM_SCREEN_Y) * 2);  // 60 degree vertical FOV (see render)
    d = d * (1.f / length(d));
  }
}
int Game::aimTarget(Vec3& point) {
  Vec3 o, d; aimRay(o, d);
  int bi = T.moveIndex("blaster");
  float range = (bi >= 0 ? T.moves[bi].reach : 170.f) * prog::rangeMult(P().rangeLvl);
  float tmin = length(kaiju.cam.target - kaiju.cam.pos) * 0.5f, tmax = tmin * 2 + range, tw = 0;
  float limit = world.rayHit(o, d, tmin, tmax, tw) ? tw : tmax;
  float tp = 0; int pi = people.pickRay(o, d, tmin, limit, C.blasterAimRadius, &tp);
  point = pi >= 0 ? People::center(people.list[pi]) : o + d * limit;
  return pi;
}

// ---------------------------------------------------------------- frame
void Game::frame(float dt, const InputState& in) {
  G = this;
  sounds.clear();
  usingPad = in.usingPad; mouseCaptured = in.mouseCaptured;
  fpsAcc += dt; fpsFrames++; if (fpsAcc > 0.5f) { fps = fpsFrames / fpsAcc; fpsAcc = 0; fpsFrames = 0; }
  dt = std::min(dt, 0.05f);
  realTime += dt;
  if (in.pressed[A_MUTE]) { save.settings.muted = !save.settings.muted; toast(save.settings.muted ? "Sound muted (M)" : "Sound on"); persist(); }
  for (auto& t : toasts) t.t -= dt;
  toasts.erase(std::remove_if(toasts.begin(), toasts.end(), [](const Toast& t) { return t.t <= 0; }), toasts.end());
  for (auto& f : floats) { f.t -= dt; f.pos.y += dt * 6; }
  floats.erase(std::remove_if(floats.begin(), floats.end(), [](const FloatText& f) { return f.t <= 0; }), floats.end());
  bigT = std::max(0.f, bigT - dt); flashT = std::max(0.f, flashT - dt); rumbleT = std::max(0.f, rumbleT - dt); if (rumbleT <= 0) rumble = 0;
  for (auto& v : loopVol) v *= 0.0f;
  if (mode == Mode::Title) titleOrbit += dt * 0.08f;

  tickPops(dt);
  if (mode == Mode::Intro) {
    updateIntro(dt, in);
  } else if (mode == Mode::Ending) {
    updateEnding(dt, in); world.update(dt); people.update(dt, kaiju.pos.x, kaiju.pos.z, 0, kaiju.radius()); fx->update(dt);
  } else if (mode == Mode::Spin && menus.empty()) {
    updateSpin(dt, in); fx->update(dt);
  } else if (!menus.empty()) {
    updateMenus(in, dt);
    if (mode == Mode::Title) { world.update(dt); people.update(dt, kaiju.pos.x, kaiju.pos.z, 0, kaiju.radius()); fx->update(dt); debris->update(dt); }
  } else if (mode == Mode::Title) {
    openMainMenu();
  } else if (mode == Mode::Play && !pendingOffers.empty()) {
    std::string id = pendingOffers.front(); pendingOffers.erase(pendingOffers.begin());
    offerEquip(id);
  } else if (mode == Mode::Play) {
    if (in.cursorX >= 0) { lastCurX = in.cursorX; lastCurY = in.cursorY; }  // Vault: HUD bar hover
    if (in.pressed[A_PAUSE] && deevoT < 0) openPause();
    else if (in.pressed[A_SKILLS] && !zoo.active && deevoT < 0) openSkills();
    else if (in.pressed[A_STORE]) openStore();
    else if (in.pressed[A_EGGS] || (in.p2Active && in.p2Pressed[A_EGGS])) beginSpin(SPIN_TOKEN);  // Vault/Wren: EGGS shortcut
    else if (in.click && hudBarClick(in.cursorX, in.cursorY)) {}  // Vault: bottom HUD buttons
    else {
      time += dt;
      chestHintT = std::max(0.f, chestHintT - dt); needEatT = std::max(0.f, needEatT - dt);
      for (int sI = 0; sI < 2; sI++) if (in.pressed[sI == 0 ? A_CYCLE1 : A_CYCLE2]) {  // instant quick-cycle, no pause
        if (prog::cycleSlot(P(), T, sI)) { int mi = T.moveIndex(P().slots[sI]); toast(T.moves[mi].name + " in slot " + std::to_string(sI + 1)); play(SND_ui); persist(); }
        else toast("No other skills for slot " + std::to_string(sI + 1) + " yet");
      }
      InputState kin = in; oneButtonEat(kin);
      if (in.p2Active && !splitScreen() && std::fabs(kin.lookX) + std::fabs(kin.lookY) == 0) { kin.lookX = in.p2LookX; kin.lookY = in.p2LookY; }  // co-op: the shared camera also follows the controller's right stick  // pad Y (= A_EAT + A_GRAB) eats / opens when something is in reach, else grabs
      if (in.pressed[A_PAY] && !enemies.nukeActive) { if (!doSnap()) sandboxUltimate(); }  // R / RB: Do the Snap (L20 final form) / Future-Sandbox rainbow wave
      if (deevoT >= 0) { InputState none; kaiju.update(dt, none); } else kaiju.update(dt, kin);
      updateCoop(dt, in);  // Titan co-op
      world.update(dt); bodies.update(dt); debris->update(dt);
      people.update(dt, kaiju.pos.x, kaiju.pos.z, kaiju.speedNow, kaiju.radius());
      enemies.update(dt, in);
      splats.update(dt); fx->update(dt);
      updateBoss(dt); updateGlove(dt); updateArc(dt); updateLevel(dt); updateEgg(dt); updateChests(dt);
      updateZoo(dt, in); updateGates(dt); updateDeevo(dt);
      world.updateStreaming(kaiju.pos.x, kaiju.pos.z, false, coopOn() && coopJoined ? &kaiju2.pos : nullptr);
      if (in.pressed[A_LEGEND]) openControls();
      // tutorial finale: the police yes/no prompt (sandbox only)
      if (!adventure && !save.tutorial.done && save.tutorial.step >= TUT_N - 1 && tutorialGoalsMet() && mode == Mode::Play) {
        save.tutorial.done = true; save.settings.policeAsked = true; tutFinaleT = 2.2f;  // banner first, then the police question
        Profile& p = P(); p.keys += (int)C.tutorialRewardKeys; p.coins += (int)C.tutorialRewardCoins; p.totalCoins += (int)C.tutorialRewardCoins;
        bigMessage("TUTORIAL COMPLETE!", "Great smashing!"); play(SND_tutorialDone); flashT = 0.4f;
        toast("Reward: " + std::to_string((int)C.tutorialRewardKeys) + " chest key + " + std::to_string((int)C.tutorialRewardCoins) + " coins!");
        persist();
      }
      if (tutFinaleT > 0) tutFinaleT = std::max(0.f, tutFinaleT - dt);
      if (tutFinaleT == 0 && menus.empty() && mode == Mode::Play) {
        tutFinaleT = -1;
        MenuItem yes; yes.label = "Yes - bring on the sirens!"; yes.act = [this]() { save.settings.police = true; toast("Police ON"); popMenu(); persist(); };
        MenuItem no; no.label = "No - peaceful smashing"; no.act = [this]() { save.settings.police = false; toast("Police OFF"); popMenu(); persist(); };
        prompt("Turn on the police system?", "Smashing makes police chase you: cars, a helicopter, tanks... even a nuke countdown you can pay 150 coins to cancel! Police are OFF unless you pick Yes. Change it any time: pause menu > Police.", {no, yes});  // "No" first: confirming the default keeps sandbox police OFF
        persist();
      }
    }
  } else if (mode == Mode::Reveal) {
    time += dt; updateEgg(dt); tickPops(0); fx->update(dt); debris->update(dt); world.update(dt); bodies.update(dt);
    if (mode == Mode::Reveal) { kaiju.cam.target = egg.pos + Vec3(0, 8, 0); }
  } else if (mode == Mode::GameOver) {
    fx->update(dt); debris->update(dt);
  }
  wantCapture = mode == Mode::Play && menus.empty();
  if (mode != Mode::Play || !menus.empty()) for (auto& v : loopVol) v = 0;
}

// ---------------------------------------------------------------- render
static void drawEgg(RenderList& rl, const Egg& e, float time) {
  int mi = std::max(0, T.monsterIndex(e.monster)); MonsterDef m = G->lookedDef(mi);
  if (e.hasLook) { m.body = e.look.primary; m.belly = e.look.secondary; m.accent = e.look.accent; }
  if (e.hasLook && e.state <= 1) m.accent = mixColor(m.accent, E.rarity(e.look.rarity).color, 0.5f);  // rarity glow on the shell
  if (e.state <= 1) {
    float shake = e.state == 1 ? std::sin(time * 60) * 0.15f : std::sin(time * 25) * 0.12f * e.wobble;
    Mat4 root = Mat4::rotZ(shake) * Mat4::translation(e.pos.x, 0, e.pos.z);
    float glow = 0.6f + 0.4f * std::sin(time * 4);
    rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(7, 9.5f, 7) * Mat4::translation(0, 4.6f, 0) * root, mixColor(hexColor(0xfff3d0), m.accent, 0.15f)));
    for (int i = 0; i < 7; i++) { float a = i * 2.4f, y = 2.5f + (i % 3) * 2.4f; rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(1.6f, 1.6f, 0.6f) * Mat4::rotY(a) * Mat4::translation(std::sin(a) * 3.3f, y, std::cos(a) * 3.3f) * root, i % 2 ? m.accent : m.body, 1)); }
    for (int c = 0; c < e.hits; c++) { float a = c * 1.1f; rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(0.25f, 2.6f, 0.25f) * Mat4::rotZ(0.5f * (c % 2 ? 1 : -1)) * Mat4::rotY(a) * Mat4::translation(std::sin(a) * 3.45f, 5 + (c % 3), std::cos(a) * 3.45f) * root, hexColor(0x2a1424), 1)); }
    Color halo = m.accent; halo.a = 0.18f * glow;
    rl.alpha[MESH_SPHERE].push_back(makeInstance(Mat4::scale(11, 13, 11) * Mat4::translation(e.pos.x, 5, e.pos.z), halo, 1));
  } else {
    float k = clampf((e.t - 0.8f) / 3.0f, 0, 1), s = m.scale * (0.12f + 0.88f * (1 - (1 - k) * (1 - k))) * (G->hasLook(m.id) || e.hasLook ? std::max(0.6f, E.growthStart * 1.4f) : 1.f);
    RigPose p; p.time = time; p.glow = 1.2f + 0.4f * std::sin(time * 8); p.jaw = e.state == 3 ? 0.7f : 0.2f; p.headX = e.state == 3 ? -0.3f : 0;
    p.walk = 1; p.walkPhase = time * 4; { float sw = std::sin(p.walkPhase) * 0.55f; p.armL = sw * 0.6f; p.armR = -sw * 0.6f; }
    ModelReq mr; mr.key = modelKey(MK_PREVIEW, 2); mr.clip = e.state == 3 ? MC_CHEER : MC_WALK; mr.glow = 0.2f + 0.2f * std::sin(time * 8);
    if (!G->monsterModel(rl, m, e.pos, time * 0.6f, s, mr)) { FinishMarks fm = finishMarks(rl); drawMonster(rl, m, e.pos, time * 0.6f, s, p); applyFinish(rl, fm, G->lookOf(m.id).finish, time); }
  }
}

void Game::render3D(RenderList& rl, int w, int h) {
  rl.clear(); rl.width = w; rl.height = h; rl.time = time; rl.vpX = 0; rl.vpW = 0;
  rl.skyZenith = hexColor(0x241446); rl.skyHorizon = hexColor(0xf0b48a);
  rl.sky = rl.skyHorizon; rl.fog = rl.skyHorizon; rl.exposure = 1.18f; rl.contrast = 1.08f;
  rl.fogNear = C.fogNear; rl.fogFar = C.fogFar;
  // lights are reset every frame (the platform reuses one RenderList, so the L20 darkening must not compound)
  { static const RenderList kDefaults; rl.sunColor = kDefaults.sunColor; rl.sunIntensity = kDefaults.sunIntensity; rl.hemiSky = kDefaults.hemiSky; rl.hemiGround = kDefaults.hemiGround; rl.hemiIntensity = kDefaults.hemiIntensity; }
  if (skyDark > 0.001f) {  // L20: the sky darkens as THE BULK arrives (and clears after the snap)
    float k = clampf(skyDark, 0, 1);
    // dark and stormy but never so dark an 8-year-old loses sight of THE BULK (fog pushed out, not in, as he grows)
    rl.skyZenith = mixColor(rl.skyZenith, hexColor(0x07010e), k); rl.skyHorizon = mixColor(rl.skyHorizon, hexColor(0x3a1860), k);
    rl.sky = rl.skyHorizon; rl.fog = rl.skyHorizon; rl.exposure = lerpf(rl.exposure, 0.82f, k);
    float far = boss.active ? 1.0f + 0.35f * clampf(boss.growth() - 1, 0, 1.5f) : 0.9f;
    rl.fogNear = lerpf(rl.fogNear, rl.fogNear * far, k); rl.fogFar = lerpf(rl.fogFar, rl.fogFar * far, k);
    rl.sunIntensity *= 1 - 0.4f * k; rl.hemiIntensity *= 1 - 0.25f * k; rl.sunColor = mixColor(rl.sunColor, hexColor(0xb070ff), 0.6f * k);
  }
  Vec3 eye, target;
  bool charSel = !menus.empty() && menus.back().kind == 3;
  float introRise = 0, introYaw = 0; Vec3 introPos(-40, 0, 40);
  if (mode == Mode::Intro) {  // slow push-in across the dusk skyline toward the rising monster
    float k = clampf((introT - INTRO_CARD) / (INTRO_REVEAL_END - INTRO_CARD + 1), 0, 1), e = k * k * (3 - 2 * k);
    target = introPos + Vec3(0, 12 + 4 * e, 0);
    float a = 0.9f + 0.45f * e, dist = 175 - 95 * e;
    eye = target + Vec3(std::sin(a) * dist, 34 - 22 * e, std::cos(a) * dist);
    float r = clampf((introT - 5.5f) / 4.0f, 0, 1); introRise = -30 * (1 - r * r * (3 - 2 * r));
    introYaw = std::atan2(eye.x - introPos.x, eye.z - introPos.z);
    if (introT > 5.5f && introT < 10) eye += Vec3(frand() - 0.5f, frand() - 0.5f, frand() - 0.5f) * 0.6f;  // ground rumble
  } else if (mode == Mode::Title && !charSel) {
    float a = titleOrbit; target = Vec3(-40, 12, 40);
    eye = target + Vec3(std::sin(a) * 120, 55, std::cos(a) * 120);
  } else if (charSel) {
    float a = titleOrbit * 3 + 0.6f, sc = T.monsters[previewMonster].scale;
    target = kaiju.pos + Vec3(0, 9 * sc, 0); eye = target + Vec3(std::sin(kaiju.facing + 0.5f) * 38 * sc, 6 * sc, std::cos(kaiju.facing + 0.5f) * 38 * sc);
    (void)a;
  } else if (mode == Mode::Ending && ending.stage >= 2) {  // closing shot: the new enclosure
    Vec3 c(E.zooX, 0, E.zooZ); float t = ending.stage == 2 ? ending.stageT : 12;
    float push = clampf((t - 5) / 3, 0, 1);
    target = c + Vec3(0, 5, 0); eye = c + Vec3(28 - 10 * push, 13 - 3 * push, -48 + 16 * push);
  } else if (mode == Mode::Ending || mode == Mode::Spin) {
    float a = realTime * 0.06f; target = Vec3(E.zooX, 10, E.zooZ); eye = target + Vec3(std::sin(a) * 110, 50, std::cos(a) * 110);
  } else if (mode == Mode::Reveal || (egg.active && egg.state > 0)) {
    float a = revealT * 0.4f + kaiju.cam.yaw;
    target = egg.pos + Vec3(0, 8, 0); eye = target + Vec3(std::sin(a) * 42, 14, std::cos(a) * 42);
  } else {
    eye = kaiju.cam.pos; target = kaiju.cam.target;
    if (kaiju.cam.shake > 0) eye += Vec3(frand() - 0.5f, frand() - 0.5f, frand() - 0.5f) * (kaiju.cam.shake * 1.2f);
  }
  rl.eye = eye;
  rl.opaque[MESH_SPHERE].push_back(makeInstance(Mat4::scale(2400, 2400, 2400) * Mat4::translation(eye.x, eye.y, eye.z), Color{1, 1, 1, 1}, 2));  // sky shell just inside the far plane; the shader paints the dusk gradient
  {  // model animation step: wall-clock while anything is moving, frozen while the pause menu is up
    bool paused = mode == Mode::Play && !menus.empty() && !charSel;
    rl.animDt = lastAnimT < 0 || paused ? 0.f : clampf(realTime - lastAnimT, 0, 0.1f); lastAnimT = realTime;
  }
  rl.view = Mat4::lookAtRH(eye, target, Vec3(0, 1, 0));
  rl.proj = Mat4::perspectiveRH(60.f * PI / 180.f, (float)w / std::max(1, h), 0.7f, 1400.f);
  Frustum fr; fr.fromMatrix(rl.view * rl.proj);
  lastEye = eye; world.render(rl, fr, eye);
  bodies.render(rl); debris->render(rl, eye); splats.render(rl); people.render(rl, eye); enemies.render(rl); fx->render(rl);
  if (charSel) {
    RigPose p; p.time = titleOrbit * 12; p.glow = 0.9f; p.walk = 1; p.walkPhase = p.time;
    { float sw = std::sin(p.walkPhase) * 0.55f; p.armL = sw * 0.6f; p.armR = -sw * 0.6f; }
    MonsterDef md = lookedDef(previewMonster);
    if (!save.monsters.count(md.id)) { md.body = md.belly = hexColor(0x15121c); md.accent = hexColor(0x3a3450); p.glow = 0.5f; }  // locked: silhouette
    ModelReq mr; mr.key = modelKey(MK_PREVIEW, 0); mr.clip = MC_WALK;  // walk cycle, not the bind T-pose
    if (!save.monsters.count(md.id)) { mr.tint = Color{0.07f, 0.06f, 0.11f, 1}; mr.speed = 0.45f; }  // locked: silhouette, still stepping
    if (!monsterModel(rl, md, kaiju.pos, kaiju.facing + std::sin(titleOrbit * 6) * 0.6f, T.monsters[previewMonster].scale, mr)) {
      FinishMarks fm = finishMarks(rl);
      drawMonster(rl, md, kaiju.pos, kaiju.facing + std::sin(titleOrbit * 6) * 0.6f, T.monsters[previewMonster].scale, p);
      if (save.monsters.count(md.id)) applyFinish(rl, fm, lookOf(md.id).finish, realTime);
    }
  } else if (mode == Mode::Intro) {
    RigPose p; p.time = introT * 2; p.glow = 1.0f + 0.5f * clampf((introT - 8) / 2, 0, 1); p.jaw = introT > 9 && introT < 11 ? 0.7f : 0.1f; p.headX = introT > 9 && introT < 11 ? -0.3f : 0;
    p.walk = 1; p.walkPhase = p.time; { float sw = std::sin(p.walkPhase) * 0.55f; p.armL = sw * 0.6f; p.armR = -sw * 0.6f; }
    ModelReq mr; mr.key = modelKey(MK_PREVIEW, 1); mr.clip = introT > 9 && introT < 11 ? MC_CHEER : MC_WALK; mr.glow = std::max(0.f, p.glow - 1) * 0.4f;
    if (!monsterModel(rl, T.monsters[0], introPos + Vec3(0, introRise, 0), introYaw, T.monsters[0].scale * 1.3f, mr))
      drawMonster(rl, T.monsters[0], introPos + Vec3(0, introRise, 0), introYaw, T.monsters[0].scale * 1.3f, p);
  } else { kaiju.render(rl, mode == Mode::Title ? titleOrbit * 12 : time); renderCoop(rl); }  // Titan co-op
  if (egg.active) drawEgg(rl, egg, time);
  drawBoss(rl, boss, time);
  if (mode == Mode::Ending && ending.stage >= 2) renderClosingShot(rl, ending.stage == 2 ? ending.stageT : 12);
  else renderZoo(rl);
  renderGates(rl); renderArc(rl); renderDeevo(rl);
  if (gloveDrop.active) {  // glowing, bobbing, slowly turning on the ground
    float bob = 2.2f + std::sin(time * 2.5f) * 0.6f;
    drawGlove(rl, Mat4::scale(1.4f, 1.4f, 1.4f) * Mat4::rotY(time * 1.2f) * Mat4::translation(gloveDrop.pos.x, bob, gloveDrop.pos.z), time);
    Color halo = hexColor(0xb06bff); halo.a = 0.18f + 0.1f * std::sin(time * 5);
    rl.alpha[MESH_SPHERE].push_back(makeInstance(Mat4::scale(8, 8, 8) * Mat4::translation(gloveDrop.pos.x, bob, gloveDrop.pos.z), halo, 1));
  }
  if (mode != Mode::Intro && mode != Mode::Title && gloveEquipped()) {  // worn on Tommyzilla's hand
    Vec3 h = kaiju.hand(); float k = 0.45f * std::max(1.f, kaiju.scale());
    drawGlove(rl, Mat4::scale(k, k, k) * Mat4::rotY(kaiju.facing) * Mat4::translation(h.x, h.y, h.z), time);
  }
  for (auto& c : chests) {  // locked skill chest: wooden box, gold bands and a padlock; the lid flips open
    Mat4 root = Mat4::rotY(0.4f) * Mat4::translation(c.pos.x, 0, c.pos.z);
    float bob = (c.state == 0 ? std::sin(time * 3) * 0.15f : 0) + c.drop * c.drop * 30;  // supply-drop chests fall from the sky
    rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(5, 3, 3.4f) * Mat4::translation(0, 1.5f + bob, 0) * root, hexColor(0x8a5a2a)));
    for (float bx : {-1.6f, 1.6f}) rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(0.5f, 3.1f, 3.5f) * Mat4::translation(bx, 1.5f + bob, 0) * root, hexColor(0xffd36b), 0.5f));
    float lid = c.state == 0 ? 0 : -std::min(1.9f, c.t * 6);
    rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(5.2f, 1.2f, 3.6f) * Mat4::translation(0, 0.6f, 1.8f) * Mat4::rotX(lid) * Mat4::translation(0, 3.0f + bob, -1.8f) * root, hexColor(0xa06a32)));
    if (c.state == 0 && c.free) {  // free supply drop: no padlock, rainbow halo
      Color halo = hexColor(((int)(time * 3)) % 2 ? 0x7ad7ff : 0xff8ad8); halo.a = 0.18f;
      rl.alpha[MESH_SPHERE].push_back(makeInstance(Mat4::scale(9, 7, 9) * Mat4::translation(c.pos.x, 2.5f + bob, c.pos.z), halo, 1));
    } else if (c.state == 0) {
      rl.opaque[MESH_BOX].push_back(makeInstance(Mat4::scale(1.1f, 1.1f, 0.5f) * Mat4::translation(0, 2.6f + bob, 1.85f) * root, hexColor(0xffe03a), 1));
      rl.opaque[MESH_RING].push_back(makeInstance(Mat4::scale(0.9f, 0.9f, 0.9f) * Mat4::rotX(PI / 2) * Mat4::translation(0, 3.3f + bob, 1.85f) * root, hexColor(0xc8ccd4), 1));
      Color halo = hexColor(0xffd36b); halo.a = 0.15f + 0.08f * std::sin(time * 4);
      rl.alpha[MESH_SPHERE].push_back(makeInstance(Mat4::scale(9, 7, 9) * Mat4::translation(c.pos.x, 2.5f, c.pos.z), halo, 1));
    }
  }
}
bool Game::render(RenderList& rl, int w, int h, RenderList* p2view) {
  const bool split = p2view && splitScreen() && w >= 2;
  if (!split) {
    render3D(rl, w, h);
    if (mode == Mode::Play || mode == Mode::Reveal || mode == Mode::GameOver) drawHud(rl);
  } else {
    // 2P split screen: left half = P1's camera, right half = P2's (each its own aspect), each with its own HUD
    const int wl = w / 2, wr = w - wl;
    render3D(rl, wl, h); rl.vpX = 0; rl.vpW = wl;
    std::swap(kaiju, kaiju2); coopSwapped = true;
    render3D(*p2view, wr, h); p2view->vpX = wl; p2view->vpW = wr;
    coopSwapped = false; std::swap(kaiju, kaiju2);
    for (int side = 0; side < 2; side++) {
      RenderList& src = side ? *p2view : rl;
      RenderList hud; hud.width = src.width; hud.height = h; hud.view = src.view; hud.proj = src.proj; hud.eye = src.eye; hud.time = time;
      hud.uiScale = std::max(1.f, std::round(h / 720.f * 4.f * 0.75f) / 2.f);  // a half-width HUD: 3/4 size text (1.5 at 720p, 2 at 1080p)
      const bool pad0 = usingPad, cap0 = mouseCaptured;
      if (side) { std::swap(kaiju, kaiju2); coopSwapped = true; usingPad = true; mouseCaptured = true; }  // P2 is the controller: pad prompts, no mouse hint
      drawHud(hud);
      if (side) { coopSwapped = false; std::swap(kaiju, kaiju2); usingPad = pad0; mouseCaptured = cap0; }
      for (size_t i = 0; i + 2 < hud.ui.size(); i += 3) {  // keep each HUD inside its own half (drop triangles that spill over)
        float cx = (hud.ui[i].x + hud.ui[i + 1].x + hud.ui[i + 2].x) / 3;
        if (cx < 0 || cx > (float)src.width) continue;
        for (int k = 0; k < 3; k++) { UIVertex v = hud.ui[i + k]; v.x += (float)src.vpX; rl.ui.push_back(v); }
      }
    }
    UIDraw u(rl); u.W = (float)w; u.rect((float)wl - 2, 0, 4, (float)h, hexColor(0x15121c));  // divider
    rl.width = w;  // shared 2D (spinner, menus) stays fullscreen
  }
  // ---- 2D (fullscreen)
  if (mode == Mode::Spin) drawSpinner(rl);
  if (mode == Mode::Ending) {
    if (ending.stage >= 2 && ending.usedVideo && video) rl.video = video->frame(rl.videoW, rl.videoH, rl.videoStride, rl.videoSerial);  // last frame holds after the end
    drawEnding(rl);
  }
  if (mode == Mode::Intro) drawIntro(rl);
  drawMenus(rl);
  return split;
}
}  // namespace tz
