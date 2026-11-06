// 2D drawing into the RenderList UI vertex stream: rectangles, stars, and anti-aliased vector text (Poppins glyphs baked into
// font_vec.h by tools/gen_font_vec.py -- no font files at runtime). Text is proportional; use textW() for widths.
#pragma once
#include <string>
#include <vector>
#include "../game/render_types.h"
namespace tz {
struct UIDraw {
  RenderList& rl; float W, H, s;  // s = base UI scale (font pixel size for normal text)
  bool chunky = false;  // false = Poppins Medium (body), true = Poppins Bold (titles / emphasis)
  explicit UIDraw(RenderList& r);
  void rect(float x, float y, float w, float h, Color c);
  void rectGrad(float x, float y, float w, float h, Color tl, Color tr, Color bl, Color br);  // per-corner colours (smooth gradients)
  void frame(float x, float y, float w, float h, Color c, float t);
  void star(float cx, float cy, float r, Color c);
  void tri(float x1, float y1, float x2, float y2, float x3, float y3, Color c);
  void kaijuHead(float cx, float cy, float r, Color c, Color eye);  // rampage-level symbol: a spiky, toothy kaiju head
  float textW(const std::string& t, float scale) const;
  float lineH(float scale) const { return 13 * scale; }
  void text(float x, float y, const std::string& t, float scale, Color c, bool shadow = true);
  void textC(float cx, float y, const std::string& t, float scale, Color c, bool shadow = true) { text(cx - textW(t, scale) / 2, y, t, scale, c, shadow); }
  void textR(float rx, float y, const std::string& t, float scale, Color c, bool shadow = true) { text(rx - textW(t, scale), y, t, scale, c, shadow); }
  std::vector<std::string> wrap(const std::string& t, float maxW, float scale) const;
  bool project(const Vec3& p, float& sx, float& sy) const;  // world -> screen (false if behind the camera)
};
}  // namespace tz
