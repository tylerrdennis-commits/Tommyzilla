// Dev tool (Linux build only): prints what the loader sees in model files - counts, bounds, materials, clips, mesh nodes.
//   tz_model_inspect assets/polypizza/Dino.glb [more files...]
#include <cstdio>
#include "model/model_loader.h"
using namespace tz;
int main(int argc, char** argv) {
  int bad = 0;
  for (int a = 1; a < argc; a++) {
    mdl::Model m; std::string err;
    if (!mdl::loadModel(argv[a], m, err)) { std::printf("%s: FAILED %s\n", argv[a], err.c_str()); bad++; continue; }
    std::printf("%s\n  bounds (%.2f %.2f %.2f)..(%.2f %.2f %.2f)\n", mdl::describeModel(m).c_str(), m.boundsMin.x, m.boundsMin.y, m.boundsMin.z, m.boundsMax.x, m.boundsMax.y, m.boundsMax.z);
    std::printf("  materials:"); for (auto& mt : m.materials) std::printf(" %s(#%02x%02x%02x%s)", mt.name.c_str(), (int)(mt.baseColor.r * 255), (int)(mt.baseColor.g * 255), (int)(mt.baseColor.b * 255), mt.texture >= 0 ? ",tex" : ""); std::printf("\n");
    std::printf("  clips:"); for (auto& c : m.clips) std::printf(" %s(%.2fs)", c.name.c_str(), c.duration); std::printf("\n");
    std::printf("  mesh nodes:"); for (auto& n : m.nodes) if (n.mesh >= 0) std::printf(" %s", n.name.c_str()); std::printf("\n");
  }
  return bad;
}
