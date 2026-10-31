// Vault: bottom HUD bar - concise info + direct buttons (egg spinner, power spinner, store) so Start is rarely needed.
#pragma once
namespace tz { namespace hudbar {
enum Button { BTN_EGGS, BTN_POWERS, BTN_STORE, BTN_COUNT };
struct Rect { float x, y, w, h; };
struct Layout { Rect bar; Rect btn[BTN_COUNT]; };
Layout layout(float W, float H, float s);          // pure: right-aligned buttons in a rounded bar on the bottom edge
int hit(const Layout& L, float x, float y);        // button index or -1
} }  // namespace tz::hudbar
