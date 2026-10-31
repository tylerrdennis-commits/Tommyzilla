#include "ground.h"
#include <algorithm>
namespace tz {
const GroundMaterial& groundMaterial(int surface) {
  static const GroundMaterial m[GS_COUNT] = {
      {"concrete", "textures/ground_concrete.png", 8.f, 0x3b3f4a},   // base ground (was the slate 0x3b3f4a box)
      {"asphalt", "textures/ground_asphalt.png", 8.f, 0x2b2e36},     // streets (+ shader lane markings)
      {"grass", "textures/ground_grass.png", 10.f, 0x4f9a4a},        // parks / lawns
      {"water", "textures/ground_water.png", 16.f, 0x1f5d8c},        // harbour, ocean (scrolling UVs)
      {"paving", "textures/ground_paving.png", 4.f, 0x8a8478},       // Dallas Road promenade
  };
  return m[std::max(0, std::min(GS_COUNT - 1, surface))];
}
std::vector<std::vector<uint8_t>> buildMips(const std::vector<uint8_t>& rgba, int size) {
  std::vector<std::vector<uint8_t>> out;
  if (size <= 0 || rgba.size() != (size_t)size * size * 4) return out;
  out.push_back(rgba);
  for (int s = size; s > 1; s /= 2) {
    const std::vector<uint8_t>& a = out.back(); int h = s / 2;
    std::vector<uint8_t> b((size_t)h * h * 4);
    for (int y = 0; y < h; y++) for (int x = 0; x < h; x++) for (int c = 0; c < 4; c++) {
      int sum = a[((size_t)(2 * y) * s + 2 * x) * 4 + c] + a[((size_t)(2 * y) * s + 2 * x + 1) * 4 + c] + a[((size_t)(2 * y + 1) * s + 2 * x) * 4 + c] + a[((size_t)(2 * y + 1) * s + 2 * x + 1) * 4 + c];
      b[((size_t)y * h + x) * 4 + c] = (uint8_t)((sum + 2) / 4);
    }
    out.push_back(std::move(b));
  }
  return out;
}
GroundTextureSet loadGroundTextures(const std::string& dataDir, const ImageDecodeFn& decode) {
  GroundTextureSet t;
  if (!decode) { t.error = "no image decoder"; return t; }
  for (int s = 0; s < GS_COUNT; s++) {
    const GroundMaterial& m = groundMaterial(s);
    std::string path = dataDir + m.file, err; int w = 0, h = 0; std::vector<uint8_t> px;
    if (!decode(path, w, h, px, err)) { t.error = std::string(m.name) + ": " + (err.empty() ? "could not load " + path : err); return t; }
    if (w != h || w < 4 || w > 2048 || (w & (w - 1)) || px.size() != (size_t)w * h * 4) { t.error = std::string(m.name) + ": must be a square power-of-two RGBA image (got " + std::to_string(w) + "x" + std::to_string(h) + ")"; return t; }
    if (t.size == 0) t.size = w;
    else if (w != t.size) { t.error = std::string(m.name) + ": all ground textures must be the same size"; return t; }
    double sum = 0; for (size_t i = 0; i < px.size(); i += 4) sum += 0.299 * px[i] + 0.587 * px[i + 1] + 0.114 * px[i + 2];
    t.meanLum[s] = (float)(sum / ((double)w * h) / 255.0);
    if (t.meanLum[s] < 0.05f) { t.error = std::string(m.name) + ": texture is (nearly) black"; return t; }
    t.mips[s] = buildMips(px, w);
  }
  t.ok = true;
  return t;
}
}  // namespace tz
