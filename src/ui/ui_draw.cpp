#include "ui_draw.h"
#include <cmath>
#include "font_data.h"
#include "font_hd.h"
namespace tz {
namespace {
struct Run { uint8_t row, x0, len; };
std::vector<Run> gRuns[95];
struct RunHD { uint8_t row, x0, len, edge; };
std::vector<RunHD> gRunsHD[95];
bool gRunsReady = false;
void buildRuns() {
  for (int g = 0; g < 95; g++) {
    for (int row = 0; row < FONT_H; row++) {
      uint8_t bits = FONT_DATA[g][row]; int x = 0;
      while (x < FONT_W) {
        if (bits & (0x80 >> x)) { int x0 = x; while (x < FONT_W && (bits & (0x80 >> x))) x++; gRuns[g].push_back({(uint8_t)row, (uint8_t)x0, (uint8_t)(x - x0)}); }
        else x++;
      }
    }
  }
  for (int g = 0; g < 95; g++)
    for (int pass = 0; pass < 2; pass++)
      for (int row = 0; row < FONTHD_H; row++) {
        uint16_t bits = pass ? FONTHD_EDGE[g][row] : FONTHD_SOLID[g][row]; int x = 0;
        while (x < FONTHD_W) {
          if (bits & (0x8000 >> x)) { int x0 = x; while (x < FONTHD_W && (bits & (0x8000 >> x))) x++; gRunsHD[g].push_back({(uint8_t)row, (uint8_t)x0, (uint8_t)(x - x0), (uint8_t)pass}); }
          else x++;
        }
      }
  gRunsReady = true;
}
}  // namespace
UIDraw::UIDraw(RenderList& r) : rl(r), W((float)r.width), H((float)r.height) {
  s = std::max(1.0f, std::round(H / 720.f * 2.f * 2.f) / 2.f);  // 2 at 720p, 3 at 1080p (half steps in between)
  if (r.uiScale > 0) s = r.uiScale;
  if (!gRunsReady) buildRuns();
}
void UIDraw::rect(float x, float y, float w, float h, Color c) {
  UIVertex a{x, y, c.r, c.g, c.b, c.a}, b{x + w, y, c.r, c.g, c.b, c.a}, d{x, y + h, c.r, c.g, c.b, c.a}, e{x + w, y + h, c.r, c.g, c.b, c.a};
  rl.ui.push_back(a); rl.ui.push_back(b); rl.ui.push_back(d); rl.ui.push_back(b); rl.ui.push_back(e); rl.ui.push_back(d);
}
void UIDraw::frame(float x, float y, float w, float h, Color c, float t) { rect(x, y, w, t, c); rect(x, y + h - t, w, t, c); rect(x, y, t, h, c); rect(x + w - t, y, t, h, c); }
void UIDraw::star(float cx, float cy, float r, Color c) {
  for (int i = 0; i < 5; i++) {
    float a0 = -PI / 2 + i * 2 * PI / 5, a1 = a0 + PI / 5, a2 = a0 - PI / 5, ri = r * 0.45f;
    UIVertex o{cx, cy, c.r, c.g, c.b, c.a}, p{cx + std::cos(a0) * r, cy + std::sin(a0) * r, c.r, c.g, c.b, c.a};
    UIVertex q{cx + std::cos(a1) * ri, cy + std::sin(a1) * ri, c.r, c.g, c.b, c.a}, w{cx + std::cos(a2) * ri, cy + std::sin(a2) * ri, c.r, c.g, c.b, c.a};
    rl.ui.push_back(o); rl.ui.push_back(p); rl.ui.push_back(q); rl.ui.push_back(o); rl.ui.push_back(w); rl.ui.push_back(p);
  }
}
float UIDraw::textW(const std::string& t, float scale) const { return (float)t.size() * FONT_W * scale; }
void UIDraw::text(float x, float y, const std::string& t, float scale, Color c, bool shadow) {
  if (shadow) { Color sc{0, 0, 0, c.a * 0.7f}; text(x + scale, y + scale, t, scale, sc, false); }
  float cx = x;
  for (unsigned char ch : t) {
    if (ch == '\n') { cx = x; y += lineH(scale); continue; }
    int g = (ch >= 32 && ch < 127) ? ch - 32 : '?' - 32;
    if (chunky || scale < 1.0f) { for (const Run& r : gRuns[g]) rect(cx + r.x0 * scale, y + r.row * scale, r.len * scale, scale, c); }
    else {
      float h = scale * 0.5f; Color ce = c; ce.a *= 0.45f;
      for (const RunHD& r : gRunsHD[g]) rect(cx + r.x0 * h, y + r.row * h, r.len * h, h, r.edge ? ce : c);
    }
    cx += FONT_W * scale;
  }
}
std::vector<std::string> UIDraw::wrap(const std::string& t, float maxW, float scale) const {
  std::vector<std::string> out; size_t maxC = (size_t)std::max(4.f, maxW / (FONT_W * scale));
  size_t start = 0;
  while (start <= t.size()) {
    size_t nl = t.find('\n', start); std::string para = t.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
    std::string line;
    size_t i = 0;
    while (i < para.size()) {
      size_t sp = para.find(' ', i); std::string word = para.substr(i, sp == std::string::npos ? std::string::npos : sp - i);
      if (!line.empty() && line.size() + 1 + word.size() > maxC) { out.push_back(line); line.clear(); }
      while (word.size() > maxC) { out.push_back(word.substr(0, maxC)); word = word.substr(maxC); }
      line += (line.empty() ? "" : " ") + word;
      if (sp == std::string::npos) break; i = sp + 1;
    }
    out.push_back(line);
    if (nl == std::string::npos) break; start = nl + 1;
  }
  return out;
}
bool UIDraw::project(const Vec3& p, float& sx, float& sy) const {
  Mat4 vp = rl.view * rl.proj;
  float x = p.x * vp.m[0][0] + p.y * vp.m[1][0] + p.z * vp.m[2][0] + vp.m[3][0];
  float y = p.x * vp.m[0][1] + p.y * vp.m[1][1] + p.z * vp.m[2][1] + vp.m[3][1];
  float w = p.x * vp.m[0][3] + p.y * vp.m[1][3] + p.z * vp.m[2][3] + vp.m[3][3];
  if (w < 0.5f) return false;
  sx = (x / w * 0.5f + 0.5f) * W; sy = (1 - (y / w * 0.5f + 0.5f)) * H;
  return sx > -200 && sx < W + 200 && sy > -50 && sy < H + 50;
}
}  // namespace tz
