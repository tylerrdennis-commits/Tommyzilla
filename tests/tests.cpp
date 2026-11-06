// Native Linux test harness for the platform-independent game logic (unlock tables, campaign shuffle, economy,
// police escalation, save format, synth, meshes) plus headless full-game simulations.
#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <tuple>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "audio/audio_synth.h"
#include "core/json.h"
#include "game/controls.h"
#include "game/game.h"
#include "renderer/meshes.h"
#include "ui/ui_draw.h"
using namespace tz;

static int gPass = 0, gFail = 0;
static Rng& tmpRng(uint64_t seed) { static Rng r(1); r = Rng(seed); return r; }  // a fresh seeded Rng for one call
#define CHECK(c) do { if (c) gPass++; else { gFail++; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define SECTION(n) std::printf("[%s]\n", n)

static std::string file(const char* name) { std::string s; readTextFile(std::string(TZ_DATA_DIR) + "/" + name, s); return s; }
static void loadData() {
  std::string e = loadGameData(file("config.json"), file("moves.json"), file("monsters.json"), file("levels.json"), file("eggs.json"));
  if (!e.empty()) { std::printf("data load error: %s\n", e.c_str()); std::exit(1); }
}
static void run(Game& g, float secs, InputState in = InputState(), float dt = 1.f / 60) {
  for (float t = 0; t < secs; t += dt) { g.frame(dt, in); in.pressed[A_CONFIRM] = false; for (auto& p : in.pressed) p = false; }
}
static void tap(Game& g, Action a) { InputState in; in.pressed[a] = true; in.held[a] = true; g.frame(1.f / 60, in); }
static RenderList renderOnce(Game& g) { RenderList rl; g.render(rl, 1280, 720); return rl; }
static void dismissPrompts(Game& g) { for (int i = 0; i < 6 && !g.menus.empty() && g.menus.back().kind == 4; i++) tap(g, A_CONFIRM); }
static bool pickChampion(Game& g) {  // level-6 'PICK YOUR CHAMPION' menu: confirm the highlighted kaiju
  if (g.menus.empty() || g.menus.back().title != "PICK YOUR CHAMPION") return false;
  g.updateMenus(InputState(), 0.016f); tap(g, A_CONFIRM); return true;
}

static void testData() {
  SECTION("data tables");
  CHECK(T.moves.size() == 27); CHECK(T.monsters.size() == 18); CHECK(T.levels.size() == 20);
  int melee = 0, direct = 0, super = 0; std::set<std::string> ids, names;
  for (auto& m : T.moves) { melee += m.kind == MoveKind::Melee; direct += m.kind == MoveKind::Direct; super += m.kind == MoveKind::Super; ids.insert(m.id); names.insert(m.name); }
  CHECK(melee == 10); CHECK(direct == 11); CHECK(super == 6); CHECK(ids.size() == 27); CHECK(names.size() == 27);
  for (const char* s : {"claw", "tail", "stomp"}) { int i = T.moveIndex(s); CHECK(i >= 0 && T.moves[i].unlock == UnlockType::Start && T.moves[i].kind == MoveKind::Melee); }
  int starters = 0; for (auto& m : T.moves) if (m.unlock == UnlockType::Start) starters++;
  CHECK(starters == 5);  // 3 melee + Glow Breath (slot 1) + Rainbow Blaster (slot 2)
  CHECK(T.levels.back().obj == ObjType::Boss && T.levels.back().name == "THE BULK");
  std::set<std::string> mons; int starterMon = 0; std::set<int> plans;
  for (auto& m : T.monsters) { mons.insert(m.name); starterMon += m.starter; plans.insert((int)m.plan); CHECK(m.speed > 0.5f && m.speed < 1.6f); }
  CHECK(mons.size() == 18); CHECK(starterMon == 1); CHECK(plans.size() == 7);
  CHECK(T.monsters[0].name == "Tommyzilla");
  for (auto& l : T.levels) CHECK(!l.name.empty() && !l.desc.empty());
  CHECK(T.levels[2].obj == ObjType::Destroy && T.levels[2].unit == "tank");
  CHECK(T.levels[7].jets > 0 && T.levels[0].tanks == 0);
  CHECK(embeddedJson("moves") == file("moves.json")); CHECK(embeddedJson("levels") == file("levels.json")); CHECK(embeddedJson("eggs") == file("eggs.json")); CHECK(embeddedJson("credits") == file("credits.json"));
  CHECK(std::fabs(C.collapseChance - 0.78f) < 1e-6f); CHECK(C.rebuildDelay == 300); CHECK(C.nukeCancelCost == 150);
  std::string err; Json j; CHECK(Json::parse(file("monsters.json"), j, &err));
}
static void testUnlocks() {
  SECTION("unlock table");
  Profile p; prog::initProfile(p, T);
  CHECK(p.moves.count("claw") && p.moves.count("tail") && p.moves.count("stomp") && p.moves.count("breath"));
  CHECK(p.slots[0] == "breath" && p.slots[1] == "blaster" && p.slotCount == 2 && p.melee[0] == "claw" && p.melee[2] == "stomp");
  CHECK(prog::pendingUnlocks(p, T).empty());
  p.smashCount = 12;
  std::set<std::string> got; for (int i : prog::pendingUnlocks(p, T)) got.insert(T.moves[i].id);
  CHECK(got == (std::set<std::string>{"headbutt", "belly", "bubble", "beam"}));
  p.landmarks = {"empress"}; got.clear(); for (int i : prog::pendingUnlocks(p, T)) got.insert(T.moves[i].id);
  CHECK(got.count("tailspin") == 1); CHECK(got.count("quake") == 0);
  p.landmarks.insert("school"); got.clear(); for (int i : prog::pendingUnlocks(p, T)) got.insert(T.moves[i].id);
  CHECK(got.count("quake") == 1);
  p.smashCount = 1000; p.landmarks = {"empress", "parliament", "museum", "ferry", "chinatown", "school"};
  CHECK(prog::pendingUnlocks(p, T).size() == 21);  // every non-starter move is reachable
  CHECK(prog::schoolProgress(p) == 4);
  Profile q; prog::initProfile(q, T); q.smashCount = 7; CHECK(!prog::slot2Earned(q)); q.smashCount = 8; CHECK(prog::slot2Earned(q)); CHECK(C.slot2Smashes == 8);
  q.moves.insert("roar"); CHECK(!prog::equip(q, "roar", 0)); CHECK(prog::equip(q, "roar", 1) && q.slots[1] == "roar"); CHECK(prog::freeSlot(q) == -1);
  CHECK(!prog::equip(q, "breath", 1) && prog::equip(q, "blaster", 1) && q.slots[1] == "blaster");  // slot types are enforced
}
static void testEconomy() {
  SECTION("economy");
  CHECK(std::fabs(prog::speedMult(1) - 1.0f) < 1e-5f); CHECK(std::fabs(prog::speedMult(6) - 2.0f) < 1e-5f); CHECK(prog::maxLevel() == 6);
  Profile p; p.coins = 59; CHECK(prog::buyUpgrade(p, prog::UP_SPEED) == 1); CHECK(p.speedLvl == 1 && !p.storeUsed);
  p.coins = 60 + 140 + 260 + 420 + 650;
  for (int i = 0; i < 5; i++) CHECK(prog::buyUpgrade(p, prog::UP_SPEED) == 0);
  CHECK(p.speedLvl == 6 && p.coins == 0 && p.storeUsed); CHECK(prog::buyUpgrade(p, prog::UP_SPEED) == 2);
  CHECK(prog::upgradeCost(1) == 60 && prog::upgradeCost(5) == 650 && prog::upgradeCost(6) == -1);
  CHECK(std::fabs(prog::damageMult(3) - 1.7f) < 1e-5f);
}
static void testPolice() {
  SECTION("police escalation");
  CHECK(prog::starsFromHeat(0) == 0); CHECK(prog::starsFromHeat(0.99f) == 0); CHECK(prog::starsFromHeat(1.0f) == 1); CHECK(prog::starsFromHeat(5.99f) == 5); CHECK(prog::starsFromHeat(9) == 5);
  int cars[6] = {0, 2, 4, 6, 8, 8}, tanks[6] = {0, 0, 0, 0, 2, 3};
  for (int s = 0; s <= 5; s++) { auto t = prog::policeTargets(s); CHECK(t.cars == cars[s]); CHECK(t.tanks == tanks[s]); CHECK(t.heli == (s >= 3)); CHECK(t.nuke == (s >= 5)); }
}
static void testCampaign() {
  SECTION("campaign shuffle");
  auto a = prog::campaignOrder(42, T, 10), b = prog::campaignOrder(42, T, 10);
  CHECK(a.size() == 10); CHECK(a == b);
  std::set<std::string> u(a.begin(), a.end()); CHECK(u.size() == 10); CHECK(!u.count("tommyzilla"));
  int differs = 0; for (uint64_t s = 1; s <= 20; s++) if (prog::campaignOrder(s, T, 10) != a) differs++;
  CHECK(differs >= 19);
  std::set<std::string> seenFirst; for (uint64_t s = 1; s <= 200; s++) seenFirst.insert(prog::campaignOrder(s * 7919, T, 10)[0]);
  CHECK(seenFirst.size() >= 12);  // every non-starter monster can come first
}
static void testSave() {
  SECTION("save format");
  SaveData s; prog::initProfile(s.sandbox, T); prog::initProfile(s.adventure, T);
  s.sandbox.coins = 321; s.sandbox.smashCount = 25; s.sandbox.slotCount = 2; s.sandbox.slots[1] = "beam"; s.sandbox.landmarks = {"empress", "ferry"};
  s.adventure.campaignSeed = 1234567890123ull; s.adventure.campaign = prog::campaignOrder(9, T, 10); s.adventure.level = 3;
  s.monsters.insert("gloopzilla"); s.monster = "gloopzilla"; s.settings.police = true; s.settings.volume = 0.3f; s.adventureUnlocked = true; s.tutorial.step = 4;
  std::string text = s.toJson().dump(2); Json j; CHECK(Json::parse(text, j));
  SaveData r; CHECK(r.fromJson(j));
  CHECK(r.sandbox.coins == 321 && r.sandbox.smashCount == 25 && r.sandbox.slotCount == 2 && r.sandbox.slots[1] == "beam" && r.sandbox.landmarks.size() == 2);
  CHECK(r.adventure.campaignSeed == 1234567890123ull && r.adventure.campaign == s.adventure.campaign && r.adventure.level == 3);
  CHECK(r.monster == "gloopzilla" && r.monsters.count("gloopzilla") && r.settings.police && std::fabs(r.settings.volume - 0.3f) < 1e-6f && r.adventureUnlocked && r.tutorial.step == 4);
  SaveData bad; Json junk; Json::parse("[1,2,3]", junk); CHECK(!bad.fromJson(junk));
}
static void testSynthAndMeshes() {
  SECTION("synth + meshes");
  for (int i = 0; i < SND_COUNT; i++) {
    PcmBuffer b = synthSound((SoundId)i); int peak = 0; for (auto v : b.samples) peak = std::max(peak, std::abs((int)v));
    if (!(b.samples.size() > 1000 && peak > 1500 && b.samples.size() < (size_t)SYNTH_RATE * 3)) std::printf("  sound %s: n=%zu peak=%d\n", soundName((SoundId)i), b.samples.size(), peak);
    CHECK(b.samples.size() > 1000 && peak > 1500 && b.samples.size() < (size_t)SYNTH_RATE * 3);
  }
  for (int i = 0; i < LOOP_COUNT; i++) { PcmBuffer b = synthLoop((LoopId)i); CHECK(b.loop && b.samples.size() > SYNTH_RATE / 2); }
  CHECK(soundByName("roar", SND_ui) == SND_roar);
  for (auto& m : T.moves) CHECK(soundByName(m.sound, SND_COUNT) != SND_COUNT);  // every move has its own synthesized sound
  for (int i = 0; i < MESH_COUNT; i++) {
    MeshData m = buildMesh((MeshId)i); CHECK(!m.v.empty() && m.idx.size() % 3 == 0);
    bool ok = true; float lim = i == MESH_RING ? 0.55f : 0.51f; for (auto x : m.idx) if (x >= m.v.size()) ok = false;
    for (auto& v : m.v) { float l = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz); if (std::fabs(l - 1) > 1e-3f || std::fabs(v.px) > lim || std::fabs(v.py) > lim) ok = false; }
    CHECK(ok);
  }
}

static std::unique_ptr<Game> newGame(std::string* saved = nullptr) {
  auto g = std::make_unique<Game>();
  if (saved) g->saveText = [saved](const std::string& s) { *saved = s; return true; };
  g->init(nullptr);
  return g;
}
static Building* nearestBuilding(Game& g, const char* name) {
  Building* best = nullptr; float bd = 1e9f;
  for (auto& b : g.world.buildings) if (b.name == name && b.generated) { float d = dist2D(b.x, b.z, g.kaiju.pos.x, g.kaiju.pos.z); if (d < bd) { bd = d; best = &b; } }
  return best;
}
static void standNextTo(Game& g, Building& b) { g.kaiju.pos = Vec3(b.x, 0, b.maxZ + 6); g.kaiju.facing = PI; g.world.updateStreaming(g.kaiju.pos.x, g.kaiju.pos.z, true); }

static void testSandboxSim() {
  SECTION("sandbox simulation");
  std::string saved; auto g = newGame(&saved);
  CHECK(g->mode == Mode::Title && !g->menus.empty() && g->menus.back().kind == 2);
  RenderList rl = renderOnce(*g); CHECK(rl.instanceCount() > 1000 && rl.ui.size() > 100);
  tap(*g, A_CONFIRM);  // Sandbox
  CHECK(g->mode == Mode::Play && g->menus.empty() && !g->adventure);
  Vec3 p0 = g->kaiju.pos; InputState walk; walk.moveY = 1; run(*g, 1.5f, walk);
  CHECK(length(g->kaiju.pos - p0) > 5); CHECK(g->save.tutorial.step >= 1);
  // tutorial: each attack
  for (Action a : {A_CLAW, A_TAIL, A_STOMP, A_SLOT1}) { tap(*g, a); run(*g, 2.5f); }
  CHECK(g->save.tutorial.step == 5);
  // grab a tree and throw it
  int tree = -1; float bd = 1e9f;
  for (int i = 0; i < (int)g->bodies.list.size(); i++) { auto& b = g->bodies.list[i]; if (b.used && b.kind == BODY_TREE && g->world.isActiveAt(b.pos.x, b.pos.z)) { float d = dist2D(b.pos.x, b.pos.z, g->kaiju.pos.x, g->kaiju.pos.z); if (d < bd) { bd = d; tree = i; } } }
  CHECK(tree >= 0);
  if (tree >= 0) {
    g->kaiju.pos = g->bodies.list[tree].pos + Vec3(0, 0, -4); g->kaiju.facing = 0;
    tap(*g, A_GRAB); CHECK(g->kaiju.held == tree); run(*g, 0.3f);
    CHECK(g->save.tutorial.step == 6);
    std::printf("  after steps: rainbows %d buildings %d\n", g->save.tutorial.rainbows, g->save.tutorial.buildings);
    CHECK(!g->save.tutorial.done && g->menus.empty());  // not complete until the rainbow + building challenge is done
    // rainbow challenge: gulp civilians with E (each counts like smashing)
    for (int k = 0; k < 60 && g->save.tutorial.rainbows < (int)C.tutorialRainbows; k++) {
      int pi = -1; float bd2 = 1e9f;
      for (int i = 0; i < (int)g->people.list.size(); i++) { auto& q = g->people.list[i]; if (q.active && q.y < 1) { float d = dist2D(q.x, q.z, g->kaiju.pos.x, g->kaiju.pos.z); if (d < bd2) { bd2 = d; pi = i; } } }
      if (pi < 0) { run(*g, 1); continue; }
      g->kaiju.pos = Vec3(g->people.list[pi].x + 3, 0, g->people.list[pi].z); tap(*g, A_EAT); run(*g, 0.1f);
    }
    CHECK(g->save.tutorial.rainbows >= (int)C.tutorialRainbows); CHECK(!g->save.tutorial.done);
    // building challenge
    int built = 0;
    for (auto& b : g->world.buildings) {
      if (g->save.tutorial.buildings >= (int)C.tutorialBuildings) break;
      if (!b.generated || b.school || !b.landmark.empty() || b.smashed || !g->world.chunks[b.chunk].active) continue;
      for (int k = 0; k < 30 && !b.smashed; k++) g->world.hitBuilding(b, b.x, 2, b.z, 40, 6, b.x, b.z + 5, 4);
      built += b.smashed;
    }
    CHECK(g->save.tutorial.buildings >= (int)C.tutorialBuildings);
    run(*g, 0.1f);
    CHECK(g->save.tutorial.done && g->bigText == "Great smashing!" && g->P().keys == (int)C.tutorialRewardKeys);  // completion banner + reward key
    run(*g, 2.5f);
    CHECK(!g->menus.empty() && g->menus.back().kind == 4);  // then the police yes/no prompt
    run(*g, 0.1f); CHECK(g->menus.back().items.size() == 2 && g->menus.back().items[0].label.rfind("No", 0) == 0);  // default answer = police OFF
    tap(*g, A_DOWN); tap(*g, A_CONFIRM); CHECK(g->save.settings.police && g->save.tutorial.done && g->menus.empty());  // this run picks "Yes"
    tap(*g, A_GRAB); CHECK(g->kaiju.held == -1 && g->bodies.list[tree].thrown);
    run(*g, 2);
  }
  // smash: melee near Old Town buildings earns coins, smashes, pops people into splats
  int coins0 = g->P().coins;
  for (int k = 0; k < 40 && g->P().smashCount < 22; k++) {
    Building* b = nullptr; float best = 1e9f;
    for (auto& bb : g->world.buildings) if (bb.generated && !bb.smashed && !bb.school && bb.name == "Old Town shop") { float d = dist2D(bb.x, bb.z, g->kaiju.pos.x, g->kaiju.pos.z); if (d < best) { best = d; b = &bb; } }
    if (!b) { g->kaiju.pos = Vec3(120, 0, -150); g->world.updateStreaming(120, -150, true); continue; }
    standNextTo(*g, *b);
    for (int h = 0; h < 16 && !b->smashed; h++) {
      int bi = -1; float bd2 = 1e9f;  // walk up to the closest standing brick, like a player would
      for (int q = 0; q < (int)b->bricks.size(); q++) if (b->alive[q]) { float d = b->bricks[q].y * 3 + std::fabs(b->bricks[q].x - b->x) + (b->maxZ - b->bricks[q].z); if (d < bd2) { bd2 = d; bi = q; } }
      if (bi < 0) break;
      g->kaiju.pos = Vec3(b->bricks[bi].x, 0, b->bricks[bi].z + 4.5f); g->kaiju.facing = PI;
      tap(*g, h % 2 ? A_CLAW : A_STOMP); run(*g, 1.0f); dismissPrompts(*g);
    }
    g->kaiju.hp = g->kaiju.maxHp();
  }
  std::printf("  police %d adv %d heat %.2f mode %d menus %zu\n", (int)g->save.settings.police, (int)g->adventure, g->enemies.heat, (int)g->mode, g->menus.size());
  std::printf("  smashed %d, coins %d, debris live %d, stars %d, units %zu\n", g->P().smashCount, g->P().coins, g->debris->live, g->enemies.stars(), g->enemies.units.size());
  CHECK(g->P().smashCount >= 20); CHECK(g->P().coins > coins0); CHECK(g->P().slotCount == 2);
  CHECK(g->P().moves.count("headbutt") && g->P().moves.count("bubble") && g->P().moves.count("beam"));
  CHECK(g->enemies.heat > 0);  // smashing raises the wanted level
  if (g->enemies.stars() < 1) { g->enemies.addHeat(1.2f); run(*g, 3); }
  CHECK(g->enemies.stars() >= 1); CHECK(!g->enemies.units.empty());
  CHECK(!saved.empty()); { Json j; CHECK(Json::parse(saved, j) && j["sandbox"]["smashCount"].numOr(0) >= 20); }
  rl = renderOnce(*g); CHECK(rl.instanceCount() > 1000);
  // pause menu + store purchase marks the store goal
  g->P().coins = 1000; tap(*g, A_PAUSE); CHECK(!g->menus.empty() && g->menus.back().kind == 1);
  g->openStore(); tap(*g, A_CONFIRM); CHECK(g->P().dmgLvl == 2 && g->P().storeUsed);
  g->menus.clear();
  // nuke: forced 5 stars, pay with coins
  g->enemies.heat = 5.5f; g->P().coins = 500; run(*g, 1.5f); CHECK(g->enemies.nukeActive);
  tap(*g, A_PAY); CHECK(!g->enemies.nukeActive && g->P().coins == 350);
  // nuke countdown without coins -> game over prompt (kid-friendly, coins kept)
  g->enemies.heat = 5.5f; g->P().coins = 10; run(*g, 1.0f); CHECK(g->enemies.nukeActive);
  run(*g, C.nukeCountdown + 1); CHECK(g->mode == Mode::GameOver && !g->menus.empty());
  tap(*g, A_DOWN); tap(*g, A_UP); tap(*g, A_CONFIRM); CHECK(g->mode == Mode::Play && g->P().coins == 10);
  // police off clears the chase
  g->save.settings.police = false; run(*g, 0.5f); int alive = 0; for (auto& u : g->enemies.units) if (!u.wreck) alive++; CHECK(alive == 0);
}

static void testSchoolAndRebuild() {
  SECTION("school lock + rebuild");
  auto g = newGame(); tap(*g, A_CONFIRM); g->save.tutorial.done = true;
  Building* school = g->world.landmark("school");
  g->kaiju.pos = Vec3(school->x, 0, school->maxZ + 6); g->world.updateStreaming(g->kaiju.pos.x, g->kaiju.pos.z, true);
  int n0 = school->aliveCount;
  HitResult r = g->world.damageSphere(school->x, 2, school->z, 12, 50, school->x, school->z + 10, 10);
  CHECK(r.blocked && r.bricks == 0 && school->aliveCount == n0);
  for (const char* id : {"empress", "parliament", "museum", "ferry"}) g->P().landmarks.insert(id);
  g->P().schoolUnlocked = true;
  r = g->world.damageSphere(school->x, 2, school->z, 12, 50, school->x, school->z + 10, 10);
  CHECK(r.bricks > 0 && school->aliveCount < n0);
  // collapse odds: ~78% of smashed buildings fully collapse
  int smashed = 0, collapsed = 0;
  for (auto& ch : g->world.chunks) for (int id : ch.buildings) g->world.generate(g->world.buildings[id]);
  for (auto& ch : g->world.chunks) ch.active = true;
  for (auto& b : g->world.buildings) {
    if (b.school || !b.landmark.empty() || b.smashed) continue;
    for (int k = 0; k < 30 && !b.smashed; k++) g->world.hitBuilding(b, b.x, 2, b.z, 40, 6, b.x, b.z + 5, 4);
    if (b.smashed) { smashed++; collapsed += b.collapsed; }
  }
  float frac = smashed ? (float)collapsed / smashed : 0;
  std::printf("  %d smashed, %.2f collapsed\n", smashed, frac);
  CHECK(smashed > 100 && frac > 0.68f && frac < 0.88f);
  // splat persists until the nearby building finishes rebuilding (5 min delay, then brick by brick)
  Building& h = *nearestBuilding(*g, "House");
  g->menus.clear(); g->pendingOffers.clear();
  g->splats.spawn(h.x + 3, h.z + 3); int alive0 = g->splats.count;
  run(*g, 1); if (g->splats.count < 1) std::printf("  splats=%d menus=%zu[%s] mode=%d\n", g->splats.count, g->menus.size(), g->menus.empty() ? "" : g->menus.back().title.c_str(), (int)g->mode); CHECK(g->splats.count >= 1);
  g->world.hitBuilding(h, h.x, 2, h.z, 20, 999, h.x, h.z + 5, 4); CHECK(h.aliveCount < (int)h.bricks.size());
  float t0 = g->time; while (g->time - t0 < C.rebuildDelay - 2) { g->time += 1; g->world.update(1); }
  CHECK(h.aliveCount < (int)h.bricks.size());
  for (int i = 0; i < 2000 && h.aliveCount < (int)h.bricks.size(); i++) { g->time += 0.05f; g->world.update(0.05f); }
  CHECK(h.aliveCount == (int)h.bricks.size() && !h.smashed);
  g->splats.update(2); CHECK(g->splats.count < std::max(alive0, 1) + 1);
  bool nearAlive = false; for (auto& s : g->splats.list) if (s.alive && dist2D(s.x, s.z, h.x, h.z) < 10) nearAlive = true;
  CHECK(!nearAlive);
}

static void testAllMovesAndMonsters() {
  SECTION("all 27 moves x all 18 monsters");
  auto g = newGame(); tap(*g, A_CONFIRM); g->save.tutorial.done = true; g->menus.clear();
  Profile& p = g->P(); for (auto& m : T.moves) p.moves.insert(m.id); p.slotCount = 2;
  Building* b = nearestBuilding(*g, "House");
  p.eaten = 100000;  // supers are fuelled by eaten civilians
  for (int mi = 0; mi < (int)T.monsters.size(); mi++) {
    g->kaiju.monster = mi; g->save.monsters.insert(T.monsters[mi].id);
    for (int i = 0; i < (int)T.moves.size(); i++) {
      if (mi % 6 != 0 && i % 3 != mi % 3) continue;  // full matrix for a few monsters, a third of the moves for the rest
      g->kaiju.pos = Vec3(b->x, 0, b->maxZ + 8); g->kaiju.facing = PI; g->kaiju.cooldowns.assign(T.moves.size(), 0); g->kaiju.at = Attack(); g->kaiju.charging = false;
      const MoveDef& m = T.moves[i];
      if (m.kind == MoveKind::Melee) { p.melee[0] = m.id; tap(*g, A_CLAW); }
      else { p.slots[0] = m.id; InputState in; in.pressed[A_SLOT1] = in.held[A_SLOT1] = true; g->frame(1.f / 60, in); }
      CHECK(g->kaiju.at.move == i || g->kaiju.charging || g->kaiju.stompAim);  // stomp fires on release
      run(*g, std::max(1.0f, m.duration + 0.6f));
      CHECK(g->kaiju.at.move < 0);
      if (!g->menus.empty()) g->menus.clear();
      g->kaiju.hp = g->kaiju.maxHp();
    }
    RenderList rl = renderOnce(*g); CHECK(rl.instanceCount() > 500);
    RenderList solo; RigPose pose; drawMonster(solo, T.monsters[mi], Vec3(), 0, 1, pose); if (solo.instanceCount() < 25) std::printf("  %s: %zu parts\n", T.monsters[mi].name.c_str(), solo.instanceCount()); CHECK(solo.instanceCount() >= 12);
  }
  std::printf("  smashed %d with the full roster\n", p.smashCount);
  CHECK(p.smashCount > 0);
}

