// Monster rigs: 18 monsters built from 7 primitive body plans (+ small per-monster variants).
#include "game.h"
namespace tz {
namespace {
struct Rig {
  RenderList& rl;
  void add(MeshId m, Color c, const Mat4& local, const Mat4& parent, float unlit = 0) { rl.opaque[m].push_back(makeInstance(local * parent, c, unlit)); }
  // shape helpers: size first, then local placement (x,y,z, rx,ry,rz) relative to parent
  void box(Color c, float sx, float sy, float sz, const Mat4& at, const Mat4& parent, float unlit = 0) { add(MESH_BOX, c, Mat4::scale(sx, sy, sz) * at, parent, unlit); }
  void sph(Color c, float sx, float sy, float sz, const Mat4& at, const Mat4& parent, float unlit = 0) { add(MESH_SPHERE, c, Mat4::scale(sx, sy, sz) * at, parent, unlit); }
  void cone(Color c, float r, float h, const Mat4& at, const Mat4& parent, float unlit = 0, float flat = 1) { add(MESH_CONE, c, Mat4::scale(2 * r * flat, h, 2 * r) * at, parent, unlit); }
  void cyl(Color c, float r, float h, const Mat4& at, const Mat4& parent, float unlit = 0) { add(MESH_CYL, c, Mat4::scale(2 * r, h, 2 * r) * at, parent, unlit); }
  // capsule (cylinder + two spheres) of radius r and straight length len along local Y
  void capsule(Color c, float r, float len, const Mat4& at, const Mat4& parent) {
    Mat4 w = at * parent;
    add(MESH_CYL, c, Mat4::scale(2 * r, len, 2 * r), w);
    add(MESH_SPHERE, c, Mat4::scale(2 * r, 2 * r, 2 * r) * Mat4::translation(0, len / 2, 0), w);
    add(MESH_SPHERE, c, Mat4::scale(2 * r, 2 * r, 2 * r) * Mat4::translation(0, -len / 2, 0), w);
  }
};
inline Mat4 L(float x, float y, float z, float rx = 0, float ry = 0, float rz = 0) { return Mat4::rotZ(rz) * Mat4::rotY(ry) * Mat4::rotX(rx) * Mat4::translation(x, y, z); }
const Color CLAW = hexColor(0xc8ccd4), WHITE = hexColor(0xffffff), DARK = hexColor(0x12141a);

void eyes(Rig& r, const Mat4& head, float x, float y, float z, float size, Color glow, bool cartoon) {
  for (int s = -1; s <= 1; s += 2) {
    if (cartoon) { r.sph(WHITE, size, size, size * 0.7f, L(s * x, y, z), head, 1); r.sph(DARK, size * 0.45f, size * 0.45f, size * 0.3f, L(s * x, y, z + size * 0.3f), head, 1); }
    else r.box(glow, size, size * 0.64f, 0.3f, L(s * x, y, z), head, 1);
  }
}

void biped(Rig& r, const MonsterDef& m, const Mat4& root, const RigPose& p, Color glow) {
  bool slim = m.variant == "slim", fins = m.variant == "fins";
  float bw = slim ? 0.8f : 1.f;
  Mat4 body = Mat4::rotZ(std::sin(p.walkPhase) * 0.04f * p.walk) * root;
  r.capsule(m.body, 3.2f * bw, 4.5f, Mat4::scale(1, 1, 1) * L(0, 9.2f, 0, 0.45f), body);
  r.capsule(m.belly, 2.5f * bw, 3.5f, L(0, 8.6f, 1.3f, 0.45f), body);
  Mat4 head = L(0, 13.6f, 2.8f, p.headX) * body;
  r.box(m.body, 3.4f, 2.8f, 4.0f, L(0, 0.3f, 1.2f), head); r.box(m.body, 2.7f, 1.4f, 2.2f, L(0, -0.1f, 3.8f), head);
  r.box(m.body, 3.6f, 0.6f, 1.4f, L(0, 1.6f, 2.4f, -0.2f), head);
  eyes(r, head, 1.05f, 0.8f, 3.15f, 0.7f, glow, false);
  for (float s : {-0.7f, 0.f, 0.7f}) r.cone(CLAW, 0.22f, 0.5f, L(s, -0.85f, 4.6f, PI), head);
  Mat4 jaw = L(0, -1.1f, 0.8f, p.jaw) * head;
  r.box(m.belly, 2.6f, 0.7f, 4.0f, L(0, -0.2f, 2.2f), jaw);
  if (fins) for (int s = -1; s <= 1; s += 2) r.box(glow, 0.2f, 2.2f, 2.6f, L(s * 1.9f, 1.0f, 0.5f, 0, 0, s * 0.4f), head, 1);
  if (slim) r.cone(glow, 0.5f, 2.4f, L(0, 2.4f, 0.4f, -0.5f), head, 1, 0.35f);
  float armA[2] = {p.armL, p.armR};
  for (int s = 0; s < 2; s++) {
    float sx = s == 0 ? -1.f : 1.f;
    Mat4 a = L(sx * 3.0f * bw, 11.0f, 2.2f, armA[s]) * body;
    r.capsule(m.body, 0.75f, 2.4f, L(0, -1.6f, 0.6f, 0.5f), a);
    for (float o : {-0.4f, 0.f, 0.4f}) r.cone(CLAW, 0.22f, 1.0f, L(o, -3.4f, 1.9f, 2.4f), a);
    if (fins) r.box(glow, 0.15f, 1.6f, 1.6f, L(sx * 0.7f, -1.2f, -0.2f), a, 1);
  }
  float sw = std::sin(p.walkPhase) * 0.55f * p.walk;
  for (int s = 0; s < 2; s++) {
    float sx = s == 0 ? -1.f : 1.f;
    Mat4 l = L(sx * 2.3f * bw, 6.6f, -0.6f, s == 0 ? sw : -sw) * root;
    r.capsule(m.body, 1.55f * (slim ? 0.8f : 1.f), 2.6f, L(0, -2.4f, 0), l);
    r.box(m.body, 2.4f, 1.1f, 3.6f, L(0, -6.05f, 0.7f), l);
    for (float o : {-0.7f, 0.f, 0.7f}) r.cone(CLAW, 0.3f, 0.9f, L(o, -6.1f, 2.8f, PI / 2), l);
  }
  Mat4 parent = L(0, 7.6f, -2.6f) * body; float rad = 2.4f * bw;
  for (int i = 0; i < 8; i++) {
    float y = std::sin(p.time * 2.2f - i * 0.6f) * 0.08f * (1 + p.walk) - p.tailCurl * 0.18f;
    Mat4 seg = Mat4::rotY(y) * Mat4::rotX(i == 0 ? -0.15f : 0.06f) * (i > 0 ? Mat4::translation(0, -0.25f, -rad * 0.95f) : Mat4::identity()) * parent;
    r.sph(m.body, 2 * rad, 2 * rad, 2 * rad, L(0, 0, -rad * 0.5f), seg);
    r.cone(glow, rad * 0.55f, rad * 1.3f, L(0, rad * 0.95f, -rad * 0.5f), seg, 1, 0.35f);
    parent = seg; rad *= 0.82f;
  }
  const float spine[5][3] = {{14.2f, 0.9f, 1.6f}, {13.0f, -0.3f, 2.3f}, {11.6f, -1.4f, 2.7f}, {10.0f, -2.4f, 2.5f}, {8.6f, -3.1f, 2.1f}};
  for (auto& s : spine) r.cone(glow, s[2] * 0.55f, s[2] * 1.4f, L(0, s[0] + s[2] * 0.3f, s[1] - 0.6f, -0.35f), body, 1, 0.35f);
}

void quad(Rig& r, const MonsterDef& m, const Mat4& root, const RigPose& p, Color glow) {
  bool neck = m.variant == "neck", shell = m.variant == "shell", horn = m.variant == "horn";
  Mat4 body = Mat4::rotZ(std::sin(p.walkPhase) * 0.03f * p.walk) * root;
  r.sph(m.body, 9, 6.5f, 13, L(0, 7, 0), body);
  r.sph(m.belly, 7.5f, 4, 11, L(0, 5.6f, 0.3f), body);
  if (shell) { r.add(MESH_HEMI, m.accent, Mat4::scale(12, 9, 15) * L(0, 7.5f, 0), body); for (int i = 0; i < 5; i++) r.sph(scaleColor(m.accent, 0.8f), 3, 1.2f, 3, L((i % 2 ? 1.f : -1.f) * 2.5f, 11.2f, -4.f + i * 2), body); }
  else for (int i = 0; i < 5; i++) r.cone(glow, 1.2f, 2.4f, L(0, 10.2f, -4.5f + i * 2.2f, -0.2f), body, 1, 0.35f);
  float sw = std::sin(p.walkPhase) * 0.5f * p.walk;
  for (int i = 0; i < 4; i++) {
    float sx = i % 2 ? 1.f : -1.f, sz = i < 2 ? 4.0f : -4.0f, a = ((i == 0 || i == 3) ? sw : -sw);
    Mat4 leg = L(sx * 3.6f, 6, sz, a) * root;
    r.cyl(m.body, 1.4f, 5, L(0, -2.5f, 0), leg); r.box(m.body, 3, 1, 3.4f, L(0, -5.5f, 0.4f), leg);
  }
  Vec3 n0(0, 9, 5.5f), n1 = neck ? Vec3(0, 16, 10) : Vec3(0, 11.5f, 9);
  for (int i = 0; i < 4; i++) { float t = i / 3.f; Vec3 q = n0 + (n1 - n0) * t; float s = 3.4f - t * 0.8f; r.sph(m.body, s, s, s, L(q.x, q.y, q.z), body); }
  Mat4 head = L(n1.x, n1.y + 0.6f, n1.z + 1.2f, p.headX) * body;
  r.box(m.body, 3.2f, 2.6f, 4.2f, L(0, 0, 0.6f), head);
  eyes(r, head, 1.0f, 0.7f, 2.4f, 0.8f, glow, true);
  Mat4 jaw = L(0, -1.0f, 0.2f, p.jaw) * head; r.box(m.belly, 2.8f, 0.7f, 3.8f, L(0, -0.2f, 0.9f), jaw);
  if (horn) { r.cone(CLAW, 0.6f, 3.2f, L(0, 1.8f, 2.4f, 0.9f), head); r.cone(CLAW, 0.35f, 1.6f, L(0, 1.5f, 0.6f, 0.4f), head); }
  float wag = std::sin(p.time * 2.5f) * 0.25f - p.tailCurl * 0.8f;
  r.cone(m.body, 1.8f, 8, L(0, 6.5f, -10, -PI / 2 - 0.25f, wag), body);
}

void blob(Rig& r, const MonsterDef& m, const Mat4& root, const RigPose& p, Color glow) {
  bool tall = m.variant == "tall", tent = m.variant == "tentacles", drip = m.variant == "drip", spikes = m.variant == "spikes";
  float jig = std::sin(p.time * 6 + p.walkPhase) * 0.05f + std::fabs(std::sin(p.walkPhase)) * 0.08f * p.walk;
  float sy = (tall ? 17.f : 11.f) * (1 + jig) / p.squash, sxz = 12 * (1 - jig * 0.5f) * p.squash;
  float cy = sy * 0.5f + (tent ? 3.f : 0.5f);
  r.sph(m.body, sxz, sy, sxz, L(0, cy, 0), root);
  r.sph(m.belly, sxz * 0.7f, sy * 0.6f, sxz * 0.3f, L(0, cy - sy * 0.12f, sxz * 0.36f), root);
  Mat4 head = L(0, cy + sy * 0.18f, 0, p.headX * 0.5f) * root;
  eyes(r, head, 1.9f, 0.8f, sxz * 0.42f, 1.6f, glow, true);
  Mat4 jaw = L(0, -1.6f, sxz * 0.43f, p.jaw) * head; r.box(DARK, 3.2f, 0.4f + p.jaw * 2.2f, 0.6f, L(0, -0.4f, 0), jaw, 1);
  r.cone(glow, 0.5f, 3, L(0, sy * 0.5f + 1.2f, 0), head, 1); r.sph(glow, 1.2f, 1.2f, 1.2f, L(0, sy * 0.5f + 2.8f, 0), head, 1);
  float armA[2] = {p.armL, p.armR};
  for (int s = 0; s < 2; s++) { float sx = s ? 1.f : -1.f; Mat4 a = L(sx * sxz * 0.48f, cy, 1, armA[s]) * root; r.sph(m.body, 2.6f, 2.6f, 3.2f, L(0, -0.6f, 1.6f), a); }
  if (tent) for (int i = 0; i < 6; i++) { float a = i / 6.f * 6.283f; for (int k = 0; k < 4; k++) { float w = std::sin(p.time * 4 + i + k * 0.8f) * 0.6f; r.sph(m.accent, 1.6f - k * 0.25f, 1.8f, 1.6f - k * 0.25f, L(std::cos(a) * (4 + w), 3.2f - k * 0.9f, std::sin(a) * (4 + w)), root); } }
  else for (int s = -1; s <= 1; s += 2) r.sph(m.belly, 3, 1.4f, 4, L(s * 2.6f, 0.7f, 1 + (s < 0 ? std::sin(p.walkPhase) : -std::sin(p.walkPhase)) * p.walk), root);
  if (drip) for (int i = 0; i < 5; i++) { float a = i * 1.3f, y = std::fmod(p.time * 0.8f + i * 0.37f, 1.f); r.sph(m.body, 1, 1.4f, 1, L(std::cos(a) * sxz * 0.45f, cy - y * cy, std::sin(a) * sxz * 0.45f), root); }
  if (spikes) for (int i = 0; i < 8; i++) { float a = i / 8.f * 6.283f; r.cone(glow, 0.7f, 2.6f, L(std::cos(a) * sxz * 0.5f, cy + 1, std::sin(a) * sxz * 0.5f, std::sin(a) * 1.4f, 0, -std::cos(a) * 1.4f), root, 1); }
}

void crab(Rig& r, const MonsterDef& m, const Mat4& root, const RigPose& p, Color glow) {
  bool shell = m.variant == "shell";
  float bob = std::fabs(std::sin(p.walkPhase * 2)) * 0.3f * p.walk;
  Mat4 body = L(0, bob, 0) * root;
  r.sph(m.body, 14, 6, 10, L(0, 5.5f, 0), body);
  r.sph(m.belly, 11, 3, 8, L(0, 4.2f, 0.5f), body);
  if (shell) { r.add(MESH_HEMI, m.accent, Mat4::scale(15, 9, 12) * L(0, 6.5f, -0.5f), body); for (int i = -2; i <= 2; i++) r.box(scaleColor(m.accent, 0.8f), 0.6f, 3.6f, 10, L(i * 2.6f, 8.6f, -0.5f, 0, 0, i * 0.25f), body); }
  else for (int i = -2; i <= 2; i++) r.sph(glow, 1.4f, 0.8f, 1.4f, L(i * 2.4f, 8.3f, -1), body, 1);
  for (int s = -1; s <= 1; s += 2) {  // eye stalks
    Mat4 st = L(s * 1.8f, 7.5f, 3.5f, -0.3f + p.headX, 0, s * 0.2f) * body;
    r.cyl(m.body, 0.35f, 3, L(0, 1.5f, 0), st); r.sph(WHITE, 1.4f, 1.4f, 1.4f, L(0, 3.2f, 0), st, 1); r.sph(DARK, 0.6f, 0.6f, 0.4f, L(0, 3.3f, 0.55f), st, 1);
  }
  Mat4 mouth = L(0, 5, 4.6f) * body; r.box(DARK, 2.6f, 0.3f + p.jaw * 1.8f, 0.4f, L(0, 0, 0), mouth, 1);
  for (int i = 0; i < 6; i++) {
    float sx = i % 2 ? 1.f : -1.f, z = -3.f + (i / 2) * 2.6f, step = std::sin(p.walkPhase * 2 + i) * 0.35f * p.walk;
    Mat4 leg = L(sx * 6.0f, 5, z, 0, 0, sx * (0.7f + step)) * body;
    r.box(m.body, 6, 0.9f, 0.9f, L(sx * 2.6f, 0, 0), leg);
    r.box(m.body, 0.8f, 5.2f, 0.8f, L(sx * 5.4f, -2.2f, 0, 0, 0, -sx * 0.5f), leg);
  }
  float armA[2] = {p.armL, p.armR};
  for (int s = 0; s < 2; s++) {
    float sx = s ? 1.f : -1.f;
    Mat4 a = L(sx * 5.5f, 5.5f, 4.5f, armA[s] * 0.6f, sx * -0.3f) * body;
    r.box(m.body, 1.8f, 1.8f, 4, L(0, 0, 2), a);
    Mat4 claw = L(0, 0.5f, 4.4f) * a;
    r.sph(m.accent, 4, 3.4f, 4.6f, L(0, 0, 1.8f), claw);
    float open = 0.25f + std::max(0.f, -armA[s]) * 0.3f + std::sin(p.time * 3 + s) * 0.08f;
    Mat4 pin = L(0, 0.8f, 3.2f, -open) * claw; r.box(m.accent, 1.4f, 1.0f, 3.6f, L(0, 0, 1.6f), pin);
  }
}

void bird(Rig& r, const MonsterDef& m, const Mat4& root, const RigPose& p, Color glow) {
  bool owl = m.variant == "owl", beak = m.variant == "beak";
  float hop = std::fabs(std::sin(p.walkPhase)) * 0.6f * p.walk;
  Mat4 body = L(0, hop, 0, 0.1f) * root;
  r.sph(m.body, 9, 11, 9, L(0, 8, 0), body);
  r.sph(m.belly, 6.5f, 8, 3, L(0, 7.4f, 3.2f), body);
  Mat4 head = L(0, 12.5f, 1.5f, p.headX) * body;
  r.sph(m.body, 7, 6.4f, 6.6f, L(0, 0, 0), head);
  if (owl) { for (int s = -1; s <= 1; s += 2) { r.sph(WHITE, 2.8f, 2.8f, 1, L(s * 1.5f, 0.4f, 2.9f), head, 1); r.sph(glow, 1.2f, 1.2f, 0.6f, L(s * 1.5f, 0.4f, 3.4f), head, 1); r.cone(m.body, 0.8f, 2.4f, L(s * 2.2f, 3.3f, 0, 0, 0, -s * 0.4f), head); } }
  else eyes(r, head, 1.6f, 0.8f, 2.8f, 1.5f, glow, true);
  float bl = beak ? 4.5f : 2.6f;
  r.cone(beak ? m.accent : hexColor(0xffb347), beak ? 1.5f : 0.9f, bl, L(0, -0.6f, 3.2f + bl * 0.4f, PI / 2), head);
  Mat4 jaw = L(0, -1.3f, 3.0f, p.jaw) * head; r.cone(scaleColor(beak ? m.accent : hexColor(0xffb347), 0.8f), beak ? 1.2f : 0.7f, bl * 0.8f, L(0, 0, bl * 0.4f, PI / 2), jaw);
  float flap = std::sin(p.time * 3) * 0.1f + p.walk * std::sin(p.walkPhase * 2) * 0.3f;
  float armA[2] = {p.armL, p.armR};
  for (int s = 0; s < 2; s++) {
    float sx = s ? 1.f : -1.f;
    Mat4 w = L(sx * 4.2f, 10, -0.5f, armA[s] * 0.5f, 0, sx * (0.25f + flap - std::min(0.f, armA[s]) * 0.4f)) * body;
    r.box(m.accent, 1.0f, 7, 4.5f, L(sx * 0.6f, -3, 0), w);
    for (int f = 0; f < 3; f++) r.box(scaleColor(m.accent, 0.85f), 0.6f, 2.6f, 1.4f, L(sx * 0.6f, -6.8f, -1.4f + f * 1.4f), w);
  }
  float sw = std::sin(p.walkPhase) * 0.5f * p.walk;
  for (int s = 0; s < 2; s++) {
    float sx = s ? 1.f : -1.f;
    Mat4 l = L(sx * 2, 3.6f, 0, s ? -sw : sw) * root;
    r.cyl(hexColor(0xffb347), 0.4f, 3.6f, L(0, -1.8f, 0), l);
    for (float a : {-0.5f, 0.f, 0.5f}) r.box(hexColor(0xffb347), 0.5f, 0.4f, 2.2f, L(std::sin(a) * 0.9f, -3.5f, std::cos(a) * 0.9f, 0, a), l);
  }
  for (int f = -1; f <= 1; f++) r.box(m.accent, 1.4f, 0.4f, 5, L(f * 1.4f, 5, -5.5f, -0.4f - p.tailCurl * 0.6f, f * 0.3f), body);
}

void robot(Rig& r, const MonsterDef& m, const Mat4& root, const RigPose& p, Color glow) {
  bool hat = m.variant == "hat";
  Mat4 body = L(0, std::fabs(std::sin(p.walkPhase)) * 0.3f * p.walk, 0, 0, 0, std::sin(p.walkPhase) * 0.03f * p.walk) * root;
  r.box(m.body, 8, 7, 5, L(0, 10, 0), body);
  r.box(m.belly, 6, 3, 5.2f, L(0, 7.0f, 0), body);
  r.box(glow, 3, 1.6f, 0.4f, L(0, 11, 2.6f), body, 1);
  for (int i = 0; i < 3; i++) r.sph(i == 1 ? glow : hexColor(0xffd36b), 0.7f, 0.7f, 0.3f, L(-1.4f + i * 1.4f, 8.6f, 2.6f), body, 1);
  Mat4 head = L(0, 14.6f, 0.3f, p.headX) * body;
  r.box(m.body, 5, 3.6f, 4, L(0, 0, 0), head); r.box(glow, 4, 1, 0.3f, L(0, 0.4f, 2.05f), head, 1);
  Mat4 jaw = L(0, -1.4f, 1.2f, p.jaw) * head; r.box(m.belly, 4, 0.8f, 1.8f, L(0, -0.2f, 0.4f), jaw);
  if (hat) { r.cyl(DARK, 2.0f, 3.4f, L(0, 3.5f, 0), head); r.cyl(DARK, 3.0f, 0.3f, L(0, 1.9f, 0), head); r.cyl(m.accent, 2.05f, 0.6f, L(0, 2.3f, 0), head); }
  else { r.cyl(CLAW, 0.15f, 2.6f, L(0, 3.0f, 0), head); r.sph(glow, 0.9f, 0.9f, 0.9f, L(0, 4.4f, 0), head, 1); }
  float armA[2] = {p.armL, p.armR};
  for (int s = 0; s < 2; s++) {
    float sx = s ? 1.f : -1.f; Mat4 a = L(sx * 5.0f, 12.5f, 0, armA[s]) * body;
    r.sph(m.belly, 2.4f, 2.4f, 2.4f, L(0, 0, 0), a); r.box(m.body, 1.8f, 5, 1.8f, L(sx * 0.3f, -2.8f, 0), a);
    r.box(m.belly, 2.4f, 2.0f, 2.4f, L(sx * 0.3f, -5.8f, 0.3f), a);
  }
  float sw = std::sin(p.walkPhase) * 0.5f * p.walk;
  for (int s = 0; s < 2; s++) {
    float sx = s ? 1.f : -1.f; Mat4 l = L(sx * 2.2f, 6, 0, s ? -sw : sw) * root;
    r.box(m.belly, 2.4f, 5.4f, 2.4f, L(0, -2.6f, 0), l); r.box(m.body, 3, 1.2f, 4, L(0, -5.4f, 0.5f), l);
  }
}

void serpent(Rig& r, const MonsterDef& m, const Mat4& root, const RigPose& p, Color glow) {
  int n = m.variant == "long" ? 20 : 14;
  float phase = p.walkPhase * 1.4f + p.time * 1.5f;
  for (int i = n - 1; i >= 0; i--) {
    float t = (float)i / n, rad = 2.6f * (1 - t * 0.6f), z = 4.0f - i * 2.6f;
    float x = std::sin(phase - i * 0.55f) * (1.2f + t * 3.2f) * (0.5f + 0.5f * p.walk + 0.3f) + (p.tailCurl * std::sin(t * PI) * 6);
    float y = i < 4 ? rad + (4 - i) * 1.4f : rad;
    r.sph(i % 2 ? m.body : scaleColor(m.body, 0.9f), 2 * rad, 2 * rad, 2 * rad, L(x, y, z), root);
    r.sph(m.belly, 1.6f * rad, rad, 1.6f * rad, L(x, y - rad * 0.55f, z), root);
    if (i % 2 == 0) r.cone(glow, rad * 0.5f, rad * 1.2f, L(x, y + rad * 0.9f, z), root, 1, 0.35f);
  }
  Mat4 head = L(std::sin(phase) * 1.2f, 8.4f, 6.5f, p.headX - 0.1f) * root;
  r.box(m.body, 4.0f, 3.0f, 5.0f, L(0, 0.4f, 0.6f), head);
  eyes(r, head, 1.3f, 1.4f, 2.5f, 1.2f, glow, true);
  for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 3; k++) r.cone(m.accent, 0.5f, 2.4f, L(s * 2.2f, 0.6f + k * 0.8f, -1.2f, -0.3f, 0, -s * 1.1f), head, 0);
  r.cone(glow, 0.4f, 1.5f, L(-0.8f, 2.2f, 0.4f), head, 1); r.cone(glow, 0.4f, 1.5f, L(0.8f, 2.2f, 0.4f), head, 1);
  Mat4 jaw = L(0, -1.0f, 0.4f, p.jaw) * head; r.box(m.belly, 3.4f, 0.8f, 4.6f, L(0, -0.3f, 1.4f), jaw);
}
}  // namespace

