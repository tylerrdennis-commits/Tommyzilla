// Rex: procedural kaiju generator. Every species except Tommyzilla gets a seeded body (shape, colours, parts) instead of
// a per-species model; Tommyzilla keeps its model and can roll one of 4 legendary elements. Pure logic, tested on Linux.
#pragma once
#include <set>
#include <string>
#include <vector>
#include "eggs.h"
namespace tz {
enum Element : int { ELEM_NONE = -1, ELEM_EARTH = 0, ELEM_WIND = 1, ELEM_FIRE = 2, ELEM_LIGHTNING = 3 };
enum class GenBody : int { Biped, Quadruped, Serpent, Blob, Bird, Count };
enum GenPart : int { PART_HORNS, PART_SPIKES, PART_WINGS, PART_TAIL_CLUB, PART_FINS, PART_CRest, PART_CLAWS, PART_TUSKS, PART_EXTRA_EYES, PART_COUNT };
struct KaijuGenome {
  uint32_t seed = 0; GenBody body = GenBody::Biped;
  float height = 1, girth = 1, headSize = 1, legLength = 1, armLength = 1, tailLength = 1, neckLength = 1;  // 0.6..1.5
  int eyeCount = 2; std::vector<int> parts;  // GenPart ids, no repeats
  Color primary, secondary, accent, eyes; int pattern = 0;  // 0 plain, 1 stripes, 2 spots, 3 belly
  bool has(int p) const { for (int q : parts) if (q == p) return true; return false; }
};
namespace kgen {
BodyPlan planOf(GenBody b);
bool usesProcedural(const std::string& monster);           // false only for "tommyzilla"
KaijuGenome generate(uint32_t seed, const KaijuLook& look); // deterministic; colours from the look
const char* elementName(int e);                            // "Earth" ... / ""
Color elementColor(int e);
void applyElement(KaijuLook& l, int e);                    // recolour Tommyzilla + set element
// Egg roll with fewer duplicates: species the player already owns are re-rolled (up to dupRerolls tries) within the
// same rarity first, then by rerolling rarity. Tommyzilla rolls get a legendary element with legendaryChance.
std::string rollSpeciesFresh(Rng& r, const Tables& t, const std::set<std::string>& owned, int* rarityOut = nullptr, int dupRerolls = 3);
KaijuLook rollEgg(Rng& r, const Tables& t, const std::set<std::string>& owned, float legendaryChance = 0.25f);
}  // namespace kgen
}  // namespace tz
