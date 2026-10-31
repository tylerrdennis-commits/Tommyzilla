// Rex: draws a procedurally generated kaiju: the body-plan rig stretched by the genome's proportions + generated parts.
#include "game.h"
#include "kaiju_gen.h"
namespace tz {
namespace {
inline Mat4 L(float x, float y, float z, float rx = 0, float ry = 0, float rz = 0) { return Mat4::rotZ(rz) * Mat4::rotY(ry) * Mat4::rotX(rx) * Mat4::translation(x, y, z); }
void put(RenderList& rl, MeshId mesh, Color c, float sx, float sy, float sz, const Mat4& at, const Mat4& root, float unlit = 0) { rl.opaque[mesh].push_back(makeInstance(Mat4::scale(sx, sy, sz) * at * root, c, unlit)); }
}  // namespace
void drawGeneratedMonster(RenderList& rl, const MonsterDef& m, const Vec3& pos, float yaw, float scale, const RigPose& p) {
  KaijuLook look; look.monster = m.id; look.primary = m.body; look.secondary = m.belly; look.accent = m.accent; look.eyes = m.accent;
  KaijuGenome g = kgen::generate(m.genSeed, look);
  Color glow = scaleColor(m.accent, std::min(p.glow, 1.f));
  float sx = scale * g.girth, sy = scale * g.height;
  Mat4 root = Mat4::scale(sx, sy, sx) * Mat4::rotY(yaw) * Mat4::translation(pos.x, pos.y + p.sink + p.lift, pos.z);
  drawMonsterRig(rl, m, root, p, glow);
  // parts sit on a generic silhouette (head ~y 11, back ~y 8, tail behind); hidden overlap with the rig is fine
  float hy = g.body == GenBody::Blob ? 8.f : g.body == GenBody::Serpent ? 9.f : 11.f, hz = g.body == GenBody::Quadruped || g.body == GenBody::Serpent ? 9.f : 6.f;
  float hs = g.headSize;
  for (int part : g.parts) switch (part) {
    case PART_HORNS: for (int s = -1; s <= 1; s += 2) put(rl, MESH_CONE, m.belly, 0.9f * hs, 3.0f * hs, 0.9f * hs, L(s * 1.6f * hs, hy + 2.6f * hs, hz - 0.5f, -0.3f, 0, -s * 0.4f), root); break;
    case PART_SPIKES: for (int k = 0; k < 5; k++) put(rl, MESH_CONE, m.accent, 1.0f, 2.2f, 1.0f, L(0, 8.5f - k * 0.6f, 3.0f - k * 2.0f), root); break;
    case PART_WINGS: for (int s = -1; s <= 1; s += 2) put(rl, MESH_BOX, m.accent, 7.0f, 0.4f, 4.0f, L(s * 5.5f, 9.0f + std::sin(p.time * 4) * 0.6f, 0, 0, 0, s * (0.35f + 0.25f * std::sin(p.time * 4))), root); break;
    case PART_TAIL_CLUB: put(rl, MESH_SPHERE, m.accent, 2.6f * g.tailLength, 2.6f, 2.6f, L(0, 2.0f, -9.0f * g.tailLength), root); break;
    case PART_FINS: for (int s = -1; s <= 1; s += 2) put(rl, MESH_CONE, m.belly, 0.8f, 3.0f, 2.0f, L(s * 3.2f, 7.0f, 0, 0, 0, s * 1.2f), root); break;
    case PART_CRest: put(rl, MESH_BOX, m.accent, 0.4f, 2.0f * hs, 4.0f * hs, L(0, hy + 2.2f * hs, hz - 1.0f), root); break;
    case PART_CLAWS: for (int s = -1; s <= 1; s += 2) put(rl, MESH_CONE, hexColor(0xf3ead2), 0.5f, 1.6f, 0.5f, L(s * 3.8f, 4.5f, hz - 2.0f, 1.4f), root); break;
    case PART_TUSKS: for (int s = -1; s <= 1; s += 2) put(rl, MESH_CONE, hexColor(0xfffbe8), 0.5f, 1.8f, 0.5f, L(s * 1.0f * hs, hy - 1.6f * hs, hz + 2.4f * hs, PI), root); break;
    case PART_EXTRA_EYES: put(rl, MESH_SPHERE, g.eyes, 0.9f, 0.9f, 0.6f, L(0, hy + 1.8f * hs, hz + 2.4f * hs), root, 1); break;
  }
}
}  // namespace tz
