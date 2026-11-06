// Vault: software-rasterise the UI layer (no GPU) into a PPM so menu / HUD styling can be eyeballed on Linux.
// Usage: tz_vault_ui_shot out.ppm   -> left half: main menu, right half: in-game HUD (bottom bar + crosshair)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include "game/game.h"
#include "core/json.h"
using namespace tz;
static void raster(std::vector<float>& img, int W, int H, int ox, int cw, const RenderList& rl) {
  auto& v = rl.ui;
  for (size_t i = 0; i + 2 < v.size(); i += 3) {
    const UIVertex &a = v[i], &b = v[i + 1], &c = v[i + 2];
    float minx = std::min({a.x, b.x, c.x}), maxx = std::max({a.x, b.x, c.x}), miny = std::min({a.y, b.y, c.y}), maxy = std::max({a.y, b.y, c.y});
    float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); if (std::fabs(area) < 1e-6f) continue;
    for (int y = std::max(0, (int)minx * 0 + (int)miny); y <= std::min(H - 1, (int)maxy); y++)
      for (int x = std::max(0, (int)minx); x <= std::min(cw - 1, (int)maxx); x++) {
        float px = x + 0.5f, py = y + 0.5f;
        float w0 = ((b.x - px) * (c.y - py) - (b.y - py) * (c.x - px)) / area, w1 = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) / area, w2 = 1 - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        float al = a.a * w0 + b.a * w1 + c.a * w2, cr = a.r * w0 + b.r * w1 + c.r * w2, cg = a.g * w0 + b.g * w1 + c.g * w2, cb = a.b * w0 + b.b * w1 + c.b * w2;  // interpolate vertex colours so gradients show
        float* d = &img[((size_t)y * W + ox + x) * 3];
        d[0] = d[0] * (1 - al) + cr * al; d[1] = d[1] * (1 - al) + cg * al; d[2] = d[2] * (1 - al) + cb * al;
      }
  }
}
static std::string file(const char* n) { std::string s; readTextFile(std::string(TZ_DATA_DIR) + "/" + n, s); return s; }
int main(int argc, char** argv) {
  std::string e = loadGameData(file("config.json"), file("moves.json"), file("monsters.json"), file("levels.json"), file("eggs.json"));
  if (!e.empty()) { std::printf("data load error: %s\n", e.c_str()); return 1; }
  const int cw = 1280, H = 720, W = cw * 2; std::vector<float> img((size_t)W * H * 3);
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { float k = (float)y / H; float* d = &img[((size_t)y * W + x) * 3];
    d[0] = 0.35f + 0.25f * k; d[1] = 0.45f + 0.15f * k; d[2] = 0.60f - 0.1f * k; }  // stand-in sky/street backdrop
  auto g = std::make_unique<Game>(); g->init(nullptr);
  g->save.created = true; g->menus.clear(); g->openMainMenu(); g->realTime = 5; if (argc > 2) g->menus.back().index = std::atoi(argv[2]);
  { RenderList rl; rl.width = cw; rl.height = H; g->drawMenus(rl); rl.ui.clear(); g->realTime += 1; g->drawMenus(rl); raster(img, W, H, 0, cw, rl); }
  if (argc > 2) g->menus.back().index = std::atoi(argv[2]);
  g->menus.clear(); g->save.tutorial.done = true; g->startMode(false); g->menus.clear(); g->mode = Mode::Play; g->mouseCaptured = true; if (std::getenv("TZ_HEAT")) g->enemies.heat = (float)std::atof(std::getenv("TZ_HEAT"));
  std::string which = argc > 3 ? argv[3] : "hud";
  if (which == "pause") g->openPause(); else if (which == "options") g->openOptions(); else if (which == "chars") g->openCharacterSelect(false); else if (which == "store") g->openStore(); else if (which == "skills") g->openSkills(); else if (which == "stones") g->openStoneMenu();
  { RenderList rl; rl.width = cw; rl.height = H; if (which == "hud") g->drawHud(rl); g->drawMenus(rl); g->realTime += 1; rl.ui.clear(); if (which == "hud") g->drawHud(rl); g->drawMenus(rl); std::fprintf(stderr, "ui vertices this frame: %zu (%.1f MB)\n", rl.ui.size(), rl.ui.size() * sizeof(UIVertex) / 1048576.0); raster(img, W, H, cw, cw, rl); }
  FILE* f = std::fopen(argc > 1 ? argv[1] : "ui_shot.ppm", "wb"); if (!f) return 1;
  std::fprintf(f, "P6 %d %d 255\n", W, H);
  for (float v : img) { int c = (int)(std::max(0.f, std::min(1.f, v)) * 255 + 0.5f); std::fputc(c, f); }
  std::fclose(f); return 0;
}
