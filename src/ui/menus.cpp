// Menus (main, pause, store, equip, melee loadout, move list, characters, options, prompts) and the in-game HUD.
#include <algorithm>
#include <cstdio>
#include <string>
#include "../game/controls.h"
#include "../game/game.h"
#include "ui_draw.h"
#include "ui_style.h"
#include "hud_bar.h"
namespace tz {
namespace {
const Color GOLD = hexColor(0xffd36b), CYAN = hexColor(0x3ad7ff), WHITE = hexColor(0xffffff), GREY = hexColor(0xa8a4b8), PANEL = Color{0.08f, 0.05f, 0.12f, 0.88f};
const Color RED = hexColor(0xff5a4a), PEA = hexColor(0x7aff6a);
// store / controls palette (matches the HUD art): charcoal tiles, cyan focus, dusk fill, steel outlines
const Color CHARCOAL = hexColor(0x12141a), DUSK = hexColor(0x2a1424), STEEL = hexColor(0xc8ccd4);
Color dimmed(Color c, float k) { return Color{c.r * k, c.g * k, c.b * k, c.a}; }
// little drawn icons in a q x q box: 1 key ring, 2 gauntlet slot outline / glove, 3 colour swatch, 4 cracked slot,
// store glyphs 5 claw marks, 6 reach rings, 7 speed boot, 8 egg, 9 ray gun, 10 back arrow
void drawIcon(UIDraw& u, int icon, float ix, float iy, float q, Color ic, bool enabled) {
  if (icon == 11) { u.star(ix + q / 2, iy + q / 2, q * 0.5f, ic); return; }  // Vault: gold super star
  float lw = std::max(1.f, u.s / 2);
  if (icon == 1) { u.frame(ix, iy, q * 0.6f, q * 0.6f, ic, lw * 1.5f); u.rect(ix + q * 0.45f, iy + q * 0.45f, q * 0.5f, lw * 2, ic); u.rect(ix + q * 0.8f, iy + q * 0.45f, lw * 2, q * 0.3f, ic); }
  else if (icon == 2) { u.frame(ix, iy, q, q, ic, lw); if (enabled) { u.rect(ix + q * 0.2f, iy + q * 0.35f, q * 0.6f, q * 0.5f, ic); for (int f = 0; f < 4; f++) u.rect(ix + q * (0.2f + f * 0.16f), iy + q * 0.12f, q * 0.1f, q * 0.25f, ic); } }
  else if (icon == 3) u.rect(ix, iy, q, q, ic);
  else if (icon == 4) { u.frame(ix, iy, q, q, ic, lw); for (int k2 = 0; k2 < 5; k2++) u.rect(ix + q * (0.2f + 0.12f * k2), iy + q * (0.15f + 0.15f * (k2 % 3)), lw * 1.5f, q * 0.25f, ic); }
  else if (icon == 5) {  // three slanted claw marks
    for (int k = 0; k < 3; k++) for (int i = 0; i < 6; i++) u.rect(ix + q * (0.08f + 0.24f * k + 0.055f * i), iy + q * (0.08f + 0.14f * i), q * 0.11f, q * 0.17f, ic);
  } else if (icon == 6) {  // reach: target rings + arrow tips
    u.frame(ix + q * 0.05f, iy + q * 0.05f, q * 0.9f, q * 0.9f, ic, lw * 1.5f); u.frame(ix + q * 0.27f, iy + q * 0.27f, q * 0.46f, q * 0.46f, ic, lw * 1.5f);
    u.rect(ix + q * 0.43f, iy + q * 0.43f, q * 0.14f, q * 0.14f, ic);
    u.rect(ix + q * 0.46f, iy - q * 0.04f, q * 0.08f, q * 0.16f, ic); u.rect(ix + q * 0.46f, iy + q * 0.88f, q * 0.08f, q * 0.16f, ic);
  } else if (icon == 7) {  // speed boot with motion lines
    u.rect(ix + q * 0.38f, iy + q * 0.12f, q * 0.3f, q * 0.52f, ic); u.rect(ix + q * 0.38f, iy + q * 0.58f, q * 0.58f, q * 0.24f, ic);
    u.rect(ix + q * 0.34f, iy + q * 0.82f, q * 0.64f, q * 0.08f, dimmed(ic, 0.6f));
    for (int k = 0; k < 3; k++) u.rect(ix + q * (0.02f + 0.06f * k), iy + q * (0.3f + 0.2f * k), q * (0.26f - 0.06f * k), lw * 1.5f, ic);
  } else if (icon == 8) {  // egg (row-stacked ellipse, wider at the bottom) with spots
    const int R = 12; float cx = ix + q * 0.5f;
    for (int i = 0; i < R; i++) { float t = (i + 0.5f) / R, d = t < 0.58f ? (t - 0.58f) / 0.58f : (t - 0.58f) / 0.42f; float w = q * 0.72f * std::sqrt(std::max(0.f, 1 - d * d)); u.rect(cx - w / 2, iy + i * q / R, w, q / R + 0.5f, ic); }
    Color sp = dimmed(ic, 0.55f); u.rect(cx - q * 0.16f, iy + q * 0.42f, q * 0.12f, q * 0.1f, sp); u.rect(cx + q * 0.08f, iy + q * 0.62f, q * 0.14f, q * 0.11f, sp); u.rect(cx - q * 0.04f, iy + q * 0.22f, q * 0.08f, q * 0.08f, sp);
  } else if (icon == 9) {  // ray gun + zig-zag beam
    u.rect(ix + q * 0.05f, iy + q * 0.36f, q * 0.48f, q * 0.24f, ic); u.rect(ix + q * 0.12f, iy + q * 0.58f, q * 0.14f, q * 0.3f, ic);
    u.rect(ix + q * 0.53f, iy + q * 0.42f, q * 0.14f, q * 0.12f, ic); u.rect(ix + q * 0.15f, iy + q * 0.28f, q * 0.24f, q * 0.08f, dimmed(ic, 0.7f));
    for (int k = 0; k < 4; k++) u.rect(ix + q * (0.7f + 0.07f * k), iy + q * (k % 2 ? 0.34f : 0.5f), q * 0.08f, q * 0.14f, CYAN);
  } else if (icon == 10) {  // back arrow
    for (int i = 0; i < 5; i++) { float o = std::fabs(i - 2.f); u.rect(ix + q * (0.12f + 0.11f * o), iy + q * (0.24f + 0.11f * i), q * 0.14f, q * 0.12f, ic); }
    u.rect(ix + q * 0.24f, iy + q * 0.46f, q * 0.62f, q * 0.1f, ic);
  }
}
void drawCoin(UIDraw& u, float x, float y, float d, Color c) {  // small round coin (stacked rows)
  const int R = 6;
  for (int i = 0; i < R; i++) { float t = (i + 0.5f) / R * 2 - 1, w = d * std::sqrt(std::max(0.f, 1 - t * t)); u.rect(x + (d - w) / 2, y + i * d / R, w, d / R + 0.5f, c); }
  u.rect(x + d * 0.42f, y + d * 0.25f, d * 0.16f, d * 0.5f, dimmed(c, 0.6f));
}
std::string fmt1(float v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); return b; }
static uint32_t rb6(int i) { static const uint32_t c[] = {0xff9a1f, 0xffe03a, 0x4cd964, 0x3ad7ff, 0x5a6bff, 0xb06bff}; return c[((i % 6) + 6) % 6]; }
std::string mmss(float t) { int s = std::max(0, (int)std::ceil(t)); char b[16]; std::snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60); return b; }
const char* planName(BodyPlan p) {
  switch (p) { case BodyPlan::Biped: return "Biped"; case BodyPlan::Quad: return "Four-legged"; case BodyPlan::Blob: return "Blob"; case BodyPlan::Crab: return "Crab";
               case BodyPlan::Bird: return "Bird"; case BodyPlan::Robot: return "Robot"; case BodyPlan::Serpent: return "Serpent"; }
  return "?";
}
}  // namespace
void drawIconPublic(UIDraw& u, int icon, float ix, float iy, float q, Color ic) { drawIcon(u, icon, ix, iy, q, ic, true); }  // Vault

// Vault: open-animation progress per top menu (keyed by depth + title so rebuilt menus don't re-animate)
float Game::menuAnim(const Menu& m) {
  std::string key = std::to_string(menus.size()) + "|" + m.title;
  if (key != menuAnimKey) { menuAnimKey = key; menuAnimStart = realTime; }
  return style::panelAnim(realTime - menuAnimStart, save.settings.reducedMotion);
}
void Game::pushMenu(Menu m) { menus.push_back(std::move(m)); play(SND_ui, 0.6f); }
void Game::popMenu() { if (!menus.empty()) menus.pop_back(); }
void Game::prompt(const std::string& title, const std::string& body, std::vector<MenuItem> items, bool noBack) {
  Menu m; m.title = title; m.body = body; m.items = std::move(items); m.noBack = noBack; m.kind = 4; pushMenu(m);
}

