// Model math helpers, pose evaluation and post-load finalisation (see model.h).
#include "model.h"
#include <atomic>
#include <cctype>
#include <cfloat>
#include <functional>

namespace tz {
namespace mdl {

Quat normalizeQ(Quat q) {
  float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (l < 1e-12f) return Quat();
  float k = 1.f / l; return {q.x * k, q.y * k, q.z * k, q.w * k};
}
Quat slerpQ(Quat a, Quat b, float t) {
  float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  if (d < 0) { b = {-b.x, -b.y, -b.z, -b.w}; d = -d; }
  if (d > 0.9995f) return normalizeQ({lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t), lerpf(a.w, b.w, t)});
  float th = std::acos(clampf(d, -1, 1)), s = std::sin(th);
  float wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
  return {a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb};
}
Mat4 quatToMat4(const Quat& q) {
  float x = q.x, y = q.y, z = q.z, w = q.w;
  Mat4 r = Mat4::identity();
  // row-vector form = transpose of the usual column-vector rotation matrix
  r.m[0][0] = 1 - 2 * (y * y + z * z); r.m[1][0] = 2 * (x * y - z * w);     r.m[2][0] = 2 * (x * z + y * w);
  r.m[0][1] = 2 * (x * y + z * w);     r.m[1][1] = 1 - 2 * (x * x + z * z); r.m[2][1] = 2 * (y * z - x * w);
  r.m[0][2] = 2 * (x * z - y * w);     r.m[1][2] = 2 * (y * z + x * w);     r.m[2][2] = 1 - 2 * (x * x + y * y);
  return r;
}
Mat4 toMat4(const Transform& x) {
  Mat4 r = quatToMat4(x.r);
  for (int j = 0; j < 3; j++) { r.m[0][j] *= x.s.x; r.m[1][j] *= x.s.y; r.m[2][j] *= x.s.z; }  // S * R
  r.m[3][0] = x.t.x; r.m[3][1] = x.t.y; r.m[3][2] = x.t.z;                                     // * T
  return r;
}
Transform lerpTransform(const Transform& a, const Transform& b, float k) {
  Transform o;
  o.t = a.t + (b.t - a.t) * k; o.s = a.s + (b.s - a.s) * k; o.r = slerpQ(a.r, b.r, k);
  return o;
}
bool invertAffine(const Mat4& m, Mat4& out) {
  const float a = m.m[0][0], b = m.m[0][1], c = m.m[0][2], d = m.m[1][0], e = m.m[1][1], f = m.m[1][2], g = m.m[2][0], h = m.m[2][1], i = m.m[2][2];
  float A = e * i - f * h, B = -(d * i - f * g), Cc = d * h - e * g;
  float det = a * A + b * B + c * Cc;
  if (std::fabs(det) < 1e-20f) return false;
  float k = 1.f / det;
  out = Mat4::identity();
  out.m[0][0] = A * k; out.m[0][1] = -(b * i - c * h) * k; out.m[0][2] = (b * f - c * e) * k;
  out.m[1][0] = B * k; out.m[1][1] = (a * i - c * g) * k;  out.m[1][2] = -(a * f - c * d) * k;
  out.m[2][0] = Cc * k; out.m[2][1] = -(a * h - b * g) * k; out.m[2][2] = (a * e - b * d) * k;
  Vec3 t(m.m[3][0], m.m[3][1], m.m[3][2]);
  for (int j = 0; j < 3; j++) out.m[3][j] = -(t.x * out.m[0][j] + t.y * out.m[1][j] + t.z * out.m[2][j]);
  return true;
}

static std::string lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }
int Model::findClip(const std::string& name) const {
  for (size_t i = 0; i < clips.size(); i++) if (clips[i].name == name) return (int)i;
  std::string n = lower(name);
  for (size_t i = 0; i < clips.size(); i++) if (lower(clips[i].name) == n) return (int)i;
  for (size_t i = 0; i < clips.size(); i++) {
    std::string c = lower(clips[i].name); size_t bar = c.find_last_of('|');
    if (bar != std::string::npos && c.substr(bar + 1) == n) return (int)i;
  }
  return -1;
}
size_t Model::triangleCount() const { size_t n = 0; for (auto& m : meshes) n += m.indices.size() / 3; return n; }
size_t Model::skinnedMeshCount() const { size_t n = 0; for (auto& m : meshes) n += m.skinned ? 1 : 0; return n; }
std::vector<Transform> Model::restLocals() const { std::vector<Transform> v(nodes.size()); for (size_t i = 0; i < nodes.size(); i++) v[i] = nodes[i].local; return v; }

void computePose(const Model& m, const std::vector<Transform>& locals, Pose& out) {
  out.global.resize(m.nodes.size());
  for (int n : m.order) {
    Mat4 l = toMat4(locals[(size_t)n]);
    int p = m.nodes[(size_t)n].parent;
    out.global[(size_t)n] = p >= 0 ? l * out.global[(size_t)p] : l;
  }
  out.skinMats.resize(m.skins.size());
  for (size_t s = 0; s < m.skins.size(); s++) {
    const Skin& sk = m.skins[s];
    auto& v = out.skinMats[s]; v.resize(sk.joints.size());
    for (size_t j = 0; j < sk.joints.size(); j++) {
      int jn = sk.joints[j];
      v[j] = (jn >= 0 && jn < (int)out.global.size()) ? sk.inverseBind[j] * out.global[(size_t)jn] : Mat4::identity();
    }
  }
}