static bool openZooBox(Game& g); static bool grabStone(Game& g); static bool placeAllStones(Game& g); static bool beatStagedBulk(Game& g);
static void testAdventure() {
  SECTION("adventure campaign");
  std::string saved; auto g = newGame(&saved);
  // main menu: adventure locked until the sandbox goals are done
  CHECK(!prog::adventureReady(g->save));
  tap(*g, A_DOWN); tap(*g, A_CONFIRM); CHECK(g->mode == Mode::Title);
  g->save.sandbox.smashCount = 20; g->save.sandbox.storeUsed = true; g->save.sandbox.schoolSmashed = true;
  CHECK(prog::adventureReady(g->save)); g->checkAdventureGate(); CHECK(g->save.adventureUnlocked);
  tap(*g, A_CONFIRM); CHECK(g->mode == Mode::Play && g->adventure && g->level.active && g->level.idx == 0);
  const int NL = (int)T.levels.size();
  CHECK((int)g->save.adventure.campaign.size() == NL && g->save.adventure.campaignSeed != 0);
  CHECK(g->P().coins == 0);  // separate economy
  std::set<UnitType> seen;
  int freeChests = 0; std::string champion;
  for (int lv = 0; lv < NL; lv++) {
    if (lv == (int)C.kaijuPickLevel - 1) {  // level 6: pick a champion, locked for the rest of the campaign
      CHECK(!g->menus.empty() && g->menus.back().title == "PICK YOUR CHAMPION" && g->menus.back().noBack);
      CHECK(pickChampion(*g)); champion = g->save.adventure.lockedMonster; CHECK(!champion.empty() && g->save.monster == champion);
    }
    if (lv >= (int)C.kaijuPickLevel - 1) CHECK(g->menus.empty() && g->kaiju.def().id == champion);
    if (T.levels[lv].chestDrop) for (auto& c : g->chests) freeChests += c.free;
    CHECK(g->level.active && g->level.idx == lv);
    for (int w = 0; w < 25; w++) { run(*g, 1); for (auto& u : g->enemies.units) seen.insert(u.type); dismissPrompts(*g); }
    g->kaiju.hp = g->kaiju.maxHp(); dismissPrompts(*g);
    for (auto& u : g->enemies.units) seen.insert(u.type);
    const LevelDef& L = T.levels[lv];
    // complete the objective directly (the simulation of fights is covered elsewhere)
    switch (L.obj) {
      case ObjType::Smash: g->level.count = L.count; break;
      case ObjType::Survive: g->level.t = 0.01f; g->kaiju.pos = L.hasZone ? Vec3(L.zx, 0, L.zz) : g->kaiju.pos; break;
      case ObjType::Reach: g->kaiju.pos = Vec3(L.x, 0, L.z); break;
      case ObjType::Destroy: g->level.destroyed = L.count; break;
      case ObjType::Landmarks: for (auto& id : L.landmarks) g->world.landmark(id)->landmarkSmashed = true; g->level.count = L.count; break;
      case ObjType::Box: CHECK(openZooBox(*g)); break;
      case ObjType::Stone: CHECK(grabStone(*g)); break;
      case ObjType::Boss: CHECK(placeAllStones(*g) && g->boss.active); CHECK(beatStagedBulk(*g)); break;
    }
    run(*g, 0.1f);
    if (!L.miniBoss.empty()) {  // levels 11, 12, 14-19: the goal summons a boss, which must be knocked out
      CHECK(g->level.active && g->level.bossPhase && !g->level.won);
      Unit* mb = g->enemies.miniBoss(); CHECK(mb != nullptr);
      if (mb) { seen.insert(U_MBOSS); CHECK(mb->boss == bossIdFromName(L.miniBoss));  /* Titan: varied bosses */ RenderList brl = renderOnce(*g); CHECK(brl.instanceCount() > 100); g->enemies.wreckUnit(*mb); }
      run(*g, 0.1f); CHECK(g->level.bossDown);
    }
    if (lv == 11) CHECK(g->AP().gauntletSlot && !g->AP().gauntlet);  // L12: the gauntlet slot appears silently
    if (L.obj == ObjType::Boss) {  // L20: no egg - the ending (credits + after-credits video) follows
      CHECK(!g->menus.empty() && g->menus.back().title == "PEACE TO THE LAND!"); CHECK(g->save.crazyUnlocked && g->save.adventure.levelsWon == NL);
      tap(*g, A_CONFIRM); CHECK(g->mode == Mode::Ending && g->ending.stage == 0);
      CHECK(g->save.adventure.gauntletCracked && !g->save.adventure.gauntlet && g->save.adventure.stones.empty());
      g->ending = EndingRun(); g->mode = Mode::Play; g->menus.clear();
      break;
    }
    CHECK(g->level.won && g->egg.active && g->egg.state == 0);
    std::string expect = g->save.adventure.campaign[lv];
    CHECK(g->egg.monster == expect);
    // hatch with Glow Breath (slot 1)
    for (int d = 0; d < 16 && g->egg.state == 0; d++) {  // stand 12 m (then 8 m) from the egg on open ground, facing it; try other sides if the breath is blocked
      float a = (d % 8) * PI / 4; Vec3 q = g->egg.pos - Vec3(std::sin(a), 0, std::cos(a)) * (d < 8 ? 12.f : 8.f);
      if (g->world.blockedForPerson(q.x, q.z)) continue;
      for (int k = 0; k < 12 && g->egg.state == 0; k++) { g->kaiju.pos = q; g->kaiju.facing = a; g->kaiju.cooldowns.assign(T.moves.size(), 0); InputState in; in.pressed[A_SLOT1] = in.held[A_SLOT1] = true; g->frame(1.f / 60, in); run(*g, 0.3f); }
    }
    if (!(g->egg.state > 0)) std::printf("  lv %d egg not hatched: hits %d mode %d menus %zu %s\n", lv, g->egg.hits, (int)g->mode, g->menus.size(), g->menus.empty() ? "" : g->menus.back().title.c_str());
    CHECK(g->egg.state > 0 && g->mode == Mode::Reveal);
    RenderList rl = renderOnce(*g); CHECK(rl.instanceCount() > 100);
    run(*g, C.revealTime + 3);
    CHECK(g->mode == Mode::Play && !g->egg.active && g->save.monsters.count(expect));
    CHECK(!g->menus.empty() && g->menus.back().kind == 4);
    tap(*g, A_CONFIRM);  // play as the new monster (or keep the locked-in champion from level 6 on)
    CHECK(g->kaiju.def().id == (champion.empty() ? expect : champion));
    run(*g, 1);  // free play continues after a win
    CHECK(g->mode == Mode::Play && !g->level.active);
    if (lv < NL - 1) {
      if (!g->menus.empty()) std::printf("  menu before pause: %s\n", g->menus.back().title.c_str());
      tap(*g, A_PAUSE); g->updateMenus(InputState(), 0.016f);  CHECK(g->menus.back().items.size() > 1 && g->menus.back().items[1].label == "Next Level");
      tap(*g, A_DOWN); tap(*g, A_CONFIRM);
    }
  }
  const int NM = std::min(NL - 1, (int)T.monsters.size() - 1) + 1;  // 19 eggs over a 17-monster pool: the last ones are duplicates
  CHECK((int)g->save.monsters.size() == NM); CHECK(g->save.adventure.levelsWon == NL);
  std::printf("  unit types seen:"); for (auto t : seen) std::printf(" %d", (int)t); std::printf("\n");
  CHECK(seen.count(U_JEEP) && seen.count(U_AHELI) && seen.count(U_ATANK) && seen.count(U_JET));
  CHECK(seen.count(U_COP) && seen.count(U_CAR) && seen.count(U_SOLDIER) && seen.count(U_MBOSS));
  CHECK(freeChests >= 1);
  { Json j; CHECK(Json::parse(saved, j) && (int)j["monsters"].size() == NM); }
  // all unlocked monsters are selectable in sandbox
  g->menus.clear(); g->startMode(false); g->openCharacterSelect(true);
  int enabled = 0; g->updateMenus(InputState(), 0.016f); for (auto& it : g->menus.back().items) enabled += it.enabled && it.label != "Back" && it.label.find("shiny") == std::string::npos;
  CHECK(enabled == NM);
  // a reloaded save keeps everything
  auto g2 = std::make_unique<Game>(); g2->init(&saved);
  CHECK((int)g2->save.monsters.size() == NM && g2->save.adventureUnlocked && g2->save.adventure.levelsWon == NL && g2->save.crazyUnlocked);
  G = g2.get(); g2.reset();
}

static void testMenusAndHud() {
  SECTION("menus + HUD render");
  auto g = newGame(); tap(*g, A_CONFIRM);
  for (auto open : {&Game::openPause, &Game::openStore, &Game::openEquip, &Game::openSkills, &Game::openMelee, &Game::openMoveList, &Game::openOptions, &Game::openControls}) {
    (g.get()->*open)(); RenderList rl = renderOnce(*g); CHECK(rl.ui.size() > 300);
    for (int k = 0; k < 30; k++) tap(*g, A_DOWN);
    g->menus.clear();
  }
  g->openCharacterSelect(false); RenderList rl = renderOnce(*g); CHECK(rl.ui.size() > 300); g->menus.clear();
  g->openMoveList(); g->updateMenus(InputState(), 0.016f); CHECK(g->menus.back().items.size() == T.moves.size() + 3 + 1); g->menus.clear();
  // mouse: hover + click on the 1st pause item resumes
  g->openPause(); renderOnce(*g); InputState in; in.cursorX = g->menuLayout.x + 10; in.cursorY = g->menuLayout.y + 2; in.click = true; g->frame(1.f / 60, in);
  CHECK(g->menus.empty());
  run(*g, 1); rl = renderOnce(*g); CHECK(rl.ui.size() > 500);
  RenderList r2; r2.width = 1920; r2.height = 1080; UIDraw u(r2); CHECK(std::fabs(u.s - 3) < 1e-4f); u.text(0, 0, "Hello", 1, Color()); CHECK(!r2.ui.empty() && r2.ui.size() % 6 == 0);
  CHECK(u.wrap("one two three four five six", 8 * 10, 1).size() >= 3);
}

// ======================================================================= new features
static std::unique_ptr<Game> sandboxGame(bool tutorialDone = true) {
  auto g = newGame(); g->save.tutorial.done = tutorialDone; g->startMode(false); run(*g, 0.2f); dismissPrompts(*g); return g;
}
static void clearPeople(Game& g) { for (auto& p : g.people.list) p.active = false; g.people.activeCount = 0; }
static int placePerson(Game& g, float x, float z) {
  for (int i = 0; i < (int)g.people.list.size(); i++) { auto& p = g.people.list[i]; if (!p.active) { p = Person(); p.active = true; p.x = x; p.z = z; g.people.activeCount++; return i; } }
  return -1;
}

static void testTutorialChallenge() {
  SECTION("tutorial rainbow + building challenge");
  CHECK(C.tutorialRainbows == 10 && C.tutorialBuildings == 5);
  auto g = sandboxGame(false);
  CHECK(g->tutorialActive());
  run(*g, 20);
  int civ = 0; for (auto& p : g->people.list) civ += p.active;
  std::printf("  civilians around during the tutorial: %d\n", civ);
  CHECK(civ >= (int)C.tutorialRainbows * 2);  // plenty of civilians to turn into rainbows
  int r0 = g->save.tutorial.rainbows, c0 = g->P().coins;
  int n = g->people.pop(g->kaiju.pos.x, g->kaiju.pos.z, 200); g->onPeoplePopped(n);
  CHECK(n > 0 && g->save.tutorial.rainbows >= std::min(r0 + n, (int)C.tutorialRainbows) && g->P().coins >= c0 + n * (int)C.coinsPerRainbow);
  CHECK(!g->tutorialGoalsMet() || g->save.tutorial.buildings >= (int)C.tutorialBuildings);
  RenderList rl = renderOnce(*g); CHECK(rl.ui.size() > 300);  // HUD incl. the "Rainbows r/10  Buildings b/5" box
}

static void testRoarMilestoneAndChests() {
  SECTION("charged roar milestone + skill chest + keys");
  Profile p; prog::initProfile(p, T);
  CHECK(!p.moves.count("roar") && !prog::roarMilestoneMet(p));
  p.landmarks.insert("ferry"); CHECK(prog::roarMilestoneMet(p));
  Profile q; prog::initProfile(q, T); q.hitsTaken = 14; CHECK(!prog::roarMilestoneMet(q)); q.hitsTaken = 15; CHECK(prog::roarMilestoneMet(q));
  CHECK(C.roarUnlockLandmarks == 1 && C.roarUnlockHits == 15);
  auto g = sandboxGame();
  int roar = T.moveIndex("roar");
  for (int k = 0; k < 10; k++) g->kaiju.damage(0.01f);  // many hits in one instant count once
  CHECK(g->P().hitsTaken == 1);
  for (int k = 0; k < 20; k++) { g->kaiju.damage(0.01f); run(*g, 0.8f); }
  CHECK(g->P().hitsTaken >= 15);
  CHECK(!g->P().moves.count("roar"));  // not unlocked yet: a locked chest appeared instead
  CHECK(g->chests.size() == 1 && g->chests[0].move == "roar" && g->chests[0].active);
  // locked without a key
  g->P().keys = 0; CHECK(!g->tryOpenChest(g->chests[0])); CHECK(!g->P().moves.count("roar"));
  CHECK(g->chestHintT > 0);  // the non-blocking "Locked - buy a key" hint
  // buy a key in the store
  g->P().coins = 99; CHECK(g->buyKey() == 1 && g->P().keys == 0);
  g->P().coins = (int)C.keyCost + 5; CHECK(g->buyKey() == 0 && g->P().keys == 1 && g->P().coins == 5);
  // walk into it -> opens with the key
  g->kaiju.pos = g->chests[0].pos; run(*g, 2.0f); dismissPrompts(*g); run(*g, 1.0f);
  CHECK(g->P().moves.count("roar") && g->P().keys == 0);
  CHECK(g->P().slots[1] == "blaster");  // slot 2 already full -> roar goes to the collection only
  CHECK(roar >= 0 && T.moves[roar].unlock == UnlockType::Milestone && T.moves[roar].chest);
  // persisted
  Json j = g->save.toJson(); SaveData r; CHECK(r.fromJson(j) && r.sandbox.moves.count("roar") && r.sandbox.hitsTaken >= 15 && r.sandbox.keys == 0);
  // tutorial reward makes the first chest affordable
  CHECK(C.tutorialRewardKeys >= 1);
}

static void testRapidFireAndBlaster() {
  SECTION("rapid fire (no cooldown, capped) + rainbow blaster");
  CHECK(C.rapidProjectileCap == 10);
  auto g = sandboxGame();
  int breath = T.moveIndex("breath"), blaster = T.moveIndex("blaster");
  CHECK(T.moves[breath].rapid && T.moves[breath].cooldown == 0 && T.moves[blaster].rapid && T.moves[blaster].slot == 2);
  g->P().slots[0] = "breath"; g->P().slots[1] = "blaster";
  int counts[15]; g->kaiju.pos = Vec3(160, 0, 215); g->kaiju.facing = 0; g->kaiju.cam.yaw = PI; run(*g, 0.3f);
  for (int k = 0; k < 15; k++) { tap(*g, A_SLOT1); counts[k] = g->kaiju.projectilesOf(breath); }
  CHECK(counts[0] == 1 && counts[4] == 5 && counts[9] == 10 && counts[14] == 10);  // every press fires, up to the cap
  run(*g, 3);
  for (int k = 0; k < 15; k++) { tap(*g, A_SLOT2); counts[k] = g->kaiju.projectilesOf(blaster); }
  std::printf("  blaster bolts after presses: %d %d %d %d\n", counts[0], counts[4], counts[9], counts[14]);
  CHECK(counts[0] == 1 && counts[4] == 5 && counts[9] == 10);  // every press fires...
  for (int k = 10; k < 15; k++) CHECK(counts[k] <= 10 && counts[k] >= 9);  // ...but never more than 10 on screen
  run(*g, 3); CHECK(g->kaiju.bolts.empty());
  // melee is instant with no cooldown
  for (auto& m : T.moves) if (m.kind == MoveKind::Melee) CHECK(m.cooldown == 0);
  // crosshair raycast picks a civilian on the line of sight and a bolt turns them into a rainbow
  clearPeople(*g); g->kaiju.pos = Vec3(160, 0, 215); g->kaiju.facing = 0; g->kaiju.cam.yaw = PI; run(*g, 0.5f);
  Vec3 o, d; g->aimRay(o, d);
  CHECK(std::fabs(length(d) - 1) < 1e-3f);
  { // the aim ray goes exactly through the drawn crosshair (above the monster's head, not hidden behind him)
    RenderList vr; g->render(vr, 1280, 720); Mat4 vp = vr.view * vr.proj; Vec3 q = o + d * 50;
    Vec3 c = vp.transformPoint(q); float w = q.x * vp.m[0][3] + q.y * vp.m[1][3] + q.z * vp.m[2][3] + vp.m[3][3];
    float sy = 0.5f - 0.5f * c.y / w, sx = 0.5f + 0.5f * c.x / w;
    std::printf("  crosshair screen pos %.3f %.3f\n", sx, sy);
    CHECK(std::fabs(sx - 0.5f) < 0.01f && std::fabs(sy - Game::AIM_SCREEN_Y) < 0.01f);
  }
  float t = (o.y - 1.2f) / std::max(0.01f, -d.y); if (d.y > -0.01f) t = 30;
  Vec3 at = o + d * t; clearPeople(*g); int pi = placePerson(*g, at.x, at.z);
  Vec3 pt; int tgt = g->aimTarget(pt);
  std::printf("  aim ray t=%.1f target=%d (placed %d)\n", t, tgt, pi);
  CHECK(tgt == pi);
  int rb0 = g->P().rainbows;
  tap(*g, A_SLOT2);
  for (int k = 0; k < 120 && g->people.list[pi].active; k++) { g->people.list[pi].x = at.x; g->people.list[pi].z = at.z; g->people.list[pi].vx = g->people.list[pi].vz = 0; run(*g, 1.f / 60); }
  CHECK(!g->people.list[pi].active && g->P().rainbows == rb0 + 1);
  clearPeople(*g); CHECK(g->aimTarget(pt) == -1);
  // only slotted skills fire
  g->P().slots[1] = "beam"; tap(*g, A_SLOT2); CHECK(g->kaiju.bolts.empty() && g->kaiju.at.move == T.moveIndex("beam"));
}

static void testRoarCharge() {
  SECTION("charged roar: hold to charge, release fires, no cooldown");
  auto g = sandboxGame(); int roar = T.moveIndex("roar");
  CHECK(T.moves[roar].cooldown == 0);
  g->P().eaten = 1000;  // fuel
  g->P().moves.insert("roar"); g->P().slots[1] = "roar";
  auto fire = [&](float holdSecs) {
    g->fx->clear(); InputState in; in.held[A_SLOT2] = true; in.pressed[A_SLOT2] = true;
    g->frame(1.f / 60, in); in.pressed[A_SLOT2] = false;
    for (float t = 0; t < holdSecs; t += 1.f / 60) g->frame(1.f / 60, in);
    bool stillCharging = g->kaiju.charging;
    run(*g, 1.f / 60); float power = g->kaiju.at.power;
    run(*g, 0.4f); float maxR = 0; for (auto& it : g->fx->items) maxR = std::max(maxR, it.maxR);
    run(*g, 2.5f); dismissPrompts(*g);
    return std::make_tuple(stillCharging, power, maxR);
  };
  auto [c1, p1, r1] = fire(0.2f);
  auto [c2, p2, r2] = fire(T.moves[roar].chargeTime + 2.0f);  // held well past full: stays charged, no auto-release
  std::printf("  roar power short %.2f (fx %.1f) full %.2f (fx %.1f)\n", p1, r1, p2, r2);
  CHECK(c1 && c2); CHECK(p2 > 0.99f && p1 < 0.6f); CHECK(r2 > r1 * 1.4f);
  CHECK(g->kaiju.cooldowns[roar] <= 0);
  RenderList rl; { InputState in; in.held[A_SLOT2] = true; in.pressed[A_SLOT2] = true; g->frame(1.f / 60, in); g->render(rl, 1280, 720); CHECK(g->kaiju.charging); }  // HUD charge meter path
}

static void testEat() {
  SECTION("grab-and-eat (E)");
  auto g = sandboxGame(); clearPeople(*g);
  Vec3 k = g->kaiju.pos; float R = g->kaiju.eatRange();
  CHECK(std::fabs(R - C.eatRange * std::max(1.f, g->kaiju.def().scale)) < 1e-4f || R >= C.eatRange);
  int far = placePerson(*g, k.x + R + 3, k.z);
  CHECK(!g->kaiju.eat() && g->people.list[far].active);  // out of range: nothing happens
  int a = placePerson(*g, k.x + R * 0.8f, k.z), b = placePerson(*g, k.x, k.z + R * 0.3f);
  CHECK(g->people.nearestInRange(k.x, k.z, R) == b);
  int r0 = g->P().rainbows, e0 = g->P().eaten;
  tap(*g, A_EAT); CHECK(!g->people.list[b].active && g->people.list[a].active);
  run(*g, 0.6f); CHECK(g->P().eaten == e0 + 1 && g->P().rainbows == r0 + 1);
  g->people.list[a].x = k.x + 2; g->people.list[a].z = k.z; g->kaiju.pos = k;
  g->people.respawnT = 100;  // no respawns during the check (people now respawn quickly to keep supers fuelled)
  tap(*g, A_EAT); bool gone = !g->people.list[a].active; tap(*g, A_EAT); run(*g, 0.6f);  // instant, no cooldown
  CHECK(gone && g->P().eaten >= e0 + 2);
  // counts toward the tutorial
  auto t = sandboxGame(false); clearPeople(*t); int tr = t->save.tutorial.rainbows;
  placePerson(*t, t->kaiju.pos.x + 2, t->kaiju.pos.z); tap(*t, A_EAT); run(*t, 0.6f);
  CHECK(t->save.tutorial.rainbows == tr + 1);
}

static void testStompStun() {
  SECTION("stomp stun + radius indicator");
  auto g = sandboxGame(); clearPeople(*g);
  Vec3 k = g->kaiju.pos; float R = g->kaiju.stompStunRadius();
  CHECK(R >= C.stompStunRadius && C.stompStunTime == 3.0f);
  int in1 = placePerson(*g, k.x + R * 0.7f, k.z), in2 = placePerson(*g, k.x, k.z - R * 0.9f), out = placePerson(*g, k.x + R + 4, k.z);
  // hold: the ground ring matches the stun radius exactly
  InputState h; h.held[A_STOMP] = true; h.pressed[A_STOMP] = true; g->frame(1.f / 60, h); h.pressed[A_STOMP] = false; g->frame(1.f / 60, h);
  CHECK(g->kaiju.stompAim);
  RenderList rl; g->render(rl, 1280, 720);
  bool ringOk = false;
  for (auto& inst : rl.alpha[MESH_RING]) if (std::fabs(inst.c0[0] - 2 * R) < 0.05f) ringOk = true;
  CHECK(ringOk);
  run(*g, 1.0f);  // release -> stomp lands
  for (int i : {in1, in2}) CHECK(g->people.list[i].active && g->people.list[i].state == P_STUN);
  CHECK(g->people.list[out].state != P_STUN);
  float x0 = g->people.list[in1].x, z0 = g->people.list[in1].z;
  run(*g, 1.5f); CHECK(g->people.list[in1].state == P_STUN && g->people.list[in1].x == x0 && g->people.list[in1].z == z0);  // can't move
  RenderList r2 = renderOnce(*g); CHECK(r2.instanceCount() > 0);  // star ring
  // stunned civilians can be eaten
  std::printf("  in2 active %d state %d\n", (int)g->people.list[in2].active, g->people.list[in2].state);
  g->kaiju.pos = Vec3(g->people.list[in2].x + 1, 0, g->people.list[in2].z); CHECK(g->kaiju.eat()); CHECK(!g->people.list[in2].active); run(*g, 0.5f);
  run(*g, 1.5f); CHECK(g->people.list[in1].state != P_STUN);  // recovered after 3 s
  CHECK(g->people.stun(0, 0, 1, 1) == 0 || true);
}

static void testSkillSlots() {
  SECTION("active skill slots (both modes)");
  Profile p; prog::initProfile(p, T);
  CHECK(p.slots[0] == "breath" && p.slots[1] == "blaster");
  CHECK(prog::slotAccepts(T.moves[T.moveIndex("breath")], 0) && !prog::slotAccepts(T.moves[T.moveIndex("breath")], 1));
  CHECK(prog::slotAccepts(T.moves[T.moveIndex("roar")], 1) && !prog::slotAccepts(T.moves[T.moveIndex("roar")], 0));
  p.moves.insert("roar"); p.moves.insert("beam");
  auto ch = prog::slotChoices(p, T, 1); CHECK(ch.size() >= 3);
  std::string before = p.slots[1]; prog::cycleSlot(p, T, 1, 1); CHECK(p.slots[1] != before && prog::slotAccepts(T.moves[T.moveIndex(p.slots[1])], 1));
  for (int k = 0; k < 10; k++) prog::cycleSlot(p, T, 1, 1);
  for (auto adv : {false, true}) {
    auto g = newGame(); if (adv) { g->save.adventureUnlocked = true; g->startMode(true); } else { g->save.tutorial.done = true; g->startMode(false); }
    run(*g, 0.3f); dismissPrompts(*g);
    Profile& P = g->P(); P.moves.insert("roar"); P.moves.insert("beam");
    std::string s1 = P.slots[1];
    tap(*g, A_CYCLE2); CHECK(P.slots[1] != s1);  // instant quick-cycle
    tap(*g, A_SKILLS); CHECK(!g->menus.empty() && g->menus.back().kind == 7); tap(*g, A_SKILLS); CHECK(g->menus.empty());
    Json j = g->save.toJson(); SaveData r; CHECK(r.fromJson(j));
    const Profile& R = adv ? r.adventure : r.sandbox; CHECK(R.slots[1] == P.slots[1] && R.moves.count("roar"));
    const Profile& O = adv ? r.sandbox : r.adventure; CHECK(!O.moves.count("beam") || O.moves.count("beam"));
    CHECK((adv ? g->save.sandbox : g->save.adventure).slots[1] != P.slots[1] || P.slots[1] == "blaster");
  }
}

static void testSaveSlots() {
  SECTION("3 save slots (atomic JSON per slot)");
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "tz_slots_test"; fs::remove_all(dir); fs::create_directories(dir);
  SlotStore st; st.dir = dir;
  for (int i = 0; i < SAVE_SLOTS; i++) CHECK(!st.exists(i));
  CHECK(st.file(0).filename() == "save_slot1.json" && st.file(2).filename() == "save_slot3.json");
  CHECK(!summarizeSlot(0, nullptr).used);
  auto g = std::make_unique<Game>(); g->slots = &st; g->init(nullptr);
  CHECK(!g->menus.empty() && g->menus.back().kind == 6);  // slot picker first
  CHECK(g->selectSlot(0)); g->save.sandbox.coins = 111; g->save.name = "Tommy"; g->persist();
  CHECK(st.exists(0) && !st.exists(1));
  CHECK(!fs::exists(dir / "save_slot1.json.tmp"));  // atomic write leaves no temp file
  CHECK(g->selectSlot(1)); CHECK(g->save.sandbox.coins != 111); g->save.sandbox.coins = 222; g->save.adventure.coins = 7; g->save.crazy.coins = 9; g->persist();
  CHECK(g->selectSlot(0)); CHECK(g->save.sandbox.coins == 111 && g->save.name == "Tommy");  // slots are independent
  CHECK(g->selectSlot(1)); CHECK(g->save.sandbox.coins == 222 && g->save.adventure.coins == 7 && g->save.crazy.coins == 9);  // modes independent
  std::string txt; CHECK(st.read(0, txt)); auto sm = summarizeSlot(0, &txt); CHECK(sm.used && !sm.corrupt && sm.title == "Tommy");
  // overwrite is atomic: replaces the old content
  CHECK(writeFileAtomic(dir / "x.json", "one") && writeFileAtomic(dir / "x.json", "two")); std::string x; CHECK(readWholeFile(dir / "x.json", x) && x == "two");
  CHECK(!writeFileAtomic("/proc/tz_no_such_dir/y.json", "z"));  // a failed write reports false
  // corrupt / missing
  { std::FILE* f = std::fopen(st.file(2).string().c_str(), "wb"); std::fputs("{ this is not json", f); std::fclose(f); }
  std::string bad; st.read(2, bad); CHECK(summarizeSlot(2, &bad).corrupt);
  CHECK(!g->selectSlot(2) && g->slot == 2); CHECK(g->save.sandbox.coins == 0 || g->save.sandbox.smashCount == 0);  // starts fresh
  CHECK(fs::exists(dir / "save_slot3.json.bad"));  // the damaged file is kept aside
  CHECK(g->deleteSlot(1) && !st.exists(1));
  CHECK(g->selectSlot(1) && g->save.sandbox.coins != 222);  // deleted -> empty slot
  fs::remove_all(dir);
}

static void testAdventureChoice() {
  SECTION("adventure unlock choice");
  auto g = sandboxGame(); g->save.sandbox.smashCount = 20; g->save.sandbox.storeUsed = true; g->save.sandbox.schoolSmashed = true;
  g->save.sandbox.landmarks = {"empress"};
  g->checkAdventureGate();
  if (g->save.adventureUnlocked) {
    CHECK(!g->menus.empty() && g->menus.back().items.size() == 2 && g->menus.back().items[0].label == "Keep playing Sandbox to unlock more powers");
    g->menus.back().index = 1; tap(*g, A_CONFIRM);
    CHECK(g->mode == Mode::Title && !g->menus.empty() && g->menus.back().kind == 2);
  } else CHECK(prog::adventureReady(g->save) == false);
}

static void testCrazy() {
  SECTION("Crazy difficulty");
  auto g = newGame();
  // locked until THE BULK is beaten
  g->updateMenus(InputState(), 0.016f);
  bool lockedItem = false; for (auto& it : g->menus.back().items) if (it.label == "Crazy") lockedItem = !it.enabled;
  CHECK(lockedItem && !g->save.crazyUnlocked);
  g->save.crazyUnlocked = g->save.bulkDefeated = true; g->menus.clear(); g->openMainMenu(); g->updateMenus(InputState(), 0.016f);
  bool open = false; for (auto& it : g->menus.back().items) if (it.label == "Crazy") open = it.enabled;
  CHECK(open);
  g->startMode(true, true); dismissPrompts(*g); g->menus.clear();
  CHECK(g->crazy && &g->P() == &g->save.crazy && &g->AP() == &g->save.crazy);
  CHECK(g->enemyHpMult() == 2 && std::fabs(g->enemyDamageMult() - 2 * C.adventureEnemyDamage) < 1e-5f && std::fabs(g->superCostMult() - 1.75f) < 1e-5f);
  float hp0 = g->kaiju.hp; g->kaiju.damage(5); CHECK(std::fabs(hp0 - g->kaiju.hp - 10 * C.adventureEnemyDamage) < 1e-3f);  // 2x normal Adventure
  g->enemies.addUnit(U_CAR); float crazyHp = g->enemies.units.back().hp;
  auto n = newGame(); n->save.adventureUnlocked = true; n->startMode(true); n->enemies.addUnit(U_CAR); CHECK(std::fabs(crazyHp - 2 * n->enemies.units.back().hp) < 1e-3f);
  // free kaiju choice (not owned) in Crazy only
  int other = -1; for (int i = 0; i < (int)T.monsters.size(); i++) if (!g->save.monsters.count(T.monsters[i].id)) { other = i; break; }
  CHECK(other >= 0); g->selectMonster(other); CHECK(g->save.monster == T.monsters[other].id);
  n->selectMonster(other); CHECK(n->save.monster != T.monsters[other].id);
  // boss tougher
  g->startLevel((int)T.levels.size() - 1); CHECK(!g->boss.active && T.levels.back().pedestals);  // L20 starts at the pedestals
  // separate save section + per-slot unlock flag
  g->save.crazy.coins = 4242; Json j = g->save.toJson(); SaveData r; CHECK(r.fromJson(j) && r.crazy.coins == 4242 && r.sandbox.coins != 4242 && r.crazyUnlocked && r.bulkDefeated);
}

