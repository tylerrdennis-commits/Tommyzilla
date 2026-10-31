// Textured ground: surface types, their texture files and world-space tiling, the CPU texture set (with mip chain)
// and the quads the renderer draws. Platform independent; the DX12 side lives in renderer/dx12_renderer.cpp.
// Textures are tileable greyscale detail maps (data/textures/ground_*.png, made by tools/gen_ground_textures.py).
// The renderer multiplies each texel by the surface's palette colour and divides by the texture's mean brightness,
// so the palette, the lighting and the L20 storm look stay the same. If any file is missing or bad, or the GPU
// upload fails, the game keeps the old flat-coloured ground boxes (Game::groundTextured == false).
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "../core/math.h"
namespace tz {
enum GroundSurface { GS_CONCRETE, GS_ASPHALT, GS_GRASS, GS_WATER, GS_PAVING, GS_COUNT };
struct GroundMaterial {
  const char* name;      // "concrete"
  const char* file;      // relative to data/: "textures/ground_concrete.png"
  float tile;            // metres per texture repeat (world-space UVs: u = x / tile, v = z / tile)
  uint32_t color;        // the palette colour this surface used as a flat box (still the tint)
};
const GroundMaterial& groundMaterial(int surface);
inline void groundUV(int surface, float x, float z, float& u, float& v) { float t = groundMaterial(surface).tile; u = x / t; v = z / t; }
// One flat ground rectangle (top face only): x0..x1 / z0..z1 at height y. lane: 0 none, 1 dashed centre line along X
// (at z = laneAt), 2 along Z (at x = laneAt). Water UVs scroll with RenderList::time.
struct GroundQuad { float x0, z0, x1, z1, y; Color color; int surface; int lane = 0; float laneAt = 0; };
// Decoded, validated textures: one RGBA8 square image per surface (all the same power-of-two size), with mips.
struct GroundTextureSet {
  bool ok = false; std::string error; int size = 0;
  std::vector<std::vector<uint8_t>> mips[GS_COUNT];  // mips[s][0] = size x size RGBA8, then size/2 ... 1
  float meanLum[GS_COUNT] = {};                      // mean brightness (0..1) of mip 0, used to keep the palette
};
// decode(path, w, h, rgba): false on failure (missing file, bad PNG). The game passes stb_image; tests pass fakes.
using ImageDecodeFn = std::function<bool(const std::string& path, int& w, int& h, std::vector<uint8_t>& rgba, std::string& err)>;
GroundTextureSet loadGroundTextures(const std::string& dataDir, const ImageDecodeFn& decode);
std::vector<std::vector<uint8_t>> buildMips(const std::vector<uint8_t>& rgba, int size);  // 2x2 box filter down to 1x1
}  // namespace tz
