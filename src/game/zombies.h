// Rex: Crazy-mode zombie civilians. They shamble toward the kaiju and give small nibbles; eating one counts exactly like
// eating a normal person (same Kaiju::eat path). Tuning constants in zombies.cpp.
#pragma once
#include "../core/math.h"
namespace tz {
struct Person;
namespace zombie {
extern const float SPEED, BITE_DAMAGE, BITE_COOLDOWN, BITE_REACH, SENSE_RADIUS;
bool on();                                                          // Play mode + Crazy run
float step(Person& p, float dt, float kx, float kz, float dist, float kr);  // steer + bite; returns move speed
Color skin(int look);
}  // namespace zombie
}  // namespace tz
