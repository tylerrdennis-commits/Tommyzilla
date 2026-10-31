#include <cstdio>
#include <string>
#include <algorithm>
#include <memory>
#include "game/game.h"
using namespace tz;
static std::string rd(const char* n) { std::string p = std::string("/tmp/atlas-wt/data/") + n; FILE* f = std::fopen(p.c_str(), "rb"); std::string s; char b[4096]; size_t k; while ((k = std::fread(b, 1, sizeof b, f)) > 0) s.append(b, k); std::fclose(f); return s; }
int main() {
  std::string e = loadGameData(rd("config.json"), rd("moves.json"), rd("monsters.json"), rd("levels.json"), rd("eggs.json")); if (!e.empty()) { std::printf("ERR %s\n", e.c_str()); return 1; }
  auto g = std::make_unique<Game>(); g->init(nullptr); g->save.adventureUnlocked = true; g->startMode(true, false);
  auto& W = g->world;
  for (auto& q : W.groundQuads) std::printf("Q %f %f %f %f %d %f %f %f\n", q.x0, q.z0, q.x1, q.z1, q.surface, q.color.r, q.color.g, q.color.b);
  for (auto& b : W.buildings) std::printf("B %f %f %f %f %f %f %f %f %d\n", b.minX, b.minZ, b.maxX, b.maxZ, b.maxY, b.color.r, b.color.g, b.color.b, (int)(std::find(W.stoneBuildings.begin(), W.stoneBuildings.end(), b.id) != W.stoneBuildings.end()));
  for (auto& l : W.labels) if (l.kind == 0) std::printf("L %f %f %f %s\n", l.pos.x, l.pos.y, l.pos.z, l.text.c_str());
}
