#include "hud_bar.h"
#include <algorithm>
#include <cmath>
#include <string>
#include "../game/game.h"
#include "ui_draw.h"
#include "ui_style.h"
namespace tz {
namespace hudbar {
Layout layout(float W, float H, float s) {
  Layout L; float bh = 26 * s, pad = 4 * s, bw = std::max(48.f, 54 * s);   // >=48dp buttons, 8dp+ spacing
  L.bar = {6, H - bh - 2 * pad - 4, W - 12, bh + 2 * pad};
  float x = W - 6 - pad - BTN_COUNT * bw - (BTN_COUNT - 1) * 2 * pad;
  for (int i = 0; i < BTN_COUNT; i++) { L.btn[i] = {x, L.bar.y + pad, bw, bh}; x += bw + 2 * pad; }
  return L;
}
int hit(const Layout& L, float x, float y) {
  for (int i = 0; i < BTN_COUNT; i++) { const Rect& r = L.btn[i]; if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) return i; }
  return -1;
}
}  // namespace hudbar
static const char* kLabel[hudbar::BTN_COUNT] = {"EGGS", "POWERS", "STORE"};
void Game::drawHudBar(UIDraw& u) {
  float s = u.s, small = s * 0.75f; Profile& p = P();
  lastW = u.W; lastH = u.H;
  hudbar::Layout L = hudbar::layout(u.W, u.H, s);
  style::roundRect(u, L.bar.x, L.bar.y, L.bar.w, L.bar.h, 8 * s, style::bg());
  style::roundRect(u, L.bar.x + 8 * s, L.bar.y + 1, L.bar.w - 16 * s, std::max(1.f, s / 2), 0, Color{0.23f, 0.84f, 1.f, 0.5f});
  // the kaiju's name (coins / keys / eaten live in the top-left status card)
  float ty = L.bar.y + (L.bar.h - u.lineH(s)) / 2;
  std::string info = kaiju.def().name;
  float maxW = L.btn[0].x - L.bar.x - 16 * s, ts = s;
  if (u.textW(info, ts) > maxW) ts = std::max(small, ts * maxW / u.textW(info, ts));
  u.text(L.bar.x + 8 * s, ty, info, ts, style::text());
  bool usable[hudbar::BTN_COUNT] = {p.spins > 0, true, true};
  const char* keys[hudbar::BTN_COUNT] = {"", "Tab", "B"};
  const char* pads[hudbar::BTN_COUNT] = {"", "Dv", "D^"};
  int hov = (!mouseCaptured && !usingPad) ? hudbar::hit(L, lastCurX, lastCurY) : -1;
  for (int i = 0; i < hudbar::BTN_COUNT; i++) {
    const hudbar::Rect& r = L.btn[i];
    style::BtnState st = !usable[i] ? style::BTN_DISABLED : hov == i ? style::BTN_FOCUS : style::BTN_NORMAL;
    style::button(u, r.x, r.y, r.w, r.h, kLabel[i], st, i == hudbar::BTN_EGGS ? style::ROLE_PRIMARY : style::ROLE_SECONDARY, 1.f, s * 0.8f);
    const char* hint = usingPad ? pads[i] : keys[i];
    if (*hint) u.textC(r.x + r.w / 2, r.y - u.lineH(small * 0.8f) - 1, hint, small * 0.8f, style::textDim());
  }
}
bool Game::hudBarClick(float x, float y) {
  if (mouseCaptured || x < 0 || mode != Mode::Play || !menus.empty()) return false;
  int b = hudbar::hit(hudbar::layout(lastW, lastH, std::max(1.0f, std::round(lastH / 720.f * 4.f) / 2.f)), x, y);
  if (b == hudbar::BTN_EGGS) { if (P().spins > 0) beginSpin(SPIN_TOKEN); else play(SND_error, 0.6f); }
  else if (b == hudbar::BTN_POWERS) openPowerSpinner();
  else if (b == hudbar::BTN_STORE) openStore();
  return b >= 0;
}
}  // namespace tz