void Game::openMainMenu() {
  if (!menus.empty() && menus.back().kind == 2) return;
  Menu m; m.kind = 2; m.noBack = true; m.title = "TOMMYZILLA CITY SMASH";
  m.build = [this](Menu& mm) {
    mm.body = "Stomp around a cartoon Victoria, BC! Buildings rebuild brick by brick, people pop into rainbow paint, and nobody really gets hurt.";
    if (slot >= 0) mm.body += "\nPlaying in save slot " + std::to_string(slot + 1) + (save.name.empty() ? "" : " (" + save.name + ")") + ".";
    MenuItem sb; sb.label = "Sandbox"; sb.desc = "Free smashing with your own coins, keys and upgrades. Starts with a short tutorial (rainbow + building challenge). Unlock moves, skill chests and Ruth King Elementary.";
    if (!save.tutorial.done) sb.right = "Tutorial";
    sb.act = [this]() { startMode(false); };
    mm.items.push_back(sb);
    MenuItem adv; adv.label = "Adventure";
    if (save.adventureUnlocked) {
      const Profile& p = save.adventure;
      bool fresh = p.campaign.empty() || p.level >= (int)T.levels.size();
      adv.right = fresh ? "New campaign" : "Level " + std::to_string(p.level + 1) + "/" + std::to_string((int)T.levels.size());
      adv.desc = std::to_string((int)T.levels.size()) + " levels against the army, ending with the boss THE BULK. Separate coins, keys and skills. Hatch a mystery egg after every level!";
      adv.act = [this]() { startMode(true); };
    } else {
      adv.enabled = false; adv.right = "LOCKED";
      adv.desc = "Finish the sandbox goals to unlock:";
      for (auto& g : prog::adventureGoals(save)) adv.desc += std::string("\n ") + (g.done ? "[x] " : "[ ] ") + g.label;
    }
    mm.items.push_back(adv);
    MenuItem cz; cz.label = "Crazy";
    if (save.crazyUnlocked) {
      const Profile& p = save.crazy;
      bool fresh = p.campaign.empty() || p.level >= (int)T.levels.size();
      cz.right = fresh ? "New campaign" : "Level " + std::to_string(p.level + 1) + "/" + std::to_string((int)T.levels.size());
      cz.desc = "CRAZY difficulty: all " + std::to_string((int)T.levels.size()) + " levels of the same campaign, about twice as hard (enemy HP and damage x" + fmt1(C.crazyEnemyDamage) + ", THE BULK x" + fmt1(C.crazyBossHp) + " HP). Play as ANY hero kaiju. Its own coins, keys and skills.";
      cz.act = [this]() {
        bool first = save.crazy.campaign.empty();
        startMode(true, true);
        if (first) { openCharacterSelect(true); toast("CRAZY mode: pick any hero kaiju!"); }
      };
    } else { cz.enabled = false; cz.right = "LOCKED"; cz.desc = "Beat THE BULK on level " + std::to_string((int)T.levels.size()) + " of Adventure to unlock CRAZY difficulty."; }
    mm.items.push_back(cz);
    MenuItem ch; ch.label = "Characters"; ch.right = std::to_string(save.monsters.size()) + "/" + std::to_string(T.monsters.size());
    ch.desc = "Pick any monster you have hatched. Rarer kaiju start bigger and tougher, and each one has its own colours."; ch.act = [this]() { openCharacterSelect(false); };
    mm.items.push_back(ch);
    MenuItem op; op.label = "Options"; op.desc = "Sound, controls and save data"; op.act = [this]() { openOptions(); }; mm.items.push_back(op);
    if (slots) { MenuItem sl; sl.label = "Switch player / save slot"; sl.right = "Slot " + std::to_string(slot + 1); sl.desc = "Three save slots - one for each player"; sl.act = [this]() { persist(); openSlotMenu(); }; mm.items.push_back(sl); }
    {  // Vault: difficulty + player count (Titan's DiffMods); Left/Right or Confirm cycles
      MenuItem df; df.label = "Difficulty"; df.right = difficultyName(difficulty);
      df.desc = "Easy is the normal game. Hard and You're Insane give enemies and bosses more health and hits.";
      df.adjust = [this](int d) { difficulty = save.settings.difficulty = (difficulty + d + 3) % 3; persist(); }; df.act = [this]() { difficulty = save.settings.difficulty = (difficulty + 1) % 3; persist(); };  // Wren: write the saved setting Titan's startMode reads
      mm.items.push_back(df);
      MenuItem pl; pl.label = "Players"; pl.right = players == 2 ? "2 PLAYERS" : "1 PLAYER";
      pl.desc = "Play together on one screen with a second controller.";
      pl.adjust = [this](int) { players = save.settings.players = players == 2 ? 1 : 2; persist(); }; pl.act = [this]() { players = save.settings.players = players == 2 ? 1 : 2; persist(); };
      mm.items.push_back(pl);
    }
    MenuItem q; q.label = "Quit"; q.desc = "Your progress is saved automatically"; q.act = [this]() { persist(); quit = true; }; mm.items.push_back(q);
  };
  pushMenu(m);
}

void Game::openSlotMenu() {
  Menu m; m.kind = 6; m.noBack = slot < 0; m.title = "WHO'S PLAYING?";
  m.build = [this](Menu& mm) {
    mm.body = "Pick a save slot. Each slot keeps its own Sandbox, Adventure and Crazy progress.";
    for (int i = 0; i < SAVE_SLOTS; i++) {
      std::string text; bool have = slots && slots->read(i, text);
      SlotSummary sum = summarizeSlot(i, have ? &text : nullptr);
      MenuItem it; it.label = sum.title + (i == slot ? "  (current)" : ""); it.right = sum.line; it.desc = sum.detail;
      if (sum.hasAccent) { it.icon = 3; it.iconColor = sum.accent; }  // the slot's kaiju colour swatch
      it.act = [this, i]() { if (slot >= 0 && slot != i) persist(); selectSlot(i); };
      mm.items.push_back(it);
    }
    MenuItem del; del.label = "Delete a save slot..."; del.desc = "Erase a slot so a new player can start fresh (asks first)";
    del.act = [this]() {
      Menu d; d.title = "DELETE WHICH SLOT?";
      for (int i = 0; i < SAVE_SLOTS; i++) {
        MenuItem it; it.label = "Slot " + std::to_string(i + 1); bool have = slots && slots->exists(i); it.right = have ? "in use" : "empty"; it.enabled = have;
        it.act = [this, i]() {
          MenuItem no; no.label = "No, keep it"; no.act = [this]() { popMenu(); };
          MenuItem yes; yes.label = "Yes, delete slot " + std::to_string(i + 1); yes.act = [this, i]() { deleteSlot(i); toast("Slot " + std::to_string(i + 1) + " deleted"); menus.clear(); mode = Mode::Title; openSlotMenu(); };
          prompt("Delete save slot " + std::to_string(i + 1) + "?", "All of its Sandbox, Adventure and Crazy progress will be erased. This cannot be undone.", {no, yes}, false);
        };
        d.items.push_back(it);
      }
      MenuItem b; b.label = "Back"; b.act = [this]() { popMenu(); }; d.items.push_back(b);
      pushMenu(d);
    };
    mm.items.push_back(del);
    MenuItem q; q.label = "Quit"; q.act = [this]() { persist(); quit = true; }; mm.items.push_back(q);
  };
  if (slot >= 0) m.onBack = [this]() { popMenu(); };
  pushMenu(m);
}

void Game::openSkills() {
  Menu m; m.title = "SKILLS"; m.kind = 7;
  m.build = [this](Menu& mm) {
    Profile& p = P();
    auto nm = [&](int k) { int mi = T.moveIndex(p.slots[k]); return mi >= 0 ? T.moves[mi].name : std::string("(empty)"); };
    mm.body = "Slot 1 (close range) [" + btn("RT", "F") + "]: " + nm(0) + "\nSlot 2 (ranged / super) [" + btn("LT", "Q") + "]: " + nm(1) +
              "\nPick a skill to put it in its slot. Quick-swap in play with " + btn("D-pad left / right", "1 / 2") + ". Stomp and " + btn("Y", "E") + " (eat / open chests) are always on. The game is paused while this list is open.";
    for (int k = 0; k < 2; k++) {
      MenuItem h; h.label = k == 0 ? "-- SLOT 1: CLOSE RANGE --" : "-- SLOT 2: RANGED / SUPER --"; h.enabled = false; mm.items.push_back(h);
      for (auto& id : prog::slotChoices(p, T, k)) {
        const MoveDef& mv = T.moves[T.moveIndex(id)];
        MenuItem it; it.label = "  " + mv.name; it.right = p.slots[k] == id ? "EQUIPPED" : "";
        it.desc = mv.desc + (mv.glow ? " (glow - hatches eggs)" : "") + (mv.rapid ? " (rapid fire)" : "") + (mv.chargeTime > 0 ? " (hold to charge)" : "");
        if (mv.kind == MoveKind::Super) { it.icon = 11; it.iconColor = GOLD; it.label = "  SUPER: " + mv.name; }  // Vault: gold supers
        it.act = [this, id, k]() { if (prog::equip(P(), id, k)) { toast(T.moves[T.moveIndex(id)].name + " in slot " + std::to_string(k + 1)); play(SND_ui); persist(); } };
        mm.items.push_back(it);
      }
    }
    for (auto& id : p.chests) { int mi = T.moveIndex(id); if (mi < 0) continue; MenuItem it; it.label = "  " + T.moves[mi].name; it.right = "IN A LOCKED CHEST"; it.enabled = false; mm.items.push_back(it); }
    if (p.gauntletSlot || p.gauntletCracked) {  // the gauntlet slot: an empty outline until L13 (no text)
      MenuItem g;
      if (p.gauntlet) {
        g.label = "  The Bulk's Gauntlet"; g.right = p.gauntletOn ? "EQUIPPED" : "off"; g.icon = 2; g.iconColor = hexColor(0xffcc33);
        g.desc = "Holds the six stones. Stones: " + std::to_string(stoneCount()) + "/6. Each stone makes its pulse flatten more buildings (up to 10). Open Infinity Stones to assign each stone's powers to Pulse, Strike, Blast and Roar." + (adventure ? "" : " Press " + btn("RB", "R") + " for the rainbow wave!");
        g.act = [this]() { equipGauntlet(!P().gauntletOn); play(SND_ui); persist(); };
      } else if (p.gauntletCracked) { g.label = "  "; g.icon = 4; g.iconColor = Color{0.45f, 0.42f, 0.5f, 1}; g.enabled = false; }
      else { g.label = "  "; g.icon = 2; g.iconColor = Color{0.4f, 0.4f, 0.48f, 1}; g.enabled = false; }
      mm.items.push_back(g);
      if (p.gauntlet || p.gauntletSlot) {
        MenuItem inf; inf.label = "Infinity Stones"; inf.icon = 3; inf.iconColor = hexColor(0xb06bff);
        inf.right = std::to_string(stoneCount()) + "/6";
        inf.enabled = p.gauntlet;
        inf.desc = p.gauntlet
          ? "Choose a stone and assign its powers to different actions (Pulse, Strike, Blast, Roar). Each power explains itself. " + stoneBindSummary()
          : "The gauntlet slot is empty. The stones show up once you have the gauntlet.";
        inf.act = [this]() { openStoneMenu(); };
        mm.items.push_back(inf);
      }
    }
    if (p.zooKey) {  // collection: only ever listed once owned
      MenuItem k; k.label = "  Zookeeper's Key"; k.right = "collection"; k.icon = 1; k.iconColor = hexColor(0xffcc33); k.enabled = false;
      k.desc = "A jingly key ring from the Kaiju Zoo. It smells like peanuts.";
      mm.items.push_back(k);
    }
    MenuItem st; st.label = "Store (keys + upgrades)"; st.right = std::to_string(p.keys) + " keys"; st.act = [this]() { openStore(); }; mm.items.push_back(st);
    MenuItem b; b.label = "Back"; b.act = [this]() { popMenu(); }; mm.items.push_back(b);
  };
  m.onBack = [this]() { popMenu(); };
  m.index = 1;  // start on the first real choice, not the "-- SLOT 1 --" header
  pushMenu(m);
}