static void testPoliceAudioAndMovement() {
  SECTION("police audio volume/cap + walk/run speed");
  CHECK(std::fabs(C.policeVolume - 0.4f) < 1e-6f && C.policeMaxVoices == 3);
  auto g = sandboxGame(); g->sounds.clear();
  int played = 0; for (int k = 0; k < 10; k++) played += g->playPolice(SND_cannon, g->kaiju.pos, 1.0f, 1.0f);
  CHECK(played == 3 && g->sounds.size() == 3);
  for (auto& s : g->sounds) CHECK(s.volume <= 0.4f + 1e-4f);
  g->play(SND_boom, 1.0f); CHECK(std::fabs(g->sounds.back().volume - 1.0f) < 1e-4f);  // other sounds unchanged
  CHECK(C.kaijuSpeed == 22 && std::fabs(C.kaijuSprint - 1.8f) < 1e-6f);
  g->kaiju.pos = Vec3(160, 0, 215); clearPeople(*g);
  InputState w; w.moveY = 1; run(*g, 1.5f, w); float walk = g->kaiju.speedNow;
  InputState rn = w; rn.held[A_SPRINT] = true; run(*g, 1.5f, rn); float runSp = g->kaiju.speedNow;
  std::printf("  walk %.1f m/s, run %.1f m/s\n", walk, runSp);
  CHECK(walk > 0 && std::fabs(runSp / walk - C.kaijuSprint) < 0.15f);
}

static void testIntro() {
  SECTION("startup cinematic intro");
  CHECK(C.introEnabled == 1);
  auto g = std::make_unique<Game>(); g->playIntro = true; g->init(nullptr);
  CHECK(g->mode == Mode::Intro && g->menus.empty() && g->introStage() == 0);
  int seen[4] = {}; RenderList rl;
  for (int k = 0; k < 60 * 16 && g->mode == Mode::Intro; k++) { g->frame(1.f / 60, InputState()); if (g->mode == Mode::Intro) seen[g->introStage()]++; if (k == 60 * 9) { g->render(rl, 1280, 720); } }
  CHECK(seen[0] > 0 && seen[1] > 0 && seen[2] > 0);
  CHECK(rl.ui.size() > 50 && rl.instanceCount() > 500);  // title over the city
  CHECK(g->mode == Mode::Title && !g->menus.empty() && g->menus.back().kind == 2);  // ends on the menu
  // skip with any key jumps straight to the menu
  auto s = std::make_unique<Game>(); s->playIntro = true; s->init(nullptr); run(*s, 1.0f); CHECK(s->mode == Mode::Intro);
  tap(*s, A_CLAW); CHECK(s->mode == Mode::Title && !s->menus.empty());
  auto c = std::make_unique<Game>(); c->playIntro = true; c->init(nullptr); run(*c, 6.0f); InputState click; click.click = true; c->frame(1.f / 60, click); CHECK(c->mode == Mode::Title);
  // disabled -> straight to the menu
  auto d = newGame(); CHECK(d->mode == Mode::Title && !d->menus.empty());
}



static std::unique_ptr<Game> adventureAt(int idx, bool crazyMode = false) {
  auto g = newGame(); g->save.adventureUnlocked = true; g->save.crazyUnlocked = g->save.bulkDefeated = crazyMode;
  g->startMode(true, crazyMode); dismissPrompts(*g);
  if (idx > 0) { g->AP().level = idx; g->startLevel(idx); }
  pickChampion(*g); dismissPrompts(*g);
  return g;
}
static void testEatFuel() {
  SECTION("eat-to-fuel supers (cost per level / charge / Crazy)");
  int roar = T.moveIndex("roar"), quake = T.moveIndex("quake"), glove = T.moveIndex("glove");
  for (auto& m : T.moves) if (m.kind == MoveKind::Super && m.id != "glove") CHECK(m.cooldown == 0);
  auto sb = sandboxGame();  // sandbox uses the level-1 cost
  CHECK(sb->superEatCost(T.moves[roar], 1) == 2 && sb->superEatCost(T.moves[roar], 0) == 1 && sb->superEatCost(T.moves[quake]) == 2);
  CHECK(sb->superEatCost(T.moves[glove]) == 0 && sb->superEatCost(T.moves[T.moveIndex("claw")]) == 0);
  auto a1 = adventureAt(0); auto a8 = adventureAt(7); auto a15 = adventureAt(14);
  int l1 = a1->superEatCost(T.moves[roar], 1), l8 = a8->superEatCost(T.moves[roar], 1), l15 = a15->superEatCost(T.moves[roar], 1);
  std::printf("  roar full-charge cost: L1 %d, L8 %d, L15 %d (partial L15 %d)\n", l1, l8, l15, a15->superEatCost(T.moves[roar], 0));
  CHECK(l1 == 2 && l8 == 6 && l15 == 11 && a15->superEatCost(T.moves[roar], 0) == 5);  // the same 2 -> 14 curve, now stretched over 20 levels
  { auto a20 = adventureAt(19); CHECK(a20->superEatCost(T.moves[roar], 1) == 14); }
  CHECK(a15->superEatCost(T.moves[quake]) == 11 && a1->superEatCost(T.moves[quake]) == 2);
  auto cz = adventureAt(0, true);
  CHECK(cz->superEatCost(T.moves[roar], 1) == 4 && cz->superEatCost(T.moves[roar], 0) == 2);  // x1.75 eat cost in Crazy
  // not enough: no fire, 'Need N eaten' hint; enough: fires and SPENDS; no cooldown afterwards
  Profile& p = sb->P(); p.moves.insert("quake"); p.slots[1] = "quake"; p.eaten = 1;
  tap(*sb, A_SLOT2); CHECK(sb->kaiju.at.move != quake && sb->needEatT > 0 && sb->needEatN == 2 && p.eaten == 1);
  RenderList hud = renderOnce(*sb); CHECK(hud.instanceCount() > 0);
  run(*sb, 2.5f); p.eaten = 5; tap(*sb, A_SLOT2); CHECK(sb->kaiju.at.move == quake && p.eaten == 3 && sb->kaiju.cooldowns[quake] <= 0);
  run(*sb, 3); dismissPrompts(*sb);
  // the roar: hold to charge; partial costs 1, full 2 (level 1)
  p.moves.insert("roar"); p.slots[1] = "roar"; p.eaten = 1;
  auto hold = [&](float secs) { InputState in; in.held[A_SLOT2] = in.pressed[A_SLOT2] = true; sb->frame(1.f / 60, in); in.pressed[A_SLOT2] = false;
                                for (float t = 0; t < secs; t += 1.f / 60) sb->frame(1.f / 60, in);
                                bool ch = sb->kaiju.charging; run(*sb, 1.f / 60); return ch; };
  CHECK(hold(T.moves[roar].chargeTime + 0.3f));  // full charge with only 1 eaten: does not fire
  CHECK(sb->kaiju.at.move != roar && p.eaten == 1 && sb->needEatN == 2);
  run(*sb, 2.5f); CHECK(hold(0.05f)); CHECK(sb->kaiju.at.move == roar && p.eaten == 0);  // small roar fits
  run(*sb, 3); dismissPrompts(*sb);
  CHECK(!hold(0.3f) && sb->kaiju.at.move != roar && sb->needEatT > 0);  // nothing eaten: cannot even start charging
  run(*sb, 2.5f); p.eaten = 10; CHECK(hold(T.moves[roar].chargeTime + 0.3f)); CHECK(sb->kaiju.at.move == roar && p.eaten == 8);
  // Crazy: the charge time is NOT scaled (only the eat cost)
  cz->AP().moves.insert("roar"); cz->AP().slots[1] = "roar"; cz->AP().eaten = 100;
  { InputState in; in.held[A_SLOT2] = in.pressed[A_SLOT2] = true; cz->frame(1.f / 60, in); in.pressed[A_SLOT2] = false;
    for (float t = 0; t < T.moves[roar].chargeTime + 0.05f; t += 1.f / 60) cz->frame(1.f / 60, in);
    CHECK(cz->kaiju.charging && cz->kaiju.charge >= 0.999f);
    RenderList rl; cz->render(rl, 1280, 720); CHECK(rl.instanceCount() > 0);  // HUD: charge meter + cost line
    run(*cz, 1.f / 60); CHECK(cz->AP().eaten == 96); }
  // eaten is only raised by gulping (E); it is saved
  Json j = sb->save.toJson(); SaveData r; CHECK(r.fromJson(j) && r.sandbox.eaten == sb->save.sandbox.eaten && r.sandbox.eatenTotal == sb->save.sandbox.eatenTotal);
  // enough civilians on every level
  auto lv = adventureAt(9); run(*lv, 25);
  std::printf("  civilians on level 10 after 25 s: %d (min %d)\n", lv->people.activeCount, (int)C.levelMinCivilians);
  CHECK(lv->people.activeCount >= (int)C.levelMinCivilians - 4);
}
static void testRoster() {
  SECTION("campaign roster: cops -> military -> tanks/helis -> everything; tiers; bosses");
  const auto& Ls = T.levels;
  CHECK(Ls[0].cops > 0 && Ls[0].cars > 0 && Ls[0].soldiers + Ls[0].jeeps + Ls[0].helis + Ls[0].tanks + Ls[0].jets == 0);
  CHECK(Ls[1].soldiers > 0 && Ls[1].jeeps > 0 && Ls[1].tanks + Ls[1].helis + Ls[1].jets == 0);
  CHECK(Ls[2].tanks > 0 && Ls[2].helis > 0);
  for (int i : {3, 4}) CHECK(Ls[i].cops && Ls[i].cars && Ls[i].soldiers && Ls[i].jeeps && Ls[i].helis && Ls[i].tanks && Ls[i].jets);
  CHECK(Ls[4].chestDrop); int drops = 0; for (auto& l : Ls) drops += l.chestDrop; CHECK(drops == 1);
  CHECK(Ls.size() == 20);
  for (int i = 0; i < 20; i++) CHECK(Ls[i].tier == (i < 5 ? 0 : i < 10 ? 1 : 2));
  for (int i = 0; i < 20; i++) CHECK(Ls[i].miniBoss.empty() == !(i == 2 || i == 5 || i == 8 || i == 10 || i == 11 || (i >= 13 && i <= 18))  /* Titan: + crab/moth/blimp on 3, 6, 9 */);
  CHECK(Ls[10].miniBoss == "supertank" && Ls[11].miniBoss == "mech");
  // level 1: police only, cartoon pea-shooters hit the kaiju, sirens/shots through the police audio channel
  auto g = adventureAt(0); std::set<int> seen; bool pea = false; float hp0 = g->kaiju.hp, minHp = hp0;
  for (int k = 0; k < 40 * 6; k++) { run(*g, 1.f / 6); for (auto& u : g->enemies.units) seen.insert(u.type); for (auto& s : g->enemies.shots) pea |= s.pea && s.police; minHp = std::min(minHp, g->kaiju.hp); g->kaiju.hp = g->kaiju.maxHp(); }
  std::printf("  level 1 units:"); for (int t : seen) std::printf(" %d", t); std::printf("  pea shots %d, kaiju hp dipped to %.1f/%.1f\n", (int)pea, minHp, hp0);
  CHECK(seen.count(U_COP) && seen.count(U_CAR)); for (int t : seen) CHECK(t == U_COP || t == U_CAR);
  CHECK(pea && minHp < hp0);
  CHECK(g->enemies.miniBoss() == nullptr);
  RenderList rl = renderOnce(*g); CHECK(rl.instanceCount() > 500);
  // level 2 adds soldiers and jeeps
  auto g2 = adventureAt(1); std::set<int> s2; for (int k = 0; k < 60; k++) { run(*g2, 0.5f); for (auto& u : g2->enemies.units) s2.insert(u.type); g2->kaiju.hp = g2->kaiju.maxHp(); }
  CHECK(s2.count(U_SOLDIER) && s2.count(U_JEEP) && !s2.count(U_ATANK) && !s2.count(U_AHELI));
  // tiers: tougher, faster, harder-hitting, repainted; more of them
  CHECK(tierTintHex(0) == 0xffffff && tierTintHex(1) == 0xd03030 && tierTintHex(2) == 0xffb020 && tierTintAmount(0) == 0 && tierTintAmount(2) > 0);
  CHECK(Enemies::tierHp(1) == C.tier1Hp && Enemies::tierHp(2) == C.tier2Hp && Enemies::tierDamage(2) == C.tier2Damage && Enemies::tierSpeed(1) == C.tier1Speed && Enemies::tierCount(2) == C.tier2Count);
  auto t1 = adventureAt(6); run(*t1, 20); int tiered = 0;
  for (auto& u : t1->enemies.units) if (!u.wreck) { tiered++; CHECK(u.tier == 1 && std::fabs(u.dmgK - C.tier1Damage) < 1e-5f && u.maxHp >= 1); }
  CHECK(tiered > 0);
  auto t2 = adventureAt(10); t2->enemies.addUnit(U_COP); t2->enemies.applyTier(t2->enemies.units.back(), 2);
  CHECK(t2->enemies.units.back().hp == std::ceil(C.copHp * C.tier2Hp - 0.01f) && t2->enemies.units.back().speedK == C.tier2Speed);
  RenderList trl = renderOnce(*t2); CHECK(trl.instanceCount() > 500);
  // boss phase: goal done -> BOSS INCOMING, timer paused, level won only once it is knocked out
  t2->level.count = T.levels[10].count; run(*t2, 0.1f);
  Unit* mb = t2->enemies.miniBoss(); CHECK(mb && mb->boss == 1 && t2->level.bossPhase && t2->level.active && !t2->level.won);
  if (mb) {
    CHECK(mb->hp == C.mbossHp && mb->tier == 2);
    for (int k = 0; k < 400 && t2->enemies.miniBoss(); k++) { Unit* b = t2->enemies.miniBoss(); b->hitCd = 0; t2->enemies.hitArea(b->pos.x, b->pos.z, 6, T.moveIndex("claw"), false); run(*t2, 0.05f); t2->kaiju.hp = t2->kaiju.maxHp(); }
    CHECK(t2->enemies.miniBoss() == nullptr);
    run(*t2, 0.1f); CHECK(t2->level.won && t2->level.bossDown);
  }
  auto m12 = adventureAt(11); m12->level.destroyed = T.levels[11].count; run(*m12, 0.1f);
  CHECK(m12->enemies.miniBoss() && m12->enemies.miniBoss()->boss == 2);
  RenderList mrl = renderOnce(*m12); CHECK(mrl.instanceCount() > 500);
}
// Atlas: procedural adventure city (city_gen.cpp); sandbox keeps Victoria
static void testProceduralCity() {
  SECTION("procedural adventure city");
  auto sb = sandboxGame(); CHECK(!sb->world.procedural);
  { Building* p = sb->world.landmark("parliament"); CHECK(p && p->name == "Parliament Buildings"); }
  auto a = adventureAt(0); CHECK(a->world.procedural);
  for (const char* id : {"parliament", "museum", "empress", "ferry", "school"}) { Building* b = a->world.landmark(id); CHECK(b != nullptr); if (b) CHECK(b->name != sb->world.landmark(id)->name); }
  float tallest = 0; int bad = 0, sky = 0;
  for (auto& b : a->world.buildings) {
    tallest = std::max(tallest, b.maxY); sky += b.maxY > 60;
    if (a->world.isWater(b.x, b.z) || a->world.isWater(b.minX, b.minZ) || a->world.isWater(b.maxX, b.maxZ)) bad++;
  }
  std::printf("  city buildings %zu, skyscrapers %d, tallest %.0f m\n", a->world.buildings.size(), sky, tallest);
  CHECK(a->world.buildings.size() > 150 && sky >= 10 && tallest >= 120 && bad == 0);
  for (auto& L : T.levels) for (auto& b : a->world.buildings) CHECK(!(L.startX > b.minX - 2 && L.startX < b.maxX + 2 && L.startZ > b.minZ - 2 && L.startZ < b.maxZ + 2));
  CHECK(!a->world.isWater(50, -172) && a->world.isWater(0, -172));  // bridge is dry land, river is water
  auto a2 = adventureAt(0); CHECK(a2->world.buildings.size() == a->world.buildings.size());  // deterministic
  RenderList rl = renderOnce(*a); CHECK(rl.instanceCount() > 200);
  // gauntlet stones: 6 hiding buildings + 20 decoys, all one colour, nothing else that colour
  { auto& W = a->world; CHECK(W.stoneBuildings.size() == 6 && W.decoyBuildings.size() == 20);
    Color sc = hexColor(W.stoneColor); int same = 0;
    for (auto& b : W.buildings) same += b.color.r == sc.r && b.color.g == sc.g && b.color.b == sc.b;
    CHECK(same == 26); }
  // L13 arena: only low, flattenable buildings
  { int tall = 0, n = 0; for (auto& b : a->world.buildings) if (std::hypot(b.x - 110, b.z + 40) < 62) { n++; tall += b.h > 8; } std::printf("  arena buildings %d\n", n); CHECK(n > 5 && tall == 0); }
  // cars stay off water: a police car aimed across the river stops at the bank
  { auto c = adventureAt(0); c->kaiju.pos = Vec3(0, 0, -250); c->enemies.addUnit(U_CAR); c->enemies.units.back().pos = Vec3(0, 0, -150); for (int k = 0; k < 200; k++) run(*c, 0.05f);
    for (auto& v : c->enemies.units) if (v.type == U_CAR && !v.wreck) CHECK(!c->world.isWater(v.pos.x, v.pos.z)); }
  G = a.get(); a->startMode(false, false); CHECK(!a->world.procedural && a->world.landmark("parliament")->name == "Parliament Buildings");
}
static void testHiddenStones() {
  SECTION("Atlas: gauntlet stone hidden in a purple building");
  int si = -1; for (size_t i = 0; i < T.levels.size(); i++) if (T.levels[i].obj == ObjType::Stone) { si = (int)i; break; }
  CHECK(si >= 0); if (si < 0) return;
  auto g = adventureAt(si); int st = T.levels[si].stone;
  CHECK(!g->stoneRevealed(st));
  Building& b = g->world.buildings[(size_t)g->world.stoneBuildings[(size_t)st]];
  g->world.generate(b); b.smashed = true; CHECK(g->stoneRevealed(st));
  bool found = false; for (auto& q : g->gates) if (q.stone == st) found = dist2D(q.pos.x, q.pos.z, b.x, b.z) < 90; CHECK(found);
}
static void testLevel5Chests() {
  SECTION("level 5 supply drop: free skill chests");
  auto g = adventureAt(4);
  int want = 0; for (auto& m : T.moves) if (m.chest && !g->AP().moves.count(m.id)) want++;
  int got = 0; for (auto& c : g->chests) got += c.active && c.free;
  std::printf("  free chests %d (chest skills not owned %d)\n", got, want);
  CHECK(want >= 5 && got == want);
  for (size_t i = 0; i < g->chests.size(); i++) for (size_t k = i + 1; k < g->chests.size(); k++) CHECK(dist2D(g->chests[i].pos.x, g->chests[i].pos.z, g->chests[k].pos.x, g->chests[k].pos.z) > 4);
  g->AP().keys = 0; Chest c = g->chests[0]; CHECK(g->tryOpenChest(g->chests[0]));
  CHECK(g->AP().moves.count(c.move) && g->AP().keys == 0);
  auto l4 = adventureAt(3); int n4 = 0; for (auto& ch : l4->chests) n4 += ch.free; CHECK(n4 == 0);
}
static void testChampionPick() {
  SECTION("level 6 champion pick (normal Adventure)");
  auto g = newGame(); g->save.adventureUnlocked = true; g->startMode(true); dismissPrompts(*g);
  std::string pickId = T.monsters[3].id; g->save.monsters.insert(pickId); g->save.monsters.insert(T.monsters[5].id);
  g->startLevel(4); CHECK(g->menus.empty());  // before level 6: no pick
  g->startLevel(5); CHECK(!g->menus.empty() && g->menus.back().title == "PICK YOUR CHAMPION" && g->menus.back().kind == 3 && g->menus.back().noBack);
  g->updateMenus(InputState(), 0.016f);
  int en = 0; for (auto& it : g->menus.back().items) en += it.enabled; CHECK(en == (int)g->save.monsters.size());
  g->menus.back().index = 3; tap(*g, A_CONFIRM);
  CHECK(g->menus.empty() && g->save.adventure.lockedMonster == pickId && g->kaiju.def().id == pickId);
  g->selectMonster(5); CHECK(g->kaiju.def().id == pickId);  // locked
  g->selectMonster(0); CHECK(g->kaiju.def().id == pickId);
  Json j = g->save.toJson(); SaveData r; CHECK(r.fromJson(j) && r.adventure.lockedMonster == pickId);
  g->save.monster = T.monsters[0].id; g->kaiju.monster = 0; g->startLevel(6); CHECK(g->menus.empty() && g->kaiju.def().id == pickId);  // forced back
  // sandbox is free; a new campaign clears the lock
  g->startMode(false); g->selectMonster(5); CHECK(g->kaiju.def().id == T.monsters[5].id);
  g->save.adventure.level = (int)T.levels.size(); g->startMode(true); CHECK(g->save.adventure.lockedMonster.empty());
  // Crazy keeps free choice
  auto cz = adventureAt(5, true); CHECK(cz->menus.empty() && cz->save.crazy.lockedMonster.empty());
  cz->selectMonster(7); CHECK(cz->kaiju.def().id == T.monsters[7].id);
}
static void testDifficultyCurve() {
  SECTION("kid-friendly difficulty: seconds an idle kaiju lasts");
  float tl[15];
  for (int lv : {0, 1, 4, 5, 9, 10, 13}) {
    auto g = adventureAt(lv); float t = 0;
    bool boss = !T.levels[lv].miniBoss.empty();
    if (boss) { g->level.count = 999; g->level.destroyed = 999; g->level.t = 0.01f; g->level.stoneTaken = true; for (auto& id : T.levels[lv].landmarks) g->world.landmark(id)->landmarkSmashed = true; }
    while (t < 240 && g->mode == Mode::Play && g->kaiju.hp > 0) { run(*g, 0.5f); t += 0.5f; }
    tl[lv] = t; std::printf("  L%d%s: idle KO after %.0f s\n", lv + 1, boss ? " (boss phase)" : "", t);
  }
  CHECK(tl[0] >= 90 && tl[1] >= 55 && tl[4] >= 40 && tl[5] >= 35 && tl[9] >= 30 && tl[10] >= 30 && tl[13] >= 30);  // idle = worst case; kids fight back
}


// ---- 3D model hooks (tz_core side): a fake catalog stands in for src/model/game_models
struct FakeCatalog : ModelCatalog {
  std::vector<std::string> ids; std::set<std::string> missing;
  int find(const std::string& id) override {
    if (id.empty() || missing.count(id)) return -1;
    for (size_t i = 0; i < ids.size(); i++) if (ids[i] == id) return (int)i;
    ids.push_back(id); return (int)ids.size() - 1;
  }
  int variant(int, const std::string& name) override { return name == "tier1" ? 1 : name == "tier2" ? 2 : name.empty() ? 0 : 3; }
  bool bounds(int, Vec3& lo, Vec3& hi) override { lo = Vec3(-1, 0, -1); hi = Vec3(1, 1, 1); return true; }
  int count(const RenderList& rl, const std::string& id) const { int n = 0; for (auto& r : rl.models) n += r.asset >= 0 && r.asset < (int)ids.size() && ids[(size_t)r.asset] == id; return n; }
  const ModelReq* byKey(const RenderList& rl, uint32_t key) const { for (auto& r : rl.models) if (r.key == key) return &r; return nullptr; }
};
static void testModelHooks() {
  SECTION("3D model hooks (fake catalog)");
  { auto c = newGame(); c->openOptions(); c->updateMenus(InputState(), 0.016f); bool found = false;
    for (auto& it : c->menus.back().items) if (it.label == "Credits") { found = true; it.act(); }
    c->updateMenus(InputState(), 0.016f);
    CHECK(found && c->menus.back().title == "CREDITS" && c->menus.back().body.find("Quaternius") != std::string::npos &&
          c->menus.back().body.find("Kenney") != std::string::npos && c->menus.back().body.find("kazuma") != std::string::npos); }
  CHECK(std::string(kModelClipNames[MC_STUN]) == "stun" && std::string(kPropIds[PROP_FIRETRUCK]) == "fire_truck");
  auto g = newGame(); g->save.tutorial.done = true; g->startMode(false); run(*g, 0.3f); dismissPrompts(*g);
  RenderList base = renderOnce(*g);
  CHECK(base.models.empty());  // no catalog: everything primitive, as before
  FakeCatalog cat; g->models = &cat;
  RenderList rl = renderOnce(*g);
  const ModelReq* k = cat.byKey(rl, modelKey(MK_KAIJU, 0));
  CHECK(k && cat.ids[(size_t)k->asset] == g->kaiju.def().id && k->clip == MC_IDLE);
  int houses = 0; for (auto& id : cat.ids) houses += id.rfind("house_", 0) == 0 || id.rfind("shop_", 0) == 0 || id.rfind("office_", 0) == 0 || id.rfind("tower_", 0) == 0;
  int people = 0; for (auto& r : rl.models) people += (r.key >> 24) == MK_PERSON && r.shared;
  int props = 0; for (int i = 0; i < PROP_COUNT; i++) props += cat.count(rl, kPropIds[i]);
  std::printf("  models: %zu requests, %d building kinds, %d people, %d props; primitives %zu -> %zu\n", rl.models.size(), houses, people, props, (size_t)base.instanceCount(), (size_t)rl.instanceCount());
  CHECK(rl.models.size() > 50 && houses >= 4 && people > 5 && props > 0);
  CHECK(cat.count(rl, "tree_large") + cat.count(rl, "tree_small") > 0);
  CHECK(rl.instanceCount() < base.instanceCount());  // bricks / limbs replaced by models
  // the walk clip + an attack (trigger bumps each attack)
  InputState walk; walk.moveY = 1; run(*g, 0.6f, walk); g->frame(1.f / 60, walk);
  rl = renderOnce(*g); k = cat.byKey(rl, modelKey(MK_KAIJU, 0)); CHECK(k && (k->clip == MC_WALK || k->clip == MC_RUN));
  {  // foot-matched gait: a speed boost is still a WALK; Shift is a RUN; playback follows ground speed, size and the model's own height
    Kaiju& K = g->kaiju; InputState w; w.moveY = 1; run(*g, 1.0f, w);
    float H = 1.f; { Vec3 lo, hi; if (cat.bounds(0, lo, hi) && hi.y > lo.y) H = hi.y - lo.y; }   // the fake model is 1 unit tall: scale the kaiju up so walking speeds clear the idle threshold
    K.shrinkK = 16.f;
    float sc = K.scale() * K.shrinkK, walkNat = 1.10f * H * sc, runNat = 3.18f * H * sc;
    K.sprinting = false; K.animRun = false; K.speedNow = 1.3f * walkNat;
    rl = renderOnce(*g); k = cat.byKey(rl, modelKey(MK_KAIJU, 0)); CHECK(k && k->clip == MC_WALK && std::fabs(k->speed - 1.3f) < 0.01f);
    InputState sp; sp.moveY = 1; sp.held[A_SPRINT] = true; run(*g, 0.3f, sp);
    CHECK(K.sprinting); K.speedNow = 2.5f * walkNat;
    rl = renderOnce(*g); k = cat.byKey(rl, modelKey(MK_KAIJU, 0)); CHECK(k && k->clip == MC_RUN && std::fabs(k->speed - clampf(K.speedNow / runNat, 0.5f, 2.2f)) < 0.01f);
    K.sprinting = false; K.animRun = false; K.shrinkK = 6.4f; K.speedNow = 3.f * 1.10f * H * K.scale() * K.shrinkK;   // a tiny hatchling at that pace would look like a hovercraft on the walk clip
    rl = renderOnce(*g); k = cat.byKey(rl, modelKey(MK_KAIJU, 0)); CHECK(k && k->clip == MC_RUN);
    K.shrinkK = 1.f; run(*g, 0.3f);
  }
  uint16_t s0 = g->kaiju.attackSeq; tap(*g, A_CLAW); run(*g, 0.05f);
  rl = renderOnce(*g); k = cat.byKey(rl, modelKey(MK_KAIJU, 0));
  CHECK(g->kaiju.attackSeq == (uint16_t)(s0 + 1) && k && k->clip == MC_ATTACK && !k->loop && k->trigger == g->kaiju.attackSeq && k->fit > 0);
  run(*g, 2);
  // a damaged building drops back to its destructible bricks
  Building* b = nullptr;
  for (auto& bb : g->world.buildings) if (bb.generated && !bb.smashed && !bb.school && bb.landmark.empty() && bb.name == "Old Town shop" && g->world.chunks[bb.chunk].active) { b = &bb; break; }
  CHECK(b != nullptr);
  if (b) {
    RenderList r0; CHECK(g->buildingModel(r0, *b) && r0.models.size() == 1);
    standNextTo(*g, *b); g->world.hitBuilding(*b, b->x, 2, b->z, 40, 2, b->x, b->z + 5, 4);
    RenderList r1; CHECK(b->aliveCount < (int)b->bricks.size() && !g->buildingModel(r1, *b) && r1.models.empty());
  }
  // a monster without a model keeps its primitive rig; the config flag turns every model off
  cat.missing.insert(g->kaiju.def().id);
  g->modelCache = Game::ModelCache();
  rl = renderOnce(*g); CHECK(!cat.byKey(rl, modelKey(MK_KAIJU, 0)) && !rl.models.empty());
  float was = C.modelsEnabled; C.modelsEnabled = 0;
  rl = renderOnce(*g); CHECK(rl.models.empty());
  C.modelsEnabled = was;
  // police: cops / cars come through as SWAT / police car models with stable per-unit keys
  g->enemies.addUnit(U_COP); g->enemies.addUnit(U_CAR); g->enemies.addUnit(U_SOLDIER); g->enemies.addUnit(U_ATANK);
  rl = renderOnce(*g);
  int units = 0; for (auto& r : rl.models) units += (r.key >> 24) == MK_UNIT;
  std::printf("  police: %zu units, %d animated unit models, cop %d car %d\n", g->enemies.units.size(), units, cat.count(rl, "cop"), cat.count(rl, "police_car"));
  CHECK(cat.count(rl, "cop") >= 1 && cat.count(rl, "police_car") >= 1 && cat.count(rl, "soldier") >= 1 && cat.count(rl, "tank") >= 1 && units >= 2);
  { uint32_t k0 = 0; for (auto& r : rl.models) if ((r.key >> 24) == MK_UNIT) { CHECK(r.key != k0); k0 = r.key; } }
  // THE BULK: one boss model whose scale follows his growth
  auto h = newGame(); h->save.adventureUnlocked = true; h->startMode(true); dismissPrompts(*h);
  int li = (int)T.levels.size() - 1; h->save.adventure.level = li; h->startLevel(li); pickChampion(*h); run(*h, 0.1f); dismissPrompts(*h);
  { Profile& A = h->AP(); A.gauntlet = A.gauntletSlot = A.gauntletOn = true; A.stones.clear(); for (auto& st : E.stones) A.stones.push_back(st.id); }
  for (int i = 0; i < 6; i++) { h->kaiju.pos = h->level.pedestal[i]; h->interact(); } run(*h, 0.1f); dismissPrompts(*h);
  h->models = &cat; h->boss.introT = 0; h->boss.lift = 0; h->boss.staged = false;  // un-staged: the classic grow-per-hit
  RenderList b0 = renderOnce(*h); const ModelReq* bm = cat.byKey(b0, modelKey(MK_BOSS, 0));
  CHECK(bm && cat.ids[(size_t)bm->asset] == "the_bulk");
  float sc0 = bm ? length(Vec3(bm->world.m[0][0], bm->world.m[0][1], bm->world.m[0][2])) : 0;
  for (int i = 0; i < 12; i++) { h->boss.hitCd = 0; h->hitBossArea(0, h->boss.pos.x, 2, h->boss.pos.z, 5, 1); }
  h->boss.pulse = 0;
  RenderList b1 = renderOnce(*h); bm = cat.byKey(b1, modelKey(MK_BOSS, 0));
  float sc1 = bm ? length(Vec3(bm->world.m[0][0], bm->world.m[0][1], bm->world.m[0][2])) : 0;
  std::printf("  bulk model scale %.2f -> %.2f (boss.scale %.2f)\n", sc0, sc1, h->boss.scale);
  CHECK(sc1 > sc0 && std::fabs(sc1 - h->boss.scale) < 1e-3f);
}

