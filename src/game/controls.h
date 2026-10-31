// The one controls table: every Action with its XInput chord and keyboard/mouse key, grouped by context. It drives the
// in-game Controls screen (Pause > Controls, Options > Controls, LB+Start / H) and the binding tests. The live bindings are
// in src/input/input_win.cpp; tests/tests.cpp checks that file against this table so the two cannot drift apart.
#pragma once
#include <vector>
#include "input_state.h"
namespace tz {
enum ControlContext { CTX_PLAY = 0, CTX_MENU = 1, CTX_GLOBAL = 2 };  // global = active both in play and in menus
struct ControlBind {
  int action;         // Action, or -1 for the analog rows (move / look)
  const char* name;   // Controls screen label
  const char* pad;    // controller text shown to the player
  const char* key;    // keyboard + mouse text ("-" = none)
  int ctx;            // ControlContext
  const char* chord;  // canonical pad chord for the conflict check (e.g. "LB+Y")
  const char* note;   // description line on the Controls screen
};
inline const std::vector<ControlBind>& controlBinds() {
  static const std::vector<ControlBind> v = {
      {-1, "Move", "L stick", "W A S D", CTX_PLAY, "LS", "Walk around (arrow keys work too). Hold L3 / Shift to run."},
      {-1, "Look", "R stick", "Mouse", CTX_PLAY, "RS", "Turn the camera. Click the game to capture the mouse; Esc releases it."},
      {A_CLAW, "Melee 1 (claw)", "X", "J / L-click", CTX_PLAY, "X", "First melee button (set it in Pause > Melee loadout)."},
      {A_TAIL, "Melee 2 (tail)", "B", "K / R-click", CTX_PLAY, "B", "Second melee button."},
      {A_STOMP, "Melee 3 (stomp)", "A (hold)", "L / Space", CTX_PLAY, "A", "Third melee button. Stomp: hold to show the stun ring, release to stomp."},
      {A_ALT, "Alt melee (hold)", "LB", "use U / I / O", CTX_PLAY, "LB", "Hold LB to switch X / B / A to melee moves 4 / 5 / 6."},
      {A_ALT1, "Melee 4", "LB + X", "U", CTX_PLAY, "LB+X", "Fourth melee move."},
      {A_ALT2, "Melee 5", "LB + B", "I", CTX_PLAY, "LB+B", "Fifth melee move."},
      {A_ALT3, "Melee 6", "LB + A", "O", CTX_PLAY, "LB+A", "Sixth melee move."},
      {A_SLOT1, "Skill slot 1", "RT", "F", CTX_PLAY, "RT", "Close-range skill (breath, melee supers)."},
      {A_SLOT2, "Skill slot 2", "LT", "Q", CTX_PLAY, "LT", "Ranged / super skill (blaster, roar)."},
      {A_CYCLE1, "Cycle slot 1", "D-pad left", "1", CTX_PLAY, "DLEFT", "Swap the skill in slot 1."},
      {A_CYCLE2, "Cycle slot 2", "D-pad right", "2", CTX_PLAY, "DRIGHT", "Swap the skill in slot 2."},
      {A_SKILLS, "Skills list", "D-pad down", "Tab", CTX_PLAY, "DDOWN", "Pick skills for your slots (pauses the game)."},
      {A_STORE, "Store", "D-pad up", "B", CTX_PLAY, "DUP", "Upgrades, chest keys, egg spins."},
      {A_GRAB, "Grab / throw", "Y", "G", CTX_PLAY, "Y", "Pick up a tree or rock, press again to throw it (Y eats instead when someone is close)."},
      {A_EAT, "Eat / open chest", "Y", "E", CTX_PLAY, "Y near food", "Gulp the nearest civilian, or open a chest next to you."},
      {A_SPRINT, "Run", "L3", "Shift", CTX_PLAY, "L3", "Hold to run (click the left stick)."},
      {A_CAMRESET, "Reset camera", "R3", "C / M-click", CTX_PLAY, "R3", "Snap the camera behind your monster (click the right stick)."},
      {A_PAY, "Pay nuke / power", "RB", "R", CTX_PLAY, "RB", "Pay coins to stop a nuke; also the Snap and the Rainbow wave."},
      {A_LEGEND, "Controls", "LB + Start", "H", CTX_PLAY, "LB+START", "Open this Controls screen."},
      {A_EGGS, "Egg spin", "View / Back", "T", CTX_PLAY, "BACK", "Spend an egg spin token (if you have one)."},
      {A_PAUSE, "Pause", "Start", "Esc / P", CTX_GLOBAL, "START", "Pause menu (Start / Esc closes it again)."},
      {A_MUTE, "Mute", "LB + Back", "M", CTX_GLOBAL, "LB+BACK", "Sound on / off at any time."},
      {A_UP, "Menu up", "D-pad/stick", "Up / W", CTX_MENU, "DUP", "Move up in menus."},
      {A_DOWN, "Menu down", "D-pad/stick", "Down / S", CTX_MENU, "DDOWN", "Move down in menus."},
      {A_LEFT, "Menu left", "D-pad/stick", "Left / A", CTX_MENU, "DLEFT", "Move left (store grid, sliders)."},
      {A_RIGHT, "Menu right", "D-pad/stick", "Right / D", CTX_MENU, "DRIGHT", "Move right (store grid, sliders)."},
      {A_CONFIRM, "Select", "A", "Enter / Space", CTX_MENU, "A", "Choose / buy."},
      {A_CANCEL, "Back / close", "B", "Esc/Backspace", CTX_MENU, "B", "Close this menu."},
  };
  return v;
}
}  // namespace tz
