// Built-in copies of data/*.json (generated at build time by tools/embed.cmake).
#include <cstring>
#include <string>
#include "embedded_data.h"
namespace tz {
std::string embeddedJson(const char* name) {
  if (!std::strcmp(name, "config")) return reinterpret_cast<const char*>(kEmbedded_config);
  if (!std::strcmp(name, "moves")) return reinterpret_cast<const char*>(kEmbedded_moves);
  if (!std::strcmp(name, "monsters")) return reinterpret_cast<const char*>(kEmbedded_monsters);
  if (!std::strcmp(name, "levels")) return reinterpret_cast<const char*>(kEmbedded_levels);
  if (!std::strcmp(name, "eggs")) return reinterpret_cast<const char*>(kEmbedded_eggs);
  if (!std::strcmp(name, "credits")) return reinterpret_cast<const char*>(kEmbedded_credits);
  return "";
}
}  // namespace tz