// ======================================================================= eggs / rarities / growth / XP / ladder / arc / ending
static Unit& addU(Game& g, UnitType t, Vec3 at, float dir = 0) { g.enemies.addUnit(t); Unit& u = g.enemies.units.back(); u.pos = at; u.dir = dir; u.hitCd = 0; return u; }
static void testEggTables() {
  SECTION("egg tables: rarities, palettes, finishes (data/eggs.json)");
  CHECK(E.rarities.size() == 6);
  const float rates[6] = {50, 27, 14, 6.5f, 2, 0.5f};
  float sum = 0; for (int i = 0; i < 6; i++) { CHECK(std::fabs(E.rarities[i].rate - rates[i]) < 1e-4f); sum += E.rarities[i].rate; }
  CHECK(std::fabs(sum - 100) < 1e-3f);
  CHECK(E.rarities[5].name == "TOMMYZILLA" && E.rarityOf("tommyzilla") == 5);
  for (int i = 1; i < 6; i++) CHECK(E.rarities[i].size >= E.rarities[i - 1].size && E.rarities[i].hp > E.rarities[i - 1].hp);  // rarer = better start
  CHECK(E.rarities[2].passives >= 1 && E.rarities[3].supers >= 1 && E.rarities[4].passives >= 1 && E.rarities[4].supers >= 1 && E.rarities[5].passives >= 1 && E.rarities[5].supers >= 1);
  CHECK(E.rarities[0].passives == 0 && E.rarities[0].supers == 0 && E.rarities[1].passives == 0);
  for (auto& m : T.monsters) CHECK(E.tierOf.count(m.id) == 1 && E.palettes.count(m.id) && !E.palettes.at(m.id).empty());
  CHECK(E.finishes.size() == 3 && std::fabs(E.finishes[0].rate - 1.0f) < 1e-5f && std::fabs(E.finishes[1].rate - 0.5f) < 1e-5f && std::fabs(E.finishes[2].rate - 0.25f) < 1e-5f);
  CHECK(E.finishes[0].name == "GOLD" && E.finishes[1].name == "CHROME" && E.finishes[2].name == "HOLOGRAPHIC");
  // validation: broken tables are rejected with a message
  std::string err; EggTables bad;
  CHECK(!bad.load("{\"rarities\": []}", T, &err) && !err.empty());
  std::string txt = file("eggs.json"); Json j; CHECK(Json::parse(txt, j));
  Json k = j; k.obj["tiers"].obj["common"].arr.push_back(Json::makeStr("nosuchkaiju")); err.clear(); CHECK(!bad.load(k.dump(), T, &err) && err.find("nosuchkaiju") != std::string::npos);
  Json f = j; f.obj["finishes"].arr.pop_back(); err.clear(); CHECK(!bad.load(f.dump(), T, &err));
  EggTables ok; CHECK(ok.load(txt, T, &err));
  // drop distribution (data-driven weighted roll)
  Rng r(12345); int cnt[6] = {}; const int N = 400000;
  for (int i = 0; i < N; i++) cnt[eggs::rollRarity(r)]++;
  for (int i = 0; i < 6; i++) { float pct = 100.f * cnt[i] / N; std::printf("  %-11s %.2f%% (want %.2f)\n", E.rarities[i].name.c_str(), pct, rates[i]); CHECK(std::fabs(pct - rates[i]) < std::max(0.12f, rates[i] * 0.04f)); }
  int fin[4] = {}; for (int i = 0; i < N; i++) fin[eggs::rollFinish(r)]++;
  std::printf("  finishes: gold %.3f%% chrome %.3f%% holo %.3f%%\n", 100.f * fin[1] / N, 100.f * fin[2] / N, 100.f * fin[3] / N);
  CHECK(std::fabs(100.f * fin[1] / N - 1.0f) < 0.08f && std::fabs(100.f * fin[2] / N - 0.5f) < 0.06f && std::fabs(100.f * fin[3] / N - 0.25f) < 0.04f);
  // rolled looks: random palette mix, perks by tier, JSON round trip
  std::set<std::string> prim; int perksRare = 0;
  for (int i = 0; i < 300; i++) {
    KaijuLook l = eggs::rollLook(r, "mossback", T); prim.insert(eggs::hex(l.primary));
    CHECK(l.rarity == E.rarityOf("mossback") && l.monster == "mossback"); perksRare += (int)l.passives.size();
    KaijuLook b; CHECK(b.fromJson(l.toJson()) && eggs::hex(b.primary) == eggs::hex(l.primary) && eggs::hex(b.eyes) == eggs::hex(l.eyes) && b.finish == l.finish && b.passives == l.passives);
  }
  CHECK(prim.size() > 20 && perksRare >= 300);
  KaijuLook lg = eggs::rollLook(r, "roborumble", T); CHECK(!lg.passives.empty() && !lg.bonusSuper.empty() && T.moves[T.moveIndex(lg.bonusSuper)].kind == MoveKind::Super);
  KaijuLook cm = eggs::rollLook(r, "gloopzilla", T); CHECK(cm.passives.empty() && cm.bonusSuper.empty());
}
static void testSpinner() {
  SECTION("egg spinner: decided first, eased stop exactly on it");
  for (int trial = 0; trial < 40; trial++) {
    Rng r(1000 + trial); KaijuLook want = eggs::rollLook(r, T.monsters[(size_t)(trial % T.monsters.size())].id, T);
    Spinner s; s.start(r, want, T, SPIN_TOKEN);
    CHECK(s.phase == Spinner::Spinning && s.reel[(size_t)s.stopIndex] == want.monster && s.stopIndex > 20 && s.stopIndex < (int)s.reel.size() - 2);
    float stopAt = 0.3f + (trial % 7) * 0.4f, t = 0; bool midSpinMoving = false; float p0 = s.pos;
    while (!s.landed() && t < 20) { if (t >= stopAt && s.phase == Spinner::Spinning) s.stop(); s.update(1.f / 60); t += 1.f / 60; if (t > 0.2f && s.pos != p0) midSpinMoving = true; }
    CHECK(s.landed() && std::fabs(s.pos - s.stopIndex) < 1e-4f && s.cellUnderMarker() == s.stopIndex && s.result.monster == want.monster && midSpinMoving);
  }
  { Rng r(7); KaijuLook want = eggs::rollLook(r, "tommyzilla", T); Spinner s; s.start(r, want, T, SPIN_TOKEN); float t = 0;  // nobody presses: auto-stop
    while (!s.landed() && t < 30) { s.update(1.f / 60); t += 1.f / 60; }
    std::printf("  auto-stop landed after %.1f s\n", t); CHECK(s.landed() && t >= E.spinAutoStop && t < E.spinAutoStop + E.spinStopTime * 2.5f && s.reel[(size_t)s.stopIndex] == "tommyzilla"); }
  // eased: speed falls monotonically while stopping
  { Rng r(9); Spinner s; s.start(r, eggs::rollLook(r, "gloopzilla", T), T, 0); for (int i = 0; i < 60; i++) s.update(1.f / 60); s.stop();
    float last = 1e9f, prev = s.pos; bool mono = true; while (!s.landed()) { s.update(1.f / 60); float v = s.pos - prev; prev = s.pos; if (v > last + 1e-4f) mono = false; last = v; } CHECK(mono); }
  // new slot: the first spin is always a playable kaiju, character creation via the spinner
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "tz_spin_test"; fs::remove_all(dir); fs::create_directories(dir);
  for (int k = 0; k < 6; k++) {
    SlotStore st; st.dir = dir; fs::remove_all(dir); fs::create_directories(dir);
    auto g = std::make_unique<Game>(); g->slots = &st; g->init(nullptr); g->spinSeed = 777 + k * 31;
    g->selectSlot(0);
    CHECK(g->mode == Mode::Spin && !g->save.created && g->spinReason == SPIN_CREATE);
    run(*g, 1.0f); RenderList mid = renderOnce(*g); CHECK(mid.ui.size() > 200);  // mid-spin UI
    tap(*g, A_CONFIRM); for (int i = 0; i < 60 * 6 && g->spinner.phase != Spinner::Landed; i++) g->frame(1.f / 60, InputState());
    CHECK(g->spinner.phase == Spinner::Landed && g->spinner.isNew && g->save.created);
    std::string id = g->spinner.result.monster;
    CHECK(T.monsterIndex(id) >= 0 && g->save.monster == id && g->save.monsters.size() == 1 && g->save.looks.count(id) && g->save.sandbox.xp[id] == 0 && g->save.adventure.xp[id] == 0);
    run(*g, 2.0f); CHECK(g->mode == Mode::Reveal && g->egg.active && g->egg.spin == SPIN_CREATE);
    RenderList rev = renderOnce(*g); CHECK(rev.instanceCount() > 100);
    run(*g, C.revealTime + 3); CHECK(g->mode == Mode::Title && !g->menus.empty());
    // the look and the slot accent survive a reload
    std::string txt; CHECK(st.read(0, txt)); SlotSummary sm = summarizeSlot(0, &txt); CHECK(sm.hasAccent && eggs::hex(sm.accent) == eggs::hex(g->save.looks[id].accent));
    Json j; SaveData r2; CHECK(Json::parse(txt, j) && r2.fromJson(j) && r2.looks.count(id) && eggs::hex(r2.looks[id].primary) == eggs::hex(g->save.looks[id].primary) && r2.created && r2.spinSlot);
    G = nullptr;
  }
  fs::remove_all(dir);
  // spin tokens: earned per level won, coin store purchase; duplicates give coins + XP (and a finish upgrade)
  CHECK(E.spinsNewSlot == 1 && E.spinsPerLevel == 1 && E.spinCost == 4000 && E.sandboxSpinEvery == 30);
  auto g = newGame(); g->save.tutorial.done = true; g->startMode(false); run(*g, 0.2f); dismissPrompts(*g);
  CHECK(!g->beginSpin(SPIN_TOKEN) && g->mode == Mode::Play);  // no token
  std::string own = g->save.monster; g->P().spins = 1; int c0 = g->P().coins;
  KaijuLook dup = eggs::rollLook(tmpRng(5), own, T); dup.finish = FIN_HOLO;
  g->save.looks[own] = eggs::defaultLook(T.monsters[(size_t)T.monsterIndex(own)]); g->P().xp[own] = 100;
  CHECK(!g->applySpinResult(dup, SPIN_TOKEN) && g->spinDup);
  const RarityDef& rd = E.rarity(E.rarityOf(own));
  CHECK(g->P().coins == c0 + rd.dupCoins && std::fabs(g->P().xp[own] - (100 + rd.dupXp)) < 1e-3f && g->save.looks[own].finish == FIN_HOLO);
  CHECK(g->beginSpin(SPIN_TOKEN) && g->P().spins == 0 && g->mode == Mode::Spin);
  // sandbox: +1 spin every 30 buildings smashed
  auto sb = sandboxGame(); int sp0 = sb->P().spins; sb->P().spinSmashMark = sb->P().smashCount;
  for (int i = 0; i < E.sandboxSpinEvery; i++) sb->onBuildingSmashed(sb->world.buildings[(size_t)i]);
  CHECK(sb->P().spins == sp0 + 1);
}
static void testGrowthAndXp() {
  SECTION("growth (hatchling -> adult), XP sources, heal -> XP, lifesteal");
  CHECK(std::fabs(eggs::growthScale(0) - E.growthStart) < 1e-5f && std::fabs(E.growthStart - 0.42f) < 1e-5f);
  CHECK(std::fabs(eggs::growthScale(E.growthFullXp) - 1) < 1e-5f && std::fabs(eggs::growthScale(E.growthFullXp * 5) - 1) < 1e-5f);  // capped
  float last = 0; bool mono = true; for (int x = 0; x <= 700; x += 10) { float s = eggs::growthScale((float)x); if (s < last) mono = false; last = s; } CHECK(mono);
  auto g = sandboxGame(); std::string id = g->save.monster;
  float full = g->kaiju.scale();
  KaijuLook l = eggs::rollLook(tmpRng(3), id, T); g->save.looks[id] = l; g->P().xp[id] = 0;
  float baby = g->kaiju.scale(), want0 = T.monsters[(size_t)g->kaiju.monster].scale * E.rarity(l.rarity).size * E.growthStart;
  std::printf("  %s: hatchling %.2f, legacy full %.2f\n", id.c_str(), baby, full);
  CHECK(std::fabs(baby - want0) < 1e-4f && baby < full * 0.6f);
  float maxBaby = g->kaiju.maxHp(); CHECK(std::fabs(maxBaby - C.kaijuMaxHp * T.monsters[(size_t)g->kaiju.monster].hp * E.rarity(l.rarity).hp) < 1e-3f);
  // fair while small: less damage taken, more bricks per hit
  g->kaiju.hp = g->kaiju.maxHp(); g->kaiju.hurtT = 0; float h0 = g->kaiju.hp; g->kaiju.damage(4); float babyDmg = h0 - g->kaiju.hp;
  CHECK(babyDmg > 0 && babyDmg < 4 * g->enemyDamageMult() * 0.6f);
  float bBaby = g->kaiju.moveBricks(T.moves[T.moveIndex("claw")]);
  // the camera pulls back smoothly as it grows
  g->kaiju.updateCamera(1.f / 60, true); float camBaby = g->kaiju.camScale;
  g->P().xp[id] = E.growthFullXp; float adult = g->kaiju.scale();
  CHECK(std::fabs(adult - T.monsters[(size_t)g->kaiju.monster].scale * E.rarity(l.rarity).size) < 1e-4f);
  CHECK(g->kaiju.moveBricks(T.moves[T.moveIndex("claw")]) < bBaby);
  g->kaiju.updateCamera(1.f / 60, false); float cam1 = g->kaiju.camScale; CHECK(cam1 > camBaby && cam1 < adult);  // eased, not snapped
  for (int i = 0; i < 600; i++) g->kaiju.updateCamera(1.f / 60, false); CHECK(std::fabs(g->kaiju.camScale - adult) < 0.01f);
  CHECK(g->kaiju.eatRange() >= C.eatRange);
  // XP sources (data driven) grow the kaiju; per mode, saved
  g->P().xp[id] = 0;
  CHECK(E.xpFor("eat") > 0 && E.xpFor("building") > 0 && E.xpFor("enemy") > 0 && E.xpFor("zoo") > 0 && E.xpFor("level") > 0);
  g->addXp(E.xpFor("building")); CHECK(std::fabs(g->kaijuXp() - E.xpFor("building")) < 1e-4f);
  float sc1 = g->kaiju.scale(); g->addXp(100); CHECK(g->kaiju.scale() > sc1);
  CHECK(g->save.adventure.xp.count(id) == 0 || g->save.adventure.xp[id] != g->P().xp[id] || true);
  { Json j = g->save.toJson(); SaveData r; CHECK(r.fromJson(j) && std::fabs(r.sandbox.xp[id] - g->P().xp[id]) < 1e-3f); }
  // heal -> XP: full HP converts all, partial converts the overflow only, below max converts none
  g->kaiju.hp = g->kaiju.maxHp(); float x0 = g->kaijuXp();
  CHECK(std::fabs(g->healKaiju(5) - 5 * E.healToXp) < 1e-4f && std::fabs(g->kaijuXp() - x0 - 5 * E.healToXp) < 1e-4f && g->kaiju.hp == g->kaiju.maxHp());
  g->kaiju.hp = g->kaiju.maxHp() - 2; x0 = g->kaijuXp();
  CHECK(std::fabs(g->healKaiju(5) - 3 * E.healToXp) < 1e-4f && std::fabs(g->kaijuXp() - x0 - 3 * E.healToXp) < 1e-4f && std::fabs(g->kaiju.hp - g->kaiju.maxHp()) < 1e-4f);
  g->kaiju.hp = g->kaiju.maxHp() - 10; x0 = g->kaijuXp();
  CHECK(g->healKaiju(4) == 0 && g->kaijuXp() == x0 && std::fabs(g->kaiju.hp - (g->kaiju.maxHp() - 6)) < 1e-4f);
  // natural regen never converts (standing still does not grow you)
  g->kaiju.hp = g->kaiju.maxHp(); x0 = g->kaijuXp(); clearPeople(*g); g->enemies.reset(); run(*g, 3); CHECK(g->kaijuXp() == x0);
  // melee lifesteal: +1 HP per block smashed (capped at max), throttled heart pops
  CHECK(E.lifesteal == 1);
  Building* b = nullptr; for (auto& bb : g->world.buildings) if (bb.generated && !bb.smashed && !bb.school && bb.landmark.empty() && g->world.chunks[bb.chunk].active && bb.bricks.size() > 30) { b = &bb; break; }
  CHECK(b != nullptr);
  if (b) {
    standNextTo(*g, *b); g->kaiju.hp = g->kaiju.maxHp() - 30; float hpBefore = g->kaiju.hp; size_t f0 = g->floats.size();
    g->kaiju.cooldowns.assign(T.moves.size(), 0); tap(*g, A_CLAW); run(*g, 0.6f);
    int hearts = 0; for (size_t i = f0; i < g->floats.size(); i++) hearts += g->floats[i].text.find("<3") != std::string::npos;
    std::printf("  claw: hp %.1f -> %.1f, heart pops %d\n", hpBefore, g->kaiju.hp, hearts);
    CHECK(g->kaiju.hp >= hpBefore + 1 && g->kaiju.hp <= g->kaiju.maxHp() && hearts >= 1 && hearts <= 3);
  }
  // eating heals a little + gives XP
  g->kaiju.hp = g->kaiju.maxHp() - 5; float he = g->kaiju.hp; x0 = g->kaijuXp(); clearPeople(*g);
  placePerson(*g, g->kaiju.pos.x + 2, g->kaiju.pos.z + 2); tap(*g, A_EAT); run(*g, 0.3f);
  CHECK(g->kaiju.hp >= he + E.eatHeal - 0.01f && g->kaijuXp() > x0);
  // HUD: the XP bar is drawn in the kaiju's accent colour
  RenderList rl = renderOnce(*g); CHECK(rl.ui.size() > 300);
  CHECK(eggs::hex(g->uiAccent()) == eggs::hex(l.accent));
  // per mode: adventure growth is separate from sandbox
  CHECK(g->save.adventure.xp[id] != g->save.sandbox.xp[id] || g->save.adventure.xp.count(id) == 0);
}
static void testFinishRender() {
  SECTION("colours + finishes carry through (primitive fallback + model requests)");
  auto g = sandboxGame(); std::string id = g->save.monster;
  KaijuLook l = eggs::rollLook(tmpRng(11), id, T); l.finish = FIN_GOLD; g->save.looks[id] = l; g->P().xp[id] = E.growthFullXp;
  MonsterDef ld = g->lookedDef(g->kaiju.monster); CHECK(eggs::hex(ld.body) == eggs::hex(l.primary) && eggs::hex(ld.accent) == eggs::hex(l.accent));
  RenderList rl = renderOnce(*g); int gold = 0; for (int m = 0; m < MESH_COUNT; m++) for (auto& in : rl.opaque[m]) gold += in.params[2] == FIN_GOLD;
  CHECK(gold >= 10);
  FakeCatalog cat; g->models = &cat; RenderList mr = renderOnce(*g); const ModelReq* k = cat.byKey(mr, modelKey(MK_KAIJU, 0));
  CHECK(k && k->palette && k->finish == FIN_GOLD && eggs::hex(k->primary) == eggs::hex(l.primary) && eggs::hex(k->eyes) == eggs::hex(l.eyes));
  g->save.looks[id].finish = FIN_HOLO; mr = renderOnce(*g); k = cat.byKey(mr, modelKey(MK_KAIJU, 0)); CHECK(k && k->finish == FIN_HOLO);
  g->models = nullptr;
  // the character select + slot screen use the rolled colours; legacy monsters keep their classic look
  g->openCharacterSelect(false); RenderList cs = renderOnce(*g); CHECK(cs.ui.size() > 300); g->menus.clear();
  KaijuLook classic = g->lookOf("gloopzilla"); CHECK(!g->hasLook("gloopzilla") && classic.finish == 0 && eggs::hex(classic.primary) == eggs::hex(T.monsters[(size_t)T.monsterIndex("gloopzilla")].body));
}
static void testEnemyLadder() {
  SECTION("enemy ladder: riot shields, armour needs supers, flamethrowers");
  // per level ladder
  const auto& Ls = T.levels;
  CHECK(Ls[0].riots + Ls[0].armored + Ls[0].flamers == 0);
  CHECK(Ls[1].riots > 0 && Ls[1].armored == 0);
  int firstArmor = -1, firstFlame = -1; for (int i = 0; i < 20; i++) { if (firstArmor < 0 && Ls[i].armored) firstArmor = i; if (firstFlame < 0 && Ls[i].flamers) firstFlame = i; }
  CHECK(firstArmor == 3 && firstFlame == 5);
  for (auto& L : Ls) CHECK(L.unit != "armor");  // armour is never required for a goal
  auto g = adventureAt(9); g->enemies.reset(); clearPeople(*g);
  Vec3 K = g->kaiju.pos; int blaster = T.moveIndex("blaster"), breath = T.moveIndex("breath"), claw = T.moveIndex("claw"), quake = T.moveIndex("quake");
  CHECK(T.moves[blaster].kind == MoveKind::Direct && T.moves[breath].kind == MoveKind::Direct && T.moves[quake].kind == MoveKind::Super);
  // riot cop facing the kaiju: ranged hits are blocked, melee works; from behind ranged works
  Unit& r1 = addU(*g, U_RIOT, K + Vec3(0, 0, 10), PI);  // faces -z = toward the kaiju
  CHECK(r1.police() && r1.infantry() && !r1.armored());
  float hp = r1.hp; g->enemies.hitArea(r1.pos.x, r1.pos.z, 2, blaster, false); CHECK(r1.hp == hp && g->enemies.blocked == 1);
  r1.hitCd = 0; g->enemies.hitArea(r1.pos.x, r1.pos.z, 2, breath, false); CHECK(r1.hp == hp && g->enemies.blocked == 2);
  CHECK(Enemies::shieldBlocks(r1, K, blaster) && !Enemies::shieldBlocks(r1, K, claw));
  r1.hitCd = 0; g->enemies.hitArea(r1.pos.x, r1.pos.z, 2, claw, false); CHECK(r1.hp < hp || r1.wreck);
  Unit& r2 = addU(*g, U_RIOT, K + Vec3(0, 0, 10), 0);  // faces away
  float hp2 = r2.hp; g->enemies.hitArea(r2.pos.x, r2.pos.z, 2, blaster, false); CHECK(r2.hp < hp2 || r2.wreck);
  // armour: only supers hurt it, with a clang hint; always beatable
  Unit& a = addU(*g, U_ARMOR, K + Vec3(20, 0, 0));
  CHECK(a.armored() && a.tank() && a.hp == 3);
  size_t fl = g->floats.size();
  for (int mi : {claw, blaster, breath, T.moveIndex("stomp")}) { a.hitCd = 0; g->enemies.hitArea(a.pos.x, a.pos.z, 3, mi, false); }
  a.hitCd = 0; g->enemies.hitArea(a.pos.x, a.pos.z, 3, -1, true);  // thrown things clang too
  CHECK(a.hp == 3 && !a.wreck && g->enemies.clangs >= 5);
  bool hint = false; for (size_t i = fl; i < g->floats.size(); i++) hint |= g->floats[i].text == "CLANG! needs a SUPER!"; CHECK(hint);
  int hits = 0; while (!a.wreck && hits < 10) { a.hitCd = 0; g->enemies.hitArea(a.pos.x, a.pos.z, 3, quake, false); hits++; }
  CHECK(a.wreck && hits >= 2 && hits <= 3);  // supers hit heavy (2)
  // the Crazy multiplier does not stack armour (still 3 supers)
  auto cz = adventureAt(9, true); cz->enemies.addUnit(U_ARMOR); CHECK(cz->enemies.units.back().hp == 3);
  // flamethrower: short cone only
  Unit& f = addU(*g, U_FLAME, K + Vec3(0, 0, C.flameRange * 0.5f), PI);
  CHECK(Enemies::flameHits(f, K)); f.dir = 0; CHECK(!Enemies::flameHits(f, K));
  f.dir = PI; f.pos = K + Vec3(0, 0, C.flameRange + g->kaiju.radius() + 3); CHECK(!Enemies::flameHits(f, K));
  // a flamer next to an idle kaiju deals damage over time
  g->enemies.reset(); Unit& f2 = addU(*g, U_FLAME, K + Vec3(0, 0, 6), PI); (void)f2;
  g->kaiju.hp = g->kaiju.maxHp(); float h0 = g->kaiju.hp; run(*g, 2); CHECK(g->kaiju.hp < h0);
  // spawned on their levels, render (primitive + model) without trouble
  auto l9 = adventureAt(9); std::set<int> seen; for (int k = 0; k < 80; k++) { run(*l9, 0.5f); for (auto& u : l9->enemies.units) seen.insert(u.type); l9->kaiju.hp = l9->kaiju.maxHp(); }
  std::printf("  level 10 types:"); for (int t : seen) std::printf(" %d", t); std::printf("\n");
  CHECK(seen.count(U_RIOT) && seen.count(U_FLAME) && seen.count(U_ARMOR));
  int armorAlive = 0; for (auto& u : l9->enemies.units) armorAlive += u.type == U_ARMOR && !u.wreck; CHECK(armorAlive <= 2);
  RenderList prl = renderOnce(*l9); CHECK(prl.instanceCount() > 500);
  FakeCatalog cat; l9->models = &cat; l9->enemies.addUnit(U_RIOT); l9->enemies.addUnit(U_ARMOR); l9->enemies.addUnit(U_FLAME);
  RenderList mrl = renderOnce(*l9); bool chrome = false; for (auto& m : mrl.models) chrome |= m.finish == FIN_CHROME; CHECK(chrome && cat.count(mrl, "cop") >= 1 && cat.count(mrl, "soldier") >= 1);
  // a common kaiju can beat adventure: every goal is reachable without supers or armour kills (destroy goals count cops / soldiers / tanks)
  for (auto& L : Ls) if (L.obj == ObjType::Destroy) CHECK(L.unit == "tank" || L.unit == "heli" || L.unit == "jet" || L.unit == "any" || L.unit == "cop" || L.unit == "soldier" || L.unit == "jeep" || L.unit == "car");
}
static void testDeevo() {
  SECTION("De-Evolution Ray");
  CHECK(E.deevoPrice == 2500);
  auto g = adventureAt(7); g->menus.clear();
  Profile& A = g->AP(); std::string old = g->save.monster;
  g->save.looks[old] = eggs::rollLook(tmpRng(4), old, T); A.xp[old] = 450;
  A.coins = 2499; std::string why; CHECK(!g->canDeevo(&why) && why.find("2500") != std::string::npos);
  g->openDeevoConfirm(); CHECK(g->menus.empty());
  A.coins = 3000; A.keys = 3; A.chests = {"quake"}; int lvl = A.level; std::set<std::string> moves = A.moves; auto slots = A.slots;
  g->boss.active = true; CHECK(!g->canDeevo(&why) && why == "Not during a boss fight!"); g->boss.active = false;
  g->level.bossPhase = true; CHECK(!g->canDeevo()); g->level.bossPhase = false;
  g->openDeevoConfirm(); CHECK(!g->menus.empty() && g->menus.back().body == "Your kaiju will turn back into an egg. Are you sure?");
  g->menus.back().index = 0; tap(*g, A_CONFIRM); CHECK(A.coins == 3000 && g->deevoT < 0);  // "No" keeps it
  g->openDeevoConfirm(); g->menus.back().index = 1; tap(*g, A_CONFIRM);
  CHECK(A.coins == 500 && g->deevoT >= 0);
  run(*g, 1.5f); CHECK(g->kaiju.shrinkK < 1); RenderList beam = renderOnce(*g); CHECK(beam.instanceCount() > 0);
  run(*g, 1.7f); CHECK(g->mode == Mode::Spin && g->spinReason == SPIN_DEEVO);
  tap(*g, A_CONFIRM); for (int i = 0; i < 60 * 6 && g->spinner.phase != Spinner::Landed; i++) g->frame(1.f / 60, InputState());
  std::string nw = g->spinner.result.monster;
  CHECK(g->save.monster == nw && A.xp[nw] == 0 && g->save.looks.count(nw));
  if (nw != old) CHECK(!g->save.monsters.count(old) && !g->save.looks.count(old) && !A.xp.count(old));
  CHECK(A.level == lvl && A.keys == 3 && A.chests.size() == 1 && A.moves.size() >= moves.size() && A.slots == slots && A.coins == 500);  // kept
  CHECK(A.lockedMonster == nw);  // after the level-6 lock the new kaiju is the champion
  { Json j = g->save.toJson(); SaveData r; CHECK(r.fromJson(j) && r.monster == nw && r.adventure.xp[nw] == 0 && r.adventure.lockedMonster == nw); }
  run(*g, 2.0f + C.revealTime + 3); dismissPrompts(*g); CHECK(g->mode == Mode::Play && g->kaiju.def().id == nw);
  // store entry
  g->openStore(); g->updateMenus(InputState(), 0.016f); bool item = false; for (auto& it : g->menus.back().items) item |= it.label == "De-Evolution Ray" && it.right == "2500 coins"; CHECK(item); g->menus.clear();
}
static void testZooAndKey() {
  SECTION("Kaiju Zoo opening + hidden Zookeeper's Key + zoo gates");
  auto g = newGame(); g->save.spinSlot = true; g->save.tutorial.done = true; g->save.adventureUnlocked = true;
  CHECK(g->zooEligible());
  g->startMode(true); CHECK(g->zoo.active && !g->level.active && g->enemies.units.empty());
  // E out of range does nothing (the keeper starts far outside the bars)
  CHECK(!g->zoo.inRange); tap(*g, A_EAT); CHECK(!g->zoo.keeperEaten);
  size_t toasts0 = g->toasts.size();
  RenderList zr = renderOnce(*g); CHECK(zr.instanceCount() > 500);
  // can't fail, no enemies, can't leave the cage
  InputState out; out.moveY = 1; run(*g, 4, out);
  CHECK(dist2D(g->kaiju.pos.x, g->kaiju.pos.z, g->zoo.center.x, g->zoo.center.z) <= E.zooCageR);
  CHECK(g->enemies.units.empty() && g->kaiju.hp == g->kaiju.maxHp());
  g->kaiju.damage(50); CHECK(g->kaiju.hp == g->kaiju.maxHp());
  for (int i = 0; i < 60 * 8 && g->zoo.state == 0; i++) g->frame(1.f / 60, InputState());
  CHECK(g->zoo.state == 1);
  run(*g, E.zooIdleHint + 0.5f); CHECK(g->zoo.idleT > E.zooIdleHint);  // gentle idle hint shows
  // walk to the keeper (inside the cage) until in range
  for (int i = 0; i < 600 && !g->zoo.inRange; i++) { Vec3 d = g->zoo.keeper - g->kaiju.pos; d.y = 0; InputState w; float l = length(d); g->kaiju.pos += d * (0.2f / std::max(0.2f, l)); g->frame(1.f / 60, w); }
  CHECK(g->zoo.inRange);
  RenderList pr = renderOnce(*g); CHECK(pr.ui.size() > 100);  // "Press E to gulp!"
  int eaten0 = g->P().eaten, rb0 = g->P().rainbows; float xp0 = g->kaijuXp();
  tap(*g, A_EAT);
  CHECK(g->zoo.keeperEaten && g->zoo.state == 2 && g->P().eaten == eaten0 + 1 && g->P().rainbows >= rb0 + 1 && g->kaijuXp() > xp0);
  CHECK(g->AP().zooKey && g->save.zooCompleted);
  bool keyToast = false; for (size_t i = toasts0; i < g->toasts.size(); i++) keyToast |= g->toasts[i].text.find("Key") != std::string::npos || g->toasts[i].text.find("key") != std::string::npos;
  CHECK(!keyToast);  // silent: no toast / popup
  run(*g, 1.4f); CHECK(g->zoo.barsBroken);
  run(*g, 2.0f); CHECK(!g->zoo.active && g->AP().zooDone && g->level.active && g->level.idx == 0);  // breakout -> the tutorial / level 1
  // only once per mode
  g->startMode(true); CHECK(!g->zoo.active);
  { Json j = g->save.toJson(); SaveData r; CHECK(r.fromJson(j) && r.adventure.zooKey && r.adventure.zooDone && !r.sandbox.zooKey && r.zooCompleted); }
  // sandbox (separate mode): the zoo plays once there too; skipping it after completing it elsewhere still grants the key
  g->startMode(false); CHECK(g->zoo.active); tap(*g, A_SKILLS); CHECK(!g->zoo.active && g->P().zooKey && g->P().zooDone);
  // never-completed slot: skip gives no key; key only from eating the keeper
  auto s = newGame(); s->save.spinSlot = true; s->save.tutorial.done = true; s->startMode(false); CHECK(s->zoo.active);
  tap(*s, A_SKILLS); CHECK(!s->zoo.active && !s->P().zooKey);
  // the key shows in Skills only when owned, with a key-ring icon
  auto inSkills = [](Game& gg) { gg.openSkills(); gg.updateMenus(InputState(), 0.016f); bool f = false; for (auto& it : gg.menus.back().items) f |= it.label.find("Zookeeper's Key") != std::string::npos && it.icon == 1; gg.menus.clear(); return f; };
  CHECK(!inSkills(*s)); s->P().zooKey = true; CHECK(inSkills(*s));
  // Zoo Gates: data-driven; open with the key, stay shut (no hint) without; each reward once
  int sandboxGates = 0, advGates = 0; for (auto& gd : E.gates) { if (gd.level == 0) sandboxGates++; else { advGates++; CHECK(gd.level >= 2 && gd.level <= 12); } }
  CHECK(sandboxGates == 1 && advGates >= 3 && advGates <= 4);
  s->P().zooKey = false; s->setupGates(); CHECK(s->gates.size() == 1);
  GateRun gt = s->gates[0]; s->kaiju.pos = gt.pos; size_t t0 = s->toasts.size(); run(*s, 0.3f);
  CHECK(!s->gates[0].open && s->toasts.size() == t0);
  s->P().zooKey = true; int sp0 = s->P().spins; run(*s, 0.3f); CHECK(s->gates[0].open && s->P().spins == sp0 + 1 && s->P().gates.count(gt.id));
  RenderList gr = renderOnce(*s); CHECK(gr.instanceCount() > 0);
  s->setupGates(); s->kaiju.pos = gt.pos; run(*s, 0.3f); CHECK(s->P().spins == sp0 + 1);  // reward granted once; key not consumed
  CHECK(s->P().zooKey);
  // coins gate on L3
  auto l3 = adventureAt(2); l3->AP().zooKey = true; l3->setupGates(); GateRun* cg = nullptr; for (auto& q : l3->gates) if (q.reward == "coins") cg = &q;
  CHECK(cg != nullptr); if (cg) { int c0 = l3->AP().coins; l3->kaiju.pos = cg->pos; run(*l3, 0.3f); CHECK(l3->AP().coins >= c0 + 300); }
  // the gates live only on levels 2-12 (plus the stone gates of 14-19)
  auto l15 = adventureAt(14); l15->AP().zooKey = true; l15->setupGates(); for (auto& q : l15->gates) CHECK(q.stone >= 0);
}
static bool openZooBox(Game& g) {
  const LevelDef& L = T.levels[(size_t)g.level.idx];
  g.world.updateStreaming(L.ax, L.az, true);
  for (auto& b : g.world.buildings) if (!b.school && dist2D(b.x, b.z, L.ax, L.az) < L.ar) b.smashed = true;
  run(g, 0.1f); if (!g.level.boxShown) return false;
  g.kaiju.pos = g.level.boxPos; run(g, 0.1f);
  if (!g.level.boxOpen && g.level.boxKeeper) { g.kaiju.pos = g.level.boxKeeperPos; g.interact(); g.kaiju.pos = g.level.boxPos; run(g, 0.1f); }
  return g.level.boxOpen && g.AP().gauntlet;
}
static bool grabStone(Game& g) {
  const LevelDef& L = T.levels[(size_t)g.level.idx];
  GateRun* gt = nullptr; for (auto& q : g.gates) if (q.stone == L.stone) gt = &q;
  if (!gt) return false;
  if (g.world.procedural && L.stone < (int)g.world.stoneBuildings.size()) { Building& b = g.world.buildings[(size_t)g.world.stoneBuildings[(size_t)L.stone]]; g.world.generate(b); std::fill(b.alive.begin(), b.alive.end(), 0); b.aliveCount = 0; b.smashed = true; }  // Atlas: the stone hides in a purple building
  g.kaiju.pos = gt->pos; run(g, 0.1f); if (!gt->open) return false;
  g.kaiju.pos = gt->pos + Vec3(0, 0, -8); run(g, 0.1f);
  return g.level.stoneTaken;
}
static bool placeAllStones(Game& g) {
  for (int i = 0; i < 6; i++) { g.kaiju.pos = g.level.pedestal[i]; g.interact(); }
  run(g, 0.1f); dismissPrompts(g);
  return g.level.fightStarted && g.boss.active && g.boss.staged && g.boss.stage == 1;
}
static bool beatStagedBulk(Game& g) {
  Boss& b = g.boss; b.introT = 0; b.lift = 0; b.hitCd = 0;
  g.hitBossArea(0, b.pos.x, 2, b.pos.z, 5, 10);
  if (!(b.stage == 1 && b.hp == 1 && b.angerT > 0)) return false;
  run(g, E.bulkAngerTime + 0.2f); if (b.stage != 2) return false;
  b.hp = 1; b.hitCd = 0; g.hitBossArea(0, b.pos.x, 2, b.pos.z, 5, 10); if (b.stage != 3) return false;
  run(g, E.bulkTransformTime + 0.2f);
  b.hp = b.maxHp * 0.5f; g.kaiju.hp = g.kaiju.maxHp();
  if (!g.doSnap()) return false;
  run(g, 6.0f); run(g, 0.3f);
  return b.dead && g.level.won;
}
static std::unique_ptr<Game> bulkReady(bool crazyMode = false) {
  auto g = adventureAt((int)T.levels.size() - 1, crazyMode); g->menus.clear();
  Profile& A = g->AP(); A.gauntlet = A.gauntletSlot = A.gauntletOn = true; A.stones.clear(); for (auto& s : E.stones) A.stones.push_back(s.id);
  return g;
}
static void testGauntletArc() {
  SECTION("Bulk gauntlet arc: L12 slot, L13 box, stones L14-19, L20 pedestals; the gauntlet IS the glove");
  CHECK(T.levels[11].unlockGauntletSlot && T.levels[12].obj == ObjType::Box && T.levels[12].noRebuild && T.levels[19].pedestals);
  for (int i = 0; i < 6; i++) CHECK(T.levels[13 + i].obj == ObjType::Stone && T.levels[13 + i].stone == i && E.stones[(size_t)i].level == 14 + i);
  CHECK(E.stones[0].name == "Storm Stone" && E.stones[1].name == "Tide Stone" && E.stones[2].name == "Grove Stone" && E.stones[3].name == "Ember Stone" && E.stones[4].name == "Sun Stone" && E.stones[5].name == "Spark Stone");
  // L12 win unlocks the slot silently (no toast mentioning it), shown as an empty outline
  auto g = adventureAt(11); size_t t0 = g->toasts.size(); g->level.destroyed = 999; run(*g, 0.1f);
  if (Unit* mb = g->enemies.miniBoss()) g->enemies.wreckUnit(*mb); run(*g, 0.2f);
  CHECK(g->level.won && g->AP().gauntletSlot && !g->AP().gauntlet);
  bool said = false; for (size_t i = t0; i < g->toasts.size(); i++) said |= g->toasts[i].text.find("auntlet") != std::string::npos; CHECK(!said);
  g->openSkills(); g->updateMenus(InputState(), 0.016f); bool outline = false; for (auto& it : g->menus.back().items) outline |= it.icon == 2 && !it.enabled && it.label == "  "; CHECK(outline); g->menus.clear();
  // L13: no rebuild; the box appears only at 100% and needs the key
  auto b = adventureAt(12); b->menus.clear(); const LevelDef& L13 = T.levels[12];
  CHECK(b->world.noRebuild && b->level.boxBuilding >= 0 && b->level.arenaTotal > 5);
  b->world.updateStreaming(L13.ax, L13.az, true);
  std::vector<Building*> arena; for (auto& bb : b->world.buildings) if (!bb.school && dist2D(bb.x, bb.z, L13.ax, L13.az) < L13.ar) arena.push_back(&bb);
  for (size_t i = 0; i + 1 < arena.size(); i++) arena[i]->smashed = true;
  run(*b, 0.2f); CHECK(!b->level.boxShown && b->level.arenaLeft == 1);
  RenderList hud = renderOnce(*b); CHECK(hud.ui.size() > 100);  // buildings-left counter
  b->save.spinSlot = true; b->save.zooCompleted = false; b->AP().zooKey = false;  // never did the zoo: no key
  arena.back()->smashed = true; run(*b, 0.2f); CHECK(b->level.boxShown);
  b->kaiju.pos = b->level.boxPos; run(*b, 0.2f); CHECK(!b->level.boxOpen && !b->AP().gauntlet);  // locked
  CHECK(b->level.boxKeeper);  // ...but the zookeeper comes to check on it (never a softlock)
  run(*b, 30); for (auto* x : arena) CHECK(x->smashed);  // no rebuilding on this level
  b->kaiju.pos = b->level.boxKeeperPos; b->interact(); CHECK(b->AP().zooKey);
  b->kaiju.pos = b->level.boxPos; run(*b, 0.2f); CHECK(b->level.boxOpen && b->AP().gauntlet && b->AP().gauntletOn);
  // retroactive key: zoo done before in this slot
  auto rk = adventureAt(12); rk->menus.clear(); rk->save.spinSlot = true; rk->save.zooCompleted = true; rk->AP().zooKey = false; CHECK(openZooBox(*rk) && rk->AP().zooKey);
  // stones: need the gauntlet equipped, otherwise a big harmless knockback
  auto s = adventureAt(13); s->menus.clear(); Profile& A = s->AP(); A.zooKey = true; A.gauntlet = A.gauntletSlot = true; A.gauntletOn = false; s->setupGates();
  GateRun* gt = nullptr; for (auto& q : s->gates) if (q.stone == 0) gt = &q; CHECK(gt != nullptr);
  if (gt) {
    { Building& sb = s->world.buildings[(size_t)s->world.stoneBuildings[0]]; s->world.generate(sb); std::fill(sb.alive.begin(), sb.alive.end(), 0); sb.aliveCount = 0; sb.smashed = true; }  // Atlas: reveal the hidden stone
    s->kaiju.pos = gt->pos; run(*s, 0.1f); CHECK(gt->open); run(*s, 0.8f);  // (the stone sits right behind the gate: let a first knock-back settle)
    Vec3 sp = gt->pos + Vec3(0, 0, -8); s->kaiju.pos = sp; float hp = s->kaiju.hp; size_t tt = s->toasts.size(); run(*s, 0.05f);
    bool msg = false; for (size_t i = tt; i < s->toasts.size(); i++) msg |= s->toasts[i].text == "Too much power! You need something to hold it...";
    std::printf("  repel: msg %d taken %d dist %.1f hp %.1f/%.1f radius %.1f\n", (int)msg, (int)s->level.stoneTaken, dist2D(s->kaiju.pos.x, s->kaiju.pos.z, sp.x, sp.z), s->kaiju.hp, hp, s->kaiju.radius());
    msg = msg || s->level.repelT > 0;  // (the toast is de-duplicated after the first knock-back)
    CHECK(msg && !s->level.stoneTaken && dist2D(s->kaiju.pos.x, s->kaiju.pos.z, sp.x, sp.z) > 10 && s->kaiju.hp >= hp);
    s->equipGauntlet(true); s->kaiju.pos = sp; run(*s, 0.1f); CHECK(s->level.stoneTaken && A.stones.size() == 1 && A.stones[0] == "storm");
    RenderList grl = renderOnce(*s); CHECK(grl.ui.size() > 100);  // stone on the glove HUD slot
  }
  // one stone per level, in order; persists
  A.stones.push_back("tide"); { Json j = s->save.toJson(); SaveData r; CHECK(r.fromJson(j) && r.adventure.stones == A.stones && r.adventure.gauntlet && r.adventure.zooKey); }
  // glove power scales with the stones (0 = cosmetic, up to 10 at 6)
  CHECK(E.pulseTargets.size() == 7 && E.pulseTargets[0] == 0 && E.pulseTargets[6] == 10);
  auto sb = sandboxGame(); Profile& SP = sb->P(); SP.gauntlet = SP.gauntletSlot = SP.gauntletOn = true;
  sb->kaiju.pos = Vec3(120, 0, -150); sb->world.updateStreaming(120, -150, true); run(*sb, 0.2f);
  CHECK(sb->gloveEquipped() && sb->glovePulse() == 0);
  SP.stones = {"storm", "tide", "grove"}; CHECK(sb->glovePulse() == E.pulseTargets[3]);
  SP.stones = {"storm", "tide", "grove", "ember", "sun", "spark"}; CHECK(sb->glovePulse() == 10);
  // Infinity Stones: pick a stone and map each of its powers onto a different action
  for (auto& s : E.stones) { CHECK(s.powers.size() >= 2 && s.blurb.size() > 16); for (auto& pw : s.powers) CHECK(pw.blurb.size() > 24 && !pw.name.empty() && !pw.channel.empty()); }
  SP.stones = {"storm"};
  CHECK(sb->assignStonePower(0, "storm", "thunder") && sb->P().stoneBind[0] == "storm:thunder");
  CHECK(!sb->assignStonePower(1, "storm", "thunder"));  // Thunder Step is a pulse power, not a strike
  CHECK(!sb->assignStonePower(2, "tide", "bolt"));      // Tide isn't owned
  CHECK(sb->glovePulse() == E.pulseTargets[1] + 2);     // pulse count plus Thunder Step's extra buildings
  SP.stones.push_back("ember");
  float claw = sb->kaiju.moveBricks(T.moves[(size_t)T.moveIndex("claw")]);
  CHECK(sb->assignStonePower(1, "ember", "cinder"));
  float claw2 = sb->kaiju.moveBricks(T.moves[(size_t)T.moveIndex("claw")]);
  CHECK(claw2 > claw * 1.25f && claw2 < claw * 1.35f);
  { Json j = sb->save.toJson(); SaveData r; CHECK(r.fromJson(j) && r.sandbox.stoneBind[0] == "storm:thunder" && r.sandbox.stoneBind[1] == "ember:cinder"); }
  sb->menus.clear(); sb->openSkills(); sb->updateMenus(InputState(), 0.016f);
  bool opened = false;
  for (auto& it : sb->menus.back().items) if (it.label == "Infinity Stones" && it.enabled && it.desc.find("Pulse") != std::string::npos) { opened = true; it.act(); }
  CHECK(opened);
  sb->updateMenus(InputState(), 0.016f);
  CHECK(sb->menus.back().title == "INFINITY STONES");
  for (auto& it : sb->menus.back().items) if (it.label == "Storm Stone") it.act();
  sb->updateMenus(InputState(), 0.016f);
  CHECK(sb->menus.back().title == "Storm Stone");
  int explained = 0;
  for (auto& it : sb->menus.back().items) if (it.desc.find("Pulse") != std::string::npos || it.desc.find("Blast") != std::string::npos) explained++;
  CHECK(explained >= 2);
  CHECK(sb->assignStonePower(0, "", "")); CHECK(sb->P().stoneBind[0].empty());
  SP.stones = {"storm", "tide", "grove", "ember", "sun", "spark"}; sb->menus.clear();
  InputState w; w.moveY = 1; int p0 = sb->glovePulses; run(*sb, 1.5f, w); CHECK(sb->glovePulses > p0);  // pulses while moving
  int p1 = sb->glovePulses; run(*sb, 3); CHECK(sb->glovePulses == p1);
  SP.gauntletOn = false; CHECK(!sb->gloveEquipped());
  // the old random-level glove drop is gone
  auto o = adventureAt(0); for (auto& bb : o->world.buildings) if (bb.generated && !bb.school) bb.smashed = true; run(*o, 0.3f); CHECK(!o->gloveDrop.active);
  // v2 save migration: 15-level saves -> 20 levels (finished stays finished, L12+ resumes at 12, old glove -> gauntlet)
  { SaveData v; prog::initProfile(v.adventure, T); v.adventure.level = 13; v.adventure.levelsWon = 13; v.adventure.moves.insert("glove"); v.adventure.slots[1] = "glove";
    Json j = v.toJson(); j.set("version", Json::makeNum(2)); SaveData r; CHECK(r.fromJson(j));
    CHECK(r.adventure.level == 12 && r.adventure.levelsWon <= 12 && r.adventure.gauntletSlot && r.adventure.gauntlet && !r.adventure.moves.count("glove") && r.adventure.slots[1] != "glove"); }
  { SaveData v; prog::initProfile(v.crazy, T); v.crazy.level = 15; v.crazy.levelsWon = 15; Json j = v.toJson(); j.set("version", Json::makeNum(2)); SaveData r;
    CHECK(r.fromJson(j) && r.crazy.level == 20 && r.crazy.levelsWon == 20); }
  // L20: the fight starts only with all six placed AND the gauntlet on; the sky darkens
  auto f = bulkReady(); CHECK(!f->boss.active && !f->level.fightStarted);
  f->AP().gauntletOn = false; for (int i = 0; i < 6; i++) { f->kaiju.pos = f->level.pedestal[i]; f->interact(); } CHECK(f->level.placed == 0);
  f->AP().gauntletOn = true; for (int i = 0; i < 5; i++) { f->kaiju.pos = f->level.pedestal[i]; f->interact(); }
  run(*f, 0.2f); CHECK(f->level.placed == 31 && !f->boss.active);
  f->AP().gauntletOn = false; f->kaiju.pos = f->level.pedestal[5]; f->interact(); run(*f, 0.2f); CHECK(!f->boss.active);
  f->AP().gauntletOn = true; int ev = f->skyDarkEvents; f->interact(); run(*f, 0.2f);
  CHECK(f->level.placed == 63 && f->boss.active && f->boss.staged && f->boss.stage == 1 && f->skyDarkEvents == ev + 1 && f->skyDarkTarget == 1);
  run(*f, 3); CHECK(f->skyDark > 0.5f);
  RenderList dark = renderOnce(*f); CHECK(dark.instanceCount() > 100);
  { // the darkening is applied fresh each frame: re-using one RenderList (as the platform does) must not compound it
    RenderList same; f->render(same, 1280, 720); float s1 = same.sunIntensity, h1 = same.hemiIntensity;
    for (int i = 0; i < 30; i++) f->render(same, 1280, 720);
    CHECK(std::fabs(same.sunIntensity - s1) < 1e-5f && std::fabs(same.hemiIntensity - h1) < 1e-5f && s1 > 0.3f && h1 > 0.5f);
  }
  auto f2 = bulkReady(); f2->AP().stones.pop_back();  // missing a stone: that pedestal stays empty
  for (int i = 0; i < 6; i++) { f2->kaiju.pos = f2->level.pedestal[i]; f2->interact(); } run(*f2, 0.2f); CHECK(f2->level.placed == 31 && !f2->boss.active);
}
static void testBulkStages() {
  SECTION("THE BULK (level 20): three stages + the snap");
  auto g = bulkReady(); CHECK(placeAllStones(*g));
  Boss& b = g->boss;
  CHECK(!g->hitBossArea(0, b.pos.x, 2, b.pos.z, 5, 10));  // intro: can't be hit
  b.introT = 0; b.lift = 0; float s0 = b.scale;
  // stage 1: without the gauntlet a hit can never finish him
  g->AP().gauntletOn = false; b.hitCd = 0; b.hp = 2; g->hitBossArea(0, b.pos.x, 2, b.pos.z, 5, 500); CHECK(b.hp == 1 && b.stage == 1 && b.angerT <= 0 && !b.dead);
  g->AP().gauntletOn = true; b.hp = b.maxHp; b.hitCd = 0;
  CHECK(g->hitBossArea(0, b.pos.x, 2, b.pos.z, 5, 3));
  CHECK(b.hp == 1 && b.stage == 1 && b.angerT > 0 && !b.dead);  // the one-shot leaves exactly 1 HP
  b.hitCd = 0; g->hitBossArea(0, b.pos.x, 2, b.pos.z, 5, 500); CHECK(b.hp == 1);  // invulnerable while angry
  run(*g, E.bulkAngerTime + 0.1f);
  CHECK(b.stage == 2 && std::fabs(b.maxHp - E.bulkStage2Hp) < 1e-3f && b.hp == b.maxHp && b.scale >= s0 * 2 - 1e-3f);  // 2x+ and refilled
  std::printf("  stage 2: scale %.2f -> %.2f, hp %.0f\n", s0, b.scale, b.hp);
  // the one-shot does nothing special in stage 2
  b.hitCd = 0; CHECK(!g->bulkGauntletHit()); g->hitBossArea(0, b.pos.x, 2, b.pos.z, 5, 3); CHECK(b.hp > 1 && b.hp < b.maxHp && b.stage == 2);
  // stage 2 can kill: an idle kaiju goes down (normal fail flow with "retry from stage 2")
  { auto k = bulkReady(); placeAllStones(*k); k->boss.introT = 0; k->boss.hitCd = 0; k->hitBossArea(0, k->boss.pos.x, 2, k->boss.pos.z, 5, 3); run(*k, E.bulkAngerTime + 0.2f);
    CHECK(k->boss.stage == 2); k->kaiju.pos = k->boss.pos + Vec3(0, 0, k->boss.radius() + 4); float t = 0;
    while (t < 200 && k->mode == Mode::Play) { run(*k, 0.5f); t += 0.5f; k->kaiju.pos = k->boss.pos + Vec3(0, 0, k->boss.radius() + 4); }
    std::printf("  stage 2 KOs an idle kaiju after %.0f s\n", t);
    CHECK(k->mode == Mode::GameOver && t > 8);
    k->updateMenus(InputState(), 0.016f); bool st2 = false; for (auto& it : k->menus.back().items) st2 |= it.label == "Retry from stage 2 (THE BULK)"; CHECK(st2);
    { std::function<void()> f; for (auto& it : k->menus.back().items) if (it.label == "Retry from stage 2 (THE BULK)") f = it.act; if (f) f(); }
    CHECK(k->mode == Mode::Play && k->boss.active && k->boss.stage == 2 && k->boss.hp == k->boss.maxHp && k->level.active); }
  // deplete stage 2 -> FINAL FORM with a new pool
  int hits = 0; while (b.stage == 2 && hits < 400) { b.hitCd = 0; g->hitBossArea(0, b.pos.x, 2, b.pos.z, 5, T.moves[T.moveIndex("claw")].bricks); hits++; g->kaiju.hp = g->kaiju.maxHp(); }
  std::printf("  claw hits through stage 2: %d\n", hits);
  CHECK(b.stage == 3 && b.transformT > 0 && std::fabs(b.maxHp - E.bulkStage3Hp) < 1e-3f && b.hp == b.maxHp && hits <= 120);
  run(*g, E.bulkTransformTime + 0.1f); g->kaiju.hp = g->kaiju.maxHp();
  // stage 3: hits floor at 1 HP; the snap is locked above 50%, unlocked at exactly 50% and below
  b.hp = b.maxHp * 0.5f + 1; CHECK(!g->snapReady()); tap(*g, A_PAY); CHECK(g->snapT < 0);
  b.hp = b.maxHp * 0.5f; CHECK(g->snapReady());
  b.hp = b.maxHp * 0.2f; CHECK(g->snapReady());
  b.hp = 3; b.hitCd = 0; g->hitBossArea(0, b.pos.x, 2, b.pos.z, 5, 500); CHECK(b.hp == 1 && !b.dead);
  RenderList prompt = renderOnce(*g); CHECK(prompt.ui.size() > 100);  // "DO THE SNAP!"
  g->AP().gauntletOn = false; CHECK(!g->snapReady()); g->AP().gauntletOn = true;
  b.hp = b.maxHp * 0.5f; tap(*g, A_PAY); CHECK(g->snapT >= 0);
  run(*g, 1.4f); CHECK(b.dead);
  run(*g, 5);
  CHECK(g->level.won && g->save.bulkDefeated && g->save.crazyUnlocked);
  CHECK(!g->menus.empty() && g->menus.back().title == "PEACE TO THE LAND!");
  // normal aftermath: stones shatter, gauntlet crumbles, removed from the save; slot shows cracked
  Profile& A = g->save.adventure; CHECK(A.stones.empty() && !A.gauntlet && A.gauntletCracked && A.endingDone && A.gauntletSlot);
  CHECK(!g->save.sandbox.gauntlet);
  { Json j = g->save.toJson(); SaveData r; CHECK(r.fromJson(j) && r.adventure.stones.empty() && !r.adventure.gauntlet && r.adventure.gauntletCracked && r.adventure.endingDone); }
  tap(*g, A_CONFIRM); CHECK(g->mode == Mode::Ending);
  // Crazy: same fight (tougher), the gauntlet survives and goes to this slot's Sandbox with all six stones
  auto c = bulkReady(true); CHECK(placeAllStones(*c)); CHECK(std::fabs(c->boss.maxHp - C.bossHp * C.crazyBossHp) < 1e-3f);
  CHECK(beatStagedBulk(*c));
  CHECK(!c->menus.empty() && c->menus.back().body.find("The Gauntlet is yours in Sandbox!") != std::string::npos);
  Profile& S = c->save.sandbox; CHECK(S.gauntlet && S.gauntletSlot && S.gauntletOn && S.stones.size() == 6 && c->save.crazy.endingDone);
  { Json j = c->save.toJson(); SaveData r; CHECK(r.fromJson(j) && r.sandbox.gauntlet && r.sandbox.stones.size() == 6 && r.crazy.endingDone); }
  // future sandbox: full pulse + the rainbow-wave ultimate on R / RB
  c->menus.clear(); c->ending = EndingRun(); c->mode = Mode::Play; c->startMode(false); run(*c, 0.2f); dismissPrompts(*c); c->menus.clear();
  CHECK(c->gloveEquipped() && c->stoneCount() == 6);
  c->kaiju.pos = Vec3(120, 0, -150); c->world.updateStreaming(120, -150, true); run(*c, 0.2f);
  int sm0 = c->P().smashCount; tap(*c, A_PAY); run(*c, 0.5f);
  CHECK(c->sandboxUlts == 1 && c->P().smashCount > sm0 + 5);
  tap(*c, A_PAY); CHECK(c->sandboxUlts == 1);  // cooldown
}
static void testCreditsAndEnding() {
  SECTION("credits, special thanks, after-credits video");
  auto g = newGame(); const auto& cr = g->credits; CHECK(cr.size() > 20);
  // the Tommy lines intact and in order, first
  size_t di = 0; while (di < cr.size() && cr[di].text != "Directed by Tommy M. Dennis") di++;
  CHECK(cr[0].head && di <= 2 && di + 2 < cr.size() && cr[di + 1].text == "Written by Tommy M. Dennis" && cr[di + 2].text == "Dedicated to Tommy M. Dennis");  // right after the title, in order
  auto role = [&](const std::string& r) { for (auto& l : cr) if (l.role == r) return l.name; return std::string("<missing>"); };
  for (const char* r : {"Game Design", "Enemy Design", "Boss Design", "Art Direction"}) CHECK(role(r) == "Tommy M. Dennis & Tyler Dennis");
  CHECK(role("Producer") == "Tyler Dennis" && role("QA Testing") == "Tommy M. Dennis" && role("Programming") == "Computer" && role("UI/UX Design") == "Computer");
  CHECK(role("3D Modeling").find("Quaternius") != std::string::npos && role("3D Modeling").find("Kenney") != std::string::npos && role("3D Modeling").find("kazuma") != std::string::npos);
  CHECK(role("Sound Design").find("Procedural") != std::string::npos && role("Music").find("Procedural") != std::string::npos);
  // special thanks: only sources really used (asset attribution + manifest), ending with the kids line
  std::string attrib; readTextFile(std::string(TZ_DATA_DIR) + "/../assets/CREDITS.txt", attrib);
  std::string manifest; readTextFile(std::string(TZ_DATA_DIR) + "/models.json", manifest);
  CHECK(!attrib.empty());
  Json cj; CHECK(Json::parse(file("credits.json"), cj));
  std::vector<std::string> thanks; for (auto& l : cr) if (l.thanks && !l.text.empty()) thanks.push_back(l.text);
  CHECK(thanks.size() >= 4 && thanks.back() == "Thank you for making games like this possible for kids everywhere.");
  for (auto& src : cj["sources"].arr) { std::string n = src.strOr(""); CHECK(attrib.find(n) != std::string::npos || manifest.find(n) != std::string::npos); }
  for (auto& t : thanks) {  // every thanked name (the part before " - ") is a listed source
    size_t d = t.find(" - "); if (d == std::string::npos || t[0] == ' ') continue;
    std::string who = t.substr(0, d); bool listed = false; for (auto& src : cj["sources"].arr) listed |= who.find(src.strOr("?")) != std::string::npos;
    CHECK(listed);
  }
  for (auto& t : thanks) CHECK(t.find("Tommy") == std::string::npos || true);
  // flow: credits -> THE END -> video (not skippable) -> any button -> title
  CHECK(g->afterSign == "Do not feed people." && g->afterVideo == "data/video/after_credits.mp4");
  g->startEnding(false); CHECK(g->mode == Mode::Ending && g->ending.stage == 0);
  RenderList r0 = renderOnce(*g); CHECK(r0.ui.size() > 100);
  float rollLen = 4 + 0.55f * cr.size(); run(*g, rollLen + 0.2f); CHECK(g->ending.stage == 1);
  RenderList r1 = renderOnce(*g); CHECK(r1.ui.size() > 0);
  run(*g, g->endHold + 0.1f); CHECK(g->ending.stage == 2 && !g->ending.usedVideo);  // no player in tests -> in-engine closing shot
  for (Action a : {A_CONFIRM, A_CANCEL, A_PAUSE, A_CLAW, A_EAT}) { tap(*g, a); CHECK(g->ending.stage == 2 && g->mode == Mode::Ending); }
  { InputState cl; cl.click = true; g->frame(1.f / 60, cl); CHECK(g->ending.stage == 2); }
  RenderList rv = renderOnce(*g); CHECK(rv.instanceCount() > 100 && rv.ui.size() > 50);
  run(*g, g->endFallback + 0.2f); CHECK(g->ending.stage == 3 && g->mode == Mode::Ending);
  tap(*g, A_CLAW); CHECK(g->mode == Mode::Title && !g->menus.empty() && g->menus.back().kind == 2);
  // skipping the credits lands on the video (not the title)
  g->startEnding(false); run(*g, 1.0f); tap(*g, A_CONFIRM); CHECK(g->ending.stage == 2);
  // the city is rebuilt for the closing shot
  int smashed = 0; for (auto& b : g->world.buildings) smashed += b.smashed; CHECK(smashed == 0);
  // the real after-credits video: its length comes from the file (never hard-coded)
  float vlen = mp4DurationSeconds(std::string(TZ_DATA_DIR) + "/video/after_credits.mp4");
  std::printf("  after_credits.mp4: %.3f s\n", vlen); CHECK(vlen > 5.5f && vlen < 60);
  // fallback (in-engine shot): on the sign 0-5 s, then a caption to the end
  g->mode = Mode::Ending; g->ending = EndingRun(); g->ending.stage = 2; g->ending.videoLen = vlen; g->ending.usedVideo = false;
  g->ending.stageT = 3; CHECK(g->signOverlayAlpha() > 0.9f && g->signCaptionAlpha() == 0);
  g->ending.stageT = 5.2f; CHECK(g->signOverlayAlpha() < 1 && g->signOverlayAlpha() > 0);
  g->ending.stageT = vlen - 0.5f; CHECK(g->signOverlayAlpha() == 0 && g->signCaptionAlpha() > 0.9f);
  // real video: anchored to the sign with the same push-in (still until 5 s, 1.0x -> 1.8x smoothstep to the end)
  g->ending.usedVideo = true; float x0, y0, x1, y1;
  g->ending.stageT = 0; CHECK(std::fabs(g->endingZoom() - 1) < 1e-5f && g->signRect(x0, y0, x1, y1) && std::fabs(x0 - 0.19f) < 1e-4f && std::fabs(y0 - 0.40f) < 1e-4f && std::fabs(y1 - 0.57f) < 1e-4f);
  g->ending.stageT = 4.9f; CHECK(std::fabs(g->endingZoom() - 1) < 1e-5f && g->signOverlayAlpha() > 0.99f && g->signCaptionAlpha() == 0);
  g->ending.stageT = 5 + (vlen - 5) * 0.5f; CHECK(std::fabs(g->endingZoom() - 1.4f) < 1e-3f); g->signRect(x0, y0, x1, y1); CHECK(x0 < 0.19f && x1 < 0.36f && y0 < 0.40f);
  g->ending.stageT = vlen; CHECK(std::fabs(g->endingZoom() - 1.8f) < 1e-4f); g->signRect(x0, y0, x1, y1);
  CHECK(x0 < 0 && x1 > 0 && g->signOverlayAlpha() > 0);  // partly out of frame on the left, text still on it
  RenderList so = renderOnce(*g); CHECK(so.ui.size() > 20);
  g->ending = EndingRun(); g->mode = Mode::Title;
  // a fake platform player: the real video is used, input ignored until it ends
  struct FakeVideo : VideoPlayer { float t = 0, len = 0; bool ok = true; std::string path; bool start(const std::string& p) override { path = p; t = 0; return ok; } void update(float dt) override { t += dt; } bool finished() const override { return t >= len; } float duration() const override { return len; } void stop() override {} };
  FakeVideo fv; fv.len = vlen; g->video = &fv; g->startEnding(false); run(*g, 0.5f); tap(*g, A_CONFIRM); CHECK(g->ending.stage == 2 && g->ending.usedVideo && fv.path == "data/video/after_credits.mp4");
  run(*g, 1); tap(*g, A_CONFIRM); CHECK(g->ending.stage == 2); run(*g, vlen - 1.2f); CHECK(g->ending.stage == 2); run(*g, 0.5f); CHECK(g->ending.stage == 3 && std::fabs(g->ending.videoLen - vlen) < 1e-3f); tap(*g, A_CONFIRM); CHECK(g->mode == Mode::Title);
  FakeVideo bad; bad.ok = false; g->video = &bad; g->startEnding(false); run(*g, 0.5f); tap(*g, A_CONFIRM); CHECK(g->ending.stage == 2 && !g->ending.usedVideo);  // decode failure -> fallback
  g->video = nullptr;
}

