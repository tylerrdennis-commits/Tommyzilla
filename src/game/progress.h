// Progression rules (profiles, unlock tables, economy, police escalation, campaign shuffle, save format).
// Pure logic with no rendering or platform code, so the Linux test harness can exercise it directly.
#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "../core/json.h"
#include "eggs.h"
#include "tables.h"
namespace tz {
constexpr int MELEE_BINDS = 6;  // X, B, A and LB+X, LB+B, LB+A (keyboard: J K L / U I O)
struct Profile {
  int coins = 0, totalCoins = 0;
  int dmgLvl = 1, rangeLvl = 1, speedLvl = 1;      // 1..6 (5 purchasable steps)
  std::set<std::string> moves;                     // unlocked move ids
  std::string slots[2];                            // active skill slots: [0] close range (slot type 1), [1] ranged/super (slot type 2)
  int slotCount = 2;
  std::string melee[MELEE_BINDS];
  int smashCount = 0;
  int keys = 0;                                    // chest keys (bought in the store, one per locked skill chest)
  int gloveLevel = -1; bool gloveDropped = false;  // secret: which adventure level (0-based) hides the glove this playthrough
  std::string lockedMonster;  // normal Adventure: favourite kaiju picked at level 6, locked for the rest of the campaign
  int eatenTotal = 0;         // lifetime civilians eaten (eaten = the current super-attack fuel)
  std::vector<std::string> chests;                 // skills waiting in a locked chest (not yet opened)
  int eaten = 0, rainbows = 0;                     // eaten = civilians in the belly (fuel, spent by supers); rainbows (all ways)
  int hitsTaken = 0;                               // hits survived (debounced by config hitCountGap); unlocks the Charged Roar
  std::set<std::string> landmarks;                 // distinct landmarks ever smashed (by id)
  bool schoolUnlocked = false, schoolSmashed = false, storeUsed = false;
  // adventure only
  uint64_t campaignSeed = 0;
  std::vector<std::string> campaign;               // monster ids hatched at the end of each level, in order
  int level = 0;                                   // current level index
  int levelsWon = 0;                               // number of levels completed (eggs hatched)
  // eggs / growth (per mode)
  std::map<std::string, float> xp;                 // growth XP per monster id; no entry = fully grown (legacy / starter)
  int spins = 0, spinSmashMark = 0;                // egg-spin tokens; sandbox: buildings counted toward the next free spin
  bool zooDone = false, zooKey = false;            // Kaiju Zoo opening played in this mode; secret Zookeeper's Key (never announced)
  std::set<std::string> gates;                     // Zoo Gates opened (each reward once)
  // gauntlet arc (adventure / crazy; sandbox gets it from the Crazy ending)
  bool gauntletSlot = false, gauntlet = false, gauntletOn = false, gauntletCracked = false, endingDone = false;
  std::vector<std::string> stones;                 // stone ids in the gauntlet, in pickup order
  void reset();
  Json toJson() const;
  void fromJson(const Json& j);
};
struct Settings {
  bool police = false, policeAsked = false, muted = false, invertY = false;
  float volume = 0.8f, mouseSens = 1.0f;
  int difficulty = 0, players = 1;  // Titan: Easy/Hard/Insane, 1-2 players
  bool reducedMotion = false;  // Vault: menu/HUD animations off
};
struct TutorialState { int step = 0; bool done = false; int rainbows = 0, buildings = 0; std::vector<std::pair<std::string, int>> pending; };  // rainbows: civilians turned into rainbows during the tutorial
struct SaveData {
  Profile sandbox, adventure, crazy;               // three independent sections (Crazy = 2x-hard adventure)
  Settings settings;
  TutorialState tutorial;
  std::set<std::string> monsters = {"tommyzilla"};  // playable monsters (shared by both modes)
  std::string monster = "tommyzilla";               // selected
  std::string name, lastPlayed;                     // optional player name; local time of the last save
  bool adventureUnlocked = false, crazyUnlocked = false, bulkDefeated = false;
  std::map<std::string, KaijuLook> looks;           // rolled look per owned monster (shared by all modes, like the roster)
  bool created = true;                              // false: a brand-new slot that still needs its character-creation spin
  bool spinSlot = false;                            // slot made with the egg spinner (new games: zoo opening per mode)
  bool zooCompleted = false;                        // the zoo opening was finished (gulp) at least once in this slot
  int version = 3;                                  // 3 = 20-level campaign (2 = 15 levels, migrated on load)
  void reset();
  Json toJson() const;
  bool fromJson(const Json& j);
};
namespace prog {
void migrateCampaign(Profile& p);  // version-2 (15-level) save -> 20-level campaign
enum Upgrade { UP_DAMAGE, UP_RANGE, UP_SPEED };
float damageMult(int lvl);
float rangeMult(int lvl);
float speedMult(int lvl);                 // 1.0 .. 2.0 in 5 steps
int upgradeCost(int lvl);                 // cost to buy the next level, -1 if maxed
int maxLevel();
// returns 0 = bought, 1 = not enough coins, 2 = maxed
int buyUpgrade(Profile& p, Upgrade u);
bool moveConditionMet(const MoveDef& m, const Profile& p);
std::vector<int> pendingUnlocks(const Profile& p, const Tables& t);  // satisfied but not yet unlocked
int schoolProgress(const Profile& p);
bool slot2Earned(const Profile& p);
int freeSlot(const Profile& p);            // -1 if full
bool equip(Profile& p, const std::string& id, int slot);   // only into the slot matching the move's slot type
bool slotAccepts(const MoveDef& m, int slot);                // slot 0 takes type-1 skills, slot 1 takes type-2 skills
std::vector<std::string> slotChoices(const Profile& p, const Tables& t, int slot);  // owned skills that fit a slot, table order
bool cycleSlot(Profile& p, const Tables& t, int slot, int dir = 1);  // quick-cycle to the next owned skill for that slot
void normalizeSlots(Profile& p, const Tables& t);            // grant missing starter skills, fix slot types, fill empty slots (old saves)
bool roarMilestoneMet(const Profile& p);
int projectileCap();
int buyKey(Profile& p);                                     // 0 bought, 1 not enough coins
bool isPower(const MoveDef& m);
// police escalation
int starsFromHeat(float heat);
struct PoliceTargets { int cars = 0; bool heli = false; int tanks = 0; bool nuke = false; };
PoliceTargets policeTargets(int stars);
// campaign
std::vector<std::string> campaignOrder(uint64_t seed, const Tables& t, int count);
struct Goal { std::string label; bool done; };
std::vector<Goal> adventureGoals(const SaveData& s);
bool adventureReady(const SaveData& s);
void initProfile(Profile& p, const Tables& t);  // starter moves + slots + melee loadout
}  // namespace prog
}  // namespace tz
