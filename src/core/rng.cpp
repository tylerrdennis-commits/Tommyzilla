#include "rng.h"
namespace tz {
static Rng gCosmetic(0xC0FFEE1234ull);
float frand() { return gCosmetic.next(); }
float frange(float a, float b) { return gCosmetic.range(a, b); }
}  // namespace tz
