// Three save slots (one JSON file per player). Platform independent: std::filesystem paths, an atomic
// write (temp file + rename, overridable by the Windows layer with MoveFileEx), and a summary for the menu.
#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include "../core/math.h"
namespace tz {
constexpr int SAVE_SLOTS = 3;
bool readWholeFile(const std::filesystem::path& p, std::string& out);
bool writeFileAtomic(const std::filesystem::path& p, const std::string& data);  // writes p.tmp, then renames over p
struct SlotStore {
  std::filesystem::path dir;  // e.g. %APPDATA%\Tommyzilla (or saves\ beside the exe)
  std::function<bool(const std::filesystem::path&, const std::string&)> writer;  // atomic writer (default writeFileAtomic)
  std::filesystem::path file(int slot) const { return dir / ("save_slot" + std::to_string(slot + 1) + ".json"); }
  bool exists(int slot) const;
  bool read(int slot, std::string& out) const;
  bool write(int slot, const std::string& text) const;
  bool remove(int slot) const;
  bool backupCorrupt(int slot) const;  // keeps a damaged file as save_slotN.json.bad before it is overwritten
};
struct SlotSummary {
  bool used = false, corrupt = false;
  std::string title;   // "Slot 1" or the player name
  std::string line;    // short one-liner for the menu's right column
  std::string detail;  // longer description
  bool hasAccent = false; Color accent{1, 1, 1, 1};  // the slot kaiju's rolled accent colour (slot screen swatch)
};
SlotSummary summarizeSlot(int slot, const std::string* json);  // json == nullptr -> empty slot
}  // namespace tz
