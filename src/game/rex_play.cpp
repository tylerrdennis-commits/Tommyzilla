// Rex: one-button eat target check, easy zookeeper gulp, crazy-mode zombies.
#include "game.h"
#include "zombies.h"
namespace tz {
bool Game::eatTargetNear() {
  if (zoo.active) return zoo.inRange;
  if (adventure && level.boxKeeper && dist2D(kaiju.pos.x, kaiju.pos.z, level.boxKeeperPos.x, level.boxKeeperPos.z) <= kaiju.eatRange()) return true;
  if (people.nearestInRange(kaiju.pos.x, kaiju.pos.z, kaiju.eatRange()) >= 0) return true;
  return chestNear(kaiju.pos.x, kaiju.pos.z, C.chestRange * std::max(1.f, kaiju.scale()) + kaiju.radius()) != nullptr;
}
bool zooGulpPressed(const InputState& in) { return in.pressed[A_EAT] || in.pressed[A_CLAW] || in.pressed[A_GRAB] || in.pressed[A_CONFIRM] || in.click; }
namespace zombie {
const float SPEED = 2.6f, BITE_DAMAGE = 1.5f, BITE_COOLDOWN = 1.5f, BITE_REACH = 3.0f, SENSE_RADIUS = 70.f;
bool on() { return G && G->mode == Mode::Play && G->adventure && G->crazy; }
float step(Person& p, float dt, float kx, float kz, float dist, float kr) {
  p.biteT = std::max(0.f, p.biteT - dt);
  if (dist > SENSE_RADIUS) { if (p.t <= 0) { p.t = 2 + frand() * 3; p.dir += (frand() - 0.5f) * 2.f; } return SPEED * 0.4f; }
  float to = std::atan2(kx - p.x, kz - p.z);
  p.dir += wrapAngle(to - p.dir) * std::min(1.f, dt * 3);
  if (dist < kr + BITE_REACH) {
    if (p.biteT <= 0) { p.biteT = BITE_COOLDOWN; G->kaiju.damage(BITE_DAMAGE); }
    return 0;
  }
  return SPEED;
}
Color skin(int look) { static const uint32_t z[] = {0x8fcf6a, 0x7ab85a, 0xa3d97c, 0x6fa85a}; return hexColor(z[look & 3]); }
}  // namespace zombie
}  // namespace tz