void Game::openPause() {
  Menu m; m.kind = 1; m.title = "PAUSED";
  m.build = [this](Menu& mm) {
    Profile& p = P();
    mm.body = std::string(adventure ? (crazy ? "CRAZY" : "Adventure") : "Sandbox") + "  |  Coins: " + std::to_string(p.coins) + "  |  Keys: " + std::to_string(p.keys) + "  |  Smashed: " + std::to_string(p.smashCount) + "  |  " + kaiju.def().name;
    auto item = [&](const std::string& l, const std::string& d, std::function<void()> a, const std::string& r = "") { MenuItem it; it.label = l; it.desc = d; it.act = std::move(a); it.right = r; mm.items.push_back(it); };
    item("Resume", "Back to smashing", [this]() { popMenu(); });
    if (adventure && level.won && !egg.active && AP().level < (int)T.levels.size()) {
      const LevelDef& L = T.levels[AP().level];
      item("Next Level", "Level " + std::to_string(AP().level + 1) + ": " + L.name + " - " + L.desc, [this]() { popMenu(); startLevel(AP().level); }, ">>");
    }
    if (adventure && (level.active || level.failed)) item("Retry level", "Restart " + T.levels[level.idx].name + " with a fresh city", [this]() { popMenu(); startLevel(level.idx); });
    if (adventure && level.won && egg.active) item("Hatch the egg first!", "Hit the mystery egg with a glow move (Glow Breath, Plate-Glow Beam, Sparkle Spit...)", [this]() { popMenu(); });
    item("Store", "Spend coins on upgrades", [this]() { openStore(); }, std::to_string(p.coins) + " coins");
    if (P().spins > 0 && deevoT < 0 && !zoo.active) item("Egg Spinner", "Spin for a new kaiju or a duplicate bonus (coins + XP). Win levels, open chests or smash buildings to earn spins.",
                             [this]() { menus.clear(); beginSpin(SPIN_TOKEN); }, std::to_string(P().spins) + " spins");
    item("Skills", "Choose which owned skills sit in your 2 active slots (also Tab / D-pad down)", [this]() { openSkills(); });
    item("Melee loadout", "Choose which melee moves are on X / B / A and LB + X / B / A", [this]() { openMelee(); });
    item("Move list", "All " + std::to_string(T.moves.size()) + " moves and how to unlock them", [this]() { openMoveList(); });
    item("Characters", "Switch monster", [this]() { openCharacterSelect(true); });
    if (!adventure) {
      MenuItem pol; pol.label = "Police"; pol.right = save.settings.police ? "ON" : "OFF";  // sandbox only; default OFF (Settings::police)
      pol.desc = "Sandbox police (default OFF). ON: wanted stars bring cars, a helicopter, tanks and a nuke countdown (pay " + std::to_string((int)C.nukeCancelCost) + " coins to cancel). OFF: no police, no sirens, no wanted stars.";
      pol.act = [this]() { save.settings.police = !save.settings.police; if (!save.settings.police) enemies.reset(); toast(save.settings.police ? "Police ON" : "Police OFF"); persist(); };
      mm.items.push_back(pol);
    }
    item("Controls", "Every controller button and keyboard key (also LB+Start / H in play)", [this]() { openControls(); });
    item("Options", "Sound, controls and save data", [this]() { openOptions(); });
    item("Main menu", "Progress is saved", [this]() { stopLevel(); persist(); menus.clear(); mode = Mode::Title; openMainMenu(); });
  };
  m.onBack = [this]() { popMenu(); };
  pushMenu(m);
}

void Game::openStore() {  // icon grid: 4 tiles per row; every purchase calls the same code (and prices) as the old list
  Menu m; m.title = "STORE"; m.kind = 8; m.cols = 4; m.wide = true;
  m.build = [this](Menu& mm) {
    Profile& p = P();
    mm.body = "Coins: " + std::to_string(p.coins) + "   Keys: " + std::to_string(p.keys) + "   (melee hits and rainbows earn coins)";
    struct U { prog::Upgrade u; const char* name; int lvl; std::string d; int icon; uint32_t col; };
    U ups[3] = {{prog::UP_DAMAGE, "Mighty Claws", p.dmgLvl, "+" + std::to_string((int)(C.damagePerLevel * 100)) + "% bricks per hit for every move", 5, 0xff8a5a},
                {prog::UP_RANGE, "Big Reach", p.rangeLvl, "+" + std::to_string((int)(C.rangePerLevel * 100)) + "% attack radius", 6, 0x3ad7ff},
                {prog::UP_SPEED, "Stompy Speed", p.speedLvl, "Walk speed x" + fmt1(prog::speedMult(p.speedLvl)) + " (5 steps, max x" + fmt1(prog::speedMult(prog::maxLevel())) + ")", 7, 0x7aff6a}};
    for (auto& u : ups) {
      MenuItem it; int cost = prog::upgradeCost(u.lvl);
      it.label = u.name; it.sub = "Lv " + std::to_string(u.lvl) + "/" + std::to_string(prog::maxLevel());
      it.right = cost < 0 ? "MAX" : std::to_string(cost) + " coins"; it.desc = u.d;
      it.price = cost; it.owned = cost < 0; it.afford = cost >= 0 && p.coins >= cost; it.icon = u.icon; it.iconColor = hexColor(u.col);
      prog::Upgrade up = u.u; std::string nm = u.name;
      it.act = [this, up, nm]() {
        int r = prog::buyUpgrade(P(), up);
        if (r == 0) { play(SND_coin); toast(nm + " upgraded!"); checkAdventureGate(); persist(); }
        else if (r == 1) { play(SND_error); toast("Need more coins - smash more with melee moves!"); }
        else play(SND_error);
      };
      mm.items.push_back(it);
    }
    addKaijuPowerTiles(mm);  // Vault: 5 kaiju-specific powers after the 3 basic upgrades
    MenuItem key; key.label = "Chest key"; key.sub = "have " + std::to_string(p.keys); key.right = std::to_string((int)C.keyCost) + " coins";
    key.desc = "Opens one locked skill chest. Walk into the chest or press " + btn("Y", "E") + " next to it.";
    key.price = (int)C.keyCost; key.afford = p.coins >= key.price; key.icon = 1; key.iconColor = hexColor(0xffcc33);
    key.act = [this]() { buyKey(); };
    mm.items.push_back(key);
    MenuItem sp; sp.label = "Egg spin"; sp.sub = "have " + std::to_string(p.spins); sp.right = std::to_string(E.spinCost) + " coins";
    sp.desc = "One spin of the egg spinner. New kaiju join your team; a duplicate gives coins + XP.";
    sp.price = E.spinCost; sp.afford = p.coins >= sp.price; sp.icon = 8; sp.iconColor = hexColor(0xf3ead2);
    sp.act = [this]() { Profile& pp = P(); if (pp.coins < E.spinCost) { play(SND_error); toast("Need more coins!"); return; } pp.coins -= E.spinCost; pp.spins++; play(SND_coin); toast("+1 egg spin! Use it from the pause menu."); persist(); };
    mm.items.push_back(sp);
    MenuItem dv; dv.label = "De-Evolution Ray"; dv.right = std::to_string(E.deevoPrice) + " coins";
    std::string why; bool ok = canDeevo(&why);
    dv.desc = "Zap your kaiju back into an egg and spin a brand new one! You keep your levels, skills, keys, chests and other coins." + (ok ? std::string() : "\n" + why);
    dv.sub = ok ? "" : "not yet"; dv.price = E.deevoPrice; dv.afford = p.coins >= dv.price; dv.icon = 9; dv.iconColor = hexColor(0xb06bff);
    dv.enabled = true; dv.act = [this]() { std::string w; if (!canDeevo(&w)) { play(SND_error); toast(w); return; } openDeevoConfirm(); };
    mm.items.push_back(dv);
    MenuItem b; b.label = "Back"; b.desc = "Close the store (" + btn("B", "Esc") + ")"; b.icon = 10; b.iconColor = STEEL; b.act = [this]() { popMenu(); }; mm.items.push_back(b);
  };
  pushMenu(m);
}

void Game::openEquip() { openSkills(); }

void Game::openMelee() {
  Menu m; m.title = "MELEE LOADOUT";
  m.build = [this](Menu& mm) {
    Profile& p = P();
    mm.body = "Melee moves never use a power slot. Hold LB (or use U / I / O) for the second set.";
    static const char* names[MELEE_BINDS] = {"X  /  J or left-click", "B  /  K or right-click", "A  /  Space or L", "LB+X  /  U", "LB+B  /  I", "LB+A  /  O"};
    for (int b = 0; b < MELEE_BINDS; b++) {
      MenuItem it; it.label = names[b]; int cur = T.moveIndex(p.melee[b]); it.right = cur >= 0 ? T.moves[cur].name : "(empty)";
      it.act = [this, b]() {
        Menu s; s.title = "Choose a melee move";
        for (auto& mv : T.moves) {
          if (mv.kind != MoveKind::Melee || !P().moves.count(mv.id)) continue;
          MenuItem si; si.label = mv.name; si.desc = mv.desc; std::string id = mv.id;
          si.act = [this, b, id]() { Profile& pp = P(); for (auto& x : pp.melee) if (x == id) x.clear(); pp.melee[b] = id; persist(); popMenu(); };
          s.items.push_back(si);
        }
        MenuItem e; e.label = "(empty)"; e.act = [this, b]() { P().melee[b].clear(); persist(); popMenu(); }; s.items.push_back(e);
        pushMenu(s);
      };
      mm.items.push_back(it);
    }
    MenuItem bk; bk.label = "Back"; bk.act = [this]() { popMenu(); }; mm.items.push_back(bk);
  };
  pushMenu(m);
}

void Game::openMoveList() {
  Menu m; m.title = "MOVE LIST"; m.wide = true;
  m.build = [this](Menu& mm) {
    Profile& p = P(); int owned = 0; for (auto& mv : T.moves) if (p.moves.count(mv.id)) owned++;
    mm.body = std::to_string(owned) + "/" + std::to_string(T.moves.size()) + " unlocked in " + (adventure ? (crazy ? "Crazy" : "Adventure") : "Sandbox") + " (" + std::to_string(p.smashCount) + " buildings smashed)";
    const MoveKind kinds[3] = {MoveKind::Melee, MoveKind::Direct, MoveKind::Super};
    const char* heads[3] = {"-- MELEE (X/B/A buttons; can also go in slot 1) --", "-- DIRECT POWERS (slot 1 close / slot 2 ranged) --", "-- SUPER AOE (slot 2) --"};
    for (int k = 0; k < 3; k++) {
      MenuItem h; h.label = heads[k]; h.enabled = false; mm.items.push_back(h);
      for (auto& mv : T.moves) {
        if (mv.kind != kinds[k]) continue;
        MenuItem it; bool has = p.moves.count(mv.id) > 0;
        it.label = "  " + mv.name; it.right = has ? "unlocked" : "locked";
        it.desc = mv.desc + (mv.glow ? " (glow)" : "") + "\nUnlock: " + mv.unlockText();
        if (mv.unlock == UnlockType::Hidden && !has) { it.label = "  ???"; it.right = "secret"; it.desc = "A secret... nobody knows where it is."; }
        it.act = []() {};
        mm.items.push_back(it);
      }
    }
    MenuItem b; b.label = "Back"; b.act = [this]() { popMenu(); }; mm.items.push_back(b);
  };
  pushMenu(m);
}

