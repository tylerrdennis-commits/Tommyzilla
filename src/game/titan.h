// Titan rebuild: difficulty modes, player-count scaling, police escape/search, heli swoop, boss anti-cheese.
// Pure helpers (no Game dependency) so tests can cover them directly.
#pragma once
namespace tz {
enum Difficulty { DIFF_EASY = 0, DIFF_HARD = 1, DIFF_INSANE = 2, DIFF_COUNT };
const char* difficultyName(int d);  // "Easy", "Hard", "You're Insane"
struct DiffMods { float enemyHp = 1, enemyDamage = 1, bossHp = 1, coins = 1, spawnRate = 1; };
// players 1..2: more players -> tougher enemies AND a bit more coins (shared reward, no grind pressure)
DiffMods difficultyMods(int difficulty, int players);

// Police chase state (sandbox). Arrived: siren stops so the soundtrack plays. Searching: kaiju escaped.
struct PoliceChase {
  bool arrived = false, searching = false; float escapeT = 0;
  // call each frame; returns true the frame the kaiju escapes
  bool update(float dt, int policeUnits, float nearestDist, float kaijuSpeed);
  void onBuildingSmashed() { searching = false; escapeT = 0; }
  float sirenVolume(float base, int policeUnits) const;  // quieter overall; silent once arrived or searching
  static constexpr int kSmallGroup = 3; static constexpr float kEscapeDist = 70, kEscapeSpeed = 14, kEscapeTime = 2.5f, kArriveDist = 45, kQuiet = 0.45f;
};

// helicopter swoop: orbit radius/height dip low and close every few seconds so a kid can smash it
struct HeliSwoop { float radius, height; bool low; };
HeliSwoop heliSwoop(float t, float baseHeight);

// boss anti-cheese: tracks point-blank camping and stomp spam
struct CheeseMeter {
  float closeT = 0, stompHeat = 0;
  void update(float dt, float distToBoss, float bossRadius);
  void onStomp() { stompHeat += 1; }
  bool punishClose() const { return closeT > 2.5f; }
  bool punishStomp() const { return stompHeat >= 3.5f; }
  void consume() { closeT = 0; stompHeat = 0; }
};
}  // namespace tz
