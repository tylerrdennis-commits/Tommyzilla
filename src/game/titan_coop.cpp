// Titan: 2-player local co-op. Player 1 = keyboard/mouse (Game::kaiju), player 2 = controller (Game::kaiju2).
// P2 is simulated by swapping it into Game::kaiju for its update/render so every system that reads G->kaiju
// (attacks, eating, models) treats it as "the" monster for that moment. P2 shares P1's camera and never ends
// the run: a knocked-out P2 pops back next to P1 after a short nap.
#include <algorithm>
#include "game.h"
namespace tz {
InputState p2InputFrom(const InputState& in, bool ownCamera) {
  InputState p; p.moveX = in.p2MoveX; p.moveY = in.p2MoveY;
  p.lookX = ownCamera ? in.p2LookX : 0; p.lookY = ownCamera ? in.p2LookY : 0;  // split screen: P2 has its own camera; else the camera belongs to P1
  for (int k = 0; k < A_COUNT; k++) { p.held[k] = in.p2Held[k]; p.pressed[k] = in.p2Pressed[k]; }
  p.held[A_PAUSE] = p.pressed[A_PAUSE] = false; p.held[A_CAMRESET] = p.pressed[A_CAMRESET] = false;
  p.padConnected = p.usingPad = true; return p;
}
bool Game::coopOn() const { return players >= 2 && mode == Mode::Play; }
bool Game::splitScreen() const { return coopOn() && coopJoined; }
void Game::updateCoop(float dt, const InputState& in) {
  if (!coopOn()) { coopJoined = false; return; }
  if (!coopJoined) {
    if (!in.p2Active) return;
    kaiju2 = Kaiju(); kaiju2.player = 1; kaiju2.monster = kaiju.monster; kaiju2.pos = kaiju.pos + Vec3(12, 0, 0); kaiju2.facing = kaiju.facing; kaiju2.hp = kaiju.maxHp();
    kaiju2.cam = kaiju.cam; kaiju2.updateCamera(1, true);  // P2's own camera starts behind P2
    coopJoined = true; coopNapT = 0; toast("Player 2 joined! (controller)");
  }
  if (coopNapT > 0) { coopNapT -= dt; if (coopNapT <= 0) { kaiju2.pos = kaiju.pos + Vec3(10, 0, 0); kaiju2.hp = kaiju.maxHp(); toast("Player 2 is back!"); } return; }
  const bool split = splitScreen();  // split screen: P2 keeps and steers its own camera
  Camera cam = kaiju.cam;
  std::swap(kaiju, kaiju2); coopSwapped = true;
  if (!split) kaiju.cam = cam;
  InputState p2 = p2InputFrom(in, split);
  if (!zoo.active) oneButtonEat(p2);  // P2 (the controller) eats with Y too
  kaiju.update(dt, p2);
  coopSwapped = false; std::swap(kaiju, kaiju2);
  kaiju.cam = cam;
  if (kaiju2.hp <= 0) { coopNapT = 4; toast("Player 2 is taking a nap... back soon!"); }
}
bool Game::coopP2Nearer(const Vec3& from) const {
  if (!coopOn() || !coopJoined || coopNapT > 0 || coopSwapped || kaiju2.hp <= 0) return false;
  return dist2D(kaiju2.pos.x, kaiju2.pos.z, from.x, from.z) < dist2D(kaiju.pos.x, kaiju.pos.z, from.x, from.z);
}
bool Game::coopFocusBegin(const Vec3& from) {  // Wren: enemies + bosses chase / shoot whichever player is nearest
  if (!coopP2Nearer(from)) return false;
  Camera cam = kaiju.cam; coopHeldCam = kaiju2.cam; std::swap(kaiju, kaiju2); kaiju.cam = cam; coopSwapped = true; return true;  // keep P1's camera (shake etc.); P2's own camera is held aside
}
void Game::coopFocusEnd(bool swapped) {
  if (!swapped) return;
  Camera cam = kaiju.cam; coopSwapped = false; std::swap(kaiju, kaiju2); kaiju.cam = cam; kaiju2.cam = coopHeldCam;  // split screen: P2's camera survives
}
void Game::renderCoop(RenderList& rl) {
  if (!coopOn() || !coopJoined || coopNapT > 0) return;
  std::swap(kaiju, kaiju2); coopSwapped = true;
  kaiju.render(rl, time);
  coopSwapped = false; std::swap(kaiju, kaiju2);
}
}  // namespace tz