void Game::openCharacterSelect(bool fromPause) {
  Menu m; m.title = "CHARACTERS"; m.kind = 3;
  m.index = std::max(0, T.monsterIndex(save.monster));
  m.build = [this, fromPause](Menu& mm) {
    mm.body = std::to_string(save.monsters.size()) + "/" + std::to_string(T.monsters.size()) + " monsters. Hatch more in Adventure mode!";
    for (int i = 0; i < (int)T.monsters.size(); i++) {
      const MonsterDef& md = T.monsters[i]; bool has = crazy || save.monsters.count(md.id) > 0;  // Crazy: every hero is playable
      MenuItem it; it.label = has ? md.name : "???"; it.right = md.id == save.monster ? "PLAYING" : (has ? "" : "locked");
      if (has && hasLook(md.id)) { KaijuLook lk = lookOf(md.id); const RarityDef& rd = E.rarity(lk.rarity);
        it.right = (md.id == save.monster ? "PLAYING  " : "") + rd.name + (lk.finish ? std::string(" ") + E.finishes[lk.finish - 1].name : std::string()); }
      it.desc = has ? md.desc + "\n" + planName(md.plan) + "  speed x" + fmt1(md.speed) + "  HP x" + fmt1(md.hp) + "  melee x" + fmt1(md.melee) + "  power x" + fmt1(md.power)
                    : std::string("A mystery monster. Hatch it from an egg in Adventure mode.");
      it.enabled = has;
      it.act = [this, i, fromPause]() { selectMonster(i); if (fromPause) popMenu(); };
      mm.items.push_back(it);
    }
    MenuItem b; b.label = "Back"; b.act = [this]() { popMenu(); }; mm.items.push_back(b);
  };
  pushMenu(m);
}

void Game::openOptions() {
  Menu m; m.title = "OPTIONS";
  m.build = [this](Menu& mm) {
    Settings& s = save.settings;
    { MenuItem ch; ch.label = "Cheats"; ch.right = vault.cheatsUnlocked ? "" : "LOCKED"; ch.desc = vault.cheatsUnlocked ? "Silly cheats for fun." : "Finish Crazy mode to unlock.";
      ch.enabled = vault.cheatsUnlocked; ch.act = [this]() { openCheats(); }; mm.items.push_back(ch); }  // Vault
    MenuItem snd; snd.label = "Sound"; snd.right = s.muted ? "OFF" : "ON"; snd.desc = "Toggle with M at any time"; snd.act = [this]() { save.settings.muted = !save.settings.muted; persist(); };
    mm.items.push_back(snd);
    MenuItem vol; vol.label = "Volume"; vol.right = std::to_string((int)std::lround(s.volume * 100)) + "%"; vol.desc = "Left / right to change";
    vol.adjust = [this](int d) { save.settings.volume = clampf(save.settings.volume + d * 0.1f, 0, 1); persist(); };
    vol.act = [this]() { save.settings.volume = save.settings.volume >= 0.99f ? 0.f : clampf(save.settings.volume + 0.1f, 0, 1); persist(); };
    mm.items.push_back(vol);
    MenuItem ms; ms.label = "Mouse sensitivity"; ms.right = "x" + fmt1(s.mouseSens); ms.desc = "Left / right to change";
    ms.adjust = [this](int d) { save.settings.mouseSens = clampf(save.settings.mouseSens + d * 0.1f, 0.2f, 3.f); persist(); };
    ms.act = [this]() { save.settings.mouseSens = save.settings.mouseSens >= 2.95f ? 0.2f : save.settings.mouseSens + 0.2f; persist(); };
    mm.items.push_back(ms);
    MenuItem inv; inv.label = "Invert look Y"; inv.right = s.invertY ? "ON" : "OFF"; inv.act = [this]() { save.settings.invertY = !save.settings.invertY; persist(); };
    mm.items.push_back(inv);
    { MenuItem rmI; rmI.label = "Reduced motion"; rmI.right = s.reducedMotion ? "ON" : "OFF"; rmI.desc = "Turns off menu slide and button pop animations.";
      rmI.act = [this]() { save.settings.reducedMotion = !save.settings.reducedMotion; persist(); }; mm.items.push_back(rmI); }
    MenuItem ct; ct.label = "Controls"; ct.desc = "Controller and keyboard / mouse layout"; ct.act = [this]() { openControls(); }; mm.items.push_back(ct);
    MenuItem rs; rs.label = "Reset save"; rs.desc = "Erase all progress (both modes, monsters, settings)";
    rs.act = [this]() {
      MenuItem yes; yes.label = "Yes, erase everything"; yes.act = [this]() {
        save.reset(); prog::initProfile(save.sandbox, T); prog::initProfile(save.adventure, T); prog::initProfile(save.crazy, T); persist();
        adventure = false; stopLevel(); enemies.reset(); world.restoreAll(); egg = Egg(); kaiju.monster = 0; kaiju.respawn(-55, 135, PI);
        menus.clear(); mode = Mode::Title; openMainMenu(); toast("Save data reset");
      };
      MenuItem no; no.label = "No, keep my progress"; no.act = [this]() { popMenu(); };
      prompt("Reset all progress?", "This cannot be undone.", {no, yes}, false);
    };
    mm.items.push_back(rs);
    MenuItem cr; cr.label = "Credits"; cr.desc = "Who made the game and the free 3D models it uses";
    cr.act = [this]() {
      MenuItem ok; ok.label = "Back"; ok.act = [this]() { popMenu(); };
      prompt("CREDITS", std::string(kGameCredits) + "\n" + kModelCredits, {ok});
    };
    mm.items.push_back(cr);
    MenuItem b; b.label = "Back"; b.act = [this]() { popMenu(); }; mm.items.push_back(b);
  };
  pushMenu(m);
}

void Game::openControls() {  // one row per action from the shared controls table (src/game/controls.h)
  Menu m; m.title = "CONTROLS"; m.wide = true; m.kind = 9;
  m.body = "Controller and keyboard side by side. Up / down to scroll, " + btn("B", "Esc") + " to close.";
  for (const ControlBind& c : controlBinds()) {
    MenuItem it; it.label = c.name; it.right = c.pad; it.right2 = c.key; it.desc = c.note; it.act = []() {};
    m.items.push_back(it);
  }
  MenuItem b; b.label = "Back"; b.desc = "Close the Controls screen"; b.act = [this]() { popMenu(); }; m.items.push_back(b);
  pushMenu(m);
}

// ---------------------------------------------------------------- menu input
void Game::updateMenus(const InputState& in, float dt) {
  menuPressT = std::max(0.f, menuPressT - dt);
  if (menus.empty()) return;
  if (menus.back().kind == 10) { updatePowerSpin(dt); if (pspin.active) return; }  // Vault: no input mid-spin
  Menu& m = menus.back();
  if (m.build) { m.items.clear(); m.build(m); }
  int n = (int)m.items.size(); if (n == 0) { if (in.pressed[A_CANCEL] && !m.noBack) popMenu(); return; }
  m.index = std::max(0, std::min(n - 1, m.index));
  if (m.kind == 3) previewMonster = std::min(m.index, (int)T.monsters.size() - 1);
  bool clicked = false;
  const MenuLayout& L = menuLayout;
  if (m.cols > 0) {  // tile grid (store): d-pad / stick / arrows move across the grid, the mouse picks a tile
    int dx = (in.pressed[A_RIGHT] ? 1 : 0) - (in.pressed[A_LEFT] ? 1 : 0), dy = (in.pressed[A_DOWN] ? 1 : 0) - (in.pressed[A_UP] ? 1 : 0);
    if (dx || dy) { m.index = gridMove(gridMove(m.index, n, m.cols, dx, 0), n, m.cols, 0, dy); play(SND_ui, 0.5f); }
    if (in.cursorX >= 0 && L.cols == m.cols && L.tw > 0 && in.cursorX >= L.x && in.cursorY >= L.y) {
      int col = (int)((in.cursorX - L.x) / (L.tw + L.gap)), row = (int)((in.cursorY - L.y) / L.h);
      float ox = in.cursorX - L.x - col * (L.tw + L.gap), oy = in.cursorY - L.y - row * L.h;
      int idx = row * m.cols + col;
      if (col < m.cols && row < L.count && ox <= L.tw && oy <= L.th && idx < n) {
        bool moved = std::fabs(in.cursorX - lastCurX) + std::fabs(in.cursorY - lastCurY) > 0.5f;
        if (moved || in.click) m.index = idx;
        if (in.click) clicked = true;
      }
    }
  } else {
    if (in.pressed[A_UP]) { m.index = (m.index + n - 1) % n; play(SND_ui, 0.5f); }
    if (in.pressed[A_DOWN]) { m.index = (m.index + 1) % n; play(SND_ui, 0.5f); }
  }
  if (m.cols <= 0 && in.cursorX >= 0 && L.h > 0 && in.cursorX >= L.x && in.cursorX <= L.x + L.w && in.cursorY >= L.y && in.cursorY < L.y + L.h * L.count) {
    int row = L.first + (int)((in.cursorY - L.y) / L.h);
    bool moved = std::fabs(in.cursorX - lastCurX) + std::fabs(in.cursorY - lastCurY) > 0.5f;
    if (row >= 0 && row < n && (moved || in.click)) m.index = row;
    if (in.click && row >= 0 && row < n) clicked = true;
  }
  if (in.cursorX >= 0) { lastCurX = in.cursorX; lastCurY = in.cursorY; }
  MenuItem& it = m.items[m.index];
  if (m.cols <= 0 && (in.pressed[A_LEFT] || in.pressed[A_RIGHT]) && it.adjust) { auto f = it.adjust; f(in.pressed[A_LEFT] ? -1 : 1); play(SND_ui, 0.5f); return; }
  if (in.pressed[A_CONFIRM] || clicked) {
    if (it.enabled && it.act) { auto f = it.act; play(SND_ui); menuPressT = style::kAnimSec * 0.75f; f(); }
    else play(SND_error, 0.6f);
    return;
  }
  bool back = in.pressed[A_CANCEL] || (m.kind == 1 && in.pressed[A_PAUSE]) || (m.kind == 7 && in.pressed[A_SKILLS]);
  if (back && !m.noBack) { auto ob = m.onBack; play(SND_back); if (ob) ob(); else popMenu(); }
}

