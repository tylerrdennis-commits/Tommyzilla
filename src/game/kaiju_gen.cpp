// Rex: see kaiju_gen.h
#include "kaiju_gen.h"
#include <algorithm>
namespace tz {
namespace kgen {
BodyPlan planOf(GenBody b) { switch (b) { case GenBody::Quadruped: return BodyPlan::Quad; case GenBody::Serpent: return BodyPlan::Serpent; case GenBody::Blob: return BodyPlan::Blob; case GenBody::Bird: return BodyPlan::Bird; default: return BodyPlan::Biped; } }
bool usesProcedural(const std::string& m) { return m != "tommyzilla"; }
static float rf(Rng& r, float a, float b) { return r.range(a, b); }
KaijuGenome generate(uint32_t seed, const KaijuLook& look) {
  Rng r((uint64_t)seed * 2654435761ull + 17);
  KaijuGenome g; g.seed = seed;
  g.body = (GenBody)r.irange(0, (int)GenBody::Count - 1);
  g.height = rf(r, .75f, 1.35f); g.girth = rf(r, .7f, 1.4f); g.headSize = rf(r, .8f, 1.4f);
  g.legLength = rf(r, .6f, 1.3f); g.armLength = rf(r, .6f, 1.4f); g.tailLength = rf(r, .5f, 1.5f); g.neckLength = rf(r, .6f, 1.5f);
  if (g.body == GenBody::Serpent) g.legLength = 0;
  if (g.body == GenBody::Blob) { g.legLength = rf(r, .3f, .6f); g.tailLength = 0; }
  int n = r.irange(1, 2 + std::min(2, look.rarity));  // rarer eggs get more parts
  std::vector<int> pool; for (int i = 0; i < PART_COUNT; i++) pool.push_back(i);
  if (g.body == GenBody::Bird) { pool.erase(std::remove(pool.begin(), pool.end(), (int)PART_WINGS), pool.end()); g.parts.push_back(PART_WINGS); }
  r.shuffle(pool);
  for (int i = 0; i < n && i < (int)pool.size(); i++) g.parts.push_back(pool[(size_t)i]);
  g.eyeCount = g.has(PART_EXTRA_EYES) ? r.irange(3, 4) : 2;
  g.primary = look.primary; g.secondary = look.secondary; g.accent = look.accent; g.eyes = look.eyes; g.pattern = r.irange(0, 3);
  return g;
}
const char* elementName(int e) { static const char* n[] = {"Earth", "Wind", "Fire", "Lightning"}; return e >= 0 && e < 4 ? n[e] : ""; }
Color elementColor(int e) { static const uint32_t c[] = {0x7a5a2e, 0x9fe8d8, 0xff5a1f, 0xffe23a}; return e >= 0 && e < 4 ? hexColor(c[e]) : hexColor(0xffffff); }
void applyElement(KaijuLook& l, int e) {
  if (e < 0 || e > 3) return;
  static const uint32_t prim[] = {0x6b4f2a, 0x5fc7c0, 0xc8321a, 0x3a4ad8}, sec[] = {0x3f7a3a, 0xe6fbff, 0xffb23a, 0xbfe6ff}, acc[] = {0xa8e05a, 0xffffff, 0xffe066, 0xfff23a};
  l.element = e; l.primary = hexColor(prim[e]); l.secondary = hexColor(sec[e]); l.accent = hexColor(acc[e]); l.eyes = elementColor(e);
}
std::string rollSpeciesFresh(Rng& r, const Tables& t, const std::set<std::string>& owned, int* rarityOut, int dupRerolls) {
  int ra = 0; std::string id = eggs::rollSpecies(r, t, &ra);
  for (int k = 0; k < dupRerolls && owned.count(id); k++) {
    std::vector<std::string> fresh; for (auto& s : eggs::speciesOf(ra, t)) if (!owned.count(s)) fresh.push_back(s);
    if (!fresh.empty()) { id = r.pick(fresh); break; }
    id = eggs::rollSpecies(r, t, &ra);  // whole tier owned: try another rarity
  }
  if (rarityOut) *rarityOut = ra;
  return id;
}
KaijuLook rollEgg(Rng& r, const Tables& t, const std::set<std::string>& owned, float legendaryChance) {
  std::string id = rollSpeciesFresh(r, t, owned);
  KaijuLook l = eggs::rollLook(r, id, t);
  if (usesProcedural(id)) l.genSeed = (uint32_t)(r.nextU64() | 1);
  else if (r.next() < legendaryChance) applyElement(l, r.irange(0, 3));
  return l;
}
}  // namespace kgen
}  // namespace tz
