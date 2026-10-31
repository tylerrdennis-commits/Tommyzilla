#include "progress.h"
#include <algorithm>
#include <cstdlib>
#include "config.h"
#include "../core/rng.h"
namespace tz {
void Profile::reset() { *this = Profile(); }

Json Profile::toJson() const {
  Json j = Json::makeObj();
  j.set("coins", Json::makeNum(coins)); j.set("totalCoins", Json::makeNum(totalCoins));
  j.set("dmgLvl", Json::makeNum(dmgLvl)); j.set("rangeLvl", Json::makeNum(rangeLvl)); j.set("speedLvl", Json::makeNum(speedLvl));
  Json mv = Json::makeArr(); for (auto& m : moves) mv.arr.push_back(Json::makeStr(m)); j.set("moves", mv);
  Json sl = Json::makeArr(); for (auto& s : slots) sl.arr.push_back(Json::makeStr(s)); j.set("slots", sl);
  j.set("slotCount", Json::makeNum(slotCount));
  Json me = Json::makeArr(); for (auto& s : melee) me.arr.push_back(Json::makeStr(s)); j.set("melee", me);
  j.set("smashCount", Json::makeNum(smashCount)); j.set("hitsTaken", Json::makeNum(hitsTaken)); j.set("eaten", Json::makeNum(eaten)); j.set("rainbows", Json::makeNum(rainbows));
  j.set("lockedMonster", Json::makeStr(lockedMonster)); j.set("eatenTotal", Json::makeNum(eatenTotal));
  j.set("gloveLevel", Json::makeNum(gloveLevel)); j.set("gloveDropped", Json::makeBool(gloveDropped));
  j.set("keys", Json::makeNum(keys)); Json ch = Json::makeArr(); for (auto& c : chests) ch.arr.push_back(Json::makeStr(c)); j.set("chests", ch);
  Json lm = Json::makeArr(); for (auto& s : landmarks) lm.arr.push_back(Json::makeStr(s)); j.set("landmarks", lm);
  j.set("schoolUnlocked", Json::makeBool(schoolUnlocked)); j.set("schoolSmashed", Json::makeBool(schoolSmashed)); j.set("storeUsed", Json::makeBool(storeUsed));
  j.set("campaignSeed", Json::makeStr(std::to_string(campaignSeed)));
  Json ca = Json::makeArr(); for (auto& s : campaign) ca.arr.push_back(Json::makeStr(s)); j.set("campaign", ca);
  j.set("level", Json::makeNum(level)); j.set("levelsWon", Json::makeNum(levelsWon));
  Json xj = Json::makeObj(); for (auto& kv : xp) xj.set(kv.first, Json::makeNum(kv.second)); j.set("xp", xj);
  j.set("spins", Json::makeNum(spins)); j.set("spinSmashMark", Json::makeNum(spinSmashMark));
  j.set("zooDone", Json::makeBool(zooDone)); j.set("zooKey", Json::makeBool(zooKey));
  Json gt = Json::makeArr(); for (auto& g : gates) gt.arr.push_back(Json::makeStr(g)); j.set("gates", gt);
  j.set("gauntletSlot", Json::makeBool(gauntletSlot)); j.set("gauntlet", Json::makeBool(gauntlet)); j.set("gauntletOn", Json::makeBool(gauntletOn));
  j.set("gauntletCracked", Json::makeBool(gauntletCracked)); j.set("endingDone", Json::makeBool(endingDone));
  Json st = Json::makeArr(); for (auto& g : stones) st.arr.push_back(Json::makeStr(g)); j.set("stones", st);
  j.set("lastBoss", Json::makeStr(lastBoss));
  return j;
}
void Profile::fromJson(const Json& j) {
  reset();
  coins = (int)j["coins"].numOr(0); totalCoins = (int)j["totalCoins"].numOr(0);
  dmgLvl = std::max(1, (int)j["dmgLvl"].numOr(1)); rangeLvl = std::max(1, (int)j["rangeLvl"].numOr(1)); speedLvl = std::max(1, (int)j["speedLvl"].numOr(1));
  for (auto& m : j["moves"].arr) moves.insert(m.strOr(""));
  for (size_t i = 0; i < 2 && i < j["slots"].size(); i++) slots[i] = j["slots"][i].strOr("");
  slotCount = std::max(1, std::min(2, (int)j["slotCount"].numOr(2)));
  for (size_t i = 0; i < (size_t)MELEE_BINDS && i < j["melee"].size(); i++) melee[i] = j["melee"][i].strOr("");
  smashCount = (int)j["smashCount"].numOr(0); hitsTaken = (int)j["hitsTaken"].numOr(0); eaten = (int)j["eaten"].numOr(0); rainbows = (int)j["rainbows"].numOr(0);
  lockedMonster = j["lockedMonster"].strOr(""); eatenTotal = (int)j["eatenTotal"].numOr(0);
  gloveLevel = (int)j["gloveLevel"].numOr(-1); gloveDropped = j["gloveDropped"].boolOr(false);
  keys = std::max(0, (int)j["keys"].numOr(0)); for (auto& c : j["chests"].arr) chests.push_back(c.strOr(""));
  for (auto& m : j["landmarks"].arr) landmarks.insert(m.strOr(""));
  schoolUnlocked = j["schoolUnlocked"].boolOr(false); schoolSmashed = j["schoolSmashed"].boolOr(false); storeUsed = j["storeUsed"].boolOr(false);
  campaignSeed = std::strtoull(j["campaignSeed"].strOr("0").c_str(), nullptr, 10);
  for (auto& m : j["campaign"].arr) campaign.push_back(m.strOr(""));
  level = (int)j["level"].numOr(0); levelsWon = (int)j["levelsWon"].numOr(0);
  for (auto& kv : j["xp"].obj) xp[kv.first] = (float)kv.second.numOr(0);
  spins = std::max(0, (int)j["spins"].numOr(0)); spinSmashMark = (int)j["spinSmashMark"].numOr(0);
  zooDone = j["zooDone"].boolOr(false); zooKey = j["zooKey"].boolOr(false);
  for (auto& g : j["gates"].arr) gates.insert(g.strOr(""));
  gauntletSlot = j["gauntletSlot"].boolOr(false); gauntlet = j["gauntlet"].boolOr(false); gauntletOn = j["gauntletOn"].boolOr(false);
  gauntletCracked = j["gauntletCracked"].boolOr(false); endingDone = j["endingDone"].boolOr(false);
  for (auto& g : j["stones"].arr) stones.push_back(g.strOr(""));
  lastBoss = j["lastBoss"].strOr("");
}
void SaveData::reset() { *this = SaveData(); }
Json SaveData::toJson() const {
  Json j = Json::makeObj();
  j.set("version", Json::makeNum(3));
  Json lk = Json::makeObj(); for (auto& kv : looks) lk.set(kv.first, kv.second.toJson()); j.set("looks", lk);
  j.set("created", Json::makeBool(created)); j.set("spinSlot", Json::makeBool(spinSlot)); j.set("zooCompleted", Json::makeBool(zooCompleted));
  j.set("sandbox", sandbox.toJson()); j.set("adventure", adventure.toJson()); j.set("crazy", crazy.toJson());
  Json s = Json::makeObj();
  s.set("difficulty", Json::makeNum(settings.difficulty)); s.set("players", Json::makeNum(settings.players)); s.set("police", Json::makeBool(settings.police)); s.set("policeAsked", Json::makeBool(settings.policeAsked)); s.set("muted", Json::makeBool(settings.muted));
  s.set("invertY", Json::makeBool(settings.invertY)); s.set("volume", Json::makeNum(settings.volume)); s.set("mouseSens", Json::makeNum(settings.mouseSens)); s.set("reducedMotion", Json::makeBool(settings.reducedMotion));
  j.set("settings", s);
  Json t = Json::makeObj(); t.set("step", Json::makeNum(tutorial.step)); t.set("done", Json::makeBool(tutorial.done)); t.set("rainbows", Json::makeNum(tutorial.rainbows)); t.set("buildings", Json::makeNum(tutorial.buildings)); j.set("tutorial", t);
  Json m = Json::makeArr(); for (auto& id : monsters) m.arr.push_back(Json::makeStr(id)); j.set("monsters", m);
  j.set("name", Json::makeStr(name)); j.set("lastPlayed", Json::makeStr(lastPlayed));
  j.set("monster", Json::makeStr(monster)); j.set("eggs", Json::makeNum(eggs));
  Json inf = Json::makeArr(); for (auto& id : infinity) inf.arr.push_back(Json::makeStr(id)); j.set("infinity", inf);
  j.set("adventureUnlocked", Json::makeBool(adventureUnlocked));
  j.set("crazyUnlocked", Json::makeBool(crazyUnlocked)); j.set("bulkDefeated", Json::makeBool(bulkDefeated));
  return j;
}
bool SaveData::fromJson(const Json& j) {
  if (j.type != Json::Object) return false;
  reset();
  sandbox.fromJson(j["sandbox"]); adventure.fromJson(j["adventure"]); crazy.fromJson(j["crazy"]);
  const Json& s = j["settings"];
  settings.difficulty = std::max(0, std::min(2, (int)s["difficulty"].numOr(0))); settings.players = std::max(1, std::min(2, (int)s["players"].numOr(1))); settings.police = s["police"].boolOr(false); settings.policeAsked = s["policeAsked"].boolOr(false); settings.muted = s["muted"].boolOr(false);
  settings.invertY = s["invertY"].boolOr(false); settings.volume = (float)s["volume"].numOr(0.8); settings.mouseSens = (float)s["mouseSens"].numOr(1.0); settings.reducedMotion = s["reducedMotion"].boolOr(false);
  tutorial.step = (int)j["tutorial"]["step"].numOr(0); tutorial.done = j["tutorial"]["done"].boolOr(false); tutorial.rainbows = (int)j["tutorial"]["rainbows"].numOr(0); tutorial.buildings = (int)j["tutorial"]["buildings"].numOr(0);
  created = j["created"].boolOr(true);  // older saves never had a creation spin: they count as created
  zooCompleted = j["zooCompleted"].boolOr(false); spinSlot = j["spinSlot"].boolOr(false);
  bool legacy = !j.has("looks");
  monsters.clear();
  for (auto& m : j["monsters"].arr) monsters.insert(m.strOr("tommyzilla"));
  if (legacy || monsters.empty()) monsters.insert("tommyzilla");  // pre-spinner saves always had Tommyzilla
  for (auto& kv : j["looks"].obj) { KaijuLook l; if (l.fromJson(kv.second)) looks[kv.first] = l; }
  monster = j["monster"].strOr(*monsters.begin()); if (!monsters.count(monster)) monster = *monsters.begin();
  version = (int)j["version"].numOr(2);
  if (version < 3) { prog::migrateCampaign(adventure); prog::migrateCampaign(crazy); prog::migrateCampaign(sandbox); version = 3; }
  eggs = j.has("eggs") ? std::max(0, (int)j["eggs"].numOr(0)) : 30;
  infinity.clear(); for (auto& id : j["infinity"].arr) infinity.insert(id.strOr(""));
  adventureUnlocked = j["adventureUnlocked"].boolOr(false);
  crazyUnlocked = j["crazyUnlocked"].boolOr(false); bulkDefeated = j["bulkDefeated"].boolOr(false);
  name = j["name"].strOr(""); lastPlayed = j["lastPlayed"].strOr("");
  return true;
}

namespace prog {
// 15-level saves (version 2): L1-12 are unchanged; anyone on old L13-L15 restarts the new gauntlet arc at L13;
// a finished old campaign stays finished (a new 20-level run starts next time). The old random-level Bulk's Glove
// becomes the gauntlet (owned, slot unlocked, no stones yet) and leaves the skill slots.
void migrateCampaign(Profile& p) {
  const int OLD = 15, NEW = (int)T.levels.size();
  if (p.level >= OLD) { p.level = NEW; p.levelsWon = NEW; }
  else if (p.level >= 12) { p.level = 12; p.levelsWon = std::min(p.levelsWon, 12); }
  if (p.levelsWon >= 12) p.gauntletSlot = true;
  if (p.moves.count("glove")) {
    p.moves.erase("glove"); p.gauntlet = true; p.gauntletSlot = true; p.gauntletOn = true;
    for (auto& s : p.slots) if (s == "glove") s.clear();
  }
}
float damageMult(int l) { return 1 + C.damagePerLevel * (l - 1); }
float rangeMult(int l) { return 1 + C.rangePerLevel * (l - 1); }
float speedMult(int l) { return 1 + C.speedPerLevel * (l - 1); }
int maxLevel() { return (int)C.upgradeSteps + 1; }
std::string bossAfter(const std::string& requested, const std::string& previous) {
  static const char* kOrder[] = {"supertank", "mech", "moth", "crab", "blimp"};
  if (!requested.empty() && requested != previous) return requested;
  for (const char* id : kOrder) if (std::string(id) != previous) return id;
  return "supertank";
}
int upgradeCost(int l) { return C.upgradeCost(l); }
int buyUpgrade(Profile& p, Upgrade u) {
  int& l = u == UP_DAMAGE ? p.dmgLvl : u == UP_RANGE ? p.rangeLvl : p.speedLvl;
  int cost = upgradeCost(l); if (cost < 0) return 2;
  if (p.coins < cost) return 1;
  p.coins -= cost; l++; p.storeUsed = true; return 0;
}
bool moveConditionMet(const MoveDef& m, const Profile& p) {
  switch (m.unlock) {
    case UnlockType::Start: return true;
    case UnlockType::Buildings: return p.smashCount >= m.unlockCount;
    case UnlockType::Landmark: return p.landmarks.count(m.unlockLandmark) > 0;
    case UnlockType::Milestone: return roarMilestoneMet(p);
    case UnlockType::Hidden: return false;  // only by finding it
  }
  return false;
}
std::vector<int> pendingUnlocks(const Profile& p, const Tables& t) {
  std::vector<int> r;
  for (size_t i = 0; i < t.moves.size(); i++) if (!p.moves.count(t.moves[i].id) && moveConditionMet(t.moves[i], p)) r.push_back((int)i);
  return r;
}
int schoolProgress(const Profile& p) { int n = 0; for (auto& s : C.schoolLandmarks) if (p.landmarks.count(s)) n++; return n; }
bool slot2Earned(const Profile& p) { return p.smashCount >= (int)C.slot2Smashes; }
int freeSlot(const Profile& p) { for (int i = 0; i < p.slotCount; i++) if (p.slots[i].empty()) return i; return -1; }
bool isPower(const MoveDef& m) { return m.kind != MoveKind::Melee; }
bool roarMilestoneMet(const Profile& p) { return (int)p.landmarks.size() >= (int)C.roarUnlockLandmarks || p.hitsTaken >= (int)C.roarUnlockHits; }
int buyKey(Profile& p) { int c = (int)C.keyCost; if (p.coins < c) return 1; p.coins -= c; p.keys++; return 0; }
int projectileCap() { return std::max(1, (int)C.rapidProjectileCap); }
bool slotAccepts(const MoveDef& m, int slot) { return m.slot == slot + 1; }
std::vector<std::string> slotChoices(const Profile& p, const Tables& t, int slot) {
  std::vector<std::string> r;
  for (auto& m : t.moves) if (p.moves.count(m.id) && slotAccepts(m, slot)) r.push_back(m.id);
  return r;
}
bool cycleSlot(Profile& p, const Tables& t, int slot, int dir) {
  if (slot < 0 || slot >= p.slotCount) return false;
  std::vector<std::string> c = slotChoices(p, t, slot);
  if (c.empty()) return false;
  int cur = -1; for (int i = 0; i < (int)c.size(); i++) if (c[i] == p.slots[slot]) cur = i;
  int n = (int)c.size(), next = cur < 0 ? 0 : ((cur + dir) % n + n) % n;
  if (c[next] == p.slots[slot]) return false;
  p.slots[slot] = c[next]; return true;
}
void normalizeSlots(Profile& p, const Tables& t) {
  for (auto& m : t.moves) if (m.unlock == UnlockType::Start) p.moves.insert(m.id);
  p.slotCount = std::max(p.slotCount, std::min(2, (int)C.startSlots));
  for (int s = 0; s < 2; s++) {
    int mi = t.moveIndex(p.slots[s]);
    if (!p.slots[s].empty() && (mi < 0 || !p.moves.count(p.slots[s]) || !slotAccepts(t.moves[mi], s) || s >= p.slotCount)) p.slots[s].clear();
  }
  if (p.slots[0] == p.slots[1]) p.slots[1].clear();
  for (int s = 0; s < p.slotCount; s++) if (p.slots[s].empty()) {
    for (auto& m : t.moves) if (m.unlock == UnlockType::Start && isPower(m) && slotAccepts(m, s)) { p.slots[s] = m.id; break; }
    if (p.slots[s].empty()) { auto c = slotChoices(p, t, s); for (auto& id : c) if (isPower(t.moves[t.moveIndex(id)])) { p.slots[s] = id; break; } }
  }
}
bool equip(Profile& p, const std::string& id, int slot) {
  if (slot < 0 || slot >= p.slotCount || !p.moves.count(id)) return false;
  int mi = T.moveIndex(id); if (mi < 0 || !slotAccepts(T.moves[mi], slot)) return false;
  for (int i = 0; i < 2; i++) if (p.slots[i] == id) p.slots[i].clear();  // a move lives in one slot at most
  p.slots[slot] = id; return true;
}
int starsFromHeat(float heat) { int s = (int)(heat + 1e-6f); return s < 0 ? 0 : s > 5 ? 5 : s; }
PoliceTargets policeTargets(int stars) {
  PoliceTargets t;
  t.cars = std::min((int)C.maxCars, stars * (int)C.carsPerStar);
  t.heli = stars >= (int)C.heliStars;
  t.tanks = stars >= (int)C.tankStars ? std::min((int)C.maxTanks, stars - (int)C.tankStars + 2) : 0;
  t.nuke = stars >= (int)C.nukeStars;
  return t;
}
std::vector<std::string> campaignOrder(uint64_t seed, const Tables& t, int count) {
  std::vector<std::string> pool;
  for (auto& m : t.monsters) if (!m.starter) pool.push_back(m.id);
  Rng r(seed ^ 0xA5A5F00D12345ull);
  r.shuffle(pool);
  if (pool.empty()) return pool;
  std::vector<std::string> out = pool;
  while ((int)out.size() < count) { std::vector<std::string> again = pool; r.shuffle(again); for (auto& id : again) out.push_back(id); }  // longer campaign than roster: repeat eggs (duplicates give a bonus)
  if ((int)out.size() > count) out.resize(count);
  return out;
}
std::vector<Goal> adventureGoals(const SaveData& s) {
  const Profile& p = s.sandbox;
  return {
    {"Smash " + std::to_string((int)C.slot2Smashes) + " buildings (" + std::to_string(std::min(p.smashCount, (int)C.slot2Smashes)) + "/" + std::to_string((int)C.slot2Smashes) + ")", prog::slot2Earned(p)},
    {"Buy an upgrade in the store", p.storeUsed},
    {"Smash Ruth King Elementary (unlock it with " + std::to_string((int)C.schoolUnlockCount) + " landmarks: " + std::to_string(schoolProgress(p)) + "/" + std::to_string((int)C.schoolUnlockCount) + ")", p.schoolSmashed},
  };
}
bool adventureReady(const SaveData& s) { for (auto& g : adventureGoals(s)) if (!g.done) return false; return true; }
void initProfile(Profile& p, const Tables& t) {
  for (auto& m : t.moves) if (m.unlock == UnlockType::Start) p.moves.insert(m.id);
  p.slotCount = std::max(1, std::min(2, (int)C.startSlots));
  p.slots[0] = p.slots[1] = "";
  normalizeSlots(p, t);  // slot 1 = Glow Breath, slot 2 = Rainbow Blaster
  const char* def[MELEE_BINDS] = {"claw", "tail", "stomp", "", "", ""};
  for (int i = 0; i < MELEE_BINDS; i++) p.melee[i] = def[i];
}
}  // namespace prog
}  // namespace tz
