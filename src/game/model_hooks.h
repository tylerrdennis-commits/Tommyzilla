// Loaded 3D models as the game sees them. tz_core does not depend on the model module (src/model): the game only
// asks a ModelCatalog (implemented by src/model/game_models.cpp, set by the platform layer) which entity ids have a
// model, and emits ModelReq entries into the RenderList instead of its primitive rig. Animation state (clip
// crossfades, crowd sharing, LOD) lives on the model side, keyed by ModelReq::key.
#pragma once
#include <cstdint>
#include <string>
#include "../core/math.h"
namespace tz {
// logical clips; data/models.json maps each to a clip name in the file (unmapped ones fall back, e.g. eat -> attack)
enum ModelClip : uint8_t { MC_IDLE, MC_WALK, MC_RUN, MC_ATTACK, MC_DEATH, MC_HIT, MC_EAT, MC_STUN, MC_JUMP, MC_CHEER, MC_COUNT };
extern const char* const kModelClipNames[MC_COUNT];  // "idle", "walk", "run", "attack", "death", "hit", "eat", "stun", "jump", "cheer"
// stable animation keys: category in the top byte, entity index / uid below
enum ModelKeyKind : uint32_t { MK_KAIJU = 1, MK_PREVIEW, MK_BOSS, MK_UNIT, MK_PERSON, MK_STATIC };
inline uint32_t modelKey(ModelKeyKind k, uint32_t id) { return ((uint32_t)k << 24) | (id & 0xffffffu); }

struct ModelReq {
  int asset = -1;              // ModelCatalog::find index
  int variant = 0;             // ModelCatalog::variant index (0 = base look)
  uint32_t key = 0;            // entity key (animation state); 0 = static (rest pose, no animator)
  Mat4 world = Mat4::identity();  // entity transform (scale * rotation * translation); the manifest offset is applied on top
  uint8_t clip = MC_IDLE;
  bool loop = true;            // false: play once and hold the last frame (death, stun)
  bool shared = false;         // crowd member: shares one animation per (asset, clip, phase bucket) with others
  uint16_t trigger = 0;        // change it to restart the clip (a new attack / hit)
  float speed = 1;             // playback rate multiplier
  float fit = 0;               // > 0: play the clip in this many seconds (attack length = move duration)
  float phase = 0;             // 0..1 crowd phase (picks the shared bucket)
  Color tint{1, 1, 1, 1};      // multiply (fade, wreck darkening, locked-monster silhouette)
  Color mix{1, 1, 1, 1}; float mixK = 0;  // blend material colours toward `mix` (enemy tier paint, hit flash)
  float glow = 0;              // adds base colour * glow
  bool palette = false;        // rolled kaiju look: *_Main materials -> primary, *_Secondary -> accent, *Eye* -> eyes
  Color primary{1, 1, 1, 1}, secondary{1, 1, 1, 1}, accent{1, 1, 1, 1}, eyes{1, 1, 1, 1};
  uint8_t finish = 0;          // 1 gold, 2 chrome, 3 holographic (model shader)
  float finishT = 0;           // time for the animated finishes
};
// credits screen (Options > Credits) and the victory screen
inline constexpr const char* kGameCredits = "TOMMYZILLA CITY SMASH\nStarring Tommy and Tommyzilla\nVillain: THE BULK\nMade with love for Tommy - thanks for playing!";
inline constexpr const char* kModelCredits = "3D models (CC0 / public domain):\n  Quaternius - Ultimate Monsters, Mech Bee / Mech Fox, Blue Demon, SWAT + Soldier, tank, props (quaternius.com, via poly.pizza)\n  Kenney - Car Kit, City Kits (commercial, suburban, industrial, roads), Mini Characters (kenney.nl)\n  kazuma - Helicopter (via poly.pizza)";
struct ModelCatalog {
  virtual ~ModelCatalog() {}
  virtual int find(const std::string& id) = 0;                   // -1: no model -> draw the primitive rig
  virtual int variant(int asset, const std::string& name) = 0;   // 0 = base look (also for unknown names)
  virtual bool bounds(int asset, Vec3& lo, Vec3& hi) = 0;        // entity-space rest bounds (after the manifest offset)
};
}  // namespace tz