void drawMonsterRig(RenderList& rl, const MonsterDef& m, const Mat4& root, const RigPose& p, Color glow) {  // Rex: plan rig at an arbitrary root
  Rig r{rl};
  switch (m.plan) {
    case BodyPlan::Biped: biped(r, m, root, p, glow); break;
    case BodyPlan::Quad: quad(r, m, root, p, glow); break;
    case BodyPlan::Blob: blob(r, m, root, p, glow); break;
    case BodyPlan::Crab: crab(r, m, root, p, glow); break;
    case BodyPlan::Bird: bird(r, m, root, p, glow); break;
    case BodyPlan::Robot: robot(r, m, root, p, glow); break;
    case BodyPlan::Serpent: serpent(r, m, root, p, glow); break;
  }
}
void drawMonster(RenderList& rl, const MonsterDef& m, const Vec3& pos, float yaw, float scale, const RigPose& p) {
  Rig r{rl};
  Color glow = scaleColor(m.accent, std::min(p.glow, 1.f));
  glow = mixColor(glow, hexColor(0xffffff), std::max(0.f, p.glow - 1) * 0.6f);
  if (m.genSeed) { drawGeneratedMonster(rl, m, pos, yaw, scale, p); return; }  // Rex: kaiju_gen_draw.cpp
  Mat4 root = Mat4::scale(scale, scale, scale) * Mat4::rotY(yaw) * Mat4::translation(pos.x, pos.y + p.sink + p.lift, pos.z);
  switch (m.plan) {
    case BodyPlan::Biped: biped(r, m, root, p, glow); break;
    case BodyPlan::Quad: quad(r, m, root, p, glow); break;
    case BodyPlan::Blob: blob(r, m, root, p, glow); break;
    case BodyPlan::Crab: crab(r, m, root, p, glow); break;
    case BodyPlan::Bird: bird(r, m, root, p, glow); break;
    case BodyPlan::Robot: robot(r, m, root, p, glow); break;
    case BodyPlan::Serpent: serpent(r, m, root, p, glow); break;
  }
}
}  // namespace tz
