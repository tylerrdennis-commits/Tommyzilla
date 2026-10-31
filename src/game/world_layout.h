// Layout helper shared by the Victoria map (world.cpp) and the procedural adventure city (city_gen.cpp).
#pragma once
#include <cmath>
#include "game.h"
namespace tz {
namespace wl {
// three.js-geometry style helpers: dimensions first, then a placement matrix
inline Part gbox(uint32_t c, float w, float h, float d, const Mat4& M) { return part(MESH_BOX, hexColor(c), Mat4::scale(w, h, d) * M); }
inline Part gcyl(uint32_t c, float rt, float rb, float h, const Mat4& M) { float r = rt + rb; return part(MESH_CYL, hexColor(c), Mat4::scale(r, h, r) * M); }
inline Part gcone(uint32_t c, float r, float h, const Mat4& M) { return part(MESH_CONE, hexColor(c), Mat4::scale(2 * r, h, 2 * r) * M); }
inline Part gsph(uint32_t c, float r, const Mat4& M) { return part(MESH_SPHERE, hexColor(c), Mat4::scale(2 * r, 2 * r, 2 * r) * M); }
inline Part ghemi(uint32_t c, float r, const Mat4& M) { return part(MESH_HEMI, hexColor(c), Mat4::scale(2 * r, 2 * r, 2 * r) * M); }
inline Part pyramidRoof(float w, float d, float h, uint32_t c) { return part(MESH_PYRAMID, hexColor(c), Mat4::scale(w, h, d) * Mat4::translation(0, h / 2, 0)); }
inline Mat4 M(float x, float y, float z, float rx = 0, float ry = 0, float rz = 0, float sx = 1, float sy = 1, float sz = 1) { return partM(x, y, z, rx, ry, rz, sx, sy, sz); }
inline Mat4 brickMat(float x, float y, float z, float s) { Mat4 m = Mat4::identity(); m.m[0][0] = m.m[1][1] = m.m[2][2] = s; m.m[3][0] = x; m.m[3][1] = y; m.m[3][2] = z; return m; }

struct Rect { float x0, z0, x1, z1; };
struct Layout {
  Rng r{20260420};
  std::vector<Rect> occupied;
  World* w;
  float rr(float a, float b) { return r.range(a, b); }
  int ri(int a, int b) { return r.irange(a, b); }
  uint32_t pick(std::initializer_list<uint32_t> l) { return *(l.begin() + r.irange(0, (int)l.size() - 1)); }
  bool occupiedAt(float x0, float z0, float x1, float z1, float m) {
    for (auto& o : occupied) if (x1 + m > o.x0 && x0 - m < o.x1 && z1 + m > o.z0 && z0 - m < o.z1) return true;
    for (auto& o : w->water) if (x1 + m > o.x0 && x0 - m < o.x1 && z1 + m > o.z0 && z0 - m < o.z1) return true;
    float H = C.worldHalf;
    return std::fabs(x0) > H - 10 || std::fabs(x1) > H - 10 || std::fabs(z0) > H - 10 || std::fabs(z1) > H - 10;
  }
  void reserve(float x0, float z0, float x1, float z1) { occupied.push_back({x0, z0, x1, z1}); }
  Building& add(const std::string& name, float x, float z, int bw, int bd, int bh, uint32_t color, std::vector<uint32_t> wins = {}) {
    Building b; float B = C.brick;
    b.id = (int)w->buildings.size(); b.name = name; b.x = x; b.z = z; b.w = bw; b.d = bd; b.h = bh; b.color = hexColor(color);
    b.windowCols = wins.empty() ? std::vector<uint32_t>{0x3ad7ff, 0xffd36b, 0x1a1d26, 0x1a1d26} : wins;
    b.minX = x - bw * B / 2; b.maxX = x + bw * B / 2; b.minZ = z - bd * B / 2; b.maxZ = z + bd * B / 2; b.maxY = bh * B;
    b.radius = std::sqrt((bw * B / 2) * (bw * B / 2) + (bh * B / 2) * (bh * B / 2) + (bd * B / 2) * (bd * B / 2)) + B;
    occupied.push_back({b.minX, b.minZ, b.maxX, b.maxZ});
    w->buildings.push_back(b);
    return w->buildings.back();
  }
  void label(const std::string& t, float x, float y, float z, int kind) { w->labels.push_back({t, Vec3(x, y, z), kind}); }
  struct PropDef { int type; float x, z, s; };
  std::vector<PropDef> props;
};
}  // namespace wl
using namespace wl;
// Procedural adventure city (city_gen.cpp): fills buildings, labels, water, ground, props.
void buildProceduralCity(Layout& L, World& w, uint64_t seed);
}  // namespace tz
