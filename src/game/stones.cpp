// Infinity Stone power map. Owned stones (Profile::stones) stay the gauntlet collection;
// stoneBind[4] says which of a stone's powers is doing Pulse / Strike / Blast / Roar.
#include "game.h"
#include <algorithm>
namespace tz {
namespace {
int channelOf(const std::string& name) {
  if (name == "pulse") return 0;
  if (name == "strike") return 1;
  if (name == "blast") return 2;
  if (name == "roar") return 3;
  return -1;
}
const char* channelLabel(int c) {
  static const char* n[] = {"Pulse", "Strike", "Blast", "Roar"};
  return c >= 0 && c < 4 ? n[c] : "?";
}
bool ownsStone(const Profile& p, const std::string& id) {
  return std::find(p.stones.begin(), p.stones.end(), id) != p.stones.end();
}
const StoneDef* findStone(const std::string& id) {
  for (auto& s : E.stones) if (s.id == id) return &s;
  return nullptr;
}
const StonePower* findPower(const StoneDef& s, const std::string& id) {
  for (auto& p : s.powers) if (p.id == id) return &p;
  return nullptr;
}
bool parseBind(const std::string& bind, std::string& stoneId, std::string& powerId) {
  auto c = bind.find(':');
  if (c == std::string::npos || c == 0 || c + 1 >= bind.size()) return false;
  stoneId = bind.substr(0, c); powerId = bind.substr(c + 1);
  return true;
}
}  // namespace

bool Game::assignStonePower(int channel, const std::string& stoneId, const std::string& powerId) {
  Profile& p = P();
  if (channel < 0 || channel > 3) return false;
  if (stoneId.empty() || powerId.empty()) { p.stoneBind[channel].clear(); return true; }
  if (!ownsStone(p, stoneId)) return false;
  const StoneDef* s = findStone(stoneId);
  const StonePower* pw = s ? findPower(*s, powerId) : nullptr;
  if (!pw || channelOf(pw->channel) != channel) return false;
  p.stoneBind[channel] = stoneId + ":" + powerId;
  return true;
}
bool Game::stonePowerOn(int channel, StonePower& out) const {
  if (!gauntletReady() || channel < 0 || channel > 3) return false;
  const Profile& p = const_cast<Game*>(this)->P();
  std::string sid, pid;
  if (!parseBind(p.stoneBind[channel], sid, pid) || !ownsStone(p, sid)) return false;
  const StoneDef* s = findStone(sid);
  const StonePower* pw = s ? findPower(*s, pid) : nullptr;
  if (!pw || channelOf(pw->channel) != channel) return false;
  out = *pw;
  return true;
}
float Game::stoneBrickMul(int moveKind) const {
  int ch = moveKind == 0 ? 1 : moveKind == 1 ? 2 : -1;  // melee -> strike, direct -> blast
  StonePower pw;
  if (ch < 0 || !stonePowerOn(ch, pw)) return 1.f;
  return pw.bricks > 0 ? pw.bricks : 1.f;
}
float Game::stoneRangeMul(int moveKind) const {
  int ch = moveKind == 0 ? 1 : moveKind == 1 ? 2 : moveKind == 2 ? 3 : -1;
  StonePower pw;
  if (ch < 0 || !stonePowerOn(ch, pw)) return 1.f;
  return pw.radius > 0 ? pw.radius : 1.f;
}
void Game::onSuperFired() {
  StonePower pw;
  if (!stonePowerOn(3, pw) || pw.heal <= 0) return;
  healKaiju(pw.heal);
  fx->shell(kaiju.hand(), 6, 0.45f, hexColor(0x9dffb0));
}
std::string Game::stoneBindSummary() const {
  const Profile& p = const_cast<Game*>(this)->P();
  std::string s = "Now: ";
  for (int c = 0; c < 4; c++) {
    if (c) s += "   ";
    s += channelLabel(c);
    s += ": ";
    std::string sid, pid;
    const StoneDef* st = nullptr;
    const StonePower* pw = nullptr;
    if (parseBind(p.stoneBind[c], sid, pid)) { st = findStone(sid); pw = st ? findPower(*st, pid) : nullptr; }
    if (pw && ownsStone(p, sid)) s += pw->name;
    else s += "empty";
  }
  return s;
}
void Game::openStoneMenu() {
  Menu m; m.title = "INFINITY STONES"; m.wide = true;
  m.build = [this](Menu& mm) {
    Profile& p = P();
    mm.body = "Pick a stone, read what each power does, then assign powers to different actions. One power per action; choosing another replaces it. The gauntlet has to be equipped for them to work.\n" + stoneBindSummary();
    for (size_t i = 0; i < E.stones.size(); i++) {
      const StoneDef& s = E.stones[i];
      bool have = ownsStone(p, s.id);
      MenuItem it; it.label = s.name; it.icon = 3; it.iconColor = s.color;
      it.right = have ? "owned" : "not found";
      it.desc = s.blurb + (have ? "\nOpen it to assign its powers." : "\nYou can read its powers now. Assigning waits until you find it (Zoo Gate, gauntlet on).");
      it.act = [this, i]() { openStonePowers((int)i); };
      mm.items.push_back(it);
    }
    MenuItem b; b.label = "Back"; b.desc = "Return to skills"; b.act = [this]() { popMenu(); }; mm.items.push_back(b);
  };
  m.onBack = [this]() { popMenu(); };
  pushMenu(m);
}
void Game::openStonePowers(int stoneIndex) {
  if (stoneIndex < 0 || stoneIndex >= (int)E.stones.size()) return;
  Menu m; m.title = E.stones[(size_t)stoneIndex].name; m.wide = true;
  m.build = [this, stoneIndex](Menu& mm) {
    const StoneDef& s = E.stones[(size_t)stoneIndex];
    Profile& p = P();
    bool have = ownsStone(p, s.id);
    mm.body = s.blurb + (have ? "\nPress a power to assign it. Press it again to clear that action." : "\nNot in the gauntlet yet — you can still read every power.");
    if (s.powers.empty()) {
      MenuItem none; none.label = "No powers"; none.enabled = false; none.desc = "This stone has no powers in the data."; mm.items.push_back(none);
    }
    for (const StonePower& pw : s.powers) {
      int ch = channelOf(pw.channel);
      bool on = false;
      std::string sid, pid;
      if (ch >= 0 && parseBind(p.stoneBind[ch], sid, pid)) on = sid == s.id && pid == pw.id && have;
      MenuItem it; it.label = "  " + pw.name; it.icon = 3; it.iconColor = s.color;
      it.right = on ? std::string(channelLabel(ch)) + " ON" : (ch >= 0 ? channelLabel(ch) : "?");
      it.desc = pw.blurb;
      if (ch >= 0) it.desc += on ? "\nAssigned. Press to take it off this action." : "\nPress to assign it to " + std::string(channelLabel(ch)) + ". Replaces whatever is there.";
      it.enabled = have && ch >= 0;
      std::string sidOwn = s.id, pname = pw.name;
      it.act = [this, ch, sidOwn, pname, on]() {
        const StoneDef* st = nullptr;
        for (auto& q : E.stones) if (q.id == sidOwn) st = &q;
        const StonePower* found = nullptr;
        if (st) for (auto& q : st->powers) if (q.name == pname) found = &q;
        if (!on && !found) { play(SND_error); return; }
        if (on) assignStonePower(ch, "", "");
        else if (!assignStonePower(ch, sidOwn, found->id)) { play(SND_error); return; }
        toast(on ? std::string(channelLabel(ch)) + " cleared" : pname + " assigned to " + channelLabel(ch) + (gauntletReady() ? "" : " (equip the gauntlet to use it)"));
        play(SND_ui); persist();
      };
      mm.items.push_back(it);
    }
    MenuItem b; b.label = "Back"; b.desc = "All six stones"; b.act = [this]() { popMenu(); }; mm.items.push_back(b);
  };
  m.onBack = [this]() { popMenu(); };
  pushMenu(m);
}
}  // namespace tz
