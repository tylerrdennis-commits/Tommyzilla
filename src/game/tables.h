// Data-driven tables: moves (25), monsters (18), adventure levels (10). Loaded from data/*.json
// (embedded copies are compiled in, so the exe also works without the data folder).
#pragma once
#include <string>
#include <vector>
#include "../core/math.h"
namespace tz {
enum class MoveKind { Melee, Direct, Super };
enum class MoveShape { Front, Rear, Around, Spin, Dash, Cone, Beam, Projectile, Lob, Roar, Quake, Meteor, Nova, Tornado, Blaster, Glove };
enum class UnlockType { Start, Buildings, Landmark, Milestone, Hidden };  // Hidden: secret item, only found in the world  // Milestone: any N landmarks OR survive M hits (config)
struct MoveDef {
  std::string id, name, desc, sound;
  MoveKind kind = MoveKind::Melee; MoveShape shape = MoveShape::Front;
  float radius = 6, reach = 6, bricks = 10, cooldown = 1, impulse = 15, duration = 0.5f, speed = 50, chargeTime = 0;
  Color color; bool glow = false, slows = false;
  bool rapid = false;  // quick particle/projectile weapon: no cooldown, fires on every press (capped on screen)
  bool chest = false;  // unlocking puts it in a LOCKED skill chest (opened with a store key) instead of granting it
  int slot = 1;        // active-skill slot type: 1 = close range (melee smash / breath), 2 = ranged / super; 0 = basic action (never slotted)
  UnlockType unlock = UnlockType::Start; int unlockCount = 0; std::string unlockLandmark;
  std::string unlockText() const;
};
enum class BodyPlan { Biped, Quad, Blob, Crab, Bird, Robot, Serpent };
struct MonsterDef {
  std::string id, name, desc; BodyPlan plan = BodyPlan::Biped;
  std::string variant; Color body, accent, belly; float scale = 1, speed = 1, hp = 1, melee = 1, power = 1; bool starter = false;
  uint32_t genSeed = 0;  // Rex: procedural body seed from the rolled look (0 = classic rig/model)
};
enum class ObjType { Smash, Survive, Reach, Destroy, Landmarks, Boss, Box, Stone };
struct LevelDef {
  std::string id, name, desc; ObjType obj = ObjType::Smash;
  int count = 0; float time = 0; float x = 0, z = 0, r = 0; bool hasZone = false; float zx = 0, zz = 0, zr = 0;
  std::string filter, unit; std::vector<std::string> landmarks;
  float startX = 0, startZ = 0; float bossX = 0, bossZ = 0;
  int cops = 0, cars = 0, soldiers = 0, jeeps = 0, helis = 0, tanks = 0, jets = 0; float interval = 5; int reward = 100;
  int tier = 0; std::string miniBoss; bool chestDrop = false;  // tier 0-2 enemy variants; end-of-level boss (supertank / mech); L5 skill-chest drop
  int riots = 0, armored = 0, flamers = 0;                     // enemy ladder: riot cops, armoured (super-only) tanks, flamethrower troops
  bool noRebuild = false, unlockGauntletSlot = false, pedestals = false; int stone = -1;  // gauntlet arc (L12 slot, L13 box, L14-19 stones, L20 pedestals)
  float ax = 0, az = 0, ar = 0;                                // L13 box arena (district that must be flattened)
};
struct Tables {
  std::vector<MoveDef> moves; std::vector<MonsterDef> monsters; std::vector<LevelDef> levels;
  int moveIndex(const std::string& id) const;
  int monsterIndex(const std::string& id) const;
  bool load(const std::string& movesJson, const std::string& monstersJson, const std::string& levelsJson, std::string* err);
};
extern Tables T;
}  // namespace tz