void Game::drawMenus(RenderList& rl) {
  UIDraw u(rl); float s = u.s;
  menuLayout = MenuLayout();
  if (flashT > 0) u.rect(0, 0, u.W, u.H, Color{1, 1, 1, std::min(1.f, flashT)});
  if (menus.empty()) return;
  Menu& m = menus.back();
  if (m.build) { int idx = m.index; m.items.clear(); m.build(m); m.index = idx; }
  bool main = m.kind == 2, chars = m.kind == 3;
  if (!main && !chars) u.rect(0, 0, u.W, u.H, Color{0, 0, 0, 0.45f * menuAnim(m)});
  if (m.cols > 0) { drawGridMenu(u, m); return; }
  if (m.kind == 10) { drawPowerSpin(u, m); return; }  // Vault: power spinner
  float pw = chars ? std::min(u.W * 0.46f, 250 * s) : std::min(u.W - 30, (m.wide ? 330.f : 290.f) * s);
  float px = chars ? 16 * s / 2 : (u.W - pw) / 2;
  float titleS = main ? std::min(s * 2.0f, (pw - 20) / (8 * (float)m.title.size())) : s * 1.4f, bodyS = s * 0.75f, itemS = s * 0.95f;
  std::vector<std::string> body = u.wrap(m.body, pw - 16 * s / 2, bodyS);
  float itemH = u.lineH(itemS) + 5 * s;
  const MenuItem* sel = m.items.empty() ? nullptr : &m.items[std::max(0, std::min((int)m.items.size() - 1, m.index))];
  std::vector<std::string> desc = sel ? u.wrap(sel->desc, pw - 16 * s / 2, bodyS) : std::vector<std::string>();
  bool ctl = m.kind == 9; if (ctl) itemS = s * 0.8f;
  float headH = 8 * s + u.lineH(titleS) + 4 * s + body.size() * u.lineH(bodyS) + 6 * s + (ctl ? u.lineH(bodyS) + 2 * s : 0);
  itemH = u.lineH(itemS) + 5 * s;
  float descH = std::max<size_t>(desc.size(), main ? 4 : 2) * u.lineH(bodyS) + 10 * s;
  float maxPanel = u.H - 20 * s;
  int maxRows = std::max(3, (int)((maxPanel - headH - descH) / itemH));
  int rows = std::min((int)m.items.size(), maxRows);
  if (m.index < m.scroll) m.scroll = m.index;
  if (m.index >= m.scroll + rows) m.scroll = m.index - rows + 1;
  m.scroll = std::max(0, std::min(m.scroll, (int)m.items.size() - rows));
  float ph = headH + rows * itemH + descH;
  float py = main ? std::max(10 * s, u.H * 0.5f - ph * 0.5f + 20 * s) : std::max(10.f, (u.H - ph) / 2);
  float an = menuAnim(m), rm = save.settings.reducedMotion;  // Vault: 200ms slide-up + fade on open
  (void)rm; py += (1 - an) * 24 * s;
  style::panel(u, px, py, pw, ph, main);
  if (!m.noBack) style::closeX(u, px + pw - 22 * s, py + 6 * s, 16 * s, false);  // visible X (Esc/B also closes)
  float y = py + 8 * s;
  style::titleText(u, px + pw / 2, y, m.title, titleS, main ? style::cyan() : style::accent()); y += u.lineH(titleS) + 4 * s;
  for (auto& l : body) { u.text(px + 8 * s / 2 + 4, y, l, bodyS, GREY); y += u.lineH(bodyS); }
  y += 6 * s;
  float colPad = px + pw * 0.41f, colKey = px + pw * 0.665f;  // Controls screen columns
  if (ctl) { u.text(px + 10 * s / 2, y, "ACTION", bodyS, CYAN); u.text(colPad, y, "CONTROLLER", bodyS, CYAN); u.text(colKey, y, "KEYBOARD", bodyS, CYAN); y += u.lineH(bodyS) + 2 * s; }
  menuLayout = {px + 4, y, pw - 8, itemH, m.scroll, rows};
  for (int r = 0; r < rows; r++) {
    int i = m.scroll + r; const MenuItem& it = m.items[i]; bool on = i == m.index;
    if (!ctl) {  // Vault: big rounded buttons; first item on the main menu is the one primary (accent) action
      style::BtnState bs = !it.enabled ? style::BTN_DISABLED : on ? (menuPressT > 0 ? style::BTN_PRESSED : style::BTN_FOCUS) : style::BTN_NORMAL;
      style::BtnRole role = (main && i == 0) ? style::ROLE_PRIMARY : style::ROLE_SECONDARY;
      float sc = on ? style::pressScale(bs, style::kAnimSec * 0.75f - menuPressT, save.settings.reducedMotion) : 1.f;
      style::button(u, px + 6 * s, y + 1, pw - 12 * s, itemH - 2 * s, "", bs, role, sc, itemS);
    } else if (on) u.rect(px + 4, y, pw - 8, itemH - 2, Color{0.23f, 0.84f, 1, 0.22f});
    Color c = !it.enabled ? Color{0.55f, 0.52f, 0.62f, 1} : it.icon == 11 ? GOLD : on ? WHITE : Color{0.85f, 0.85f, 0.92f, 1};
    if (chars && i < (int)T.monsters.size() && it.enabled) { MonsterDef ld = lookedDef(i); float q = itemH * 0.4f; u.rect(px + 8, y + itemH * 0.25f, q, q, ld.body); u.rect(px + 8 + q * 0.5f, y + itemH * 0.25f, q * 0.5f, q, ld.accent); }
    if (it.icon) {  // little drawn icons (see drawIcon)
      float q = itemH * 0.55f, ix = px + pw - 8 * s / 2 - q - (it.right.empty() ? 0 : u.textW(it.right, itemS * 0.85f) + 6 * s), iy = y + (itemH - q) / 2 - 1;
      drawIcon(u, it.icon, ix, iy, q, it.iconColor, it.enabled);
    }
    u.text(px + 10 * s / 2 + (chars ? itemH * 0.5f : 0), y + 2.5f * s, (ctl ? (on ? "> " : "  ") : "  ") + it.label, itemS, c);
    if (ctl) { u.text(colPad, y + 2.5f * s, it.right, itemS, on ? GOLD : STEEL); u.text(colKey, y + 2.5f * s, it.right2, itemS, on ? GOLD : GREY); }
    else if (!it.right.empty()) u.textR(px + pw - 8 * s / 2, y + 2.5f * s, it.right, itemS * 0.85f, on ? GOLD : GREY);
    y += itemH;
  }
  if ((int)m.items.size() > rows) u.textR(px + pw - 6, py + 8 * s, std::to_string(m.index + 1) + "/" + std::to_string(m.items.size()), bodyS, GREY);
  y += 4 * s;
  u.rect(px + 8, y - 2 * s, pw - 16, std::max(1.f, s / 2), Color{0.35f, 0.3f, 0.5f, 1});
  for (auto& l : desc) { u.text(px + 8 * s / 2 + 4, y, l, bodyS, Color{0.9f, 0.88f, 0.75f, 1}); y += u.lineH(bodyS); }
  if (main) u.textC(u.W / 2, u.H - u.lineH(bodyS) - 6, "Native DirectX 12 build - all art and sound generated in code", bodyS * 0.9f, GREY);
}

// Store icon grid: charcoal tiles with a drawn glyph, the name, the level / count and the coin price. Focus = cyan frame
// on dusk; maxed = steel "MAX" + check; can't afford = dimmed tile, red struck-through price. Description below.
void Game::drawGridMenu(UIDraw& u, Menu& m) {
  float s = u.s; int n = (int)m.items.size(), cols = std::max(1, m.cols), rowsN = (n + cols - 1) / cols;
  m.index = std::max(0, std::min(n - 1, m.index));
  float pw = std::min(u.W - 30, 340.f * s), px = (u.W - pw) / 2;
  float titleS = s * 1.4f, bodyS = s * 0.75f, gap = 4 * s, inner = pw - 12 * s;
  float tw = (inner - gap * (cols - 1)) / cols, th = tw * 1.08f;
  std::vector<std::string> body = u.wrap(m.body, pw - 12 * s, bodyS);
  const MenuItem* sel = n ? &m.items[(size_t)m.index] : nullptr;
  std::vector<std::string> desc = sel ? u.wrap(sel->desc, pw - 12 * s, bodyS) : std::vector<std::string>();
  float headH = 8 * s + u.lineH(titleS) + 4 * s + body.size() * u.lineH(bodyS) + 6 * s;
  float descH = u.lineH(s * 0.9f) + std::max<size_t>(desc.size(), 2) * u.lineH(bodyS) + 12 * s + u.lineH(bodyS * 0.9f) + 4 * s;  // + the button hint row
  float avail = u.H - 20 * s - headH - descH;
  if (rowsN * (th + gap) > avail) th = std::max(30.f, avail / rowsN - gap);
  float ph = headH + rowsN * (th + gap) + descH, py = std::max(10.f, (u.H - ph) / 2);
  py += (1 - menuAnim(m)) * 24 * s;
  style::panel(u, px, py, pw, ph, true);
  style::closeX(u, px + pw - 22 * s, py + 6 * s, 16 * s, false);
  float y = py + 8 * s;
  style::titleText(u, px + pw / 2, y, m.title, titleS, style::accent()); y += u.lineH(titleS) + 4 * s;
  for (auto& l : body) { u.text(px + 6 * s, y, l, bodyS, STEEL); y += u.lineH(bodyS); }
  y += 6 * s;
  float gx = px + 6 * s, gy = y, lw = std::max(1.f, s / 2);
  menuLayout = {gx, gy, inner, th + gap, 0, rowsN, cols, tw, th, gap};
  for (int i = 0; i < n; i++) {
    const MenuItem& it = m.items[(size_t)i]; bool on = i == m.index, back = it.price < 0 && !it.owned;
    float tx = gx + (i % cols) * (tw + gap), ty = gy + (i / cols) * (th + gap);
    bool poor = !it.owned && !back && !it.afford;
    (void)lw;
    if (on) { style::glow(u, tx, ty, tw, th, 6 * s, style::cyan(), 4 * s); style::roundRect(u, tx - s, ty - s, tw + 2 * s, th + 2 * s, 7 * s, style::cyan()); }
    style::roundRect(u, tx, ty, tw, th, 6 * s, on ? Color{0.10f, 0.30f, 0.38f, 0.96f} : Color{0.12f, 0.15f, 0.19f, 0.92f});
    float nameS = std::min(s * 0.8f, (tw - 6 * s) / std::max(1.f, u.textW(it.label, 1)));
    float q = std::min(tw * 0.46f, th - u.lineH(nameS) - u.lineH(s * 0.7f) * 2 - 12 * s); q = std::max(8.f, q);
    Color ic = it.owned ? STEEL : poor ? dimmed(it.iconColor, 0.45f) : it.iconColor;
    drawIcon(u, it.icon, tx + (tw - q) / 2, ty + 5 * s, q, ic, true);
    float ly = ty + 5 * s + q + 3 * s;
    u.textC(tx + tw / 2, ly, it.label, nameS, poor ? GREY : on ? WHITE : STEEL); ly += u.lineH(nameS);
    float smallS = s * 0.65f;
    if (!it.sub.empty()) u.textC(tx + tw / 2, ly, it.sub, smallS, GREY);
    ly += u.lineH(smallS) + 1 * s;
    if (it.owned) {  // maxed upgrade: steel check mark + MAX
      std::string t = "MAX"; float w = u.textW(t, smallS * 1.1f), cx = tx + tw / 2 - (w + 8 * s) / 2;
      u.rect(cx, ly + 4 * s, 2 * s, 2 * s, STEEL); u.rect(cx + 2 * s, ly + 5.5f * s, 2 * s, 2 * s, STEEL); u.rect(cx + 4 * s, ly + 1.5f * s, 2 * s, 5 * s, STEEL);
      u.text(cx + 8 * s, ly, t, smallS * 1.1f, STEEL);
    } else if (it.price >= 0) {  // coin + price (red and struck through when you can't afford it)
      std::string t = std::to_string(it.price); float ts = smallS * 1.1f, d = u.lineH(ts) * 0.7f, w = d + 2 * s + u.textW(t, ts), cx = tx + tw / 2 - w / 2;
      drawCoin(u, cx, ly + 1 * s, d, poor ? dimmed(GOLD, 0.5f) : GOLD);
      u.text(cx + d + 2 * s, ly, t, ts, poor ? RED : GOLD);
      if (poor) u.rect(cx + d + 1 * s, ly + u.lineH(ts) * 0.42f, u.textW(t, ts) + 2 * s, std::max(1.f, s * 0.6f), RED);
    }
    if (poor) style::roundRect(u, tx, ty, tw, th, 6 * s, Color{0, 0, 0, 0.25f});
  }
  y = gy + rowsN * (th + gap) + 4 * s;
  u.rect(px + 8, y - 2 * s, pw - 16, std::max(1.f, s / 2), Color{0.35f, 0.3f, 0.5f, 1});
  if (sel) {
    std::string head = sel->label + (sel->owned ? "  -  MAX" : sel->price >= 0 ? "  -  " + std::to_string(sel->price) + " coins" + (sel->afford ? "" : "  (need " + std::to_string(sel->price - P().coins) + " more)") : "");
    u.text(px + 6 * s, y, head, s * 0.9f, CYAN); y += u.lineH(s * 0.9f) + 2 * s;
    for (auto& l : desc) { u.text(px + 6 * s, y, l, bodyS, Color{0.9f, 0.88f, 0.75f, 1}); y += u.lineH(bodyS); }
  }
  u.textR(px + pw - 6 * s, py + ph - u.lineH(bodyS * 0.9f) - 3 * s, btn("D-pad move  A buy  B close", "Arrows move  Enter buy  Esc close"), bodyS * 0.9f, GREY);
}