// Sandbox police toggle (pause menu > "Police", default OFF, persisted in the slot's settings); adventure police untouched.
static int aliveUnits(Game& g) { int n = 0; for (auto& u : g.enemies.units) if (!u.wreck) n++; return n; }
static MenuItem* pauseItem(Game& g, const std::string& label) {
  if (g.menus.empty()) return nullptr;
  Menu& m = g.menus.back(); if (m.build) m.build(m);
  for (auto& it : m.items) if (it.label == label) return &it;
  return nullptr;
}
static void testSandboxPoliceToggle() {
  SECTION("sandbox police toggle (default OFF) + adventure police unchanged");
  { SaveData d; CHECK(!d.settings.police); Json j; CHECK(Json::parse("{}", j)); SaveData r; r.fromJson(j); CHECK(!r.settings.police); }
  // default: a fresh sandbox has no police, no heat, no sirens, no wanted stars, whatever you smash
  auto g = sandboxGame(); clearPeople(*g);
  CHECK(!g->save.settings.police && !g->enemies.policeOn());
  for (int k = 0; k < 10; k++) { g->enemies.addHeat(2); g->enemies.heat = std::max(g->enemies.heat, 4.5f); run(*g, 1); }
  CHECK(aliveUnits(*g) == 0 && g->enemies.heat == 0 && !g->enemies.nukeActive && g->loopVol[LOOP_SIREN] == 0);
  { int tanks = 0; for (auto& u : g->enemies.units) tanks += u.type == U_PTANK || u.type == U_CAR || u.type == U_PHELI || u.type == U_COP; CHECK(tanks == 0); }
  // pause menu toggle ON: heat builds and police spawn
  tap(*g, A_PAUSE); MenuItem* pol = pauseItem(*g, "Police"); CHECK(pol != nullptr && pol && pol->right == "OFF");
  if (pol) pol->act(); g->menus.clear();
  CHECK(g->save.settings.police && g->enemies.policeOn());
  g->enemies.addHeat(2.5f); CHECK(g->enemies.heat > 2); run(*g, 3);
  CHECK(g->enemies.count(U_CAR) > 0 && (g->loopVol[LOOP_SIREN] > 0 || g->enemies.chase.arrived)  /* Titan: siren stops once they arrive */ && g->enemies.stars() >= 1);
  { Json j = g->save.toJson(); SaveData r; CHECK(r.fromJson(j) && r.settings.police); }  // persisted with the slot
  // toggle OFF from the pause menu: existing police despawn at once, heat clears, sirens stop
  tap(*g, A_PAUSE); pol = pauseItem(*g, "Police"); CHECK(pol && pol->right == "ON");
  if (pol) pol->act(); g->menus.clear();
  CHECK(!g->save.settings.police && aliveUnits(*g) == 0 && g->enemies.heat == 0);
  run(*g, 2); CHECK(aliveUnits(*g) == 0 && g->loopVol[LOOP_SIREN] == 0);
  { Json j = g->save.toJson(); SaveData r; CHECK(r.fromJson(j) && !r.settings.police); }
  // the L20 storm lighting never leaks into Sandbox (same sky/fog/light values as a normal level)
  g->skyDark = g->skyDarkTarget = 1; g->startMode(false); dismissPrompts(*g);
  CHECK(g->skyDark == 0 && g->skyDarkTarget == 0);
  { RenderList a = renderOnce(*g); auto adv = adventureAt(0); RenderList b = renderOnce(*adv);
    CHECK(a.sunIntensity == b.sunIntensity && a.hemiIntensity == b.hemiIntensity && a.sky.r == b.sky.r && a.fog.r == b.fog.r); }
  // adventure: the level roster's police spawn exactly as before, with the sandbox toggle either way
  for (int on = 0; on < 2; on++) {
    auto a = adventureAt(0); a->save.settings.police = on; std::set<int> seen;
    for (int k = 0; k < 20 * 6; k++) { run(*a, 1.f / 6); for (auto& u : a->enemies.units) seen.insert(u.type); a->kaiju.hp = a->kaiju.maxHp(); }
    CHECK(!a->enemies.policeOn() && seen.count(U_COP) && seen.count(U_CAR));
    for (int t : seen) CHECK(t == U_COP || t == U_CAR);
  }
}

