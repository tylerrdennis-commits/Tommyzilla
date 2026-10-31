#include "meshes.h"
#include <cmath>
#include <utility>
namespace tz {
namespace {
void tri(MeshData& m, Vec3 a, Vec3 b, Vec3 c) {  // flat-shaded triangle; all tri() shapes are convex around the origin
  Vec3 n = normalize(cross(b - a, c - a));
  if (dot(n, (a + b + c) * (1.f / 3)) < 0) { n = n * -1.f; std::swap(b, c); }  // outward normal, counter-clockwise winding
  uint16_t base = (uint16_t)m.v.size();
  for (const Vec3& p : {a, b, c}) m.v.push_back({p.x, p.y, p.z, n.x, n.y, n.z});
  m.idx.push_back(base); m.idx.push_back(base + 1); m.idx.push_back(base + 2);
}
void quad(MeshData& m, Vec3 a, Vec3 b, Vec3 c, Vec3 d) { tri(m, a, b, c); tri(m, a, c, d); }
void box(MeshData& m) {
  const float h = 0.5f;
  Vec3 p[8] = {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h}, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}};
  quad(m, p[0], p[3], p[2], p[1]); quad(m, p[4], p[5], p[6], p[7]); quad(m, p[0], p[4], p[7], p[3]);
  quad(m, p[1], p[2], p[6], p[5]); quad(m, p[3], p[7], p[6], p[2]); quad(m, p[0], p[1], p[5], p[4]);
}
void sphere(MeshData& m, int seg, int rings, float maxTheta) {  // smooth; maxTheta = PI for a sphere, PI/2 for a dome
  for (int r = 0; r <= rings; r++) {
    float th = maxTheta * r / rings;
    for (int s = 0; s <= seg; s++) {
      float ph = 2 * PI * s / seg; Vec3 n(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
      m.v.push_back({n.x * 0.5f, n.y * 0.5f, n.z * 0.5f, n.x, n.y, n.z});
    }
  }
  for (int r = 0; r < rings; r++) for (int s = 0; s < seg; s++) {
    uint16_t a = (uint16_t)(r * (seg + 1) + s), b = (uint16_t)(a + seg + 1);
    m.idx.insert(m.idx.end(), {a, b, (uint16_t)(a + 1), (uint16_t)(a + 1), b, (uint16_t)(b + 1)});
  }
}
void cylinder(MeshData& m, int seg, float rTop, float rBot, bool caps) {
  float slope = (rBot - rTop);
  for (int s = 0; s <= seg; s++) {
    float a = 2 * PI * s / seg, c = std::cos(a), sn = std::sin(a); Vec3 n = normalize(Vec3(c, slope, sn));
    m.v.push_back({c * rTop, 0.5f, sn * rTop, n.x, n.y, n.z}); m.v.push_back({c * rBot, -0.5f, sn * rBot, n.x, n.y, n.z});
  }
  for (int s = 0; s < seg; s++) { uint16_t a = (uint16_t)(s * 2); m.idx.insert(m.idx.end(), {a, (uint16_t)(a + 1), (uint16_t)(a + 2), (uint16_t)(a + 2), (uint16_t)(a + 1), (uint16_t)(a + 3)}); }
  if (!caps) return;
  for (int capI = 0; capI < 2; capI++) {
    float y = capI ? -0.5f : 0.5f, r = capI ? rBot : rTop, ny = capI ? -1.f : 1.f; if (r <= 0) continue;
    uint16_t c0 = (uint16_t)m.v.size(); m.v.push_back({0, y, 0, 0, ny, 0});
    for (int s = 0; s <= seg; s++) { float a = 2 * PI * s / seg; m.v.push_back({std::cos(a) * r, y, std::sin(a) * r, 0, ny, 0}); }
    for (int s = 0; s < seg; s++) m.idx.insert(m.idx.end(), {c0, (uint16_t)(c0 + 1 + s), (uint16_t)(c0 + 2 + s)});
  }
}
void cone(MeshData& m, int seg) {
  Vec3 tip(0, 0.5f, 0);
  for (int s = 0; s < seg; s++) {
    float a0 = 2 * PI * s / seg, a1 = 2 * PI * (s + 1) / seg;
    Vec3 p0(std::cos(a0) * 0.5f, -0.5f, std::sin(a0) * 0.5f), p1(std::cos(a1) * 0.5f, -0.5f, std::sin(a1) * 0.5f);
    tri(m, tip, p1, p0); tri(m, Vec3(0, -0.5f, 0), p0, p1);
  }
}
void pyramid(MeshData& m) {
  Vec3 tip(0, 0.5f, 0), a(-0.5f, -0.5f, -0.5f), b(0.5f, -0.5f, -0.5f), c(0.5f, -0.5f, 0.5f), d(-0.5f, -0.5f, 0.5f);
  tri(m, tip, b, a); tri(m, tip, c, b); tri(m, tip, d, c); tri(m, tip, a, d); quad(m, a, b, c, d);
}
void disc(MeshData& m, int seg) {
  uint16_t c0 = 0; m.v.push_back({0, 0, 0, 0, 1, 0});
  for (int s = 0; s <= seg; s++) { float a = 2 * PI * s / seg; m.v.push_back({std::cos(a) * 0.5f, 0, std::sin(a) * 0.5f, 0, 1, 0}); }
  for (int s = 0; s < seg; s++) m.idx.insert(m.idx.end(), {c0, (uint16_t)(1 + s), (uint16_t)(2 + s)});
}
void ring(MeshData& m, int seg, int tube) {
  const float R = 0.5f, r = 0.04f;
  for (int i = 0; i <= seg; i++) {
    float u = 2 * PI * i / seg;
    for (int j = 0; j <= tube; j++) {
      float v = 2 * PI * j / tube; Vec3 n(std::cos(v) * std::cos(u), std::sin(v), std::cos(v) * std::sin(u));
      m.v.push_back({(R + r * std::cos(v)) * std::cos(u), r * std::sin(v), (R + r * std::cos(v)) * std::sin(u), n.x, n.y, n.z});
    }
  }
  for (int i = 0; i < seg; i++) for (int j = 0; j < tube; j++) {
    uint16_t a = (uint16_t)(i * (tube + 1) + j), b = (uint16_t)(a + tube + 1);
    m.idx.insert(m.idx.end(), {a, b, (uint16_t)(a + 1), (uint16_t)(a + 1), b, (uint16_t)(b + 1)});
  }
}
void rock(MeshData& m) {  // faceted icosahedron
  const float t = (1 + std::sqrt(5.f)) / 2;
  Vec3 v[12] = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
  for (auto& p : v) p = normalize(p) * 0.5f;
  const int f[20][3] = {{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};
  for (auto& q : f) tri(m, v[q[0]], v[q[1]], v[q[2]]);
}
}  // namespace
MeshData buildMesh(MeshId id) {
  MeshData m;
  switch (id) {
    case MESH_BOX: box(m); break;
    case MESH_CYL: cylinder(m, 14, 0.5f, 0.5f, true); break;
    case MESH_CONE: cone(m, 12); break;
    case MESH_SPHERE: sphere(m, 16, 10, PI); break;
    case MESH_PYRAMID: pyramid(m); break;
    case MESH_DISC: disc(m, 16); break;
    case MESH_RING: ring(m, 40, 6); break;
    case MESH_ROCK: rock(m); break;
    case MESH_HEMI: sphere(m, 16, 6, PI / 2); break;
    default: box(m); break;
  }
  return m;
}
}  // namespace tz
