#include "saveslots.h"
#include <fstream>
#include <system_error>
#include "../core/json.h"
#include "progress.h"
#include "tables.h"
#include "eggs.h"
namespace tz {
namespace fs = std::filesystem;
bool readWholeFile(const fs::path& p, std::string& out) {
  std::ifstream f(p, std::ios::binary); if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return !f.bad();
}
bool writeFileAtomic(const fs::path& p, const std::string& data) {
  std::error_code ec;
  if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
  fs::path tmp = p; tmp += ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc); if (!f) return false;
    f.write(data.data(), (std::streamsize)data.size()); f.flush();
    if (!f) { f.close(); fs::remove(tmp, ec); return false; }
  }
  fs::rename(tmp, p, ec);  // POSIX rename replaces atomically; the Windows build passes a MoveFileEx writer instead
  if (ec) { fs::remove(tmp, ec); return false; }
  return true;
}
bool SlotStore::exists(int slot) const { std::error_code ec; return !dir.empty() && fs::exists(file(slot), ec); }
bool SlotStore::read(int slot, std::string& out) const { return !dir.empty() && slot >= 0 && slot < SAVE_SLOTS && readWholeFile(file(slot), out); }
bool SlotStore::write(int slot, const std::string& text) const {
  if (dir.empty() || slot < 0 || slot >= SAVE_SLOTS) return false;
  std::error_code ec; fs::create_directories(dir, ec);
  return writer ? writer(file(slot), text) : writeFileAtomic(file(slot), text);
}
bool SlotStore::remove(int slot) const { std::error_code ec; if (dir.empty()) return false; fs::path t = file(slot); t += ".tmp"; fs::remove(t, ec); return fs::remove(file(slot), ec); }
bool SlotStore::backupCorrupt(int slot) const {
  std::error_code ec; fs::path b = file(slot); b += ".bad";
  fs::copy_file(file(slot), b, fs::copy_options::overwrite_existing, ec); return !ec;
}
SlotSummary summarizeSlot(int slot, const std::string* json) {
  SlotSummary s; s.title = "Slot " + std::to_string(slot + 1);
  if (!json) { s.line = "Empty"; s.detail = "Empty - start a new game here"; return s; }
  Json j; SaveData d;
  if (!Json::parse(*json, j) || !d.fromJson(j)) { s.used = true; s.corrupt = true; s.line = "Damaged save"; s.detail = "This save file could not be read. Picking it starts a fresh game (the old file is kept as .bad)."; return s; }
  s.used = true;
  if (!d.name.empty()) s.title = d.name;
  int lv = std::min(d.adventure.levelsWon, (int)T.levels.size());
  std::string adv = d.adventureUnlocked ? "Adventure " + std::to_string(lv) + "/" + std::to_string((int)T.levels.size()) : "Adventure locked";
  s.line = adv + "  " + std::to_string(d.sandbox.coins) + "c";
  s.detail = "Sandbox: " + std::to_string(d.sandbox.coins) + " coins, " + std::to_string(d.sandbox.keys) + " keys, " + std::to_string(d.sandbox.smashCount) + " smashed" +
             (d.tutorial.done ? "" : " (tutorial in progress)") + "\n" + adv + ": " + std::to_string(d.adventure.coins) + " coins, " + std::to_string(d.adventure.keys) + " keys\n" +
             "Monsters: " + std::to_string(d.monsters.size()) + "   Last played: " + (d.lastPlayed.empty() ? std::string("-") : d.lastPlayed);
  auto it = d.looks.find(d.monster);
  if (it != d.looks.end()) {
    s.hasAccent = true; s.accent = it->second.accent;
    int mi = T.monsterIndex(d.monster);
    s.detail += "\nPlaying: " + (mi >= 0 ? T.monsters[mi].name : d.monster) + " (" + E.rarity(it->second.rarity).name + ")";
  }
  return s;
}
}  // namespace tz
