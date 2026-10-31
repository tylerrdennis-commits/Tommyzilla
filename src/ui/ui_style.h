// Vault: modern mobile-game UI style (docs: menu_style_brief). Dark translucent rounded panels with soft glow,
// charcoal/cyan palette + one accent (gold) for primary actions, big buttons with distinct states, 150-300ms ease-out.
#pragma once
#include "ui_draw.h"
namespace tz { namespace style {
// palette
inline Color bg() { return Color{0.05f, 0.06f, 0.08f, 0.82f}; }        // panel (charcoal, translucent)
inline Color cyan() { return Color{0.23f, 0.84f, 1.f, 1.f}; }           // plate glow / focus
inline Color accent() { return Color{1.f, 0.78f, 0.25f, 1.f}; }         // single accent: primary action
inline Color steel() { return Color{0.78f, 0.80f, 0.83f, 1.f}; }
inline Color text() { return Color{0.95f, 0.97f, 1.f, 1.f}; }
inline Color textDim() { return Color{0.70f, 0.74f, 0.80f, 1.f}; }      // >=4.5:1 on bg
enum BtnState { BTN_NORMAL, BTN_FOCUS, BTN_PRESSED, BTN_DISABLED };
enum BtnRole { ROLE_PRIMARY, ROLE_SECONDARY };
// motion (pure, unit-tested)
constexpr float kAnimSec = 0.2f;                                         // panel slide/fade (within 150-300ms)
float easeOut(float t);                                                  // cubic ease-out, clamped 0..1
float panelAnim(float sinceOpen, bool reducedMotion);                    // 0..1 progress (1 immediately if reduced)
float pressScale(BtnState s, float sincePress, bool reducedMotion);      // 1.0, or ~0.94 dip on press then back
// drawing
void roundRect(UIDraw& u, float x, float y, float w, float h, float r, Color c);
void glow(UIDraw& u, float x, float y, float w, float h, float r, Color c, float spread);
void panel(UIDraw& u, float x, float y, float w, float h, bool primary = false);
void button(UIDraw& u, float x, float y, float w, float h, const std::string& label, BtnState st, BtnRole role, float scale, float textS);
void titleText(UIDraw& u, float cx, float y, const std::string& t, float scale, Color c);  // chunky display title
void closeX(UIDraw& u, float x, float y, float q, bool focus);
void crosshair(UIDraw& u, float cx, float cy, float s, bool lock, Color c);
} }  // namespace tz::style
