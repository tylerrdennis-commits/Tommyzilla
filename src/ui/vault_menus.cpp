// Vault: store tiles for kaiju powers, the power spinner, and the cheats menu (unlocked by finishing Crazy mode).
#include <algorithm>
#include <cmath>
#include "../game/game.h"
#include "ui_draw.h"
#include "ui_style.h"
namespace tz {
void drawIconPublic(UIDraw& u, int icon, float ix, float iy, float q, Color ic);  // menus.cpp
void Game::addKaijuPowerTiles(Menu& mm) {
  const MonsterDef& d = kaiju.def(); std::string kid = d.id;
  for (auto& pw : vpow::kaijuPowers(kid, d.name)) {
    int lvl = vault.level(kid, pw.stat), c = vpow::cost(lvl);
    MenuItem it; it.label = pw.name; it.sub = "Lv " + std::to_string(lvl) + "/" + std::to_string(vpow::kMaxLevel);
    it.desc = d.name + " power: " + pw.desc + ".";
    it.price = c; it.owned = c < 0; it.afford = c >= 0 && P().coins >= c; it.icon = pw.icon; it.iconColor = hexColor(pw.color);
    it.right = c < 0 ? "MAX" : std::to_string(c) + " coins";
    vpow::Stat st = pw.stat; std::string nm = pw.name;
    it.act = [this, kid, st, nm]() {
      if (vault.buy(kid, st, P().coins)) { play(SND_coin); toast(nm + " upgraded!"); persist(); } else play(SND_error);
    };
    mm.items.push_back(it);
  }
}
void Game::openPowerSpinner() {
  Menu m; m.title = "POWER SPIN"; m.kind = 10; pspin = PowerSpin();
  m.build = [this](Menu& mm) {
    mm.body = "Spin for a free level on one of " + kaiju.def().name + "'s powers!";
    MenuItem sp; int c = vpow::spinCost(); sp.label = "SPIN  (" + std::to_string(c) + " coins)"; sp.enabled = P().coins >= c && !pspin.active;
    sp.desc = P().coins >= c ? "Lower-level powers are more likely to win." : "Need " + std::to_string(c - P().coins) + " more coins.";
    sp.act = [this]() {
      int pick = vpow::spinPick(vault, kaiju.def().id, (uint64_t)(realTime * 1000) ^ 0xA11CEull);
      if (pick < 0) { toast("All powers are maxed!"); play(SND_error); return; }
      P().coins -= vpow::spinCost(); pspin = PowerSpin(); pspin.active = true; pspin.target = pick;
    };
    mm.items.push_back(sp);
    MenuItem sk; sk.label = "Skills"; sk.desc = "Pick skills for your two slots."; sk.act = [this]() { openSkills(); }; mm.items.push_back(sk);
    MenuItem st; st.label = "Store"; st.act = [this]() { openStore(); }; mm.items.push_back(st);
  };
  pushMenu(m);
}
void Game::updatePowerSpin(float dt) {
  if (!pspin.active) return;
  pspin.t += save.settings.reducedMotion ? vpow::kSpinSec : dt;
  if (pspin.t >= vpow::kSpinSec) {
    pspin.active = false; pspin.shown = true;
    const MonsterDef& d = kaiju.def(); vpow::Stat st = (vpow::Stat)pspin.target;
    vault.setLevel(d.id, st, vault.level(d.id, st) + 1);
    play(SND_coin); toast(vpow::kaijuPowers(d.id, d.name)[(size_t)pspin.target].name + " +1 level!"); persist();
  }
}
// reel styled like the egg spinner: a row of tiles sliding under a fixed centre marker, decelerating onto the prize
void Game::drawPowerSpin(UIDraw& u, Menu& m) {
  float s = u.s; const MonsterDef& d = kaiju.def(); auto pw = vpow::kaijuPowers(d.id, d.name); int n = (int)pw.size();
  float pw_ = std::min(u.W - 30, 340.f * s), px = (u.W - pw_) / 2, ph = 190 * s, py = (u.H - ph) / 2;
  style::panel(u, px, py, pw_, ph, true);
  style::closeX(u, px + pw_ - 22 * s, py + 6 * s, 16 * s, false);
  style::titleText(u, px + pw_ / 2, py + 8 * s, m.title, s * 1.4f, style::accent());
  u.textC(px + pw_ / 2, py + 30 * s, m.body, s * 0.75f, style::textDim());
  float ry = py + 46 * s, th = 56 * s, tw = 62 * s, cx = px + pw_ / 2;
  float off = pspin.active ? vpow::spinOffset(pspin.t, pspin.target, n, pspin.laps) : (pspin.shown ? (float)(pspin.laps * n + pspin.target) : 0.f);
  style::roundRect(u, px + 8 * s, ry - 4 * s, pw_ - 16 * s, th + 8 * s, 8 * s, Color{0, 0, 0, 0.35f});
  int base = (int)std::floor(off); float frac = off - base;
  for (int k = -3; k <= 3; k++) {
    float x = cx + (k - frac) * (tw + 6 * s) - tw / 2; if (x < px + 8 * s || x + tw > px + pw_ - 8 * s) continue;
    int idx = ((base + k) % n + n) % n; const auto& p = pw[(size_t)idx];
    bool centre = k == 0 && frac < 0.5f || k == 1 && frac >= 0.5f;
    style::roundRect(u, x, ry, tw, th, 6 * s, centre ? Color{0.10f, 0.30f, 0.38f, 0.96f} : Color{0.12f, 0.15f, 0.19f, 0.92f});
    drawIconPublic(u, p.icon, x + tw / 2 - 12 * s, ry + 5 * s, 24 * s, hexColor(p.color));
    std::string lv = "Lv " + std::to_string(vault.level(d.id, p.stat));
    u.textC(x + tw / 2, ry + 32 * s, p.name.substr(0, 9), s * 0.6f, style::text());
    u.textC(x + tw / 2, ry + 43 * s, lv, s * 0.6f, style::textDim());
  }
  style::roundRect(u, cx - s, ry - 6 * s, 2 * s, th + 12 * s, 0, style::accent());  // centre marker
  float y = ry + th + 12 * s, bh = 22 * s;
  menuLayout = {px + 6 * s, y, pw_ - 12 * s, bh + 4 * s, 0, (int)m.items.size()};
  for (int i = 0; i < (int)m.items.size(); i++) {
    const MenuItem& it = m.items[(size_t)i]; bool on = i == m.index;
    style::button(u, px + 6 * s, y, pw_ - 12 * s, bh, it.label, !it.enabled ? style::BTN_DISABLED : on ? style::BTN_FOCUS : style::BTN_NORMAL,
                  i == 0 ? style::ROLE_PRIMARY : style::ROLE_SECONDARY, 1.f, s * 0.9f);
    y += bh + 4 * s;
  }
}
void Game::vaultTick() {
  if (mode == Mode::Ending && ending.crazyEnding && !vault.cheatsUnlocked) { vault.cheatsUnlocked = true; persist(); }
  if (!vault.cheatsUnlocked) return;
  if (vault.god && mode == Mode::Play) kaiju.hp = std::max(kaiju.hp, kaiju.maxHp());
  if (vault.freeSupers) P().eaten = std::max(P().eaten, 99);
}
void Game::openCheats() {
  Menu m; m.title = "CHEATS"; m.kind = 11;
  m.build = [this](Menu& mm) {
    mm.body = vault.cheatsUnlocked ? "You beat Crazy mode! Have fun." : "Finish Crazy mode to unlock cheats.";
    auto tog = [&](const char* label, bool& v, const char* desc) {
      MenuItem it; it.label = label; it.right = v ? "ON" : "OFF"; it.desc = desc; it.enabled = vault.cheatsUnlocked;
      bool* pv = &v; it.act = [this, pv]() { *pv = !*pv; persist(); }; mm.items.push_back(it);
    };
    tog("Invincible", vault.god, "Nothing can hurt your kaiju.");
    tog("Free super attacks", vault.freeSupers, "Always have enough eaten for supers.");
    MenuItem c; c.label = "+10000 coins"; c.enabled = vault.cheatsUnlocked; c.act = [this]() { P().coins += 10000; play(SND_coin); persist(); }; mm.items.push_back(c);
    MenuItem sk; sk.label = "Unlock all skills"; sk.enabled = vault.cheatsUnlocked;
    sk.act = [this]() { for (auto& mv : T.moves) P().moves.insert(mv.id); toast("All skills unlocked!"); persist(); }; mm.items.push_back(sk);
    MenuItem kj; kj.label = "Unlock all kaiju"; kj.enabled = vault.cheatsUnlocked;
    kj.act = [this]() { for (auto& md : T.monsters) save.monsters.insert(md.id); toast("All kaiju unlocked!"); persist(); }; mm.items.push_back(kj);
    MenuItem pw; pw.label = "Max this kaiju's powers"; pw.enabled = vault.cheatsUnlocked;
    pw.act = [this]() { for (int s = 0; s < vpow::ST_COUNT; s++) vault.setLevel(kaiju.def().id, (vpow::Stat)s, vpow::kMaxLevel); persist(); }; mm.items.push_back(pw);
    MenuItem b; b.label = "Back"; b.act = [this]() { popMenu(); }; mm.items.push_back(b);
  };
  pushMenu(m);
}
}  // namespace tz
