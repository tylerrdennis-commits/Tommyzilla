#include "ui_style.h"
#include <algorithm>
#include <cmath>
namespace tz { namespace style {
float easeOut(float t) { t = std::max(0.f, std::min(1.f, t)); float k = 1 - t; return 1 - k * k * k; }
float panelAnim(float since, bool rm) { return rm ? 1.f : easeOut(since / kAnimSec); }
float pressScale(BtnState s, float since, bool rm) {
  if (rm || s != BTN_PRESSED) return 1.f;
  float t = std::max(0.f, std::min(1.f, since / 0.15f));
  return 0.94f + 0.06f * easeOut(t);
}
void roundRect(UIDraw& u, float x, float y, float w, float h, float r, Color c) {
  r = std::max(0.f, std::min(r, std::min(w, h) / 2));
  if (r < 1) { u.rect(x, y, w, h, c); return; }
  // convex outline (4 arcs) drawn as one triangle fan: smooth corners at a fraction of the vertices the old stepped rows used
  int seg = std::max(3, std::min(8, (int)std::ceil(r / 3.f)));
  static std::vector<float> P; P.clear();
  const float cx[4] = {x + w - r, x + r, x + r, x + w - r}, cy[4] = {y + r, y + r, y + h - r, y + h - r}, a0[4] = {-PI / 2, PI, PI / 2, 0};
  for (int k = 0; k < 4; k++) {   // top-right, top-left, bottom-left, bottom-right (counter-clockwise on screen)
    float start = k == 0 ? -PI / 2 : k == 1 ? -PI : k == 2 ? PI / 2 : 0, dir = k == 0 ? -1.f : -1.f; (void)a0; (void)dir;
    float from = k == 0 ? 0.f : k == 1 ? -PI / 2 : k == 2 ? -PI : PI / 2; (void)start;
    for (int i = 0; i <= seg; i++) { float ang = from - (PI / 2) * i / seg; P.push_back(cx[k] + std::cos(ang) * r); P.push_back(cy[k] + std::sin(ang) * r); }
  }
  size_t n = P.size() / 2; float mx = x + w / 2, my = y + h / 2;
  for (size_t i = 0; i < n; i++) {
    size_t j = (i + 1) % n;
    float ax = mx, ay = my, bx = P[i * 2], by = P[i * 2 + 1], ex = P[j * 2], ey = P[j * 2 + 1];
    if ((bx - ax) * (ey - ay) - (by - ay) * (ex - ax) < 0) { std::swap(bx, ex); std::swap(by, ey); }   // same winding as UIDraw::rect
    u.rl.ui.push_back(UIVertex{ax, ay, c.r, c.g, c.b, c.a}); u.rl.ui.push_back(UIVertex{bx, by, c.r, c.g, c.b, c.a}); u.rl.ui.push_back(UIVertex{ex, ey, c.r, c.g, c.b, c.a});
  }
}
void glow(UIDraw& u, float x, float y, float w, float h, float r, Color c, float spread) {
  for (int i = 3; i >= 1; i--) { float e = spread * i / 3; Color g = c; g.a = c.a * 0.10f * (4 - i);
    roundRect(u, x - e, y - e, w + 2 * e, h + 2 * e, r + e, g); }
}
void gradV(UIDraw& u, float x, float y, float w, float h, Color top, Color bottom) { u.rectGrad(x, y, w, h, top, top, bottom, bottom); }
void gradH(UIDraw& u, float x, float y, float w, float h, Color left, Color right) { u.rectGrad(x, y, w, h, left, right, left, right); }
void panel(UIDraw& u, float x, float y, float w, float h, bool primary) {
  float s = u.s, r = 12 * s;
  for (int i = 4; i >= 1; i--) roundRect(u, x - i * 2 * s, y - i * 2 * s + 6 * s, w + i * 4 * s, h + i * 4 * s, r + i * 2 * s, Color{0, 0, 0, 0.07f});  // soft drop shadow
  roundRect(u, x - 1, y - 1, w + 2, h + 2, r + 1, primary ? Color{0.23f, 0.84f, 1.f, 0.35f} : Color{1, 1, 1, 0.12f});   // hairline border
  roundRect(u, x, y, w, h, r, Color{0.05f, 0.06f, 0.09f, 0.90f});
  gradV(u, x + 1, y + r, w - 2, h - 2 * r, Color{0.11f, 0.14f, 0.19f, 0.30f}, Color{0.0f, 0.0f, 0.0f, 0.18f});         // gentle vertical sheen
  roundRect(u, x + r, y + 1, w - 2 * r, std::max(1.f, s / 2), 0, Color{1, 1, 1, 0.10f});                                // top highlight
}
void button(UIDraw& u, float x, float y, float w, float h, const std::string& label, BtnState st, BtnRole role, float sc, float ts) {
  float cx = x + w / 2, cy = y + h / 2; w *= sc; h *= sc; x = cx - w / 2; y = cy - h / 2;
  float r = std::min(h / 2, 8 * u.s);
  Color tc = text();
  if (role == ROLE_PRIMARY) {   // the one accent action: solid gold, dark text
    Color base = Color{0.96f, 0.72f, 0.20f, 1}; tc = Color{0.12f, 0.08f, 0.02f, 1};
    if (st == BTN_FOCUS || st == BTN_PRESSED) { glow(u, x, y, w, h, r, accent(), 5 * u.s); base = Color{1.f, 0.82f, 0.35f, 1}; }
    if (st == BTN_PRESSED) base = Color{1.f, 0.90f, 0.55f, 1};
    if (st == BTN_DISABLED) { base = Color{0.28f, 0.25f, 0.18f, 0.8f}; tc = Color{0.55f, 0.52f, 0.45f, 1}; }
    roundRect(u, x, y, w, h, r, base);
  } else if (st == BTN_FOCUS || st == BTN_PRESSED) {   // focus: soft cyan wash fading right + solid accent bar on the left
    float a = st == BTN_PRESSED ? 0.46f : 0.30f;
    roundRect(u, x, y, w, h, r, Color{0.23f, 0.84f, 1.f, 0.06f});
    gradH(u, x + std::max(3.f, 3 * u.s), y, w - std::max(3.f, 3 * u.s), h, Color{0.23f, 0.84f, 1.f, a}, Color{0.23f, 0.84f, 1.f, 0.0f});
    roundRect(u, x, y + h * 0.16f, std::max(3.f, 3 * u.s), h * 0.68f, 1.5f * u.s, cyan());
  } else if (st == BTN_DISABLED) {
    tc = Color{0.50f, 0.53f, 0.58f, 1};
    roundRect(u, x, y, w, h, r, Color{1, 1, 1, 0.02f});
  } else {
    roundRect(u, x, y, w, h, r, Color{1, 1, 1, 0.045f});   // resting: barely-there plate
  }
  if (label.empty()) return;
  ts *= sc; float tw = u.textW(label, ts); if (tw > w - 8 * u.s) { ts *= (w - 8 * u.s) / tw; }
  u.textC(cx, cy - u.lineH(ts) * 0.42f, label, ts, tc);
}
void titleText(UIDraw& u, float cx, float y, const std::string& t, float scale, Color c) {   // clean smooth title (no retro outline)
  bool was = u.chunky; u.chunky = true;   // Bold weight
  u.textC(cx + scale * 0.6f, y + scale * 0.9f, t, scale, Color{0, 0, 0, 0.45f}, false);
  u.textC(cx, y, t, scale, c, false);
  u.chunky = was;
}
void closeX(UIDraw& u, float x, float y, float q, bool focus) {
  roundRect(u, x, y, q, q, q / 2, focus ? Color{0.10f, 0.30f, 0.38f, 0.96f} : Color{0.12f, 0.15f, 0.19f, 0.9f});
  if (focus) glow(u, x, y, q, q, q / 2, cyan(), 3 * u.s);
  float ts = q / 13.f * 0.8f; u.textC(x + q / 2 + ts * 0.5f, y + (q - u.lineH(ts)) / 2 + ts, "X", ts, text());
}
void crosshair(UIDraw& u, float cx, float cy, float s, bool lock, Color c) {
  float g = (lock ? 7.f : 5.f) * s, L = 6 * s, t = std::max(1.f, s * 0.75f);
  Color sh{0, 0, 0, 0.55f * c.a};   // dark outline so it reads on bright sky and dark streets
  auto bar = [&](float x, float y, float w, float h) { u.rect(x - 1, y - 1, w + 2, h + 2, sh); u.rect(x, y, w, h, c); };
  bar(cx - g - L, cy - t / 2, L, t); bar(cx + g, cy - t / 2, L, t); bar(cx - t / 2, cy - g - L, t, L); bar(cx - t / 2, cy + g, t, L);
  bar(cx - t, cy - t, 2 * t, 2 * t);
}
} }  // namespace tz::style
