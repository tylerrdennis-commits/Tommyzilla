#include "input_win.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xinput.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace tz {
namespace {
float axis(SHORT v, float dead) {
  float f = v / 32767.f; f = std::max(-1.f, std::min(1.f, f));
  return f;
}
void radialDeadzone(float& x, float& y, float dead) {
  float m = std::sqrt(x * x + y * y);
  if (m < dead) { x = y = 0; return; }
  float k = std::min(1.f, (m - dead) / (1 - dead)) / m; x *= k; y *= k;
}
}  // namespace

bool WinInput::init(HWND__* hwnd) {
  hwnd_ = hwnd;
  RAWINPUTDEVICE rid[2] = {};
  rid[0].usUsagePage = 0x01; rid[0].usUsage = 0x02; rid[0].hwndTarget = (HWND)hwnd;  // mouse
  rid[1].usUsagePage = 0x01; rid[1].usUsage = 0x06; rid[1].hwndTarget = (HWND)hwnd;  // keyboard
  return RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE)) == TRUE;
}

void WinInput::applyCapture(bool on) {
  if (on == captured_) return;
  captured_ = on;
  HWND h = (HWND)hwnd_;
  if (on) {
    RECT rc; GetClientRect(h, &rc);
    POINT tl = {rc.left, rc.top}, br = {rc.right, rc.bottom}; ClientToScreen(h, &tl); ClientToScreen(h, &br);
    RECT clip = {tl.x, tl.y, br.x, br.y}; ClipCursor(&clip);
    while (ShowCursor(FALSE) >= 0) {}
    SetCapture(h);
  } else {
    ClipCursor(nullptr);
    while (ShowCursor(TRUE) < 0) {}
    ReleaseCapture();
  }
}
void WinInput::releaseCapture() { userCapture_ = false; applyCapture(false); }

bool WinInput::handleMessage(unsigned msg, unsigned long long wparam, long long lparam) {
  switch (msg) {
    case WM_INPUT: {
      UINT size = 0;
      GetRawInputData((HRAWINPUT)lparam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
      if (size == 0 || size > 1024) return false;
      alignas(8) unsigned char buf[1024];
      if (GetRawInputData((HRAWINPUT)lparam, RID_INPUT, buf, &size, sizeof(RAWINPUTHEADER)) != size) return false;
      RAWINPUT* ri = (RAWINPUT*)buf;
      if (ri->header.dwType == RIM_TYPEMOUSE) {
        const RAWMOUSE& m = ri->data.mouse;
        if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE) && captured_) { mdx_ += (float)m.lLastX; mdy_ += (float)m.lLastY; }
        const USHORT f = m.usButtonFlags;
        auto btn = [&](int i, USHORT down, USHORT up) {
          if (f & down) { if (!mouseBtn_[i]) mouseLatch_[i] = true; mouseBtn_[i] = true; }
          if (f & up) mouseBtn_[i] = false;
        };
        if (focused_) {
          btn(0, RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_LEFT_BUTTON_UP);
          btn(1, RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_UP);
          btn(2, RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP);
          if (f & RI_MOUSE_WHEEL) wheel_ += (float)(SHORT)m.usButtonData / WHEEL_DELTA;
          if (f & (RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_DOWN)) kbmPlayer_ = true;
        }
        usingPad_ = false;
      } else if (ri->header.dwType == RIM_TYPEKEYBOARD) {
        const RAWKEYBOARD& k = ri->data.keyboard;
        UINT vk = k.VKey; if (vk == 0 || vk >= 255) return false;
        if (vk == VK_SHIFT) vk = VK_LSHIFT;
        bool down = !(k.Flags & RI_KEY_BREAK);
        if (!focused_) down = false;
        if (down && !keyDown_[vk]) { keyLatch_[vk] = true; kbmPlayer_ = true; }
        keyDown_[vk] = down;
        usingPad_ = false;
      }
      return false;  // DefWindowProc must still see WM_INPUT
    }
    case WM_MOUSEMOVE: curX_ = (short)LOWORD(lparam); curY_ = (short)HIWORD(lparam); return false;
    case WM_LBUTTONDOWN: curX_ = (short)LOWORD(lparam); curY_ = (short)HIWORD(lparam); if (!captured_) clickLatch_ = true; return false;
    case WM_ACTIVATEAPP:
    case WM_KILLFOCUS:
      if (msg == WM_KILLFOCUS || !wparam) {
        focused_ = false; applyCapture(false);
        for (auto& k : keyDown_) k = false;
        for (auto& b : mouseBtn_) b = false;
      } else focused_ = true;
      return false;
    case WM_SETFOCUS: focused_ = true; return false;
    case WM_SIZE: case WM_MOVE:
      if (captured_) { applyCapture(false); applyCapture(true); }  // re-clip to the new client rect
      return false;
  }
  (void)wparam;
  return false;
}

