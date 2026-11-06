// Egg spinner, rarity tiers, rolled colour looks + finishes, growth/XP curve, zoo/gate/stone tables (data/eggs.json).
// Pure logic (no rendering / platform), exercised directly by the Linux tests.
#pragma once
#include <map>
#include <string>
#include <vector>
#include "../core/json.h"
#include "../core/math.h"
#include "../core/rng.h"
#include "tables.h"
namespace tz {
enum Finish : int { FIN_NONE = 0, FIN_GOLD = 1, FIN_CHROME = 2, FIN_HOLO = 3 };
struct RarityDef { std::string id, name; float rate = 0; Color color; float size = 1, hp = 1; int passives = 0, supers = 0, dupCoins = 0; float dupXp = 0; };
struct PaletteDef { std::string name; float weight = 1; Color primary, secondary, accent, eyes; };
struct FinishDef { std::string id, name; float rate = 0; Color color; };
struct PassiveDef { std::string id, name, desc; };
struct GateDef { std::string id; int level = 0; float x = 0, z = 0; std::string reward; int amount = 0; };  // level 0 = sandbox, else 1-based adventure level
// One thing a stone can be assigned to. channel is pulse / strike / blast / roar.
// bricks / radius scale that action, extra adds buildings to the walk pulse, heal runs when a super fires.
struct StonePower {
  std::string id, name, blurb, channel;
  float bricks = 1, radius = 1, heal = 0;
  int extra = 0;
  bool people = false, zap = false;
};
struct StoneDef {  // stone gate on that (1-based) level; powers are assigned in the Infinity Stones menu
  std::string id, name, blurb;
  Color color;
  int level = 0;
  float x = 0, z = 0;
  std::vector<StonePower> powers;
};
struct EggTables {
  std::vector<RarityDef> rarities;
  std::map<std::string, int> tierOf;                       // monster id -> rarity index
  std::map<std::string, std::vector<PaletteDef>> palettes; // monster id -> allowed palettes ([0] = classic)
  float hueJitter = 22, swapChance = 0.2f;
  std::vector<FinishDef> finishes;                         // index + 1 = Finish id (gold, chrome, holo)
  std::vector<PassiveDef> passives;
  std::vector<std::string> bonusSupers;
  int spinsNewSlot = 1, spinsPerLevel = 1, spinCost = 400, sandboxSpinEvery = 30; float chestSpinChance = 0.5f;
  float spinAutoStop = 7, spinStopTime = 1.4f, spinCellsPerSec = 14; int reelLength = 40;
  float growthStart = 0.42f, growthFullXp = 600, growthCurve = 0.8f, babyDamageTaken = 0.55f, babyPower = 1.5f, camSmooth = 1.5f;
  std::map<std::string, float> xp;
  float healToXp = 1, lifesteal = 1, eatHeal = 2;
  int deevoPrice = 2500;
  float zooEatReach = 2.0f; float zooCageR = 13, zooKeeperStop = 9, zooIdleHint = 7, zooX = -55, zooZ = 135; int zooOnlookers = 8;
  std::vector<GateDef> gates;
  std::vector<StoneDef> stones;
  int gauntletSlotLevel = 12, gauntletBoxLevel = 13; std::vector<int> pulseTargets = {0, 2, 3, 5, 6, 8, 10}; float repelPush = 60;
  float bulkStage2Hp = 220, bulkStage3Hp = 200, bulkStage2Scale = 2.2f, bulkStage2Damage = 6.0f, bulkAngerTime = 2.0f, bulkTransformTime = 2.5f, snapThreshold = 0.5f, sandboxUltRadius = 120, sandboxUltCooldown = 20;
  bool load(const std::string& json, const Tables& t, std::string* err);
  int rarityOf(const std::string& monster) const { auto it = tierOf.find(monster); return it == tierOf.end() ? 0 : it->second; }
  float xpFor(const std::string& src) const { auto it = xp.find(src); return it == xp.end() ? 0.f : it->second; }
  int speciesOfCount(size_t rarity, const Tables& t) const;
  const RarityDef& rarity(int i) const { return rarities[(size_t)std::max(0, std::min((int)rarities.size() - 1, i))]; }
};
extern EggTables E;
struct InputState;
bool zooGulpPressed(const InputState& in);  // Rex: eat / claw / grab / confirm all gulp the zookeeper
std::string loadEggData(const std::string& json);  // "" ok; uses the embedded copy when json is empty

constexpr int kShinyEggCost = 10;   // eggs spent per shiny attempt
constexpr int kShinyOdds = 10;       // 1 in 10
inline constexpr const char* kShinyOddsLine = "Spend 10 eggs for a 1 in 10 chance at a shiny. This is a chance, not a guarantee.";
struct ShinyRoll { bool spent = false; bool hit = false; };
struct InfinityStoneDef { const char* id; const char* name; const char* what; };
inline constexpr InfinityStoneDef kInfinityStones[] = {
    {"space", "Space", "Each step covers more ground."},
    {"mind", "Mind", "Shows facing and pace on screen."},
    {"reality", "Reality", "Recolors the hero while equipped."},
    {"power", "Power", "Swells the hero's size for the run."},
    {"time", "Time", "Slows enemies, easing the fight."},
    {"soul", "Soul", "An aura that follows the hero."},
};
inline constexpr int kInfinityCount = 6;

struct KaijuLook {
  std::string monster; int rarity = 0; int palette = 0; Color primary, secondary, accent, eyes; int finish = FIN_NONE;
  bool shiny = false;  // hue-flipped palette + gold spark; a missed egg roll never clears it
  std::vector<std::string> passives; std::string bonusSuper;
  uint32_t genSeed = 0; int element = -1;  // Rex: procedural body seed (0 = classic model); Tommyzilla legendary element (-1 none, see kaiju_gen.h)
  bool has(const std::string& passive) const { for (auto& p : passives) if (p == passive) return true; return false; }
  Json toJson() const;
  bool fromJson(const Json& j);
};
namespace eggs {
Color hueShift(Color c, float deg);
std::string hex(Color c);
int rollRarity(Rng& r);                                         // weighted by rarity rates
std::vector<std::string> speciesOf(int rarity, const Tables& t);
std::string rollSpecies(Rng& r, const Tables& t, int* rarityOut = nullptr);
int rollFinish(Rng& r, float mult = 1);                         // independent chances, rarest first
KaijuLook defaultLook(const MonsterDef& m);                     // classic palette, no finish (legacy saves)
KaijuLook rollLook(Rng& r, const std::string& monster, const Tables& t);  // palette mix + finish + perks of its tier
ShinyRoll rollShiny(int& eggs, Rng& r);  // spends kShinyEggCost when eggs allow it; hit is a 1-in-kShinyOdds roll
float growthFrac(float xp);                                     // 0..1
float growthScale(float xp);                                    // hatchling start .. 1 (cap)
}  // namespace eggs

// CS:GO-style case reel: the outcome is decided first (weighted roll), the reel is built so the decided egg sits at
// stopIndex, and stopping eases the reel onto exactly that cell.
struct Spinner {
  enum Phase { Idle, Spinning, Stopping, Landed };
  Phase phase = Idle;
  std::vector<std::string> reel;   // monster id per cell
  std::vector<int> reelRarity;
  int stopIndex = 0; KaijuLook result;
  float pos = 0, t = 0, stopT = 0, stopFrom = 0, stopDur = 1, landedT = 0; int reason = 0; bool isNew = false;
  void start(Rng& r, const KaijuLook& decided, const Tables& t, int reason);
  void stop();                     // player input: begin the eased slowdown (also auto after E.spinAutoStop)
  void update(float dt);
  bool landed() const { return phase == Landed; }
  int cellUnderMarker() const { return (int)std::floor(pos + 0.5f); }
};
}  // namespace tz