// ---------------------------------------------------------------- startup cinematic overlay
void Game::drawIntro(RenderList& rl) {
  UIDraw u(rl); float s = u.s;
  float t = introT, black = 0;
  if (t < INTRO_CARD) {  // studio card on black: fade in 1 s, hold 2 s, fade out 1 s
    black = 1;
    float a = t < 1 ? t : t < 3 ? 1 : std::max(0.f, 4 - t);
    // near-black veil over the (already loading) city. Not fully opaque: under Wine/vkd3d a fully opaque
    // full-screen quad made the card text disappear.
    u.rect(0, 0, u.W, u.H, Color{0, 0, 0, 0.985f});
    u.textC(u.W / 2, u.H * 0.45f, "Tommy D Productions", s * 2.2f, Color{1, 1, 1, a});
    u.textC(u.W / 2, u.H * 0.45f + u.lineH(s * 2.2f) + 4 * s, "presents", s * 1.3f, Color{0.8f, 0.8f, 0.9f, a});
    return;
  }
  if (t < INTRO_CARD + 1.2f) black = 1 - (t - INTRO_CARD) / 1.2f;
  if (t > INTRO_REVEAL_END) black = std::min(1.f, (t - INTRO_REVEAL_END) / (INTRO_END - INTRO_REVEAL_END));
  if (t > 8.0f) {  // the title slowly fades and grows in
    float k = clampf((t - 8.0f) / 2.5f, 0, 1), sc = s * (3.2f + 2.0f * k);
    float w = u.textW("TOMMYZILLA", sc); if (w > u.W - 20) sc *= (u.W - 20) / w;
    u.textC(u.W / 2, u.H * 0.16f, "TOMMYZILLA", sc, Color{0.23f, 0.84f, 1, k});
    if (t > 10.5f) u.textC(u.W / 2, u.H * 0.16f + u.lineH(sc) + 6 * s, "CITY SMASH", s * 1.6f, Color{1, 0.83f, 0.42f, clampf((t - 10.5f) / 1.0f, 0, 1)});
  }
  u.textR(u.W - 8, u.H - u.lineH(s * 0.75f) - 6, "press any key to skip", s * 0.75f, Color{0.7f, 0.7f, 0.8f, 0.7f});
  if (black > 0) u.rect(0, 0, u.W, u.H, Color{0, 0, 0, black});
}

// ---------------------------------------------------------------- HUD
std::string Game::levelHud() {
  if (!adventure) return "";
  const Profile& ap = AP();
  if (!level.active) {
    if (level.won && egg.active) return "Hatch the mystery egg! Hit it with a glow move  " + std::to_string(egg.hits) + "/" + std::to_string((int)C.eggGlowHits);
    if (level.won) return ap.level < (int)T.levels.size() ? "Free play - open the pause menu and choose Next Level" : "Campaign complete! Start a new one from the main menu";
    if (level.failed) return "Level failed - retry from the pause menu";
    return "";
  }
  const LevelDef& L = T.levels[level.idx];
  std::string s = "L" + std::to_string(level.idx + 1) + " " + L.name + ": ";
  if (level.bossPhase) return s + "knock out the " + (L.miniBoss == "mech" ? "MEGA MECH" : "GIANT SUPER-TANK") + "!";
  switch (L.obj) {
    case ObjType::Smash: s += "smash " + std::string(L.filter == "oldtown" ? "Old Town " : "") + "buildings " + std::to_string(level.count) + "/" + std::to_string(L.count); break;
    case ObjType::Survive: s += "survive " + mmss(level.t) + (L.hasZone && dist2D(kaiju.pos.x, kaiju.pos.z, L.zx, L.zz) >= L.zr ? "  (get back in the park!)" : ""); break;
    case ObjType::Reach: s += "reach the Ferry Terminal (" + std::to_string((int)dist2D(kaiju.pos.x, kaiju.pos.z, L.x, L.z)) + " m)"; break;
    case ObjType::Destroy: s += "destroy " + (L.unit == "any" ? std::string("enemies") : L.unit + "s") + " " + std::to_string(level.destroyed) + "/" + std::to_string(L.count); break;
    case ObjType::Landmarks: {
      int done = 0; for (auto& id : L.landmarks) { Building* b = world.landmark(id); if (b && b->landmarkSmashed) done++; }
      s += "landmarks " + std::to_string(done) + "/" + std::to_string(L.landmarks.size()) + ", buildings " + std::to_string(level.count) + "/" + std::to_string(L.count); break;
    }
    case ObjType::Boss:
      if (L.pedestals && !boss.active) { int n = 0; for (int i = 0; i < 6; i++) if (level.placed & (1 << i)) n++;
        s += "place the stones " + std::to_string(n) + "/6" + (gauntletReady() ? "" : " (put the gauntlet on!)"); }
      else if (boss.staged) s += boss.stage == 1 ? "hit THE BULK with the gauntlet!" : boss.stage == 2 ? "STAGE 2: beat the giant BULK!" : snapReady() ? "DO THE SNAP!" : "FINAL FORM: get him to half health!";
      else s += boss.intro() ? "here he comes..." : "defeat THE BULK!";
      break;
    case ObjType::Box: s += level.boxShown ? (level.boxOpen ? "the box is open!" : "open the zookeeper's box!") : "flatten the whole district  - " + std::to_string(level.arenaLeft) + " buildings left"; break;
    case ObjType::Stone:
      if (level.stoneTaken) { s += "got the stone!"; break; }
      s += "find the " + E.stones[L.stone].name + " (behind a locked zoo gate)";
      for (auto& g : gates) if (g.stone == L.stone) {  // where the gate is (Adventure city: it sits beside a purple building)
        float dx = g.pos.x - kaiju.pos.x, dz = g.pos.z - kaiju.pos.z; int m = (int)std::hypot(dx, dz);
        s += "  - gate " + std::to_string(m) + "m away (follow the beam)";
        if (!stoneRevealed(L.stone) && m < 40) s += " (smash the purple building to unlock it!)";
      }
      break;
  }
  if (L.time > 0 && L.obj != ObjType::Survive) s += "   " + mmss(level.t);
  return s;
}

