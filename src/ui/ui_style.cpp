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
  u.rect(x, y + r, w, h - 2 * r, c);
  int steps = std::max(2, (int)(r / 2));                                   // corner rows, stepped for a rounded silhouette
  for (int i = 0; i < steps; i++) {
    float y0 = r * i / steps, y1 = r * (i + 1) / steps, dy = r - (y0 + y1) / 2;
    float in = r - std::sqrt(std::max(0.f, r * r - dy * dy));
    u.rect(x + in, y + y0, w - 2 * in, y1 - y0, c);
    u.rect(x + in, y + h - y1, w - 2 * in, y1 - y0, c);
  }
}
void glow(UIDraw& u, float x, float y, float w, float h, float r, Color c, float spread) {
  for (int i = 3; i >= 1; i--) { float e = spread * i / 3; Color g = c; g.a = c.a * 0.10f * (4 - i);
    roundRect(u, x - e, y - e, w + 2 * e, h + 2 * e, r + e, g); }
}
void panel(UIDraw& u, float x, float y, float w, float h, bool primary) {
  float s = u.s, r = 8 * s;
  roundRect(u, x + 2 * s, y + 4 * s, w, h, r, Color{0, 0, 0, 0.35f});    // soft shadow
  glow(u, x, y, w, h, r, primary ? cyan() : Color{0.23f, 0.84f, 1.f, 0.5f}, 4 * s);
  roundRect(u, x, y, w, h, r, bg());
  roundRect(u, x + r, y + 1, w - 2 * r, std::max(1.f, s / 2), 0, Color{0.23f, 0.84f, 1.f, 0.55f});  // top rim light
}
void button(UIDraw& u, float x, float y, float w, float h, const std::string& label, BtnState st, BtnRole role, float sc, float ts) {
  float cx = x + w / 2, cy = y + h / 2; w *= sc; h *= sc; x = cx - w / 2; y = cy - h / 2;
  float r = std::min(h / 2, 10 * u.s);
  Color base = role == ROLE_PRIMARY ? Color{0.55f, 0.40f, 0.08f, 0.95f} : Color{0.12f, 0.15f, 0.19f, 0.92f};
  Color tc = role == ROLE_PRIMARY ? Color{1, 0.95f, 0.85f, 1} : text();
  if (st == BTN_DISABLED) { base = Color{0.10f, 0.11f, 0.13f, 0.7f}; tc = Color{0.50f, 0.53f, 0.58f, 1}; }
  if (st == BTN_FOCUS || st == BTN_PRESSED) {
    Color gc = role == ROLE_PRIMARY ? accent() : cyan(); if (st == BTN_PRESSED) gc.a = 1.f; else gc.a = 0.8f;
    glow(u, x, y, w, h, r, gc, (st == BTN_PRESSED ? 6.f : 4.f) * u.s);
    roundRect(u, x - 2 * u.s / 2 - 1, y - u.s - 1, w + u.s + 2, h + 2 * u.s + 2, r + u.s, gc);  // focus ring
    if (role == ROLE_SECONDARY) base = Color{0.10f, 0.30f, 0.38f, 0.96f};
    else base = Color{0.75f, 0.55f, 0.12f, 1};
    if (st == BTN_PRESSED) { base.r = std::min(1.f, base.r * 1.25f); base.g = std::min(1.f, base.g * 1.25f); base.b = std::min(1.f, base.b * 1.25f); }
  }
  roundRect(u, x, y, w, h, r, base);
  ts *= sc; float tw = u.textW(label, ts); if (tw > w - 8 * u.s) { ts *= (w - 8 * u.s) / tw; }
  u.textC(cx, cy - u.lineH(ts) * 0.42f, label, ts, tc);
}
void titleText(UIDraw& u, float cx, float y, const std::string& t, float scale, Color c) {
  float o = std::max(1.f, scale * 0.5f);   // chunky: thick dark outline + doubled stroke
  Color k{0.02f, 0.03f, 0.05f, c.a}; bool was = u.chunky; u.chunky = true;
  for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 2; dy++) if (dx || dy) u.textC(cx + dx * o, y + dy * o, t, scale, k, false);
  u.textC(cx, y, t, scale, c, false); u.textC(cx + o * 0.5f, y, t, scale, c, false); u.chunky = was;
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