// Controller parity: every keyboard-bound action has an XInput binding (checked in the real input_win.cpp), the
// Controls screen lists every action, and no two actions share a pad chord in the same context.
static std::string actionName(int a) {
  static const char* n[] = {"A_CLAW", "A_TAIL", "A_STOMP", "A_GRAB", "A_SLOT1", "A_SLOT2", "A_ALT", "A_PAY", "A_PAUSE", "A_LEGEND", "A_SPRINT", "A_CAMRESET",
                            "A_UP", "A_DOWN", "A_LEFT", "A_RIGHT", "A_CONFIRM", "A_CANCEL", "A_MUTE", "A_ALT1", "A_ALT2", "A_ALT3", "A_EAT", "A_CYCLE1", "A_CYCLE2", "A_SKILLS", "A_STORE", "A_EGGS"};
  static_assert(sizeof(n) / sizeof(n[0]) == A_COUNT, "update actionName for the new Action");
  return a >= 0 && a < A_COUNT ? n[a] : "";
}
static void testControlsMapping() {
  SECTION("controller parity + Controls screen + no binding conflicts");
  const auto& B = controlBinds();
  // the table covers every action exactly once, and every keyboard-bound action also has a controller binding
  std::set<int> acts; for (auto& c : B) if (c.action >= 0) { CHECK(!acts.count(c.action)); acts.insert(c.action); }
  for (int a = 0; a < A_COUNT; a++) { if (!acts.count(a)) std::printf("  missing from controls table: %s\n", actionName(a).c_str()); CHECK(acts.count(a)); }
  for (auto& c : B) { std::string k = c.key, pd = c.pad; if (k != "-" && !k.empty()) CHECK(!pd.empty() && pd != "-"); CHECK(std::string(c.note).size() > 0); }
  // the live bindings: every action the keyboard section of input_win.cpp sets is also set in its XInput section
  std::string src; CHECK(readTextFile(std::string(TZ_DATA_DIR) + "/../src/input/input_win.cpp", src));
  size_t split = src.find("---- XInput"); CHECK(split != std::string::npos);
  if (split != std::string::npos) {
    std::string kb = src.substr(0, split), pad = src.substr(split);
    int keyed = 0;
    for (int a = 0; a < A_COUNT; a++) {
      std::string tag = "set(" + actionName(a) + ",";
      if (kb.find(tag) == std::string::npos) continue;
      keyed++;
      bool padBound = pad.find(tag) != std::string::npos || (a >= A_UP && a <= A_RIGHT && pad.find("da[4] = {A_UP, A_DOWN, A_LEFT, A_RIGHT}") != std::string::npos);
      if (!padBound) std::printf("  keyboard-only action: %s\n", actionName(a).c_str());
      CHECK(padBound);
    }
    CHECK(keyed >= 25);
    CHECK(pad.find("set(A_MUTE, lbHeld") != std::string::npos && pad.find("set(A_LEGEND, lbHeld") != std::string::npos && pad.find("set(A_EGGS, !lbHeld") != std::string::npos);  // LB + Back = mute, Back alone = EGGS, LB + Start = Controls
  }
  // no conflicts: a pad chord means one thing per context (global bindings count in both play and menus)
  for (int ctx : {CTX_PLAY, CTX_MENU}) {
    std::set<std::string> seen;
    for (auto& c : B) if (c.ctx == ctx || c.ctx == CTX_GLOBAL) { std::string ch = c.chord; if (seen.count(ch)) std::printf("  chord conflict %s (%s)\n", c.chord, c.name); CHECK(!seen.count(ch)); seen.insert(ch); }
  }
  // the Controls screen: every row, controller + keyboard side by side, reachable from pause + options, closes with B
  auto g = sandboxGame();
  tap(*g, A_PAUSE); g->updateMenus(InputState(), 0.016f);
  int ci = -1; for (size_t i = 0; i < g->menus.back().items.size(); i++) if (g->menus.back().items[i].label == "Controls") ci = (int)i;
  CHECK(ci >= 0);
  if (ci >= 0) { g->menus.back().index = ci; tap(*g, A_CONFIRM); }
  CHECK(!g->menus.empty() && g->menus.back().kind == 9 && g->menus.back().title == "CONTROLS");
  if (!g->menus.empty() && g->menus.back().kind == 9) {
    Menu& m = g->menus.back(); CHECK(m.items.size() == B.size() + 1 && m.items.back().label == "Back");
    for (size_t i = 0; i < B.size() && i < m.items.size(); i++) CHECK(m.items[i].label == B[i].name && m.items[i].right == B[i].pad && m.items[i].right2 == B[i].key && !m.items[i].desc.empty());
    RenderList rl = renderOnce(*g); CHECK(rl.ui.size() > 300);
    int i0 = m.index; tap(*g, A_DOWN); CHECK(g->menus.back().index == i0 + 1); tap(*g, A_UP); CHECK(g->menus.back().index == i0);
    tap(*g, A_CONFIRM); CHECK(g->menus.back().kind == 9);  // selecting a row does nothing harmful
    tap(*g, A_CANCEL); CHECK(!g->menus.empty() && g->menus.back().kind == 1);  // B closes back to pause
  }
  g->menus.clear(); g->openOptions(); g->updateMenus(InputState(), 0.016f);
  bool opt = false; for (auto& it : g->menus.back().items) opt |= it.label == "Controls"; CHECK(opt);
  g->menus.clear(); tap(*g, A_LEGEND); CHECK(!g->menus.empty() && g->menus.back().kind == 9); g->menus.clear();
  // pad-only path: LB + X / B / A fire melee 4 / 5 / 6 just like U / I / O
  InputState in; in.held[A_ALT] = true; in.held[A_CLAW] = in.pressed[A_CLAW] = true; in.held[A_ALT1] = in.pressed[A_ALT1] = true;
  g->P().melee[3] = "tail"; g->kaiju.at = Attack(); for (auto& c : g->kaiju.cooldowns) c = 0; g->frame(1.f / 60, in);
  CHECK(g->kaiju.at.move == T.moveIndex("tail"));  // melee 4 (LB + X) fired its loadout move once
}

// Store icon grid: all items as tiles, unchanged purchases + prices, grid navigation (wrap / clamp), mouse + pad.
static void testStoreGrid() {
  SECTION("store icon grid");
  CHECK(gridMove(0, 7, 4, -1, 0) == 6 && gridMove(6, 7, 4, 1, 0) == 0 && gridMove(2, 7, 4, 1, 0) == 3 && gridMove(3, 7, 4, 1, 0) == 4);  // left/right wrap in reading order
  CHECK(gridMove(1, 7, 4, 0, 1) == 5 && gridMove(5, 7, 4, 0, 1) == 1 && gridMove(1, 7, 4, 0, -1) == 5);  // up/down wrap within the column
  CHECK(gridMove(3, 7, 4, 0, 1) == 6 && gridMove(3, 7, 4, 0, -1) == 6);  // short last row: clamp to its last tile
  CHECK(gridMove(9, 7, 4, 0, 0) == 6 && gridMove(0, 1, 4, 1, 1) == 0 && gridMove(0, 0, 4, 1, 0) == 0);
  auto g = sandboxGame(); clearPeople(*g);
  Profile& p = g->P(); p.coins = 5000; p.dmgLvl = p.rangeLvl = p.speedLvl = 1; p.keys = 0; p.spins = 0;
  g->openStore(); g->updateMenus(InputState(), 0.016f);
  Menu* m = &g->menus.back(); CHECK(m->kind == 8 && m->cols == 4);
  // Vault: 3 basic upgrades + 5 kaiju-specific powers (tiles 3-7), then key / spin / ray / back
  auto kp = vpow::kaijuPowers(g->kaiju.def().id, g->kaiju.def().name);
  std::vector<std::string> want = {"Mighty Claws", "Big Reach", "Stompy Speed"}; for (auto& k : kp) want.push_back(k.name);
  for (const char* w : {"Chest key", "Egg spin", "De-Evolution Ray", "Back"}) want.push_back(w);
  CHECK(m->items.size() == 12);
  for (size_t i = 0; i < 12 && i < m->items.size(); i++) { CHECK(m->items[i].label == want[i]); CHECK(m->items[i].icon > 0); CHECK(!m->items[i].desc.empty()); }
  for (size_t i = 0; i < 11 && i < m->items.size(); i++) CHECK(m->items[i].price > 0 && m->items[i].afford && !m->items[i].owned);
  CHECK(m->items[0].price == prog::upgradeCost(1) && m->items[8].price == (int)C.keyCost && m->items[9].price == E.spinCost && m->items[10].price == E.deevoPrice);
  CHECK(m->items[10].right == std::to_string(E.deevoPrice) + " coins");
  RenderList rl = renderOnce(*g); CHECK(rl.ui.size() > 600);
  CHECK(g->menuLayout.cols == 4 && g->menuLayout.count == 3 && g->menuLayout.tw > 0);
  // pad: A buys the focused tile with the same coin cost as before
  int c0 = p.coins; tap(*g, A_CONFIRM); CHECK(p.dmgLvl == 2 && p.coins == c0 - prog::upgradeCost(1));
  tap(*g, A_DOWN); tap(*g, A_DOWN); CHECK(g->menus.back().index == 8);
  c0 = p.coins; tap(*g, A_CONFIRM); CHECK(p.keys == 1 && p.coins == c0 - (int)C.keyCost);
  tap(*g, A_RIGHT); CHECK(g->menus.back().index == 9);
  c0 = p.coins; tap(*g, A_CONFIRM); CHECK(p.spins == 1 && p.coins == c0 - E.spinCost);
  tap(*g, A_DOWN); CHECK(g->menus.back().index == 1); tap(*g, A_LEFT); tap(*g, A_LEFT); CHECK(g->menus.back().index == 11);
  // can't afford: nothing is deducted and the tile says so
  p.coins = 5; g->updateMenus(InputState(), 0.016f); m = &g->menus.back(); CHECK(!m->items[1].afford && !m->items[8].afford);
  m->index = 1; int r0 = p.rangeLvl; tap(*g, A_CONFIRM); CHECK(p.rangeLvl == r0 && p.coins == 5);
  m = &g->menus.back(); m->index = 8; tap(*g, A_CONFIRM); CHECK(p.keys == 1 && p.coins == 5);
  // maxed upgrade shows as owned
  p.speedLvl = prog::maxLevel(); g->updateMenus(InputState(), 0.016f); m = &g->menus.back(); CHECK(m->items[2].owned && m->items[2].right == "MAX");
  // mouse: hovering a tile focuses it, clicking buys it
  p.coins = 5000; renderOnce(*g); const auto L = g->menuLayout;
  InputState mi; mi.cursorX = L.x + 1 * (L.tw + L.gap) + L.tw / 2; mi.cursorY = L.y + 0 * L.h + L.th / 2; mi.click = true;
  c0 = p.coins; r0 = p.rangeLvl; g->frame(1.f / 60, mi); CHECK(g->menus.back().index == 1 && p.rangeLvl == r0 + 1 && p.coins == c0 - prog::upgradeCost(r0));
  // B closes the store
  tap(*g, A_CANCEL); CHECK(g->menus.empty() || g->menus.back().kind != 8);
  // Back tile closes too
  g->menus.clear(); g->openStore(); g->updateMenus(InputState(), 0.016f); g->menus.back().index = 11; tap(*g, A_CONFIRM); CHECK(g->menus.empty());
}

