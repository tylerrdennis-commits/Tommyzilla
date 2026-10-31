// Vault: menu style + bottom HUD bar tests (pure layout / motion math, and that the drawing helpers emit geometry)
#include <cmath>
#include <cstdio>
#include <memory>
#include "core/json.h"
#include "game/game.h"
#include "game/vault_powers.h"
#include "ui/hud_bar.h"
#include "ui/ui_style.h"
using namespace tz;
static int gPass = 0, gFail = 0;
#define CHECK(c) do { if (c) gPass++; else { gFail++; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static std::string file(const char* n) { std::string s; readTextFile(std::string(TZ_DATA_DIR) + "/" + n, s); return s; }
static void testPowers() {
  auto a = vpow::kaijuPowers("tommyzilla", "Tommyzilla"), b = vpow::kaijuPowers("gloopzilla", "Gloopzilla");
  CHECK(a.size() == 5 && b.size() == 5);
  for (int i = 0; i < 5; i++) CHECK(a[(size_t)i].stat == i && !a[(size_t)i].name.empty());
  CHECK(a[0].name.find("Tommyzilla") == 0 && b[0].name.find("Gloopzilla") == 0);
  CHECK(vpow::cost(0) > 0 && vpow::cost(vpow::kMaxLevel) == -1 && vpow::cost(1) > vpow::cost(0));
  vpow::State st; int coins = 100000;
  for (int i = 0; i < 7; i++) st.buy("tommyzilla", vpow::ST_DAMAGE, coins);
  CHECK(st.level("tommyzilla", vpow::ST_DAMAGE) == vpow::kMaxLevel && st.level("gloopzilla", vpow::ST_DAMAGE) == 0);
  CHECK(std::fabs(st.mult("tommyzilla", vpow::ST_DAMAGE) - 1.4f) < 1e-5f && st.mult("tommyzilla", vpow::ST_THRIFT) == 1.f);
  int poor = 10; CHECK(!st.buy("tommyzilla", vpow::ST_SPEED, poor) && poor == 10);
  st.cheatsUnlocked = true; st.god = true; vpow::State r; r.fromJson(st.toJson());
  CHECK(r.level("tommyzilla", vpow::ST_DAMAGE) == vpow::kMaxLevel && r.cheatsUnlocked && r.god);
  vpow::State locked; locked.god = true; vpow::State r2; r2.fromJson(locked.toJson()); CHECK(!r2.god);  // cheats need the unlock
  for (uint64_t seed = 1; seed < 200; seed++) { int k = vpow::spinPick(st, "tommyzilla", seed); CHECK(k > 0 && k < 5); }  // never the maxed one
  for (int s2 = 0; s2 < 5; s2++) st.setLevel("tommyzilla", (vpow::Stat)s2, vpow::kMaxLevel);
  CHECK(vpow::spinPick(st, "tommyzilla", 7) == -1);
  CHECK(vpow::spinOffset(0, 2, 5, 3) == 0.f && std::fabs(vpow::spinOffset(vpow::kSpinSec, 2, 5, 3) - 17.f) < 1e-4f);
}
static void testGameMenus() {
  std::string e = loadGameData(file("config.json"), file("moves.json"), file("monsters.json"), file("levels.json"), file("eggs.json"));
  CHECK(e.empty()); if (!e.empty()) return;
  auto g = std::make_unique<Game>(); g->init(nullptr); g->save.tutorial.done = true; g->startMode(false); g->menus.clear(); g->mode = Mode::Play;
  g->openStore(); Menu& m = g->menus.back(); m.items.clear(); m.build(m);
  int basic = 0, kp = 0; auto pw = vpow::kaijuPowers(g->kaiju.def().id, g->kaiju.def().name);
  for (auto& it : m.items) { if (it.label == "Mighty Claws" || it.label == "Big Reach" || it.label == "Stompy Speed") basic++; for (auto& p : pw) if (it.label == p.name) kp++; }
  CHECK(basic == 3 && kp == 5);
  g->P().coins = 100000; int before = g->P().coins;
  for (auto& it : m.items) if (it.label == pw[1].name) it.act();
  CHECK(g->vault.level(g->kaiju.def().id, vpow::ST_RANGE) == 1 && g->P().coins < before);
  g->menus.clear(); for (auto& mv : T.moves) g->P().moves.insert(mv.id);
  g->openSkills(); Menu& sk = g->menus.back(); sk.items.clear(); sk.build(sk);
  int gold = 0; for (auto& it : sk.items) if (it.icon == 11) gold++; CHECK(gold > 0);
  g->menus.clear(); g->openPowerSpinner(); Menu& ps = g->menus.back(); ps.items.clear(); ps.build(ps);
  int sum0 = 0; for (int s2 = 0; s2 < 5; s2++) sum0 += g->vault.level(g->kaiju.def().id, (vpow::Stat)s2);
  ps.items[0].act(); CHECK(g->pspin.active);
  for (int i = 0; i < 400 && g->pspin.active; i++) g->updatePowerSpin(1.f / 60);
  int sum1 = 0; for (int s2 = 0; s2 < 5; s2++) sum1 += g->vault.level(g->kaiju.def().id, (vpow::Stat)s2);
  CHECK(!g->pspin.active && sum1 == sum0 + 1);
  g->menus.clear(); CHECK(!g->vault.cheatsUnlocked);
  g->openCheats(); Menu& ch = g->menus.back(); ch.items.clear(); ch.build(ch); CHECK(!ch.items[0].enabled);
  g->mode = Mode::Ending; g->ending.crazyEnding = true; g->vaultTick(); CHECK(g->vault.cheatsUnlocked);
  g->mode = Mode::Play; g->vault.god = true; g->kaiju.hp = 1; g->vaultTick(); CHECK(g->kaiju.hp >= g->kaiju.maxHp());
}
int main() {
  testPowers(); testGameMenus();
  // motion: 150-300ms ease-out, reduced motion = instant
  CHECK(style::kAnimSec >= 0.15f && style::kAnimSec <= 0.3f);
  CHECK(style::easeOut(0) == 0.f && style::easeOut(1) == 1.f && style::easeOut(2) == 1.f && style::easeOut(-1) == 0.f);
  CHECK(style::easeOut(0.5f) > 0.5f);                                     // ease-out front-loads progress
  CHECK(style::panelAnim(0, true) == 1.f && style::panelAnim(0, false) == 0.f && style::panelAnim(style::kAnimSec, false) == 1.f);
  CHECK(style::pressScale(style::BTN_PRESSED, 0, false) < 0.97f);          // press = slight scale-down
  CHECK(style::pressScale(style::BTN_PRESSED, 0, true) == 1.f);            // not with reduced motion
  CHECK(style::pressScale(style::BTN_FOCUS, 0, false) == 1.f);
  // bottom bar layout: three >=48px buttons inside the bar, on screen, no overlap, at the bottom
  for (float H : {720.f, 1080.f, 1440.f}) {
    float W = H * 16 / 9, s = std::max(1.0f, std::round(H / 720.f * 4.f) / 2.f);
    hudbar::Layout L = hudbar::layout(W, H, s);
    CHECK(L.bar.y + L.bar.h <= H && L.bar.y > H * 0.8f);
    for (int i = 0; i < hudbar::BTN_COUNT; i++) {
      const hudbar::Rect& r = L.btn[i];
      CHECK(r.w >= 48 && r.h >= 48 * H / 1080.f);
      CHECK(r.x >= L.bar.x && r.x + r.w <= L.bar.x + L.bar.w && r.y >= L.bar.y && r.y + r.h <= L.bar.y + L.bar.h);
      CHECK(hudbar::hit(L, r.x + r.w / 2, r.y + r.h / 2) == i);
      if (i) CHECK(r.x - (L.btn[i - 1].x + L.btn[i - 1].w) >= 8 * H / 1080.f);
    }
    CHECK(hudbar::hit(L, W / 2, H / 2) == -1);
  }
  // drawing helpers emit triangles
  RenderList rl; rl.width = 1280; rl.height = 720; UIDraw u(rl);
  style::panel(u, 10, 10, 200, 100); size_t n0 = rl.ui.size(); CHECK(n0 > 0 && n0 % 3 == 0);
  style::button(u, 10, 10, 120, 40, "PLAY", style::BTN_FOCUS, style::ROLE_PRIMARY, 1, 2); CHECK(rl.ui.size() > n0);
  size_t n1 = rl.ui.size(); style::crosshair(u, 640, 300, 2, false, Color{1, 1, 1, 1}); CHECK(rl.ui.size() > n1);
  std::printf("vault_tests: %d passed, %d failed\n", gPass, gFail);
  return gFail ? 1 : 0;
}
