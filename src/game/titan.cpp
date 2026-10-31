#include "titan.h"
#include <algorithm>
#include <cmath>
namespace tz {
const char* difficultyName(int d) { return d <= DIFF_EASY ? "Easy" : d == DIFF_HARD ? "Hard" : "You're Insane"; }
DiffMods difficultyMods(int d, int players) {
  DiffMods m; d = std::max(0, std::min(2, d)); players = std::max(1, std::min(2, players));
  static const float hp[3] = {1.0f, 1.4f, 2.0f}, dmg[3] = {1.0f, 1.4f, 2.0f}, coin[3] = {1.0f, 1.25f, 1.6f}, sp[3] = {1.0f, 1.2f, 1.5f};
  float pk = players == 2 ? 1.6f : 1.f;
  m.enemyHp = hp[d] * pk; m.bossHp = hp[d] * (players == 2 ? 1.8f : 1.f); m.enemyDamage = dmg[d];
  m.coins = coin[d] * (players == 2 ? 1.5f : 1.f); m.spawnRate = sp[d] * (players == 2 ? 1.3f : 1.f);
  return m;
}
bool PoliceChase::update(float dt, int n, float nearest, float speed) {
  if (n == 0) { arrived = false; escapeT = 0; return false; }
  if (nearest < kArriveDist) arrived = true;
  if (n <= kSmallGroup && nearest > kEscapeDist && speed >= kEscapeSpeed) escapeT += dt; else escapeT = std::max(0.f, escapeT - dt);
  if (escapeT >= kEscapeTime) { searching = true; arrived = false; escapeT = 0; return true; }
  return false;
}
float PoliceChase::sirenVolume(float base, int n) const { return (n == 0 || arrived || searching) ? 0.f : base * kQuiet; }
HeliSwoop heliSwoop(float t, float h) {
  float ph = std::fmod(std::max(0.f, t), 7.f);  // 7 s cycle: 4 s orbit, 3 s swoop
  float k = ph < 4 ? 0.f : std::sin((ph - 4) / 3.f * 3.14159265f);
  return {32 - 22 * k, h - (h - 9) * k, k > 0.5f};
}
void CheeseMeter::update(float dt, float d, float r) {
  if (d < r + 6) closeT += dt; else closeT = std::max(0.f, closeT - dt * 2);
  stompHeat = std::max(0.f, stompHeat - dt * 0.5f);
}
}  // namespace tz
