// Atlas: gauntlet stones in the procedural city hide inside one purple building each (World::stoneBuildings);
// 20 more purple buildings are decoys. A stone's Zoo Gate appears once its building is mostly smashed.
#include "game.h"
namespace tz {
bool Game::stoneRevealed(int stone) {
  if (!world.procedural || stone < 0 || stone >= (int)world.stoneBuildings.size()) return true;  // Victoria map: fixed gates, always visible
  const Building& b = world.buildings[(size_t)world.stoneBuildings[(size_t)stone]];
  if (world.stonesFound & (1u << stone)) return true;  // once found it stays found (even after the building rebuilds)
  bool found = b.smashed || b.collapsed || (b.generated && b.health() < 0.6f);
  if (found) world.stonesFound |= 1u << stone;
  return found;
}
}  // namespace tz