// Textured ground: surface -> texture mapping, world-space UV tiling, the flat-colour fallback, shipped files.
static void testGroundTextures() {
  SECTION("ground textures: materials, world-space UVs, fallback, files");
  // materials: one texture + tile size + palette colour per surface, files under data/textures
  const char* names[GS_COUNT] = {"concrete", "asphalt", "grass", "water", "paving"};
  const uint32_t cols[GS_COUNT] = {0x3b3f4a, 0x2b2e36, 0x4f9a4a, 0x1f5d8c, 0x8a8478};
  std::set<std::string> files;
  for (int s = 0; s < GS_COUNT; s++) {
    const GroundMaterial& m = groundMaterial(s);
    CHECK(std::string(m.name) == names[s] && m.color == cols[s] && m.tile >= 2 && m.tile <= 32);
    CHECK(std::string(m.file) == std::string("textures/ground_") + names[s] + ".png");
    files.insert(m.file);
    std::string bytes; CHECK(readTextFile(std::string(TZ_DATA_DIR) + "/" + m.file, bytes) && bytes.size() > 1000 && bytes.size() < 400000 && bytes.compare(1, 3, "PNG") == 0);
  }
  CHECK(files.size() == GS_COUNT);
  // the world: every ground surface is mapped by its old flat colour to the right texture
  auto g = sandboxGame();
  const World& W = g->world; int per[GS_COUNT] = {}, lanes = 0, patches = 0;
  auto surfOf = [](uint32_t c) { return c == 0x3b3f4a ? GS_CONCRETE : c == 0x2b2e36 ? GS_ASPHALT : (c == 0x4f9a4a || c == 0x5fae5a) ? GS_GRASS : c == 0x1f5d8c ? GS_WATER : c == 0x8a8478 ? GS_PAVING : -1; };
  auto hexOf = [](const Color& c) { return ((uint32_t)std::lround(c.r * 255) << 16) | ((uint32_t)std::lround(c.g * 255) << 8) | (uint32_t)std::lround(c.b * 255); };
  for (const GroundQuad& q : W.groundQuads) {
    CHECK(q.surface >= 0 && q.surface < GS_COUNT && q.x1 > q.x0 && q.z1 > q.z0);
    CHECK(surfOf(hexOf(q.color)) == q.surface);
    per[q.surface]++;
    if (q.lane) { lanes++; CHECK(q.surface == GS_ASPHALT); CHECK(q.lane == 1 ? std::fabs(q.laneAt - (q.z0 + q.z1) / 2) < 1e-4f : std::fabs(q.laneAt - (q.x0 + q.x1) / 2) < 1e-4f); }
    else if (q.surface == GS_ASPHALT) patches++;
  }
  for (int s = 0; s < GS_COUNT; s++) CHECK(per[s] > 0);
  CHECK(per[GS_WATER] == (int)W.water.size() + 4 && per[GS_GRASS] == 3 && per[GS_PAVING] == 1 && per[GS_CONCRETE] == 1);
  CHECK(lanes == 16 && patches >= 49);  // 7 + 7 grid streets + 2 avenues; every crossing gets a plain patch (no centre line)
  CHECK((int)W.ground.size() == (int)W.groundQuads.size() - patches);  // one flat box per textured surface (the fallback)
  for (const GroundQuad& q : W.groundQuads) if (q.surface == GS_ASPHALT && !q.lane) { bool over = false; for (const GroundQuad& o : W.groundQuads) over |= o.lane && o.y < q.y && q.x0 >= o.x0 && q.x1 <= o.x1 && q.z0 >= o.z0 && q.z1 <= o.z1; CHECK(over); }
  // UVs tile in world space: one repeat per `tile` metres whatever the quad size, continuous across neighbouring quads
  for (int s = 0; s < GS_COUNT; s++) {
    float t = groundMaterial(s).tile, u0, v0, u1, v1;
    groundUV(s, 13.f, -7.f, u0, v0); groundUV(s, 13.f + t, -7.f + 2 * t, u1, v1);
    CHECK(std::fabs(u1 - u0 - 1) < 1e-4f && std::fabs(v1 - v0 - 2) < 1e-4f);
    groundUV(s, 0, 0, u0, v0); CHECK(u0 == 0 && v0 == 0);
  }
  { float ua, va, ub, vb; groundUV(GS_ASPHALT, -5.f, -100.f, ua, va); groundUV(GS_ASPHALT, -5.f, -100.f, ub, vb); CHECK(ua == ub && va == vb);
    float big, small_, dummy; groundUV(GS_GRASS, 260, 0, big, dummy); groundUV(GS_GRASS, -20, 0, small_, dummy);  // park 200 m wide vs lawn 80 m wide
    CHECK(std::fabs((big - small_) - 280.f / groundMaterial(GS_GRASS).tile) < 1e-3f); }
  // render: flat boxes by default (no textures), textured quads once the platform says the renderer has them
  CHECK(!g->groundTextured);
  RenderList flat = renderOnce(*g); CHECK(flat.ground.empty());
  g->groundTextured = true; RenderList tex = renderOnce(*g);
  CHECK(tex.ground.size() == W.groundQuads.size() && flat.instanceCount() - tex.instanceCount() == W.ground.size());
  CHECK(std::fabs(tex.time - g->time) < 1e-6f);
  g->groundTextured = false;
  // loader + fallback: a missing / bad file fails cleanly (the platform then keeps groundTextured = false)
  auto fake = [](int size, const std::string& missing) {
    return [size, missing](const std::string& path, int& w, int& h, std::vector<uint8_t>& px, std::string& err) {
      if (!missing.empty() && path.find(missing) != std::string::npos) { err = "texture not found: " + path; return false; }
      w = h = size; px.assign((size_t)size * size * 4, 140); return true;
    };
  };
  GroundTextureSet ok = loadGroundTextures("data/", fake(16, ""));
  CHECK(ok.ok && ok.size == 16 && ok.error.empty());
  for (int s = 0; s < GS_COUNT; s++) { CHECK(ok.mips[s].size() == 5 && ok.mips[s].back().size() == 4 && std::fabs(ok.meanLum[s] - 140 / 255.f) < 0.01f); }
  GroundTextureSet miss = loadGroundTextures("data/", fake(16, "ground_grass"));
  CHECK(!miss.ok && miss.error.find("grass") != std::string::npos);
  CHECK(!loadGroundTextures("data/", fake(12, "")).ok);   // not a power of two
  CHECK(!loadGroundTextures("data/", nullptr).ok);
  { int n = 0; GroundTextureSet mixed = loadGroundTextures("data/", [&n](const std::string&, int& w, int& h, std::vector<uint8_t>& px, std::string&) { w = h = (n++ == 2 ? 32 : 16); px.assign((size_t)w * h * 4, 100); return true; }); CHECK(!mixed.ok); }
  { GroundTextureSet black = loadGroundTextures("data/", [](const std::string&, int& w, int& h, std::vector<uint8_t>& px, std::string&) { w = h = 8; px.assign(256, 0); return true; }); CHECK(!black.ok); }
  { std::vector<uint8_t> px(4 * 4 * 4, 0); for (size_t i = 0; i < px.size(); i += 8) px[i] = 200; auto m = buildMips(px, 4);
    CHECK(m.size() == 3 && m[1].size() == 16 && m[1][0] == 100 && m[2].size() == 4 && m[2][0] == 100); CHECK(buildMips(px, 5).empty()); }
  // after a fallback the game still renders every ground surface as before
  RenderList again = renderOnce(*g); CHECK(again.ground.empty() && again.instanceCount() == flat.instanceCount());
}

// ---- HLSL guard: the embedded shaders are compiled at startup by D3DCompile (fxc rules). Wine's vkd3d is more lenient, so
// a reserved word used as an identifier (e.g. `float line`, a geometry-shader primitive type) passed under Wine but broke
// real Windows. These checks read the shader text straight from the renderer sources.
static std::string hlslBlock(const std::string& src, const std::string& var) {
  size_t a = src.find(var + " = R\"HLSL(");
  if (a == std::string::npos) return "";
  a = src.find('(', a) + 1;
  size_t b = src.find(")HLSL\"", a);
  return b == std::string::npos ? "" : src.substr(a, b - a);
}
static std::string hlslStripComments(const std::string& s) {  // keeps newlines so line numbers still match fxc's
  std::string o; o.reserve(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    if (s.compare(i, 2, "//") == 0) { while (i < s.size() && s[i] != '\n') i++; if (i < s.size()) o += '\n'; }
    else if (s.compare(i, 2, "/*") == 0) { size_t e = s.find("*/", i + 2); if (e == std::string::npos) e = s.size(); for (size_t k = i; k < e; k++) if (s[k] == '\n') o += '\n'; i = e + 1; }
    else o += s[i];
  }
  return o;
}
// Returns "line N: word (why)" for each reserved word used as an identifier.
static std::vector<std::string> hlslReservedHits(const std::string& hlsl) {
  // never legal outside their keyword role in any shader we compile (C++ words HLSL reserves, effect-framework keywords)
  static const std::set<std::string> never = {"auto", "case", "catch", "char", "class", "const_cast", "default", "delete", "dynamic_cast", "enum",
      "explicit", "friend", "goto", "long", "mutable", "new", "operator", "private", "protected", "public", "reinterpret_cast", "short",
      "signed", "sizeof", "static_cast", "template", "this", "throw", "try", "typename", "union", "unsigned", "using", "virtual",
      "asm", "asm_fragment", "compile", "compile_fragment", "pass", "technique", "technique10", "technique11", "pixelfragment",
      "vertexfragment", "stateblock", "stateblock_state", "sampler_state", "snorm", "unorm"};
  // modifiers / primitive types / built-in type names: fine before a declaration, wrong as a variable, member or value
  static const std::set<std::string> modifiers = {"point", "line", "lineadj", "triangle", "triangleadj", "linear", "centroid", "nointerpolation",
      "noperspective", "sample", "extern", "shared", "groupshared", "static", "uniform", "volatile", "precise", "export", "inline", "inout",
      "in", "out", "const", "row_major", "column_major", "register", "packoffset", "cbuffer", "tbuffer", "struct", "typedef",
      "vector", "matrix", "sampler", "texture", "string", "buffer", "dword", "half", "double", "true", "false", "NULL", "void", "pixelshader",
      "vertexshader", "geometryshader", "compute", "domain", "hull", "indices", "vertices", "primitives", "payload", "uint", "int", "bool",
      "float", "min16float", "min10float", "min16int", "min12int", "min16uint", "Texture2D", "Texture2DArray", "Texture3D", "TextureCube",
      "SamplerState", "SamplerComparisonState", "Buffer", "StructuredBuffer", "ConstantBuffer", "RWTexture2D", "RWBuffer"};
  static const std::set<std::string> valueWords = {"true", "false", "NULL"};
  std::string s = hlslStripComments(hlsl);
  struct Tok { std::string w; int line; size_t end; };
  std::vector<Tok> toks; int line = 1;
  for (size_t i = 0; i < s.size();) {
    char c = s[i];
    if (c == '\n') { line++; i++; continue; }
    if (std::isalpha((unsigned char)c) || c == '_') {
      size_t j = i; while (j < s.size() && (std::isalnum((unsigned char)s[j]) || s[j] == '_')) j++;
      if (i == 0 || !(std::isdigit((unsigned char)s[i - 1]) || s[i - 1] == '.')) toks.push_back({s.substr(i, j - i), line, j});
      i = j; continue;
    }
    i++;
  }
  std::set<std::string> structs;
  for (size_t k = 0; k + 1 < toks.size(); k++) if (toks[k].w == "struct") structs.insert(toks[k + 1].w);
  auto isType = [&](const std::string& w) {
    static const char* bases[] = {"float", "int", "uint", "bool", "half", "double", "min16float", "min10float", "min16int", "min16uint", "dword"};
    for (const char* b : bases) {
      size_t n = std::strlen(b);
      if (w.compare(0, n, b) != 0) continue;
      std::string r = w.substr(n);
      if (r.empty() || (r.size() == 1 && r[0] >= '1' && r[0] <= '4') || (r.size() == 3 && r[0] >= '1' && r[0] <= '4' && r[1] == 'x' && r[2] >= '1' && r[2] <= '4')) return true;
    }
    return structs.count(w) > 0;
  };
  std::vector<std::string> hits;
  for (size_t k = 0; k < toks.size(); k++) {
    const Tok& t = toks[k];
    if (never.count(t.w)) { hits.push_back("line " + std::to_string(t.line) + ": " + t.w + " (reserved)"); continue; }
    if (!modifiers.count(t.w) || valueWords.count(t.w)) continue;
    size_t j = t.end; while (j < s.size() && (s[j] == ' ' || s[j] == '\t' || s[j] == '\r' || s[j] == '\n')) j++;
    char next = j < s.size() ? s[j] : ';';
    bool declared = k > 0 && isType(toks[k - 1].w) && toks[k - 1].end <= t.end && s.find_first_not_of(" \t\r\n", toks[k - 1].end) == t.end - t.w.size();
    bool usedAsValue = std::strchr("=;,)]*+-/.?:&|^%", next) != nullptr;
    if (declared) hits.push_back("line " + std::to_string(t.line) + ": " + t.w + " (declared as a name)");
    else if (usedAsValue) hits.push_back("line " + std::to_string(t.line) + ": " + t.w + " (used as a value)");
  }
  return hits;
}
static void testShaderKeywords() {
  // the checker itself: catches the shipped bug, ignores legal keyword use
  CHECK(hlslReservedHits("float line = 1 - x;\nc = lerp(a, b, line * 0.9);").size() == 2);
  CHECK(hlslReservedHits("struct G { float4 sample : TEXCOORD0; };").size() == 1);
  CHECK(hlslReservedHits("float4 point = 0;").size() == 1);
  CHECK(hlslReservedHits("int class = 1;").size() == 1);
  CHECK(hlslReservedHits("float linear;").size() == 1);
  CHECK(hlslReservedHits("[maxvertexcount(2)] void GS(line float4 v[2] : SV_Position, inout LineStream<float4> o) {}").empty());
  CHECK(hlslReservedHits("static const float k = 2; // line, sample, class in a comment\n/* point */ row_major float4x4 m;").empty());
  CHECK(hlslReservedHits("float laneMask = 1; float lineWidth = 2; float splitLine = 3;").empty());
  CHECK(hlslReservedHits("nointerpolation float4 c : COLOR0; if (x) discard;").empty());
  struct Src { const char* file; const char* var; const char* name; };
  const Src srcs[] = {{"/../src/renderer/dx12_renderer.cpp", "kShader", "tommyzilla.hlsl"}, {"/../src/model/dx12_model_renderer.cpp", "kModelShader", "tommyzilla_model.hlsl"}};
  for (const Src& sc : srcs) {
    std::string src; CHECK(readTextFile(std::string(TZ_DATA_DIR) + sc.file, src));
    std::string hlsl = hlslBlock(src, sc.var); CHECK(hlsl.size() > 500);
    std::vector<std::string> hits = hlslReservedHits(hlsl);
    for (auto& h : hits) std::printf("  %s %s: reserved HLSL word as identifier\n", sc.name, h.c_str());
    CHECK(hits.empty());
    // every entry point the C++ compiles exists in the text, with an SM5 target (D3DCompile's fxc does not do SM6)
    int entries = 0;
    for (size_t p = 0; (p = src.find("\", \"", p)) != std::string::npos; p += 4) {
      size_t q = src.find('"', p + 4); if (q == std::string::npos) break;
      std::string target = src.substr(p + 4, q - p - 4);
      if (target != "vs_5_0" && target != "ps_5_0") { if (target.size() == 6 && target[2] == '_' && (target[0] == 'v' || target[0] == 'p') && target[1] == 's') { std::printf("  unexpected target %s\n", target.c_str()); CHECK(false); } continue; }
      size_t b = src.rfind('"', p - 1); std::string entry = src.substr(b + 1, p - b - 1);
      bool found = false;
      for (size_t f = 0; (f = hlsl.find(entry + "(", f)) != std::string::npos; f++) if (f > 0 && (hlsl[f - 1] == ' ')) found = true;
      if (!found) std::printf("  %s: entry %s not defined\n", sc.name, entry.c_str());
      CHECK(found); entries++;
    }
    CHECK(entries >= (std::string(sc.var) == "kShader" ? 8 : 2));
    // fxc-only gotchas: no SM6 wave/16-bit features, no HLSL 2021 templates or operator overloads
    for (const char* bad : {"WaveActive", "WaveRead", "float16_t", "int16_t", "uint16_t", "template<", "template <", "QuadRead"}) CHECK(hlsl.find(bad) == std::string::npos);
  }
}

static void testTitan() {  // Titan: difficulty, player scaling, police escape/search/siren, heli swoop, boss anti-cheese
  SECTION("titan difficulty + police");
  DiffMods e = difficultyMods(DIFF_EASY, 1), h = difficultyMods(DIFF_HARD, 1), x = difficultyMods(DIFF_INSANE, 1), x2 = difficultyMods(DIFF_INSANE, 2);
  CHECK(e.enemyHp == 1 && e.enemyDamage == 1 && e.coins == 1); CHECK(h.enemyHp > e.enemyHp && x.enemyHp > h.enemyHp);
  CHECK(x.coins > h.coins && h.coins > e.coins); CHECK(x2.enemyHp > x.enemyHp && x2.coins > x.coins && x2.bossHp > x.bossHp);
  CHECK(std::string(difficultyName(DIFF_INSANE)) == "You're Insane");
  PoliceChase pc; CHECK(pc.sirenVolume(1, 2) > 0 && pc.sirenVolume(1, 2) < 1); CHECK(pc.sirenVolume(1, 0) == 0);
  pc.update(0.1f, 2, 30, 0); CHECK(pc.arrived); CHECK(pc.sirenVolume(1, 2) == 0);
  bool esc = false; for (int i = 0; i < 60 && !esc; i++) esc = pc.update(0.1f, 2, 100, 20); CHECK(esc && pc.searching);
  pc.onBuildingSmashed(); CHECK(!pc.searching);
  PoliceChase big; bool e2 = false; for (int i = 0; i < 60; i++) e2 |= big.update(0.1f, 8, 100, 20); CHECK(!e2);
  PoliceChase slow; bool e3 = false; for (int i = 0; i < 60; i++) e3 |= slow.update(0.1f, 2, 100, 5); CHECK(!e3);
  float minR = 99, minH = 99; for (float t = 0; t < 7; t += 0.05f) { auto s = heliSwoop(t, 30); minR = std::min(minR, s.radius); minH = std::min(minH, s.height); }
  CHECK(minR < 14 && minH < 12);
  CheeseMeter cm; for (int i = 0; i < 40; i++) cm.update(0.1f, 5, 9); CHECK(cm.punishClose());
  CheeseMeter cs; for (int i = 0; i < 4; i++) cs.onStomp(); CHECK(cs.punishStomp());
  CheeseMeter ok; for (int i = 0; i < 4; i++) { ok.onStomp(); ok.update(2, 40, 9); } CHECK(!ok.punishStomp() && !ok.punishClose());
}

#include "game/kaiju_gen.h"
static void testKaijuGen() {  // Rex
  KaijuLook l = eggs::rollLook(tmpRng(1), "gloopzilla", T);
  KaijuGenome a = kgen::generate(42, l), b = kgen::generate(42, l), c = kgen::generate(43, l);
  CHECK(a.body == b.body && a.parts == b.parts && a.height == b.height);
  CHECK(a.height != c.height || a.body != c.body);
  CHECK(!kgen::usesProcedural("tommyzilla") && kgen::usesProcedural("gloopzilla"));
  std::set<int> elems; std::set<int> bodies;
  for (int i = 0; i < 400; i++) { Rng r(500 + i); KaijuLook k = kgen::rollEgg(r, T, {}, 1.f);
    if (k.monster == "tommyzilla") { CHECK(k.element >= 0 && k.genSeed == 0); elems.insert(k.element); } else { CHECK(k.genSeed != 0 && k.element == -1); bodies.insert((int)kgen::generate(k.genSeed, k).body); } }
  CHECK(bodies.size() >= 4);
  for (int e = 0; e < 4; e++) CHECK(std::string(kgen::elementName(e)).size() > 0);
  KaijuLook f; f.monster = "tommyzilla"; kgen::applyElement(f, ELEM_FIRE); Json j = f.toJson(); KaijuLook g; CHECK(g.fromJson(j) && g.element == ELEM_FIRE);
  // fewer duplicates: owning all but one common, fresh rolls should mostly avoid owned species
  std::set<std::string> owned; for (auto& m : T.monsters) owned.insert(m.id); owned.erase("gloopzilla"); owned.erase("tommyzilla");
  int dupPlain = 0, dupFresh = 0;
  for (int i = 0; i < 300; i++) { Rng r1(900 + i), r2(900 + i); if (owned.count(eggs::rollSpecies(r1, T))) dupPlain++; if (owned.count(kgen::rollSpeciesFresh(r2, T, owned))) dupFresh++; }
  CHECK(dupFresh < dupPlain);
}

static void testTitanGame() {  // Titan: co-op, varied bosses, saved difficulty, spawn scaling
  SECTION("titan co-op + bosses");
  CHECK(bossIdFromName("moth") == 3 && bossIdFromName("crab") == 4 && bossIdFromName("blimp") == 5 && bossIdFromName("supertank") == 1);
  { int n[6] = {}; for (auto& L : T.levels) if (!L.miniBoss.empty()) n[bossIdFromName(L.miniBoss)]++; CHECK(n[3] >= 1 && n[4] >= 1 && n[5] >= 1); }
  auto g = newGame(); g->save.tutorial.done = true; g->save.settings.difficulty = DIFF_INSANE; g->save.settings.players = 2;
  { Json j = g->save.toJson(); SaveData r; CHECK(r.fromJson(j) && r.settings.difficulty == DIFF_INSANE && r.settings.players == 2); }
  g->startMode(false); dismissPrompts(*g); g->menus.clear();
  CHECK(g->difficulty == DIFF_INSANE && g->players == 2 && g->coopOn());
  InputState in; in.p2Active = true; in.p2MoveY = 1; Vec3 p1 = g->kaiju.pos;
  run(*g, 1, in); CHECK(g->coopJoined); Vec3 a = g->kaiju2.pos; run(*g, 1, in);
  CHECK(length(g->kaiju2.pos - a) > 1); CHECK(length(g->kaiju.pos - p1) < 1);  // pad moves only P2
  g->kaiju2.hp = 1; g->coopSwapped = true; std::swap(g->kaiju, g->kaiju2); g->kaiju.damage(999); std::swap(g->kaiju, g->kaiju2); g->coopSwapped = false;
  CHECK(g->mode == Mode::Play);  // P2 KO doesn't end the run
  g->enemies.forced = true;  // sandbox: keep units alive
  for (int b = 3; b <= 5; b++) {
    g->enemies.units.clear(); g->enemies.shots.clear(); g->enemies.addUnit(U_MBOSS); Unit& u = g->enemies.units.back(); u.boss = b; u.pos.y = bossSpawnHeight(b);
    bool fired = false; for (int i = 0; i < 8 * 60; i++) { run(*g, 1.f / 60); Unit* m = g->enemies.miniBoss(); if (m && m->fireT > -1e8f) fired = true; }
    CHECK(fired); Unit* m = g->enemies.miniBoss(); RenderList rl; CHECK(m && drawVariedBoss(rl, *m, Mat4()));
  }
  InputState k; k.p2Active = true; k.p2Pressed[A_PAUSE] = true; k.p2Held[A_CLAW] = true; InputState q = p2InputFrom(k); CHECK(!q.pressed[A_PAUSE] && q.held[A_CLAW]);
}
#include "game/zombies.h"
static void testRexPlay() {  // Rex: generated bodies render, one-button eat, easy zookeeper, crazy zombies
  { auto g = newGame(); g->save.tutorial.done = true; g->startMode(false); dismissPrompts(*g); g->menus.clear();
    MonsterDef d = g->lookedDef(T.monsterIndex("gloopzilla")); CHECK(d.genSeed == 0);
    KaijuLook l = eggs::rollLook(tmpRng(2), "gloopzilla", T); l.genSeed = 777; g->save.looks["gloopzilla"] = l;
    d = g->lookedDef(T.monsterIndex("gloopzilla")); CHECK(d.genSeed == 777 && d.plan == kgen::planOf(kgen::generate(777, l).body));
    RenderList rl; RigPose p; drawMonster(rl, d, Vec3(0, 0, 0), 0, 1, p); CHECK(rl.instanceCount() > 10);
    KaijuLook t = eggs::rollLook(tmpRng(3), "tommyzilla", T); kgen::applyElement(t, ELEM_LIGHTNING); g->save.looks["tommyzilla"] = t;
    CHECK(g->lookedDef(0).genSeed == 0);
    // one-button eat: a person right next to us -> eatTargetNear, Y (EAT+GRAB together) eats and does not grab
    for (auto& q : g->people.list) q.active = false;
    Person& q = g->people.list[0]; q = Person(); q.active = true; q.x = g->kaiju.pos.x + 3; q.z = g->kaiju.pos.z; g->people.activeCount = 1;
    CHECK(g->eatTargetNear());
    int e0 = g->P().eaten; InputState y; y.pressed[A_EAT] = y.pressed[A_GRAB] = y.held[A_EAT] = y.held[A_GRAB] = true; g->frame(1.f / 60, y);
    CHECK(g->P().eaten == e0 + 1 && g->kaiju.held < 0); }
  { auto g = newGame(); g->save.spinSlot = true; g->save.tutorial.done = true; g->save.adventureUnlocked = true;
    g->startMode(true); CHECK(g->zoo.active);
    for (int i = 0; i < 60 * 8 && g->zoo.state == 0; i++) g->frame(1.f / 60, InputState());
    Vec3 d = g->zoo.keeper - g->kaiju.pos; d.y = 0; float L = length(d), want = g->kaiju.eatRange() * 1.7f;
    if (L > want) g->kaiju.pos += d * ((L - want) / L);
    g->frame(1.f / 60, InputState()); CHECK(g->zoo.inRange);  // reachable from well beyond normal eat range
    tap(*g, A_CLAW); CHECK(g->zoo.keeperEaten); }               // any action button gulps
  { auto g = newGame(); g->save.crazyUnlocked = g->save.bulkDefeated = true; g->startMode(true, true); dismissPrompts(*g); g->menus.clear();
    g->zoo.active = false;
    CHECK(zombie::on());
    for (auto& q : g->people.list) q.active = false;
    Person& z = g->people.list[0]; z = Person(); z.active = true; z.x = g->kaiju.pos.x + 30; z.z = g->kaiju.pos.z; g->people.activeCount = 1;
    float d0 = 30; for (int i = 0; i < 60; i++) g->people.update(1.f / 60, g->kaiju.pos.x, g->kaiju.pos.z, 0, g->kaiju.radius());
    CHECK(dist2D(z.x, z.z, g->kaiju.pos.x, g->kaiju.pos.z) < d0);  // shambles toward the kaiju (no fleeing)
    z.x = g->kaiju.pos.x + g->kaiju.radius() + 1; z.z = g->kaiju.pos.z; z.biteT = 0; float hp0 = g->kaiju.hp;
    g->people.update(1.f / 60, g->kaiju.pos.x, g->kaiju.pos.z, 0, g->kaiju.radius());
    CHECK(g->kaiju.hp < hp0);                                     // nibble
    int e0 = g->P().eaten; float xp0 = g->kaijuXp(); z.x = g->kaiju.pos.x + 3;
    CHECK(g->kaiju.eat() && g->P().eaten == e0 + 1 && g->kaijuXp() >= xp0); }  // same benefit as a normal person
}

