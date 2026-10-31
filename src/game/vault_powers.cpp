#include "vault_powers.h"
#include <algorithm>
#include <cmath>
#include "../core/json.h"
namespace tz { namespace vpow {
namespace {
uint64_t hashStr(const std::string& s) { uint64_t h = 1469598103934665603ull; for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; } return h; }
const char* kKey[ST_COUNT] = {"dmg", "reach", "speed", "hp", "thrift"};
// themed name pools (one picked per kaiju, so every kaiju's five powers read differently)
const char* kNames[ST_COUNT][6] = {
  {"Crusher Fists", "Mega Chomp", "Tower Toppler", "Rubble Maker", "Brick Buster", "Smash Surge"},
  {"Long Tail", "Giant Swipe", "Wide Roar", "Shockwave", "Big Splash", "Mighty Reach"},
  {"Turbo Feet", "Thunder Run", "Zoom Stomp", "Rocket Legs", "Dash Dash", "Speedy Scales"},
  {"Iron Hide", "Rock Skin", "Super Shell", "Tough Tummy", "Steel Scales", "Mega Heart"},
  {"Snack Saver", "Lucky Belly", "Power Gulp", "Hungry Hero", "Big Appetite", "Fuel Tank"}};
const char* kDesc[ST_COUNT] = {"+8% bricks smashed per hit, per level", "+8% attack reach, per level", "+8% walk speed, per level",
                               "+8% max health, per level", "Super attacks cost 8% fewer eaten, per level"};
const int kIcon[ST_COUNT] = {5, 6, 7, 3, 8};
const unsigned kCol[ST_COUNT] = {0xff8a5a, 0x3ad7ff, 0x7aff6a, 0xc8ccd4, 0xffd36b};
}  // namespace
std::vector<PowerDef> kaijuPowers(const std::string& id, const std::string& name) {
  std::vector<PowerDef> v; uint64_t h = hashStr(id);
  for (int s = 0; s < ST_COUNT; s++) {
    int pick = (int)((h >> (s * 7)) % 6);
    std::string nm = kNames[s][pick];
    if (s == 0 && !name.empty()) nm = name.substr(0, name.find(' ')) + " " + nm.substr(nm.find(' ') + 1);  // signature power carries the kaiju's name
    v.push_back({(Stat)s, kKey[s], nm, kDesc[s], kIcon[s], kCol[s]});
  }
  return v;
}
int cost(int level) { return level >= kMaxLevel ? -1 : 150 * (level + 1) + 50 * level * level; }
static std::string k(const std::string& kaiju, Stat s) { return kaiju + ":" + kKey[s]; }
int State::level(const std::string& kaiju, Stat s) const { auto it = levels.find(k(kaiju, s)); return it == levels.end() ? 0 : it->second; }
void State::setLevel(const std::string& kaiju, Stat s, int l) { levels[k(kaiju, s)] = std::max(0, std::min(kMaxLevel, l)); }
float State::mult(const std::string& kaiju, Stat s) const { float l = (float)level(kaiju, s); return s == ST_THRIFT ? 1 - kPerLevel * l : 1 + kPerLevel * l; }
bool State::buy(const std::string& kaiju, Stat s, int& coins) {
  int c = cost(level(kaiju, s)); if (c < 0 || coins < c) return false;
  coins -= c; setLevel(kaiju, s, level(kaiju, s) + 1); return true;
}
Json State::toJson() const {
  Json j = Json::makeObj(), l = Json::makeObj();
  for (auto& kv : levels) l.set(kv.first, Json::makeNum(kv.second));
  j.set("levels", l); j.set("cheats", Json::makeBool(cheatsUnlocked)); j.set("god", Json::makeBool(god));
  j.set("freeSupers", Json::makeBool(freeSupers)); j.set("bigHead", Json::makeBool(bigHead));
  return j;
}
void State::fromJson(const Json& j) {
  *this = State();
  for (auto& kv : j["levels"].obj) levels[kv.first] = std::max(0, std::min(kMaxLevel, (int)kv.second.numOr(0)));
  cheatsUnlocked = j["cheats"].boolOr(false); god = cheatsUnlocked && j["god"].boolOr(false);
  freeSupers = cheatsUnlocked && j["freeSupers"].boolOr(false); bigHead = cheatsUnlocked && j["bigHead"].boolOr(false);
}
int spinCost() { return 300; }
int spinPick(const State& st, const std::string& kaiju, uint64_t seed) {
  Rng r(seed); float w[ST_COUNT], tot = 0;
  for (int s = 0; s < ST_COUNT; s++) { int l = st.level(kaiju, (Stat)s); w[s] = l >= kMaxLevel ? 0.f : (float)(kMaxLevel + 1 - l); tot += w[s]; }
  if (tot <= 0) return -1;
  float x = r.next() * tot;
  for (int s = 0; s < ST_COUNT; s++) { if (w[s] <= 0) continue; if (x < w[s]) return s; x -= w[s]; }
  for (int s = ST_COUNT - 1; s >= 0; s--) if (w[s] > 0) return s;
  return -1;
}
float spinOffset(float t, int target, int n, int laps) {
  float total = (float)(laps * n + target), u = std::max(0.f, std::min(1.f, t / kSpinSec));
  float e = 1 - std::pow(1 - u, 3.f);   // fast start, slow landing (like the egg spinner)
  return total * e;
}
} }  // namespace tz::vpow