void Game::drawHud(RenderList& rl) {
  UIDraw u(rl); float s = u.s, small = s * 0.75f;
  vaultTick();  // Vault: cheats unlock + effects
  Profile& p = P();
  if (mode == Mode::Play) {
    for (auto& l : world.labels) {
      if (length(l.pos - rl.eye) > 320) continue;
      float sx, sy; if (!u.project(l.pos, sx, sy)) continue;
      Color c = l.kind == 0 ? Color{1, 1, 1, 0.9f} : l.kind == 1 ? Color{0.85f, 0.85f, 0.9f, 0.8f} : Color{0.6f, 0.85f, 1, 0.8f};
      u.textC(sx, sy, l.text, l.kind == 0 ? small : small * 0.85f, c);
    }
    for (auto& f : floats) { float sx, sy; if (u.project(f.pos, sx, sy)) { Color c = f.c; c.a = std::min(1.f, f.t * 2); u.textC(sx, sy, f.text, s, c); } }
    if (egg.active && egg.state == 0) { float sx, sy; if (u.project(egg.pos + Vec3(0, 13, 0), sx, sy)) u.textC(sx, sy, "MYSTERY EGG " + std::to_string(egg.hits) + "/" + std::to_string((int)C.eggGlowHits), small, GOLD); }
  }
  if (mode == Mode::Reveal) { if (bigT > 0) goto big; return; }
  {
    // top-left: coins and progress
    style::roundRect(u, 6, 6, 150 * s, u.lineH(s) * 3.6f + 6 * s, 6 * s, style::bg());  // Vault
    u.text(6 + 4 * s, 6 + 3 * s, "COINS " + std::to_string(p.coins) + "  KEYS " + std::to_string(p.keys), s, GOLD);
    u.text(6 + 4 * s, 6 + 3 * s + u.lineH(s), "Eaten: " + std::to_string(p.eaten), s, needEatT > 0 && ((int)(realTime * 6)) % 2 ? RED : PEA);  // super-attack fuel (E)
    float ly0 = 6 + 3 * s + u.lineH(s);
    u.text(6 + 4 * s, ly0 + u.lineH(s), std::string(adventure ? (crazy ? "CRAZY" : "ADVENTURE") : "SANDBOX") + "  smashed " + std::to_string(p.smashCount) + "  rainbows " + std::to_string(p.rainbows), small, WHITE);
    std::string line3 = kaiju.def().name;
    if (!adventure && !p.schoolUnlocked) line3 += "  | school " + std::to_string(prog::schoolProgress(p)) + "/" + std::to_string((int)C.schoolUnlockCount) + " landmarks";
    u.text(6 + 4 * s, ly0 + u.lineH(s) + u.lineH(small), line3, small, GREY);
    // health bar
    float bw = 150 * s, bh = 6 * s, bx = u.W / 2 - bw / 2, by = 6 * s;
    float k = clampf(kaiju.hp / std::max(1.f, kaiju.maxHp()), 0, 1);
    u.rect(bx - 2, by - 2, bw + 4, bh + 4, Color{0, 0, 0, 0.6f});
    Color hc = k > 0.5f ? mixColor(hexColor(0xffe03a), hexColor(0x4cd964), (k - 0.5f) * 2) : mixColor(hexColor(0xff9a1f), hexColor(0xffe03a), k * 2);
    u.rect(bx, by, bw * k, bh, kaiju.hurtT > 0 ? WHITE : hc);
    Color acc = uiAccent();
    u.frame(bx - 2, by - 2, bw + 4, bh + 4, acc, std::max(1.f, s / 2));  // frame in the kaiju's own accent colour
    u.textR(bx - 4 * s, by - 2 * s, "HP", small, WHITE);
    float y = by + bh + 4 * s;
    if (hasLook(kaiju.def().id)) {  // XP / growth bar (accent colour)
      float g = growthFrac(), xb = bw * 0.8f, xx = u.W / 2 - xb / 2, xh = 3 * s;
      u.rect(xx - 1, y - 1, xb + 2, xh + 2, Color{0, 0, 0, 0.6f}); u.rect(xx, y, xb * g, xh, acc);
      u.textR(xx - 4 * s, y - 2 * s, "XP", small * 0.8f, acc);
      if (g < 1) u.text(xx + xb + 4 * s, y - 2 * s, std::to_string((int)(g * 100)) + "% grown", small * 0.8f, GREY);
      y += xh + 4 * s;
    }
    if (boss.active && !boss.dead && !boss.intro()) {  // big boss bar
      float bw2 = 260 * s, bx2 = u.W / 2 - bw2 / 2, bh2 = 9 * s;
      u.textC(u.W / 2, y, "THE BULK   size x" + fmt1(boss.growth()) + (boss.staged ? (boss.stage == 3 ? "   FINAL FORM" : "   stage " + std::to_string(boss.stage)) : ""), s, hexColor(0xd8a8ff)); y += u.lineH(s);
      float kb = clampf(boss.hp / std::max(1.f, boss.maxHp), 0, 1);
      u.rect(bx2 - 2, y - 2, bw2 + 4, bh2 + 4, Color{0, 0, 0, 0.7f});
      u.rect(bx2, y, bw2 * kb, bh2, boss.pulse > 0 ? WHITE : hexColor(0xa050ff));
      if (boss.staged && boss.stage == 3) u.rect(bx2 + bw2 * 0.5f - 1, y - 3, 2, bh2 + 6, GOLD);  // the snap line
      y += bh2 + 6 * s;
    }
    if (Unit* mb = enemies.miniBoss()) {  // level 11-14 boss bar
      float bw2 = 240 * s, bx2 = u.W / 2 - bw2 / 2, bh2 = 8 * s;
      u.textC(u.W / 2, y, mb->boss == 2 ? "MEGA MECH" : "GIANT SUPER-TANK", s, hexColor(0xffb020)); y += u.lineH(s);
      float kb = clampf(mb->hp / std::max(1.f, mb->maxHp), 0, 1);
      u.rect(bx2 - 2, y - 2, bw2 + 4, bh2 + 4, Color{0, 0, 0, 0.7f});
      u.rect(bx2, y, bw2 * kb, bh2, hexColor(0xff6a20));
      y += bh2 + 6 * s;
    }
    std::string obj = levelHud();
    if (!obj.empty()) { u.rect(u.W / 2 - u.textW(obj, small) / 2 - 6, y - 2, u.textW(obj, small) + 12, u.lineH(small) + 4, Color{0, 0, 0, 0.45f}); u.textC(u.W / 2, y, obj, small, GOLD); y += u.lineH(small) + 6 * s; }
    if (enemies.nukeActive && ((int)(realTime * 3)) % 2 == 0) {
      std::string t = "NUKE IN " + std::to_string((int)std::ceil(enemies.nukeT)) + "!  Press " + btn("RB", "R") + " to pay " + std::to_string((int)C.nukeCancelCost) + " coins";
      u.textC(u.W / 2, y, t, s * 1.1f, hexColor(0xff9a1f)); y += u.lineH(s * 1.1f) + 4 * s;
    }
    bool isTut = false; std::string tut = tutorialText(isTut);
    if (!tut.empty() && menus.empty()) {
      std::vector<std::string> lines = u.wrap(tut, 260 * s, small);
      float tw = 0; for (auto& l : lines) tw = std::max(tw, u.textW(l, small));
      std::string head = isTut ? "TUTORIAL " + std::to_string(std::min(save.tutorial.step + 1, 7)) + "/7" : "NEW POWER";
      tw = std::max(tw, u.textW(head, small)) + 12 * s;
      float th = (lines.size() + 1) * u.lineH(small) + 8 * s;
      style::panel(u, u.W / 2 - tw / 2, y, tw, th, false);  // Vault
      u.textC(u.W / 2, y + 3 * s, head, small, CYAN);
      float ly = y + 3 * s + u.lineH(small); for (auto& l : lines) { u.textC(u.W / 2, ly, l, small, WHITE); ly += u.lineH(small); }
      y += th + 4 * s;
    }
    if (tutorialActive() && mode == Mode::Play) {  // tutorial challenge counters
      int r = std::min(save.tutorial.rainbows, (int)C.tutorialRainbows), b = std::min(save.tutorial.buildings, (int)C.tutorialBuildings);
      std::string c = "Rainbows " + std::to_string(r) + "/" + std::to_string((int)C.tutorialRainbows) + "    Buildings " + std::to_string(b) + "/" + std::to_string((int)C.tutorialBuildings);
      float cw = u.textW(c, s) + 16 * s;
      u.rect(u.W / 2 - cw / 2, y, cw, u.lineH(s) + 6 * s, Color{0.2f, 0.05f, 0.3f, 0.75f}); u.frame(u.W / 2 - cw / 2, y, cw, u.lineH(s) + 6 * s, hexColor(0xff6bd5), std::max(1.f, s / 2));
      static const uint32_t rb[] = {0xff9a1f, 0xffe03a, 0x4cd964, 0x3ad7ff, 0x5a6bff, 0xb06bff};
      u.textC(u.W / 2, y + 3 * s, c, s, r >= (int)C.tutorialRainbows && b >= (int)C.tutorialBuildings ? GOLD : hexColor(rb[(int)(realTime * 2) % 6]));
      y += u.lineH(s) + 10 * s;
    }
    // wanted stars
    if (enemies.policeOn()) {
      int st = enemies.stars();
      for (int i = 0; i < 5; i++) u.star(u.W - (5 - i) * 15 * s - 4 * s, 12 * s, 6 * s, i < st ? GOLD : Color{0.2f, 0.2f, 0.25f, 0.8f});
    }
    // toasts
    float ty = 26 * s;
    for (auto& t : toasts) {
      std::vector<std::string> lines = u.wrap(t.text, 170 * s, small);
      float a = std::min(1.f, t.t * 2);
      for (auto& l : lines) { float w = u.textW(l, small); style::roundRect(u, u.W - w - 14 * s, ty - 1, w + 8 * s, u.lineH(small) + 2, 3 * s, Color{0.05f, 0.06f, 0.08f, 0.8f * a}); u.text(u.W - w - 10 * s, ty, l, small, Color{1, 1, 1, a}); ty += u.lineH(small) + 2; }
      ty += 3 * s;
    }
    // power slots and melee bindings
    float sy = u.H - 80 * s, sw = 120 * s;
    for (int i = 0; i < 2; i++) {
      float sx = 6 + i * (sw + 6 * s);
      int smi = T.moveIndex(p.slots[i]);
      Color ic = smi >= 0 ? T.moves[smi].color : GREY;
      if (i < p.slotCount) style::glow(u, sx, sy, sw, 26 * s, 6 * s, Color{acc.r, acc.g, acc.b, 0.6f}, 2 * s);  // Vault
      style::roundRect(u, sx, sy, sw, 26 * s, 6 * s, style::bg());
      style::roundRect(u, sx + 3 * s, sy + 3 * s, 7 * s, 7 * s, 3.5f * s, ic);  // colour "icon" of the slotted skill
      u.text(sx + 12 * s, sy + 3 * s, std::string(i == 0 ? "SLOT 1 CLOSE " : "SLOT 2 RANGED ") + (i == 0 ? btn("RT", "F") : btn("LT", "Q")), small, GREY);
      std::string nm; float cdK = 0;
      if (i >= p.slotCount || p.slots[i].empty()) nm = "(empty)";
      else { int mi = T.moveIndex(p.slots[i]); if (mi >= 0) { nm = T.moves[mi].name; if (mi < (int)kaiju.cooldowns.size() && T.moves[mi].cooldown > 0) cdK = kaiju.cooldowns[mi] / T.moves[mi].cooldown; } }
      u.text(sx + 4 * s, sy + 3 * s + u.lineH(small), nm, small, i < p.slotCount ? WHITE : GREY);
      if (i < p.slotCount && smi >= 0 && T.moves[smi].kind == MoveKind::Super) {  // eat-to-fuel cost tag
        const MoveDef& md = T.moves[smi]; int lo = superEatCost(md, 0), hi = superEatCost(md, 1);
        if (hi > 0) {
          std::string tag = (lo != hi ? std::to_string(lo) + "-" + std::to_string(hi) : std::to_string(hi)) + " eaten";
          float tw = u.textW(tag, small) + 6 * s;  // tag sits on a little tab above the slot box
          u.rect(sx + sw - tw, sy - u.lineH(small) - 2 * s, tw, u.lineH(small) + 2 * s, Color{0, 0, 0, 0.55f});
          u.textR(sx + sw - 3 * s, sy - u.lineH(small) - 1 * s, tag, small, p.eaten >= lo ? PEA : RED);
        }
      }
      if (cdK > 0) u.rect(sx + 2, sy + 26 * s - 3 * s, (sw - 4) * (1 - cdK), 2 * s, CYAN);
      if (i < p.slotCount && kaiju.charging && T.moveIndex(p.slots[i]) == kaiju.chargeSlot) u.rect(sx + 2, sy + 26 * s - 3 * s, (sw - 4) * kaiju.charge, 2 * s, GOLD);
    }
    // Charged Roar: locked state / chest / charge meter (meter only while the roar is slotted and charging)
    {
      float ry = sy - u.lineH(small) - 4 * s;
      if (p.gauntletSlot || p.gauntletCracked) {  // the gauntlet slot (an empty outline with no text until the gauntlet is found)
        float gx = 6 + 2 * (sw + 6 * s), q = 26 * s;
        u.rect(gx, sy, q, q, Color{0, 0, 0, 0.45f}); u.frame(gx, sy, q, q, p.gauntlet && p.gauntletOn ? GOLD : Color{0.4f, 0.4f, 0.48f, 1}, std::max(1.f, s / 2));
        if (p.gauntletCracked) for (int k2 = 0; k2 < 5; k2++) u.rect(gx + q * (0.2f + 0.12f * k2), sy + q * (0.15f + 0.15f * (k2 % 3)), s, q * 0.25f, GREY);
        if (p.gauntlet) {
          Color gc = glovePulseT > 0 ? WHITE : hexColor(0xffcc33);
          u.rect(gx + q * 0.2f, sy + q * 0.4f, q * 0.6f, q * 0.45f, gc); for (int f = 0; f < 4; f++) u.rect(gx + q * (0.2f + f * 0.16f), sy + q * 0.15f, q * 0.1f, q * 0.28f, gc);
          for (int si = 0; si < 6; si++) if (std::find(p.stones.begin(), p.stones.end(), E.stones[si].id) != p.stones.end())
            u.rect(gx + q * (0.14f + si * 0.12f), sy + q * 0.6f, q * 0.1f, q * 0.1f, E.stones[si].color);  // stones on the glove, one by one
        }
      }
      if (!p.moves.count("roar") && T.moveIndex("roar") >= 0) {
        bool inChest = std::find(p.chests.begin(), p.chests.end(), std::string("roar")) != p.chests.end();
        std::string t = inChest ? "CHARGED ROAR: in a locked chest - " + std::string(p.keys > 0 ? "go open it!" : "buy a key (" + std::to_string((int)C.keyCost) + " coins)")
                                : "CHARGED ROAR: LOCKED - smash a landmark (" + std::to_string(std::min((int)p.landmarks.size(), (int)C.roarUnlockLandmarks)) + "/" + std::to_string((int)C.roarUnlockLandmarks) + ") or survive hits (" + std::to_string(std::min(p.hitsTaken, (int)C.roarUnlockHits)) + "/" + std::to_string((int)C.roarUnlockHits) + ")";
        u.text(6, ry, t, small, inChest ? GOLD : GREY);
      } else if (kaiju.charging && kaiju.chargeSlot >= 0 && (p.slots[0] == T.moves[kaiju.chargeSlot].id || p.slots[1] == T.moves[kaiju.chargeSlot].id)) {
        float mw = 160 * s, mx = u.W / 2 - mw / 2, my = u.H * 0.62f;
        u.textC(u.W / 2, my - u.lineH(s), kaiju.charge >= 1 ? "FULL POWER! release to ROAR" : "CHARGING ROAR...", s, kaiju.charge >= 1 ? GOLD : CYAN);
        u.rect(mx - 2, my - 2, mw + 4, 8 * s + 4, Color{0, 0, 0, 0.7f}); u.rect(mx, my, mw * kaiju.charge, 8 * s, kaiju.charge >= 1 ? GOLD : CYAN);
        int cost = superEatCost(T.moves[kaiju.chargeSlot], kaiju.charge);
        if (cost > 0) u.textC(u.W / 2, my + 8 * s + 4 * s, "cost " + std::to_string(cost) + " eaten  (you have " + std::to_string(p.eaten) + ")" + (p.eaten < cost ? " - not enough!" : ""), small, p.eaten >= cost ? PEA : RED);
      }
    }
    if (mode == Mode::Play && menus.empty()) {  // context prompts: zoo gulp, stone pedestals, the snap
      std::string pr; Color pc = GOLD;
      if (zoo.active && zoo.state <= 1 && zoo.inRange) pr = "Press " + btn("Y", "E") + " to gulp!";
      else if (zoo.active && zoo.state <= 1 && zoo.idleT > E.zooIdleHint) { pr = "Wait for the zookeeper to come close..."; pc = GREY; }
      else if (snapReady()) { pr = "DO THE SNAP!  Press " + btn("RB", "R"); pc = hexColor(rb6((int)(realTime * 8)));}
      if (!pr.empty()) { float tw = u.textW(pr, s * 1.3f) + 16 * s, ty2 = u.H * 0.66f;
        u.rect(u.W / 2 - tw / 2, ty2 - 3 * s, tw, u.lineH(s * 1.3f) + 6 * s, Color{0, 0, 0, 0.6f}); u.frame(u.W / 2 - tw / 2, ty2 - 3 * s, tw, u.lineH(s * 1.3f) + 6 * s, acc, std::max(1.f, s / 2));
        u.textC(u.W / 2, ty2, pr, s * 1.3f, pc); }
      if (level.active && T.levels[level.idx].obj == ObjType::Stone && !level.stoneTaken)
        for (auto& g : gates) if (g.stone == T.levels[level.idx].stone) { float sx, sy; if (u.project(g.pos + Vec3(0, 12, 0), sx, sy)) u.textC(sx, sy, "ZOO GATE", s * 1.3f, E.stones[(size_t)g.stone].color); }
      if (zoo.active && !zoo.barsBroken) { float sx, sy; if (u.project(zoo.center + Vec3(0, 26, -E.zooCageR - 2), sx, sy)) u.textC(sx, sy, "KAIJU ZOO", s * 1.5f, hexColor(0x4cd964)); }
      if (zoo.active && zoo.state <= 1) u.textR(u.W - 8, u.H - u.lineH(small) * 2 - 8, btn("D-pad down", "Tab") + " skip", small, GREY);
    }
    if (needEatT > 0 && mode == Mode::Play) {  // not enough fuel for a super
      std::string t = "Need " + std::to_string(needEatN) + " eaten - press " + btn("Y", "E") + " to gulp people!";
      float tw = u.textW(t, s) + 14 * s, ty2 = u.H * 0.54f;
      u.rect(u.W / 2 - tw / 2, ty2 - 3 * s, tw, u.lineH(s) + 6 * s, Color{0.3f, 0.02f, 0.02f, 0.7f * std::min(1.f, needEatT * 2)});
      u.textC(u.W / 2, ty2, t, s, Color{1, 0.55f, 0.45f, std::min(1.f, needEatT * 2)});
    }
    // Rainbow Blaster crosshair (above the monster's head) - lights up when a person is in the line of fire
    if (mode == Mode::Play && menus.empty()) {  // Vault: persistent crosshair; rainbow + lock text when the blaster is slotted
      bool blaster = p.slots[0] == "blaster" || p.slots[1] == "blaster";
      Vec3 aim; bool lock = blaster && aimTarget(aim) >= 0;
      float cx = u.W / 2, cy = u.H * AIM_SCREEN_Y, g = (lock ? 7.f : 5.f) * s, L = 6 * s;
      static const uint32_t rb[] = {0xff9a1f, 0xffe03a, 0x4cd964, 0x3ad7ff, 0x5a6bff, 0xb06bff};
      Color c = lock ? hexColor(rb[(int)(realTime * 10) % 6]) : Color{1, 1, 1, 0.85f};
      style::crosshair(u, cx, cy, s, lock, c);
      if (lock) u.textC(cx, cy + g + L + 2 * s, "RAINBOW TIME!", small, c);
    }
    std::string melee;
    static const char* pad[6] = {"X", "B", "A", "LB+X", "LB+B", "LB+A"}, *key[6] = {"J", "K", "L", "U", "I", "O"};
    for (int b = 0; b < MELEE_BINDS; b++) { int mi = T.moveIndex(p.melee[b]); if (mi < 0) continue; melee += std::string(usingPad ? pad[b] : key[b]) + " " + T.moves[mi].name + "   "; }
    melee += btn("Y", "G") + (kaiju.held >= 0 ? " Throw" : " Grab") + "   " + btn("Y", "E") + " Eat/Chest   " + btn("hold A", "hold Space") + " Stomp ring";
    drawHudBar(u);  // Vault: bottom bar (info + EGGS / POWERS / STORE)
    { hudbar::Layout HL = hudbar::layout(u.W, u.H, s); float mw = std::min(u.W - 20, u.textW(melee, small) + 12 * s), my = HL.bar.y - u.lineH(small) - 6 * s;
      style::roundRect(u, 6, my - 2 * s, mw, u.lineH(small) + 4 * s, 4 * s, Color{0.05f, 0.06f, 0.08f, 0.6f});
      u.text(6 + 6 * s, my, melee, small, style::textDim()); }
    // bottom-right info
    std::string info = std::to_string((int)std::lround(fps)) + " FPS"; if (save.settings.muted) info = "MUTED (M)   " + info;
    u.textR(u.W - 6, 6, info, small, GREY);  // Vault: moved to top-right (bottom edge is the HUD bar)
    if (!usingPad && !mouseCaptured && mode == Mode::Play && menus.empty()) u.textC(u.W / 2, u.H - 110 * s, "Click the game to capture the mouse for looking around", small, GREY);
    if (mode == Mode::GameOver) {}
  }
big:
  if (bigT > 0) {
    float a = std::min(1.f, bigT * 1.5f), bs = s * 2.6f;
    float w = std::max(u.textW(bigText, bs), u.textW(bigTop, s * 1.2f));
    if (w > u.W - 20) bs *= (u.W - 20) / w;
    float y = u.H * 0.3f;
    u.textC(u.W / 2, y, bigTop, s * 1.2f, Color{0.23f, 0.84f, 1, a});
    u.textC(u.W / 2, y + u.lineH(s * 1.2f) + 2 * s, bigText, bs, Color{1, 0.83f, 0.42f, a});
  }
}
}  // namespace tz