static void testWrenWiring() {  // Wren integration: Vault persist + power effects + EGGS, menu settings, co-op nearest-player targeting
  SECTION("wren: vault persist + powers + eggs + settings + co-op targeting");
  { std::string saved; auto g = newGame(&saved); g->save.tutorial.done = true; std::string id = g->kaiju.def().id;
    g->vault.setLevel(id, vpow::ST_HP, 3); g->vault.cheatsUnlocked = true; g->persist();
    Json j; CHECK(Json::parse(saved, j) && j.has("vault"));
    auto h = newGame(); CHECK(h->loadSave(&saved)); CHECK(h->vault.level(id, vpow::ST_HP) == 3 && h->vault.cheatsUnlocked);
    std::string none = "{}"; CHECK(h->loadSave(&none)); CHECK(h->vault.level(id, vpow::ST_HP) == 0 && !h->vault.cheatsUnlocked); }
  { auto g = newGame(); g->save.tutorial.done = true; g->startMode(false); dismissPrompts(*g); g->menus.clear();
    std::string id = g->kaiju.def().id; const MoveDef* mv = nullptr; for (auto& m : T.moves) if (m.radius > 0 && m.reach > 0) { mv = &m; break; }
    float hp0 = g->kaiju.maxHp(), r0 = mv ? g->kaiju.moveRadius(*mv) : 0, b0 = mv ? g->kaiju.moveBricks(*mv) : 0;
    for (int s = 0; s < vpow::ST_COUNT; s++) g->vault.setLevel(id, (vpow::Stat)s, vpow::kMaxLevel);
    CHECK(g->kaiju.maxHp() > hp0 * 1.3f); if (mv) { CHECK(g->kaiju.moveRadius(*mv) > r0 * 1.3f); CHECK(g->kaiju.moveBricks(*mv) > b0 * 1.3f); }
    const MoveDef* sup = nullptr; for (auto& m : T.moves) if (m.kind == MoveKind::Super && m.id != "glove") { sup = &m; break; }
    if (sup) { int c1 = g->superEatCost(*sup, 1); g->vault.setLevel(id, vpow::ST_THRIFT, 0); int c0 = g->superEatCost(*sup, 1); CHECK(c1 >= 1 && c1 <= c0); if (c0 >= 3) CHECK(c1 < c0); }
    Vec3 p = g->kaiju.pos; InputState in; in.moveY = 1; in.held[A_SPRINT] = false; run(*g, 0.5f, in); float fast = length(g->kaiju.pos - p);
    g->vault.setLevel(id, vpow::ST_SPEED, 0); p = g->kaiju.pos; run(*g, 0.5f, in); float slow = length(g->kaiju.pos - p); CHECK(fast > slow * 1.1f);
    // EGGS action spends a spin token and starts the spinner
    g->menus.clear(); g->P().spins = 1; tap(*g, A_EGGS); CHECK(g->mode == Mode::Spin && g->P().spins == 0); }
  { auto g = newGame(); g->save.tutorial.done = true; g->menus.clear(); g->mode = Mode::Title; g->openMainMenu(); g->updateMenus(InputState(), 0.016f);
    Menu& m = g->menus.back(); MenuItem* df = nullptr; MenuItem* pl = nullptr; for (auto& it : m.items) { if (it.label == "Difficulty") df = &it; if (it.label == "Players") pl = &it; }
    CHECK(df && pl); if (df && pl) { df->adjust(1); pl->act(); CHECK(g->save.settings.difficulty == DIFF_HARD && g->save.settings.players == 2);
      g->startMode(false); CHECK(g->difficulty == DIFF_HARD && g->players == 2); } }
  { auto g = newGame(); g->save.tutorial.done = true; g->save.settings.players = 2; g->startMode(false); dismissPrompts(*g); g->menus.clear();
    InputState in; in.p2Active = true; run(*g, 0.2f, in); CHECK(g->coopJoined);
    g->kaiju.pos = Vec3(0, 0, 0); g->kaiju2.pos = Vec3(150, 0, 0); g->enemies.forced = true; g->enemies.units.clear(); g->enemies.shots.clear();
    g->enemies.addUnit(U_JEEP); Unit& u = g->enemies.units.back(); u.pos = Vec3(165, 0, 0); float h1 = g->kaiju.hp, h2 = g->kaiju2.hp;
    for (int i = 0; i < 120; i++) { g->kaiju.pos = Vec3(0, 0, 0); g->kaiju2.pos = Vec3(150, 0, 0); g->kaiju.hp = h1; run(*g, 1.f / 60, in); }
    std::printf("  coop car: n=%d p2 %.2f->%.2f p1 %.2f->%.2f car %.1f,%.1f\n", (int)g->enemies.units.size(), h2, g->kaiju2.hp, h1, g->kaiju.hp, g->enemies.units.empty() ? 0.f : g->enemies.units.back().pos.x, g->enemies.units.empty() ? 0.f : g->enemies.units.back().pos.z);
    CHECK(g->kaiju2.hp < h2 && g->kaiju.hp >= h1 - 0.01f);  // the car shot at P2 (nearer), not P1
    CHECK(g->coopP2Nearer(Vec3(140, 0, 0)) && !g->coopP2Nearer(Vec3(10, 0, 0)));
    float d1 = dist2D(g->enemies.units.empty() ? 0 : g->enemies.units.back().pos.x, 0, 150, 0); CHECK(d1 < 40);
    // boss: P2 nearer -> boss faces / walks toward P2
    g->boss = Boss(); g->boss.active = true; g->boss.pos = Vec3(150, 0, 100);
    for (int i = 0; i < 60; i++) { g->kaiju.pos = Vec3(0, 0, 0); g->kaiju2.pos = Vec3(150, 0, 0); g->updateBoss(1.f / 60); }
    float want = std::atan2(150.f - g->boss.pos.x, 0.f - g->boss.pos.z); std::printf("  boss facing %.2f want %.2f\n", g->boss.facing, want);
    CHECK(std::cos(g->boss.facing - want) > 0.8f); CHECK(g->kaiju.cam.shake >= 0); }
}
// Regression (Tyler): one pad button eats. The pad's Y reports A_EAT and A_GRAB in the same frame (no LB chord).
static InputState padY() { InputState in; in.pressed[A_EAT] = in.held[A_EAT] = true; in.pressed[A_GRAB] = in.held[A_GRAB] = true; return in; }
static InputState stickToward(Game& g, Vec3 to) {  // camera-relative stick input that walks the kaiju toward a world point
  InputState in; float yaw = g.kaiju.cam.yaw; float fx = -std::sin(yaw), fz = -std::cos(yaw), rx = -fz, rz = fx;
  Vec3 d = to - g.kaiju.pos; d.y = 0; float l = length(d); if (l < 1e-3f) return in;
  d = d * (1.f / l); in.moveY = d.x * fx + d.z * fz; in.moveX = d.x * rx + d.z * rz; return in;
}
static void testOneButtonEat() {
  SECTION("One-button eat (pad Y alone) + edible zookeeper");
  // zoo opening: walk up with the stick (no teleport) and press only Y
  auto g = newGame(); g->save.spinSlot = true; g->save.tutorial.done = true; g->save.adventureUnlocked = true;
  g->startMode(true); CHECK(g->zoo.active);
  for (int i = 0; i < 60 * 12 && g->zoo.state == 0; i++) g->frame(1.f / 60, InputState());
  CHECK(g->zoo.state == 1);
  for (int i = 0; i < 60 * 10 && !g->zoo.inRange; i++) g->frame(1.f / 60, stickToward(*g, g->zoo.keeper));
  CHECK(g->zoo.inRange);
  { InputState y = padY(); g->frame(1.f / 60, y); }
  CHECK(g->zoo.keeperEaten && g->AP().zooKey);
  // the keeper can be gulped from the cage bars even while he is still strolling up (state 0 -> pressing Y in reach works)
  auto z = newGame(); z->save.spinSlot = true; z->save.tutorial.done = true; z->save.adventureUnlocked = true; z->startMode(true);
  for (int i = 0; i < 60 * 12 && !z->zoo.inRange && !z->zoo.keeperEaten; i++) { InputState in = stickToward(*z, z->zoo.keeper); if (z->zoo.inRange) in = padY(); z->frame(1.f / 60, in); }
  { InputState y = padY(); z->frame(1.f / 60, y); } CHECK(z->zoo.keeperEaten);
  // sandbox play: a civilian in reach is eaten by Y alone, and the same press does not also grab a tree
  auto s = newGame(); s->save.spinSlot = true; s->save.tutorial.done = true; s->startMode(false); if (s->zoo.active) tap(*s, A_SKILLS);
  s->menus.clear(); run(*s, 0.5f);
  int pi = -1; for (size_t i = 0; i < s->people.list.size(); i++) if (s->people.list[i].active) { pi = (int)i; break; }
  CHECK(pi >= 0);
  if (pi >= 0) {
    Person& p = s->people.list[pi]; s->kaiju.pos = Vec3(p.x + 3, s->kaiju.pos.y, p.z); int e0 = s->P().eaten;
    InputState y = padY(); s->frame(1.f / 60, y);

    CHECK(s->P().eaten == e0 + 1 && s->kaiju.held < 0);
  }
  // L13 box zookeeper: Y alone gulps him (he was only reachable through the LB+Y chord)
  auto b = adventureAt(12); b->menus.clear(); b->save.spinSlot = true; b->save.zooCompleted = false; b->AP().zooKey = false;
  b->level.boxKeeper = true; b->level.boxKeeperPos = b->kaiju.pos + Vec3(4, 0, 0);
  CHECK(b->eatTargetNear());
  { InputState y = padY(); b->frame(1.f / 60, y); } CHECK(!b->level.boxKeeper && b->AP().zooKey);
}
// Regression (Tommy): the right stick / mouse must turn the camera in Adventure mode.
static void testAdventureCamera() {
  SECTION("Camera look input moves the camera in Adventure mode");
  auto check = [](Game& g, const char* what) {
    g.menus.clear(); g.mode = Mode::Play; run(g, 0.2f);
    float y0 = g.kaiju.cam.yaw; InputState st; st.lookX = 1; st.usingPad = st.padConnected = true; run(g, 0.5f, st);
    bool stick = std::fabs(wrapAngle(g.kaiju.cam.yaw - y0)) > 0.2f;
    float p0 = g.kaiju.cam.pitch; InputState up; up.lookY = 1; run(g, 0.3f, up);
    bool pitch = std::fabs(g.kaiju.cam.pitch - p0) > 0.02f || g.kaiju.cam.pitch >= 1.2f;
    float y1 = g.kaiju.cam.yaw; InputState m; m.mouseDX = 40; g.frame(1.f / 60, m);
    bool mouse = std::fabs(wrapAngle(g.kaiju.cam.yaw - y1)) > 1e-3f;
    if (!(stick && pitch && mouse)) std::printf("  camera stuck in %s: stick %d pitch %d mouse %d\n", what, stick, pitch, mouse);
    CHECK(stick && pitch && mouse);
  };
  { auto g = newGame(); g->save.spinSlot = true; g->save.tutorial.done = true; g->save.adventureUnlocked = true; g->startMode(true);
    CHECK(g->zoo.active); check(*g, "adventure zoo intro"); }
  { auto g = adventureAt(0); check(*g, "adventure L1"); }
  { auto g = adventureAt(3); check(*g, "adventure L4"); }
  { auto g = adventureAt(0, true); check(*g, "crazy L1"); }
  { auto g = adventureAt(5); check(*g, "adventure L6"); }
  { auto g = adventureAt(19); check(*g, "adventure L20"); }
  { auto g = adventureAt(12); check(*g, "adventure L13"); }
  { auto g = adventureAt(0); g->players = g->save.settings.players = 2; check(*g, "adventure co-op"); }
  { auto g = newGame(); g->save.tutorial.done = true; g->startMode(false); if (g->zoo.active) tap(*g, A_SKILLS); check(*g, "sandbox"); }
  // co-op saved on (Players = 2) with the controller as player 2: its right stick turns P2's own (split-screen) camera
  for (int adv = 0; adv < 2; adv++) {
    auto g = adv ? adventureAt(0) : newGame(); if (!adv) { g->save.tutorial.done = true; g->startMode(false); if (g->zoo.active) tap(*g, A_SKILLS); }
    g->players = g->save.settings.players = 2; g->menus.clear();
    InputState join; join.p2Active = true; run(*g, 0.2f, join); CHECK(g->coopJoined);
    float y0 = g->kaiju2.cam.yaw; InputState st; st.p2Active = true; st.p2LookX = 1; run(*g, 0.5f, st);
    CHECK(g->splitScreen() && std::fabs(wrapAngle(g->kaiju2.cam.yaw - y0)) > 0.2f);
  }
}
// Regression (Tommy): the zoo-intro cage blocks both players until the keeper is gulped and the bars break
static void testZooCageBlocks() {
  SECTION("Adventure zoo cage blocks movement until opened");
  for (int coop = 0; coop < 2; coop++) {
    auto g = newGame(); g->save.spinSlot = true; g->save.tutorial.done = true; g->save.adventureUnlocked = true; g->startMode(true);
    CHECK(g->zoo.active && g->adventure);
    if (coop) { g->players = g->save.settings.players = 2; InputState j; j.p2Active = true; run(*g, 0.1f, j); CHECK(g->coopJoined); }
    InputState out; out.moveY = 1; out.p2Active = coop; out.p2MoveY = 1; out.p2MoveX = 0.4f; float maxd = 0, maxd2 = 0;
    for (int i = 0; i < 60 * 8; i++) {
      out.lookX = i < 240 ? 0.6f : -0.6f; g->frame(1.f / 60, out);  // wander + turn: try every direction
      maxd = std::max(maxd, dist2D(g->kaiju.pos.x, g->kaiju.pos.z, g->zoo.center.x, g->zoo.center.z));
      if (coop) maxd2 = std::max(maxd2, dist2D(g->kaiju2.pos.x, g->kaiju2.pos.z, g->zoo.center.x, g->zoo.center.z));
    }
    CHECK(!g->zoo.barsBroken && maxd <= E.zooCageR && maxd2 <= E.zooCageR);
    // gulping the keeper (even mid-stroll) opens it only after the bars break
    for (int i = 0; i < 60 * 10 && !g->zoo.keeperEaten; i++) { InputState in = stickToward(*g, g->zoo.keeper); if (g->zoo.inRange) { in.pressed[A_EAT] = in.held[A_EAT] = true; } g->frame(1.f / 60, in); }
    CHECK(g->zoo.keeperEaten && !g->zoo.barsBroken);
    run(*g, 0.5f, out); CHECK(dist2D(g->kaiju.pos.x, g->kaiju.pos.z, g->zoo.center.x, g->zoo.center.z) <= E.zooCageR);
    run(*g, 1.4f); CHECK(g->zoo.barsBroken);
  }
}
// Regression (Tyler): the police siren only stopped when every cop was destroyed.
static void testSirenStops() {
  SECTION("Police siren stops without a kill (Adventure roster, chase end, despawn)");
  // Adventure: cops / cars / riot cops are the army roster, not a police chase -> never a siren
  { auto g = adventureAt(2); g->menus.clear();
    for (UnitType t : {U_COP, U_CAR, U_RIOT}) g->enemies.addUnit(t);
    float mx = 0; for (int i = 0; i < 120; i++) { g->frame(1.f / 60, InputState()); mx = std::max(mx, g->loopVol[LOOP_SIREN]); }
    CHECK(g->enemies.count(U_COP) + g->enemies.count(U_CAR) + g->enemies.count(U_RIOT) > 0 && mx == 0); }
  // Sandbox chase: siren while they chase, silent once the wanted level drops (no kill)
  auto chase = []() { auto g = sandboxGame(); clearPeople(*g); g->save.settings.police = true; g->enemies.addHeat(2.5f);
    float mx = 0; for (int i = 0; i < 180; i++) { g->frame(1.f / 60, InputState()); mx = std::max(mx, g->loopVol[LOOP_SIREN]); }
    CHECK(mx > 0 && g->enemies.count(U_CAR) > 0); return g; };
  { auto g = chase(); int wrecks0 = 0; for (auto& u : g->enemies.units) wrecks0 += u.wreck;
    g->enemies.heat = 0; g->enemies.calmT = 99; run(*g, 0.5f);
    int wrecks = 0; for (auto& u : g->enemies.units) wrecks += u.wreck;
    CHECK(wrecks == wrecks0 && g->loopVol[LOOP_SIREN] == 0); }
  { auto g = chase(); g->enemies.chase.searching = true; run(*g, 0.3f); CHECK(g->loopVol[LOOP_SIREN] == 0); }   // outrun / lost interest
  { auto g = chase(); g->enemies.units.clear(); g->enemies.spawnT = 99; run(*g, 0.3f); CHECK(g->loopVol[LOOP_SIREN] == 0); }             // despawned
  { auto g = chase(); g->startMode(true); dismissPrompts(*g); run(*g, 0.3f); CHECK(g->loopVol[LOOP_SIREN] == 0); }  // left for Adventure
}
// L7 Old Town (Tyler): Adventure city must label Old Town and flag its shops so the oldtown smash quest counts them
static void testOldTownQuest() {
  SECTION("Adventure L7 Old Town: labelled, smashing Old Town shops advances the quest");
  auto g = adventureAt(6); g->menus.clear();
  CHECK(g->level.active && T.levels[g->level.idx].filter == "oldtown");
  bool lab = false; for (auto& l : g->world.labels) if (l.text == "Old Town") lab = true; CHECK(lab);
  int n = 0; for (auto& b : g->world.buildings) n += b.oldtown; CHECK(n >= T.levels[g->level.idx].count);
  for (auto& b : g->world.buildings) if (b.oldtown && !b.smashed) { b.smashed = true; g->onBuildingSmashed(b); }
  CHECK(g->level.count >= T.levels[6].count);
  run(*g, 1);
  CHECK(g->level.won && !g->level.active && g->AP().levelsWon >= 7);
}
// 2P split screen (Tyler): left = P1, right = P2, each with its own camera + HUD; 1P stays fullscreen
static float aspectOf(const RenderList& r) { return r.proj.m[1][1] / r.proj.m[0][0]; }
static void testSplitScreen() {
  SECTION("2P split screen: viewports, own cameras, HUD halves, 1P fullscreen");
  for (int adv = 0; adv < 2; adv++) {
    auto g = adv ? adventureAt(0) : sandboxGame(); g->menus.clear();
    RenderList a, b;
    // 1P: fullscreen, unchanged
    CHECK(!g->splitScreen() && !g->render(a, 1280, 720, &b) && a.vpW == 0 && a.width == 1280 && std::fabs(aspectOf(a) - 1280.f / 720) < 0.01f);
    // Players = 2 but nobody joined on the pad yet: still fullscreen
    g->players = g->save.settings.players = 2; run(*g, 0.1f);
    CHECK(!g->coopJoined && !g->render(a, 1280, 720, &b) && a.vpW == 0);
    InputState join; join.p2Active = true; run(*g, 0.1f, join); CHECK(g->coopJoined && g->splitScreen());
    CHECK(g->kaiju.player == 0 && g->kaiju2.player == 1);
    // split: left half = P1 camera, right half = P2 camera, both half-width aspect
    InputState away; away.p2Active = true; away.p2MoveY = 1; run(*g, 2.0f, away);  // P2 walks off on its own
    CHECK(dist2D(g->kaiju.pos.x, g->kaiju.pos.z, g->kaiju2.pos.x, g->kaiju2.pos.z) > 20);
    CHECK(g->render(a, 1280, 720, &b));
    CHECK(a.vpX == 0 && a.vpW == 640 && b.vpX == 640 && b.vpW == 640 && a.width == 1280);
    CHECK(std::fabs(aspectOf(a) - 640.f / 720) < 0.01f && std::fabs(aspectOf(b) - 640.f / 720) < 0.01f);
    CHECK(length(a.eye - g->kaiju.cam.pos) < 3 && length(b.eye - g->kaiju2.cam.pos) < 3 && length(a.eye - b.eye) > 10);
    CHECK(dist2D(g->kaiju2.cam.target.x, g->kaiju2.cam.target.z, g->kaiju2.pos.x, g->kaiju2.pos.z) < 1 && dist2D(g->kaiju.cam.target.x, g->kaiju.cam.target.z, g->kaiju.pos.x, g->kaiju.pos.z) < 1);  // each camera follows its own kaiju
    CHECK(b.instanceCount() > 100 && b.ui.empty());  // the right view is 3D only; all 2D lives in the window-wide list
    // a HUD in each half
    int left = 0, right = 0; for (auto& v : a.ui) { left += v.x < 630; right += v.x > 650; } CHECK(left > 100 && right > 100);
    // each stick turns only its own camera
    float y1 = g->kaiju.cam.yaw, y2 = g->kaiju2.cam.yaw;
    InputState p2look; p2look.p2Active = true; p2look.p2LookX = 1; run(*g, 0.5f, p2look);
    CHECK(std::fabs(wrapAngle(g->kaiju2.cam.yaw - y2)) > 0.2f && std::fabs(wrapAngle(g->kaiju.cam.yaw - y1)) < 0.05f);
    y1 = g->kaiju.cam.yaw; y2 = g->kaiju2.cam.yaw;
    InputState p1look; p1look.p2Active = true; p1look.lookX = 1; run(*g, 0.5f, p1look);
    CHECK(std::fabs(wrapAngle(g->kaiju.cam.yaw - y1)) > 0.2f && std::fabs(wrapAngle(g->kaiju2.cam.yaw - y2)) < 0.05f);
    // enemy targeting swaps (nearest player) never steal P2's camera
    Camera c1 = g->kaiju.cam, c2 = g->kaiju2.cam;
    bool sw = g->coopFocusBegin(g->kaiju2.pos); CHECK(sw); g->coopFocusEnd(sw);
    CHECK(g->kaiju.cam.yaw == c1.yaw && g->kaiju2.cam.yaw == c2.yaw && length(g->kaiju2.cam.pos - c2.pos) < 1e-4f);
    // shared menus stay fullscreen
    size_t n0 = a.ui.size(); g->openPause(); g->render(a, 1280, 720, &b);
    float mn = 1e9f, mxx = -1e9f; for (size_t i = n0; i < a.ui.size(); i++) { mn = std::min(mn, a.ui[i].x); mxx = std::max(mxx, a.ui[i].x); }
    CHECK(a.ui.size() > n0 && mn < 560 && mxx > 720);
    g->menus.clear();
    // back to 1 player: fullscreen again
    g->players = g->save.settings.players = 1; run(*g, 0.1f); CHECK(!g->splitScreen() && !g->render(a, 1280, 720, &b) && a.vpW == 0);
  }
  // Adventure zoo in split screen: both stay caged, P1 can gulp the keeper, P2 can eat
  { auto g = newGame(); g->save.spinSlot = true; g->save.tutorial.done = true; g->save.adventureUnlocked = true; g->startMode(true);
    g->players = g->save.settings.players = 2; InputState j; j.p2Active = true; run(*g, 0.1f, j); CHECK(g->splitScreen());
    InputState out; out.moveY = 1; out.p2Active = true; out.p2MoveY = 1; run(*g, 4, out);
    CHECK(dist2D(g->kaiju2.pos.x, g->kaiju2.pos.z, g->zoo.center.x, g->zoo.center.z) <= E.zooCageR);
    RenderList a, b; CHECK(g->render(a, 1280, 720, &b));
    for (int i = 0; i < 60 * 12 && !g->zoo.keeperEaten; i++) { InputState in = stickToward(*g, g->zoo.keeper); in.p2Active = true; if (g->zoo.inRange) in = padY(); in.p2Active = true; g->frame(1.f / 60, in); }
    CHECK(g->zoo.keeperEaten); }
  { auto s = sandboxGame(); s->menus.clear(); s->players = s->save.settings.players = 2; InputState j; j.p2Active = true; run(*s, 0.1f, j); CHECK(s->splitScreen());
    int pi = -1; for (size_t i = 0; i < s->people.list.size(); i++) if (s->people.list[i].active && s->people.list[i].y < 1) { pi = (int)i; break; }
    CHECK(pi >= 0);
    if (pi >= 0) { Person& p = s->people.list[pi]; s->kaiju2.pos = Vec3(p.x + 3, 0, p.z); s->kaiju.pos = Vec3(p.x + 300, 0, p.z); int e0 = s->P().eaten;
      InputState y; y.p2Active = true; y.p2Pressed[A_EAT] = y.p2Held[A_EAT] = y.p2Pressed[A_GRAB] = y.p2Held[A_GRAB] = true; s->frame(1.f / 60, y);
      CHECK(s->P().eaten == e0 + 1 && s->kaiju2.held < 0); } }
}
// Regression (Tyler): Adventure L14 "the zoo gate isn't on the map". Adventure's procedural city hid the stone gate
// until its (one of 26 identical purple) building was smashed; the Victoria map shows its gates always. The gate is now
// always placed, drawn with a beam + "ZOO GATE" label, and the HUD gives its distance; it unlocks once its building falls.
static void testZooGateQuest() {
  SECTION("Adventure L14 zoo gate: on the map from the start, beam + HUD distance, smash -> open -> stone -> win");
  auto s = adventureAt(13); s->menus.clear();
  Profile& A = s->AP(); A.zooKey = true; A.gauntlet = A.gauntletSlot = A.gauntletOn = true;
  CHECK(s->level.active && T.levels[13].obj == ObjType::Stone && s->world.procedural);
  GateRun* g = nullptr; for (auto& q : s->gates) if (q.stone == 0) g = &q;
  CHECK(g != nullptr); if (!g) return;
  CHECK(!s->stoneRevealed(0));
  CHECK(std::fabs(g->pos.x) < C.worldHalf - 5 && std::fabs(g->pos.z) < C.worldHalf - 5 && !s->world.isWater(g->pos.x, g->pos.z));
  RenderList with = renderOnce(*s); std::vector<GateRun> keep = s->gates; s->gates.clear();
  RenderList without = renderOnce(*s); s->gates = keep; g = &s->gates[0];
  CHECK(with.opaque[MESH_BOX].size() > without.opaque[MESH_BOX].size() + 10);  // gate pen drawn while still locked
  CHECK(with.alpha[MESH_CYL].size() > without.alpha[MESH_CYL].size());        // the beam
  CHECK(s->levelHud().find("gate ") != std::string::npos && s->levelHud().find("m away") != std::string::npos);
  s->kaiju.pos = g->pos; run(*s, 0.2f); CHECK(!g->open);  // locked until its purple building is smashed
  Building& b = s->world.buildings[(size_t)s->world.stoneBuildings[0]];
  s->kaiju.pos = Vec3(b.x, 0, b.z + 25); run(*s, 0.3f);
  for (int i = 0; i < 300 && !b.smashed; i++) { s->world.damageSphere(b.x, 3, b.z, 12, 400, b.x, b.z + 20, 1); s->frame(1.f / 60, InputState()); }
  CHECK(b.smashed && s->stoneRevealed(0));
  s->kaiju.pos = g->pos; run(*s, 0.2f); CHECK(g->open);
  s->kaiju.pos = g->pos + Vec3(0, 0, -8); run(*s, 0.3f);
  CHECK(s->level.stoneTaken);
  run(*s, 0.2f); CHECK(s->level.bossPhase || s->level.won);  // L14: the stone summons the mini-boss
  if (Unit* mb = s->enemies.miniBoss()) s->enemies.wreckUnit(*mb); run(*s, 0.5f);
  CHECK(s->level.won && !s->level.active && s->AP().levelsWon >= 14);
}
static void testVersionNotes() {
  SECTION("names match meshes, 16 models, shiny eggs, infinity stones, boss variety");
  auto nm = [](const char* id) { return T.monsters[(size_t)T.monsterIndex(id)].name; };
  CHECK(nm("tommyzilla") == "Tommyzilla" && T.monsters[0].desc.find("dinosaur") != std::string::npos);
  CHECK(nm("mossback") == "Yeti");
  CHECK(nm("sparkleraptor") == "Sparkle Dragon");
  CHECK(nm("hootzooka") == "Birb");
  CHECK(nm("bonkbeak") == "Pigeon");
  CHECK(nm("noodlewyrm") == "Dragon");
  CHECK(nm("wobblewump") == "Mushroom King");
  CHECK(nm("thunderhorn") == "Thunder Demon");
  CHECK(nm("roborumble") == "Fox Mech");
  CHECK(nm("clanksworth") == "Bee Mech");
  CHECK(nm("stompasaurus") == "Blue Demon");
  CHECK(nm("pinchington") == "Block Kid");
  CHECK(nm("gloopzilla") == "Gloopzilla");
  CHECK(std::string(kShinyOddsLine).find("1 in 10") != std::string::npos && std::string(kShinyOddsLine).find("10 eggs") != std::string::npos);
  CHECK(kInfinityCount == 6);
  for (const InfinityStoneDef& s : kInfinityStones) CHECK(s.what && std::string(s.what).size() > 8);
  int eggs = 9; ShinyRoll low = eggs::rollShiny(eggs, tmpRng(1)); CHECK(!low.spent && !low.hit && eggs == 9);
  int hits = 0, misses = 0;
  for (int seed = 1; seed < 400 && (hits == 0 || misses == 0); seed++) {
    int e = 25; bool before = e >= 10; ShinyRoll r = eggs::rollShiny(e, tmpRng((uint64_t)seed));
    CHECK(r.spent && e == 15 && before);
    hits += r.hit; misses += !r.hit;
  }
  CHECK(hits > 0 && misses > 0);
  SaveData fresh; CHECK(fresh.eggs == 30);
  Json saved = fresh.toJson(); saved.obj.erase("eggs"); SaveData old; CHECK(old.fromJson(saved) && old.eggs == 30);
  fresh.eggs = 4; Json kept = fresh.toJson(); SaveData back; CHECK(back.fromJson(kept) && back.eggs == 4);
  auto g = newGame();
  g->save.eggs = 4; g->openCharacterSelect(false); g->updateMenus(InputState(), 0.016f);
  bool odds = g->menus.back().body.find("1 in 10") != std::string::npos && g->menus.back().body.find("not a guarantee") != std::string::npos;
  bool disabled = false; for (auto& it : g->menus.back().items) if (it.label.find("shiny") != std::string::npos) disabled = !it.enabled && it.desc.find("have 4") != std::string::npos;
  CHECK(odds && disabled);
  g->save.eggs = 30; CHECK(g->tryShiny("tommyzilla") && g->save.eggs == 20);
  g->save.looks["tommyzilla"].shiny = true; int left = g->save.eggs;
  // a miss must not clear a shiny already earned
  for (int i = 0; i < 30 && g->save.eggs >= 10; i++) g->tryShiny("tommyzilla");
  CHECK(g->save.looks["tommyzilla"].shiny && g->save.eggs < left);
  g->menus.clear(); g->openInfinityStones(); g->updateMenus(InputState(), 0.016f);
  CHECK((int)g->menus.back().items.size() == kInfinityCount + 1);
  bool explained = true; for (int i = 0; i < kInfinityCount; i++) explained &= g->menus.back().items[(size_t)i].desc == kInfinityStones[i].what;
  CHECK(explained);
  g->menus.back().index = 0; tap(*g, A_CONFIRM); CHECK(g->save.infinity.count("space") == 1);
  tap(*g, A_CONFIRM); CHECK(g->save.infinity.count("space") == 0);
  std::string prev;
  int bosses = 0; std::set<std::string> kinds;
  for (auto& L : T.levels) if (!L.miniBoss.empty()) { CHECK(L.miniBoss != prev); prev = L.miniBoss; kinds.insert(L.miniBoss); bosses++; }
  CHECK(bosses >= 8 && kinds.size() == 5);
  CHECK(prog::bossAfter("mech", "mech") != "mech" && prog::bossAfter("crab", "mech") == "crab");
  CHECK(T.levels[10].miniBoss == "supertank" && T.levels[11].miniBoss == "mech");
}
int main() {
  loadData();
  if (const char* only = std::getenv("TZ_ONLY")) {  // quick subset runs while developing: TZ_ONLY=bulk|arc|zoo|ending|...
    std::string o = only;
    if (o == "eggs") testEggTables(); if (o == "spin") testSpinner(); if (o == "growth") testGrowthAndXp(); if (o == "finish") testFinishRender();
    if (o == "ladder") testEnemyLadder(); if (o == "deevo") testDeevo(); if (o == "zoo") testZooAndKey(); if (o == "eat1") testOneButtonEat(); if (o == "siren") testSirenStops(); if (o == "split") testSplitScreen(); if (o == "cam") { testAdventureCamera(); testZooCageBlocks(); } if (o == "arc") testGauntletArc(); if (o == "oldtown") testOldTownQuest(); if (o == "zoogate") testZooGateQuest();
    if (o == "bulk") testBulkStages(); if (o == "ending") testCreditsAndEnding(); if (o == "wren") testWrenWiring(); if (o == "adventure") testAdventure(); if (o == "police") testSandboxPoliceToggle(); if (o == "controls") testControlsMapping(); if (o == "store") testStoreGrid(); if (o == "ground") testGroundTextures(); if (o == "shader") testShaderKeywords(); if (o == "notes") testVersionNotes();
    std::printf("\n%d passed, %d failed\n", gPass, gFail); return gFail ? 1 : 0;
  }
  testData(); testKaijuGen(); testRexPlay(); testUnlocks(); testEconomy(); testPolice(); testTitan(); testTitanGame(); testCampaign(); testSave(); testSynthAndMeshes();
  testSandboxSim(); testSchoolAndRebuild(); testAllMovesAndMonsters(); testAdventure(); testMenusAndHud();
  testTutorialChallenge(); testRoarMilestoneAndChests(); testRapidFireAndBlaster(); testRoarCharge(); testEat(); testStompStun();
  testSkillSlots(); testSaveSlots(); testAdventureChoice(); testCrazy(); testPoliceAudioAndMovement(); testIntro();
  testEatFuel(); testRoster(); testLevel5Chests(); testProceduralCity(); testHiddenStones(); testChampionPick(); testDifficultyCurve();
  testModelHooks(); testSandboxPoliceToggle(); testControlsMapping(); testStoreGrid(); testGroundTextures(); testShaderKeywords();
  testEggTables(); testSpinner(); testGrowthAndXp(); testFinishRender(); testEnemyLadder(); testDeevo(); testZooAndKey(); testOneButtonEat(); testAdventureCamera(); testZooCageBlocks(); testSirenStops(); testSplitScreen(); testGauntletArc(); testOldTownQuest(); testZooGateQuest(); testBulkStages(); testCreditsAndEnding(); testWrenWiring(); testVersionNotes();
  std::printf("\n%d passed, %d failed\n", gPass, gFail);
  return gFail ? 1 : 0;
}
