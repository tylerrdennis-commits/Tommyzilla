// Windows input: raw keyboard + raw mouse (with cursor capture for mouse look) and XInput controllers.
// Produces the device-independent tz::InputState consumed by the game.
#pragma once
#include "../game/input_state.h"
struct HWND__;
namespace tz {
class WinInput {
 public:
  bool init(HWND__* hwnd);                       // registers raw input devices
  bool handleMessage(unsigned msg, unsigned long long wparam, long long lparam);  // returns true if consumed
  // Builds this frame's state. wantCapture = the game is in free play (no menu); captured mouse is released otherwise.
  InputState poll(float dt, bool wantCapture);
  bool split = false;  // Titan: 2-player co-op (keyboard/mouse = P1, controller = P2)
  void setRumble(float strength, float seconds);
  void releaseCapture();
  void shutdown();
 private:
  void applyCapture(bool on);
  HWND__* hwnd_ = nullptr;
  bool keyDown_[256] = {}, keyLatch_[256] = {};
  bool mouseBtn_[3] = {}, mouseLatch_[3] = {};
  float mdx_ = 0, mdy_ = 0, wheel_ = 0; int curX_ = -1, curY_ = -1; bool clickLatch_ = false;
  bool userCapture_ = false, captured_ = false, focused_ = true;
  int pad_ = -1; float padScan_ = 0; unsigned short prevPadButtons_ = 0; bool prevLT_ = false, prevRT_ = false;
  float repeatT_[4] = {}; bool prevDir_[4] = {};
  bool usingPad_ = false; bool kbmPlayer_ = false; /* a key / mouse button was pressed this session: someone plays P1 on keyboard */ float rumble_ = 0, rumbleT_ = 0; bool rumbling_ = false;
};
}  // namespace tz
