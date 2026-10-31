// Vault: per-kaiju upgradable powers (5 per kaiju, themed names), the power spinner, and the cheats menu state.
// Pure data + math here; menus/drawing in src/ui/vault_menus.cpp. Gameplay reads the multipliers via the vpow:: API.
#pragma once
#include <map>
#include <string>
#include <vector>
#include "../core/rng.h"
namespace tz {
struct Json;
namespace vpow {
enum Stat { ST_DAMAGE, ST_RANGE, ST_SPEED, ST_HP, ST_THRIFT, ST_COUNT };  // thrift = supers cost fewer eaten
constexpr int kMaxLevel = 5;
constexpr float kPerLevel = 0.08f;                                          // +8% per level (thrift: -8% super cost)
struct PowerDef { Stat stat; std::string key, name, desc; int icon; unsigned color; };
std::vector<PowerDef> kaijuPowers(const std::string& kaijuId, const std::string& kaijuName);  // always 5, one per stat
int cost(int level);                                                        // price for the next level, -1 at max
// saved state (one per save file)
struct State {
  std::map<std::string, int> levels;                                        // "kaijuId:stat" -> 0..kMaxLevel
  bool cheatsUnlocked = false, god = false, freeSupers = false, bigHead = false;
  int level(const std::string& kaiju, Stat s) const;
  void setLevel(const std::string& kaiju, Stat s, int l);
  float mult(const std::string& kaiju, Stat s) const;                       // 1 + kPerLevel*lvl (thrift: 1 - kPerLevel*lvl)
  bool buy(const std::string& kaiju, Stat s, int& coins);                   // false if maxed / too poor
  Json toJson() const; void fromJson(const Json& j);
};
// power spinner: lands on one of the 5 powers (weighted toward lower levels), +1 level for free
constexpr float kSpinSec = 2.6f;
int spinCost();                                                             // coins per power spin
int spinPick(const State& st, const std::string& kaiju, uint64_t seed);     // stat index, or -1 if all maxed
float spinOffset(float t, int target, int n, int laps);                     // reel position (items) at time t; ends exactly on target
}  // namespace vpow
}  // namespace tz
