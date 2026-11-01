// data/eggs.json tables + spinner / look / growth logic (see eggs.h).
#include "eggs.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
namespace tz {
EggTables E;
std::string embeddedJson(const char* name);
namespace {
Color hexOr(const Json& j, Color d) { if (j.type != Json::String) return d; return hexColor((uint32_t)std::strtoul(j.str.c_str(), nullptr, 16)); }
float num(const Json& j, const char* k, float d) { return (float)j[k].numOr(d); }
StonePower pw(const char* id, const char* name, const char* channel, const char* blurb, float bricks, float radius, int extra, float heal, bool people, bool zap) {
  StonePower p; p.id = id; p.name = name; p.channel = channel; p.blurb = blurb; p.bricks = bricks; p.radius = radius; p.extra = extra; p.heal = heal; p.people = people; p.zap = zap; return p;
}
void ensureStonePowers(EggTables& e) {
  for (auto& s : e.stones) {
    if (s.blurb.empty()) {
      if (s.id == "storm") s.blurb = "A purple storm caught in glass. It wants the sky and anything that shoots.";
      else if (s.id == "tide") s.blurb = "Deep water that pushes. It wants your swings and your roar.";
      else if (s.id == "grove") s.blurb = "Living green that likes crowds. It wants the walk-pulse and your supers.";
      else if (s.id == "ember") s.blurb = "A coal that never goes out. It wants melee and ranged attacks.";
      else if (s.id == "sun") s.blurb = "Noon, bottled. It wants beams and a brighter roar.";
      else if (s.id == "spark") s.blurb = "Static that jumps. It wants the walk-pulse and a sharper jab.";
    }
    if (!s.powers.empty()) continue;
    if (s.id == "storm") {
      s.powers.push_back(pw("thunder", "Thunder Step", "pulse", "Pulse (the walk shockwave): flattens 2 extra buildings and the rings burn storm-purple.", 1, 1, 2, 0, false, false));
      s.powers.push_back(pw("bolt", "Bolt Breath", "blast", "Blast (breath, beams, bubbles and other ranged shots): smash 25% more bricks and reach a little farther.", 1.25f, 1.1f, 0, 0, false, false));
    } else if (s.id == "tide") {
      s.powers.push_back(pw("riptide", "Riptide Swipe", "strike", "Strike (melee): smash 20% more bricks and swing a bit wider.", 1.2f, 1.12f, 0, 0, false, false));
      s.powers.push_back(pw("undertow", "Undertow Roar", "roar", "Roar (Charged Roar and other supers): the blast covers 25% more ground.", 1, 1.25f, 0, 0, false, false));
    } else if (s.id == "grove") {
      s.powers.push_back(pw("roots", "Root Shock", "pulse", "Pulse: the walk shockwave also turns nearby people into rainbow paint. Building count stays whatever your stones already give.", 1, 1, 0, 0, true, false));
      s.powers.push_back(pw("canopy", "Canopy Roar", "roar", "Roar: firing a super heals you a little. Extra health becomes growth.", 1, 1, 0, 12, false, false));
    } else if (s.id == "ember") {
      s.powers.push_back(pw("cinder", "Cinder Claws", "strike", "Strike: melee hits smash 30% more bricks.", 1.3f, 1, 0, 0, false, false));
      s.powers.push_back(pw("flare", "Flare Shot", "blast", "Blast: ranged attacks smash 20% more bricks and fly farther.", 1.2f, 1.15f, 0, 0, false, false));
    } else if (s.id == "sun") {
      s.powers.push_back(pw("solar", "Solar Lance", "blast", "Blast: ranged attacks smash 15% more bricks. Only one blast power can be assigned at a time.", 1.15f, 1, 0, 0, false, false));
      s.powers.push_back(pw("daybreak", "Daybreak Roar", "roar", "Roar: supers reach 15% farther and heal a little when they fire.", 1, 1.15f, 0, 6, false, false));
    } else if (s.id == "spark") {
      s.powers.push_back(pw("sparkwalk", "Spark Walk", "pulse", "Pulse: flattens 1 extra building and zaps enemies caught near you.", 1, 1, 1, 0, false, true));
      s.powers.push_back(pw("jab", "Static Jab", "strike", "Strike: melee hits smash 15% more bricks.", 1.15f, 1, 0, 0, false, false));
    }
  }
}
}  // namespace

bool EggTables::load(const std::string& text, const Tables& t, std::string* err) {
  Json j; std::string perr;
  if (!Json::parse(text, j, &perr)) { if (err) *err = "eggs.json: " + perr; return false; }
  *this = EggTables();
  for (auto& r : j["rarities"].arr) {
    RarityDef d; d.id = r["id"].strOr("?"); d.name = r["name"].strOr(d.id); d.rate = num(r, "rate", 0); d.color = hexOr(r["color"], hexColor(0xffffff));
    d.size = num(r, "size", 1); d.hp = num(r, "hp", 1); d.passives = (int)num(r, "passives", 0); d.supers = (int)num(r, "supers", 0);
    d.dupCoins = (int)num(r, "dupCoins", 0); d.dupXp = num(r, "dupXp", 0);
    rarities.push_back(d);
  }
  if (rarities.empty()) { if (err) *err = "eggs.json: no rarities"; return false; }
  for (size_t i = 0; i < rarities.size(); i++)
    for (auto& id : j["tiers"][rarities[i].id].arr) {
      std::string m = id.strOr("");
      if (t.monsterIndex(m) < 0) { if (err) *err = "eggs.json: unknown monster in tiers: " + m; return false; }
      if (tierOf.count(m)) { if (err) *err = "eggs.json: monster in two tiers: " + m; return false; }
      tierOf[m] = (int)i;
    }
  for (auto& m : t.monsters) if (!tierOf.count(m.id)) { if (err) *err = "eggs.json: monster has no tier: " + m.id; return false; }
  for (auto& kv : j["palettes"].obj) {
    if (t.monsterIndex(kv.first) < 0) { if (err) *err = "eggs.json: palette for unknown monster " + kv.first; return false; }
    for (auto& p : kv.second.arr) {
      PaletteDef d; d.name = p["name"].strOr("?"); d.weight = num(p, "weight", 1);
      d.primary = hexOr(p["primary"], hexColor(0x888888)); d.secondary = hexOr(p["secondary"], d.primary); d.accent = hexOr(p["accent"], d.primary); d.eyes = hexOr(p["eyes"], hexColor(0xffe03a));
      palettes[kv.first].push_back(d);
    }
  }
  hueJitter = num(j["mix"], "hueJitter", 22); swapChance = num(j["mix"], "swapChance", 0.2f);
  for (auto& f : j["finishes"].arr) { FinishDef d; d.id = f["id"].strOr("?"); d.name = f["name"].strOr(d.id); d.rate = num(f, "rate", 0); d.color = hexOr(f["color"], hexColor(0xffffff)); finishes.push_back(d); }
  if (finishes.size() != 3) { if (err) *err = "eggs.json: need exactly 3 finishes (gold, chrome, holo)"; return false; }
  for (auto& p : j["passives"].arr) passives.push_back({p["id"].strOr("?"), p["name"].strOr("?"), p["desc"].strOr("")});
  for (auto& s : j["bonusSupers"].arr) { std::string id = s.strOr(""); int mi = t.moveIndex(id); if (mi < 0 || t.moves[mi].kind != MoveKind::Super) { if (err) *err = "eggs.json: bonus super is not a super: " + id; return false; } bonusSupers.push_back(id); }
  const Json& s = j["spins"];
  spinsNewSlot = (int)num(s, "newSlot", 1); spinsPerLevel = (int)num(s, "perLevelWon", 1); spinCost = (int)num(s, "coinCost", 400); sandboxSpinEvery = (int)num(s, "sandboxEvery", 30);
  chestSpinChance = num(s, "chestChance", 0.5f); spinAutoStop = num(s, "autoStop", 7); spinStopTime = num(s, "stopTime", 1.4f); reelLength = std::max(12, (int)num(s, "reelLength", 40)); spinCellsPerSec = num(s, "cellsPerSecond", 14);
  const Json& g = j["growth"];
  growthStart = num(g, "start", 0.42f); growthFullXp = std::max(1.f, num(g, "fullXp", 600)); growthCurve = num(g, "curve", 0.8f);
  babyDamageTaken = num(g, "babyDamageTaken", 0.55f); babyPower = num(g, "babyPower", 1.5f); camSmooth = num(g, "camSmooth", 1.5f);
  for (auto& kv : j["xp"].obj) xp[kv.first] = (float)kv.second.numOr(0);
  healToXp = xpFor("healToXp"); lifesteal = xpFor("lifesteal"); eatHeal = xpFor("eatHeal");
  deevoPrice = (int)num(j["deEvolution"], "price", 2500);
  const Json& z = j["zoo"];
  zooCageR = num(z, "cageRadius", 13); zooOnlookers = (int)num(z, "onlookers", 8); zooKeeperStop = num(z, "keeperStop", 9); zooEatReach = num(z, "eatReach", 2.0f); zooIdleHint = num(z, "idleHint", 7); zooX = num(z, "x", -55); zooZ = num(z, "z", 135);
  for (auto& q : j["zooGates"].arr) {
    GateDef d; d.id = q["id"].strOr("?"); d.level = (int)num(q, "level", 0); d.x = num(q, "x", 0); d.z = num(q, "z", 0); d.reward = q["reward"].strOr("coins"); d.amount = (int)num(q, "amount", 100);
    if (d.reward != "coins" && d.reward != "spin" && d.reward != "finish") { if (err) *err = "eggs.json: bad gate reward " + d.reward; return false; }
    gates.push_back(d);
  }
  for (auto& q : j["stones"].arr) {
    StoneDef d; d.id = q["id"].strOr("?"); d.name = q["name"].strOr("?"); d.blurb = q["blurb"].strOr("");
    d.color = hexOr(q["color"], hexColor(0xffffff)); d.level = (int)num(q, "level", 0); d.x = num(q, "x", 0); d.z = num(q, "z", 0);
    for (auto& pw : q["powers"].arr) {
      StonePower p; p.id = pw["id"].strOr(""); p.name = pw["name"].strOr(p.id); p.blurb = pw["blurb"].strOr(""); p.channel = pw["channel"].strOr("");
      p.bricks = num(pw, "bricks", 1); p.radius = num(pw, "radius", 1); p.heal = num(pw, "heal", 0); p.extra = (int)num(pw, "extra", 0);
      p.people = pw["people"].boolOr(false); p.zap = pw["zap"].boolOr(false);
      if (!p.id.empty() && !p.channel.empty()) d.powers.push_back(p);
    }
    stones.push_back(d);
  }
  if (stones.size() != 6) { if (err) *err = "eggs.json: need exactly 6 stones"; return false; }
  ensureStonePowers(*this);
  const Json& ga = j["gauntlet"];
  gauntletSlotLevel = (int)num(ga, "slotLevel", 12); gauntletBoxLevel = (int)num(ga, "boxLevel", 13); repelPush = num(ga, "repelPush", 60);
  if (ga["pulseTargets"].size() == 7) { pulseTargets.clear(); for (auto& v : ga["pulseTargets"].arr) pulseTargets.push_back((int)v.numOr(0)); }
  bulkStage2Hp = num(ga, "stage2Hp", 220); bulkStage3Hp = num(ga, "stage3Hp", 200); bulkStage2Scale = num(ga, "stage2Scale", 2.2f); bulkStage2Damage = num(ga, "stage2Damage", 3);
  bulkAngerTime = num(ga, "angerTime", 2); bulkTransformTime = num(ga, "transformTime", 2.5f); snapThreshold = num(ga, "snapThreshold", 0.5f);
  sandboxUltRadius = num(ga, "sandboxUltimateRadius", 120); sandboxUltCooldown = num(ga, "sandboxUltimateCooldown", 20);
  float sum = 0; for (auto& r : rarities) sum += r.rate;
  if (sum <= 0) { if (err) *err = "eggs.json: rarity rates sum to 0"; return false; }
  for (size_t i = 0; i < rarities.size(); i++) if (speciesOfCount(i, t) == 0 && rarities[i].rate > 0) { if (err) *err = "eggs.json: tier with no species: " + rarities[i].id; return false; }
  return true;
}
int EggTables::speciesOfCount(size_t r, const Tables& t) const { int n = 0; for (auto& m : t.monsters) if (rarityOf(m.id) == (int)r) n++; return n; }
std::string loadEggData(const std::string& json) {
  std::string err;
  if (!E.load(json.empty() ? embeddedJson("eggs") : json, T, &err)) return err;
  return "";
}

Json KaijuLook::toJson() const {
  Json j = Json::makeObj();
  j.set("monster", Json::makeStr(monster)); j.set("rarity", Json::makeNum(rarity)); j.set("palette", Json::makeNum(palette));
  j.set("primary", Json::makeStr(eggs::hex(primary))); j.set("secondary", Json::makeStr(eggs::hex(secondary))); j.set("accent", Json::makeStr(eggs::hex(accent))); j.set("eyes", Json::makeStr(eggs::hex(eyes)));
  j.set("finish", Json::makeNum(finish));
  Json p = Json::makeArr(); for (auto& s : passives) p.arr.push_back(Json::makeStr(s)); j.set("passives", p);
  j.set("bonusSuper", Json::makeStr(bonusSuper));
  j.set("genSeed", Json::makeNum((double)genSeed)); j.set("element", Json::makeNum(element));
  return j;
}
bool KaijuLook::fromJson(const Json& j) {
  if (j.type != Json::Object) return false;
  monster = j["monster"].strOr(""); rarity = (int)j["rarity"].numOr(0); palette = (int)j["palette"].numOr(0);
  primary = hexOr(j["primary"], hexColor(0x888888)); secondary = hexOr(j["secondary"], primary); accent = hexOr(j["accent"], primary); eyes = hexOr(j["eyes"], hexColor(0xffe03a));
  finish = std::max(0, std::min(3, (int)j["finish"].numOr(0)));
  passives.clear(); for (auto& s : j["passives"].arr) passives.push_back(s.strOr(""));
  bonusSuper = j["bonusSuper"].strOr("");
  genSeed = (uint32_t)j["genSeed"].numOr(0); element = std::max(-1, std::min(3, (int)j["element"].numOr(-1)));
  return !monster.empty();
}

namespace eggs {
std::string hex(Color c) { char b[8]; std::snprintf(b, sizeof b, "%02x%02x%02x", (int)std::lround(clampf(c.r, 0, 1) * 255), (int)std::lround(clampf(c.g, 0, 1) * 255), (int)std::lround(clampf(c.b, 0, 1) * 255)); return b; }
Color hueShift(Color c, float deg) {
  // rotate around the grey axis (YIQ-style hue rotation), keeps brightness roughly constant
  float a = deg * PI / 180.f, cs = std::cos(a), sn = std::sin(a);
  float m[3][3] = {{0.299f + 0.701f * cs + 0.168f * sn, 0.587f - 0.587f * cs + 0.330f * sn, 0.114f - 0.114f * cs - 0.497f * sn},
                   {0.299f - 0.299f * cs - 0.328f * sn, 0.587f + 0.413f * cs + 0.035f * sn, 0.114f - 0.114f * cs + 0.292f * sn},
                   {0.299f - 0.300f * cs + 1.250f * sn, 0.587f - 0.588f * cs - 1.050f * sn, 0.114f + 0.886f * cs - 0.203f * sn}};
  Color o{clampf(m[0][0] * c.r + m[0][1] * c.g + m[0][2] * c.b, 0, 1), clampf(m[1][0] * c.r + m[1][1] * c.g + m[1][2] * c.b, 0, 1), clampf(m[2][0] * c.r + m[2][1] * c.g + m[2][2] * c.b, 0, 1), c.a};
  return o;
}
int rollRarity(Rng& r) {
  float sum = 0; for (auto& d : E.rarities) sum += d.rate;
  float x = r.next() * sum;
  for (size_t i = 0; i < E.rarities.size(); i++) { x -= E.rarities[i].rate; if (x < 0) return (int)i; }
  return 0;
}
std::vector<std::string> speciesOf(int rarity, const Tables& t) { std::vector<std::string> v; for (auto& m : t.monsters) if (E.rarityOf(m.id) == rarity) v.push_back(m.id); return v; }
std::string rollSpecies(Rng& r, const Tables& t, int* rarityOut) {
  int ra = rollRarity(r);
  std::vector<std::string> v = speciesOf(ra, t);
  while (v.empty() && ra > 0) v = speciesOf(--ra, t);
  if (rarityOut) *rarityOut = ra;
  return v.empty() ? t.monsters[0].id : r.pick(v);
}
int rollFinish(Rng& r, float mult) {
  // independent chances, checked rarest first so each finish's overall rate is ~its own percentage
  for (int f = 3; f >= 1; f--) if (r.next() * 100.f < E.finishes[(size_t)f - 1].rate * mult) return f;
  return FIN_NONE;
}
KaijuLook defaultLook(const MonsterDef& m) {
  KaijuLook k; k.monster = m.id; k.rarity = E.rarities.empty() ? 0 : E.rarityOf(m.id);
  k.primary = m.body; k.secondary = m.belly; k.accent = m.accent; k.eyes = hexColor(0xffe03a);
  auto it = E.palettes.find(m.id); if (it != E.palettes.end() && !it->second.empty()) k.eyes = it->second[0].eyes;
  return k;
}
KaijuLook rollLook(Rng& r, const std::string& monster, const Tables& t) {
  int mi = std::max(0, t.monsterIndex(monster));
  KaijuLook k = defaultLook(t.monsters[(size_t)mi]);
  auto it = E.palettes.find(monster);
  if (it != E.palettes.end() && !it->second.empty()) {
    float sum = 0; for (auto& p : it->second) sum += p.weight;
    float x = r.next() * sum; size_t pick = 0;
    for (size_t i = 0; i < it->second.size(); i++) { x -= it->second[i].weight; if (x < 0) { pick = i; break; } }
    const PaletteDef& p = it->second[pick];
    k.palette = (int)pick; k.primary = p.primary; k.secondary = p.secondary; k.accent = p.accent; k.eyes = p.eyes;
  }
  float j = r.range(-E.hueJitter, E.hueJitter);
  k.primary = hueShift(k.primary, j); k.secondary = hueShift(k.secondary, j); k.accent = hueShift(k.accent, j * 1.5f);
  if (r.next() < E.swapChance) std::swap(k.secondary, k.accent);
  k.finish = rollFinish(r);
  const RarityDef& rd = E.rarity(k.rarity);
  std::vector<std::string> pool; for (auto& p : E.passives) pool.push_back(p.id);
  r.shuffle(pool);
  for (int i = 0; i < rd.passives && i < (int)pool.size(); i++) k.passives.push_back(pool[(size_t)i]);
  if (rd.supers > 0 && !E.bonusSupers.empty()) k.bonusSuper = r.pick(E.bonusSupers);
  return k;
}
float growthFrac(float xp) { return std::pow(clampf(xp / E.growthFullXp, 0, 1), E.growthCurve); }
float growthScale(float xp) { return E.growthStart + (1 - E.growthStart) * growthFrac(xp); }
}  // namespace eggs

void Spinner::start(Rng& r, const KaijuLook& decided, const Tables& tab, int why) {
  phase = Spinning; result = decided; reason = why; t = 0; stopT = 0; pos = 0; landedT = 0;
  int n = E.reelLength; reel.assign((size_t)n, ""); reelRarity.assign((size_t)n, 0);
  for (int i = 0; i < n; i++) { int ra = 0; reel[(size_t)i] = eggs::rollSpecies(r, tab, &ra); reelRarity[(size_t)i] = ra; }
  // land far enough down the reel for a good long scroll, never at the very end
  stopIndex = n - 6 - (int)(r.next() * 4);
  reel[(size_t)stopIndex] = decided.monster; reelRarity[(size_t)stopIndex] = decided.rarity;
  // neighbours of the winner tease: a rarer egg right next to it (classic near-miss) if one exists
  if (decided.rarity + 1 < (int)E.rarities.size()) { auto v = eggs::speciesOf(decided.rarity + 1, tab); if (!v.empty()) { reel[(size_t)stopIndex + 1] = r.pick(v); reelRarity[(size_t)stopIndex + 1] = decided.rarity + 1; } }
}
void Spinner::stop() {
  if (phase != Spinning) return;
  phase = Stopping; stopT = 0; stopFrom = pos;
  // ease-out cubic starts at 3 * distance / duration: pick the duration that matches the current reel speed
  stopDur = clampf(3.f * ((float)stopIndex - pos) / std::max(1.f, E.spinCellsPerSec), 0.6f, E.spinStopTime * 2.2f);
}
void Spinner::update(float dt) {
  if (phase == Idle || phase == Landed) { if (phase == Landed) landedT += dt; return; }
  t += dt;
  if (phase == Spinning) {
    pos += E.spinCellsPerSec * dt;
    float wrapAt = (float)stopIndex - 8;  // keep looping the early part of the reel until the player stops it
    if (pos > wrapAt && wrapAt > 4) pos -= std::floor(wrapAt * 0.5f);
    if (t >= E.spinAutoStop) stop();
    return;
  }
  stopT += dt;
  float k = clampf(stopT / std::max(0.05f, stopDur), 0, 1);
  float e = 1 - (1 - k) * (1 - k) * (1 - k);  // ease-out cubic
  pos = stopFrom + ((float)stopIndex - stopFrom) * e;
  if (k >= 1) { pos = (float)stopIndex; phase = Landed; landedT = 0; }
}
}  // namespace tz