InputState WinInput::poll(float dt, bool wantCapture) {
  InputState s;
  auto key = [&](int vk) { return keyDown_[vk]; };
  auto hit = [&](int vk) { return keyLatch_[vk]; };
  auto set = [&](Action a, bool held, bool pressed) { s.held[a] = s.held[a] || held; s.pressed[a] = s.pressed[a] || pressed; };
  // Esc: release the captured mouse first; when not captured it pauses / backs out
  bool esc = hit(VK_ESCAPE);
  if (esc && captured_) { userCapture_ = false; esc = false; }
  // mouse capture: click inside the game during free play, released while menus are open
  if (wantCapture && focused_ && !captured_ && clickLatch_) { userCapture_ = true; clickLatch_ = false; mouseLatch_[0] = false; }  // the capturing click doesn't attack
  applyCapture(wantCapture && userCapture_ && focused_);
  // ---- keyboard + mouse
  float mx = (key('D') ? 1.f : 0.f) - (key('A') ? 1.f : 0.f), my = (key('W') ? 1.f : 0.f) - (key('S') ? 1.f : 0.f);
  if (wantCapture) { mx += (key(VK_RIGHT) ? 1.f : 0.f) - (key(VK_LEFT) ? 1.f : 0.f); my += (key(VK_UP) ? 1.f : 0.f) - (key(VK_DOWN) ? 1.f : 0.f); }
  float ml = std::sqrt(mx * mx + my * my); if (ml > 1) { mx /= ml; my /= ml; }
  s.moveX = mx; s.moveY = my;
  bool lmb = captured_ && mouseBtn_[0], rmb = captured_ && mouseBtn_[1];
  set(A_CLAW, key('J') || lmb, hit('J') || (captured_ && mouseLatch_[0]));
  set(A_TAIL, key('K') || rmb, hit('K') || (captured_ && mouseLatch_[1]));
  set(A_STOMP, key('L') || key(VK_SPACE), hit('L') || (wantCapture && hit(VK_SPACE)));
  set(A_ALT1, key('U'), hit('U')); set(A_ALT2, key('I'), hit('I')); set(A_ALT3, key('O'), hit('O'));
  set(A_SLOT1, key('F'), hit('F')); set(A_SLOT2, key('Q'), hit('Q'));
  set(A_GRAB, key('G'), hit('G')); set(A_EAT, key('E'), hit('E'));  // E = eat / open chest (tree grab moved from E to G)
  set(A_CYCLE1, key('1'), hit('1')); set(A_CYCLE2, key('2'), hit('2')); set(A_SKILLS, key(VK_TAB), hit(VK_TAB)); set(A_STORE, key('B'), hit('B')); set(A_SPRINT, key(VK_LSHIFT) || key(VK_RSHIFT), false);
  set(A_CAMRESET, key('C') || (captured_ && mouseBtn_[2]), hit('C') || (captured_ && mouseLatch_[2]));
  set(A_PAY, key('R'), hit('R')); set(A_EGGS, key('T'), hit('T'));  /* Vault/Wren: T = spend an egg spin */ set(A_LEGEND, key('H'), hit('H')); set(A_MUTE, key('M'), hit('M'));
  set(A_PAUSE, key('P'), hit('P') || esc);  // Tab now opens the skills overlay
  set(A_CONFIRM, key(VK_RETURN) || key(VK_SPACE), hit(VK_RETURN) || hit(VK_SPACE));
  set(A_CANCEL, key(VK_BACK), hit(VK_BACK) || esc);
  set(A_UP, key(VK_UP) || (!wantCapture && key('W')), hit(VK_UP) || (!wantCapture && hit('W')));
  set(A_DOWN, key(VK_DOWN) || (!wantCapture && key('S')), hit(VK_DOWN) || (!wantCapture && hit('S')));
  set(A_LEFT, key(VK_LEFT) || (!wantCapture && key('A')), hit(VK_LEFT) || (!wantCapture && hit('A')));
  set(A_RIGHT, key(VK_RIGHT) || (!wantCapture && key('D')), hit(VK_RIGHT) || (!wantCapture && hit('D')));
  if (captured_) { s.mouseDX = mdx_; s.mouseDY = mdy_; }
  s.wheel = wheel_;
  s.cursorX = (float)curX_; s.cursorY = (float)curY_; s.click = clickLatch_ && !captured_;
  // ---- XInput (first connected pad; disconnected slots are probed every 2 s because the call is slow)
  padScan_ -= dt;
  XINPUT_STATE xs = {};
  bool have = false;
  if (pad_ >= 0) { if (XInputGetState((DWORD)pad_, &xs) == ERROR_SUCCESS) have = true; else pad_ = -1; }
  if (!have && padScan_ <= 0) {
    padScan_ = 2;
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) if (XInputGetState(i, &xs) == ERROR_SUCCESS) { pad_ = (int)i; have = true; break; }
  }
  s.padConnected = have;
  InputState ps; bool splitNow = split && wantCapture && kbmPlayer_;  // a lone controller (nobody on the keyboard) stays player 1  // Titan: co-op splits the pad off to player 2 during play
  InputState& t = splitNow ? ps : s;
  auto pset = [&](Action a, bool held, bool pressed) { t.held[a] = t.held[a] || held; t.pressed[a] = t.pressed[a] || pressed; };
  if (have) {
    const XINPUT_GAMEPAD& g = xs.Gamepad;
    float lx = axis(g.sThumbLX, 0), ly = axis(g.sThumbLY, 0), rx = axis(g.sThumbRX, 0), ry = axis(g.sThumbRY, 0);
    radialDeadzone(lx, ly, 0.24f); radialDeadzone(rx, ry, 0.2f);
    bool lt = g.bLeftTrigger > 40, rt = g.bRightTrigger > 40;
    WORD b = g.wButtons, pb = prevPadButtons_;
    auto bt = [&](WORD m) { return (b & m) != 0; };
    auto bp = [&](WORD m) { return (b & m) && !(pb & m); };
    bool any = std::fabs(lx) + std::fabs(ly) + std::fabs(rx) + std::fabs(ry) > 0 || b != 0 || lt || rt;
    if (any) usingPad_ = true;
    if (std::fabs(lx) + std::fabs(ly) > std::fabs(t.moveX) + std::fabs(t.moveY)) { t.moveX = lx; t.moveY = ly; }
    t.lookX = rx; t.lookY = -ry;  // positive lookY = stick pulled down
    pset(A_CLAW, bt(XINPUT_GAMEPAD_X), bp(XINPUT_GAMEPAD_X));
    pset(A_TAIL, bt(XINPUT_GAMEPAD_B), bp(XINPUT_GAMEPAD_B));
    pset(A_STOMP, bt(XINPUT_GAMEPAD_A), bp(XINPUT_GAMEPAD_A));
    bool lbHeld = bt(XINPUT_GAMEPAD_LEFT_SHOULDER);  // LB = alt melee / Start / Back modifier (Y needs no chord)
    pset(A_GRAB, !lbHeld && bt(XINPUT_GAMEPAD_Y), !lbHeld && bp(XINPUT_GAMEPAD_Y));
    pset(A_EAT, bt(XINPUT_GAMEPAD_Y), bp(XINPUT_GAMEPAD_Y));  // Rex: one button - Y eats when someone is in reach (else Y grabs)
    // LB + X / B / A = melee 4 / 5 / 6 (the keyboard's U / I / O); X / B / A still report too, so A_ALT + A_CLAW keeps working
    pset(A_ALT1, lbHeld && bt(XINPUT_GAMEPAD_X), lbHeld && bp(XINPUT_GAMEPAD_X));
    pset(A_ALT2, lbHeld && bt(XINPUT_GAMEPAD_B), lbHeld && bp(XINPUT_GAMEPAD_B));
    pset(A_ALT3, lbHeld && bt(XINPUT_GAMEPAD_A), lbHeld && bp(XINPUT_GAMEPAD_A));
    pset(A_CYCLE1, bt(XINPUT_GAMEPAD_DPAD_LEFT), bp(XINPUT_GAMEPAD_DPAD_LEFT));    // quick-cycle slot 1
    pset(A_CYCLE2, bt(XINPUT_GAMEPAD_DPAD_RIGHT), bp(XINPUT_GAMEPAD_DPAD_RIGHT));  // quick-cycle slot 2
    pset(A_SKILLS, bt(XINPUT_GAMEPAD_DPAD_DOWN), bp(XINPUT_GAMEPAD_DPAD_DOWN));    // skills overlay
    pset(A_STORE, bt(XINPUT_GAMEPAD_DPAD_UP), bp(XINPUT_GAMEPAD_DPAD_UP));        // store (keys + upgrades)
    pset(A_ALT, bt(XINPUT_GAMEPAD_LEFT_SHOULDER), bp(XINPUT_GAMEPAD_LEFT_SHOULDER));
    pset(A_PAY, bt(XINPUT_GAMEPAD_RIGHT_SHOULDER), bp(XINPUT_GAMEPAD_RIGHT_SHOULDER));
    pset(A_SLOT1, rt, rt && !prevRT_); pset(A_SLOT2, lt, lt && !prevLT_);
    pset(A_PAUSE, !lbHeld && bt(XINPUT_GAMEPAD_START), !lbHeld && bp(XINPUT_GAMEPAD_START));
    pset(A_LEGEND, lbHeld && bt(XINPUT_GAMEPAD_START), lbHeld && bp(XINPUT_GAMEPAD_START));  // LB + Start = Controls screen (Back is EGGS now)
    pset(A_EGGS, !lbHeld && bt(XINPUT_GAMEPAD_BACK), !lbHeld && bp(XINPUT_GAMEPAD_BACK));    // Vault/Wren: View/Back = egg spin
    pset(A_MUTE, lbHeld && bt(XINPUT_GAMEPAD_BACK), lbHeld && bp(XINPUT_GAMEPAD_BACK));       // LB + Back = mute (keyboard M)
    pset(A_SPRINT, bt(XINPUT_GAMEPAD_LEFT_THUMB), false);
    pset(A_CAMRESET, bt(XINPUT_GAMEPAD_RIGHT_THUMB), bp(XINPUT_GAMEPAD_RIGHT_THUMB));
    pset(A_CONFIRM, bt(XINPUT_GAMEPAD_A), bp(XINPUT_GAMEPAD_A));
    pset(A_CANCEL, bt(XINPUT_GAMEPAD_B), bp(XINPUT_GAMEPAD_B));
    // menu directions: d-pad, or the left stick with auto-repeat (only while a menu is open)
    bool dir[4] = {bt(XINPUT_GAMEPAD_DPAD_UP) || (!wantCapture && ly > 0.55f), bt(XINPUT_GAMEPAD_DPAD_DOWN) || (!wantCapture && ly < -0.55f),
                   bt(XINPUT_GAMEPAD_DPAD_LEFT) || (!wantCapture && lx < -0.55f), bt(XINPUT_GAMEPAD_DPAD_RIGHT) || (!wantCapture && lx > 0.55f)};
    const Action da[4] = {A_UP, A_DOWN, A_LEFT, A_RIGHT};
    for (int i = 0; i < 4; i++) {
      bool p = false;
      if (dir[i] && !prevDir_[i]) { p = true; repeatT_[i] = 0.4f; }
      else if (dir[i]) { repeatT_[i] -= dt; if (repeatT_[i] <= 0) { p = true; repeatT_[i] = 0.12f; } }
      pset(da[i], dir[i], p); prevDir_[i] = dir[i];
    }
    prevPadButtons_ = b; prevLT_ = lt; prevRT_ = rt;
    if (splitNow) {
      s.p2Active = true; s.p2MoveX = ps.moveX; s.p2MoveY = ps.moveY; s.p2LookX = ps.lookX; s.p2LookY = ps.lookY;
      for (int k = 0; k < A_COUNT; k++) { s.p2Held[k] = ps.held[k]; s.p2Pressed[k] = ps.pressed[k]; }
      set(A_PAUSE, ps.held[A_PAUSE], ps.pressed[A_PAUSE]);  // Start still pauses for everyone
    }
    // rumble
    rumbleT_ = std::max(0.f, rumbleT_ - dt);
    float r = rumbleT_ > 0 ? rumble_ : 0;
    if (r > 0 || rumbling_) {
      XINPUT_VIBRATION v = {}; v.wLeftMotorSpeed = (WORD)(std::min(1.f, r) * 65535); v.wRightMotorSpeed = (WORD)(std::min(1.f, r * 0.7f) * 65535);
      XInputSetState((DWORD)pad_, &v); rumbling_ = r > 0;
    }
  } else {
    prevPadButtons_ = 0; prevLT_ = prevRT_ = false; for (auto& p : prevDir_) p = false;
  }
  s.usingPad = usingPad_; s.mouseCaptured = captured_;
  // consume latches
  for (auto& l : keyLatch_) l = false;
  for (auto& l : mouseLatch_) l = false;
  clickLatch_ = false; mdx_ = mdy_ = 0; wheel_ = 0;
  return s;
}

void WinInput::setRumble(float strength, float seconds) { if (seconds > 0) { rumble_ = strength; rumbleT_ = seconds; } }
void WinInput::shutdown() {
  applyCapture(false);
  if (pad_ >= 0) { XINPUT_VIBRATION v = {}; XInputSetState((DWORD)pad_, &v); }
}
}  // namespace tz
