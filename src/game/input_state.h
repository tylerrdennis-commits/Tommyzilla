// Device-independent input snapshot built by the platform layer (XInput + raw keyboard/mouse on Windows).
#pragma once
namespace tz {
enum Action {
  A_CLAW, A_TAIL, A_STOMP, A_GRAB, A_SLOT1, A_SLOT2, A_ALT, A_PAY, A_PAUSE, A_LEGEND, A_SPRINT, A_CAMRESET,
  A_UP, A_DOWN, A_LEFT, A_RIGHT, A_CONFIRM, A_CANCEL, A_MUTE, A_ALT1, A_ALT2, A_ALT3, A_EAT, A_CYCLE1, A_CYCLE2, A_SKILLS, A_STORE, A_EGGS, A_COUNT
};
struct InputState {
  float moveX = 0, moveY = 0;      // left stick / WASD; moveY > 0 = forward
  float lookX = 0, lookY = 0;      // right stick (-1..1)
  float mouseDX = 0, mouseDY = 0;  // raw mouse delta in counts this frame (only while captured)
  float wheel = 0;
  bool held[A_COUNT] = {};
  bool pressed[A_COUNT] = {};      // went down since last frame (latched so short taps are never missed)
  bool padConnected = false, usingPad = false, mouseCaptured = false;
  float cursorX = -1, cursorY = -1; bool click = false;  // menu mouse (when not captured)
  // Titan co-op: when split, the controller drives player 2 (keyboard/mouse stay player 1)
  bool p2Active = false; float p2MoveX = 0, p2MoveY = 0, p2LookX = 0, p2LookY = 0;
  bool p2Held[A_COUNT] = {}, p2Pressed[A_COUNT] = {};
};
}  // namespace tz