void skinMesh(const Mesh& me, const std::vector<Mat4>& pal, Vertex* out) {
  const size_t nj = pal.size();
  for (size_t i = 0; i < me.vertices.size(); i++) {
    const Vertex& v = me.vertices[i];
    Vertex o = v;
    float px = 0, py = 0, pz = 0, nx = 0, ny = 0, nz = 0, wsum = 0;
    for (int k = 0; k < 4; k++) {
      const float w = v.weights[k];
      if (w <= 0 || v.joints[k] >= nj) continue;
      const auto& m = pal[v.joints[k]].m;
      px += w * (v.pos[0] * m[0][0] + v.pos[1] * m[1][0] + v.pos[2] * m[2][0] + m[3][0]);
      py += w * (v.pos[0] * m[0][1] + v.pos[1] * m[1][1] + v.pos[2] * m[2][1] + m[3][1]);
      pz += w * (v.pos[0] * m[0][2] + v.pos[1] * m[1][2] + v.pos[2] * m[2][2] + m[3][2]);
      nx += w * (v.nrm[0] * m[0][0] + v.nrm[1] * m[1][0] + v.nrm[2] * m[2][0]);
      ny += w * (v.nrm[0] * m[0][1] + v.nrm[1] * m[1][1] + v.nrm[2] * m[2][1]);
      nz += w * (v.nrm[0] * m[0][2] + v.nrm[1] * m[1][2] + v.nrm[2] * m[2][2]);
      wsum += w;
    }
    if (wsum > 0) { o.pos[0] = px; o.pos[1] = py; o.pos[2] = pz; o.nrm[0] = nx; o.nrm[1] = ny; o.nrm[2] = nz; }
    out[i] = o;
  }
}

void finalizeModel(Model& m) {
  static std::atomic<uint32_t> next{1};
  m.uid = next++;
  for (auto& n : m.nodes) n.children.clear();
  for (size_t i = 0; i < m.nodes.size(); i++) if (m.nodes[i].parent >= 0) m.nodes[(size_t)m.nodes[i].parent].children.push_back((int)i);
  m.order.clear();
  std::vector<char> seen(m.nodes.size(), 0);
  std::function<void(int)> visit = [&](int n) {
    if (seen[(size_t)n]) return; seen[(size_t)n] = 1; m.order.push_back(n);
    for (int c : m.nodes[(size_t)n].children) visit(c);
  };
  for (size_t i = 0; i < m.nodes.size(); i++) if (m.nodes[i].parent < 0) visit((int)i);
  for (size_t i = 0; i < m.nodes.size(); i++) if (!seen[i]) { m.nodes[i].parent = -1; visit((int)i); }  // break cycles
  for (auto& sk : m.skins) if (sk.inverseBind.size() < sk.joints.size()) sk.inverseBind.resize(sk.joints.size(), Mat4::identity());
  // rest-pose bounds
  Pose p; computePose(m, m.restLocals(), p);
  Vec3 lo(FLT_MAX, FLT_MAX, FLT_MAX), hi(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  for (size_t ni = 0; ni < m.nodes.size(); ni++) {
    const Node& n = m.nodes[ni];
    if (n.mesh < 0 || n.mesh >= (int)m.meshes.size()) continue;
    const Mesh& me = m.meshes[(size_t)n.mesh];
    bool skin = me.skinned && n.skin >= 0 && n.skin < (int)m.skins.size();
    for (const Vertex& v : me.vertices) {
      Vec3 q(v.pos[0], v.pos[1], v.pos[2]), w;
      if (skin) {
        const auto& pal = p.skinMats[(size_t)n.skin];
        for (int k = 0; k < 4; k++) if (v.weights[k] > 0 && v.joints[k] < pal.size()) w += pal[v.joints[k]].transformPoint(q) * v.weights[k];
      } else {
        w = p.global[ni].transformPoint(q);
      }
      lo = Vec3(std::min(lo.x, w.x), std::min(lo.y, w.y), std::min(lo.z, w.z));
      hi = Vec3(std::max(hi.x, w.x), std::max(hi.y, w.y), std::max(hi.z, w.z));
    }
  }
  if (lo.x > hi.x) { lo = Vec3(); hi = Vec3(); }
  m.boundsMin = lo; m.boundsMax = hi;
  for (auto& c : m.clips) {
    float d = 0; for (auto& ch : c.channels) if (!ch.times.empty()) d = std::max(d, ch.times.back());
    c.duration = d;
  }
}

float linearToSrgb(float c) { c = clampf(c, 0, 1); return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1 / 2.4f) - 0.055f; }
float srgbToLinear(float c) { c = clampf(c, 0, 1); return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }

static void toHsv(Color c, float& h, float& s, float& v) {
  float mx = std::max(c.r, std::max(c.g, c.b)), mn = std::min(c.r, std::min(c.g, c.b)), d = mx - mn;
  v = mx; s = mx > 1e-6f ? d / mx : 0; h = 0;
  if (d > 1e-6f) {
    if (mx == c.r) h = std::fmod((c.g - c.b) / d, 6.f);
    else if (mx == c.g) h = (c.b - c.r) / d + 2;
    else h = (c.r - c.g) / d + 4;
    h /= 6; if (h < 0) h += 1;
  }
}
static Color fromHsv(float h, float s, float v, float a) {
  float r, g, b, i = std::floor(h * 6), f = h * 6 - i, p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
  switch (((int)i % 6 + 6) % 6) {
    case 0: r = v; g = t; b = p; break; case 1: r = q; g = v; b = p; break; case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break; case 4: r = t; g = p; b = v; break; default: r = v; g = p; b = q; break;
  }
  return {r, g, b, a};
}
Color hsvRecolor(Color src, Color target) {
  float sh, ss, sv, th, ts, tv; toHsv(src, sh, ss, sv); toHsv(target, th, ts, tv);
  return fromHsv(th, clampf(0.5f * (ss + ts), 0, 1), clampf(0.5f * (sv + tv), 0, 1), src.a);
}

}  // namespace mdl
}  // namespace tz
