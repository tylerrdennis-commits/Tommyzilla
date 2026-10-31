// What the game hands to the renderer each frame: instanced primitives + 2D UI triangles. Platform independent.
#pragma once
#include <vector>
#include "../core/math.h"
#include "ground.h"
#include "model_hooks.h"
namespace tz {
// Unit primitives (see renderer/meshes.cpp). All are centred on the origin and fit a 1x1x1 box:
// BOX 1x1x1; CYL r=0.5 h=1; CONE base r=0.5 at y=-0.5, tip at +0.5; SPHERE r=0.5; PYRAMID 4-sided, base 1x1;
// DISC r=0.5 flat on y=0 facing up; RING thin torus r=0.5 in XZ; ROCK faceted icosahedron r=0.5; HEMI dome r=0.5 (y 0..0.5)
enum MeshId { MESH_BOX, MESH_CYL, MESH_CONE, MESH_SPHERE, MESH_PYRAMID, MESH_DISC, MESH_RING, MESH_ROCK, MESH_HEMI, MESH_COUNT };
struct Instance {             // 80 bytes, matches the HLSL instance layout
  float c0[4], c1[4], c2[4];  // columns of the 4x3 world matrix (row-vector convention)
  float color[4];
  float params[4];            // x = unlit (1 = flat emissive colour), y = extra glow
};
inline Instance makeInstance(const Mat4& m, const Color& c, float unlit = 0, float glow = 0) {
  Instance i;
  for (int r = 0; r < 4; r++) { i.c0[r] = m.m[r][0]; i.c1[r] = m.m[r][1]; i.c2[r] = m.m[r][2]; }
  i.color[0] = c.r; i.color[1] = c.g; i.color[2] = c.b; i.color[3] = c.a;
  i.params[0] = unlit; i.params[1] = glow; i.params[2] = 0; i.params[3] = 0;
  return i;
}
struct UIVertex { float x, y; float r, g, b, a; };
struct RenderList {
  std::vector<Instance> opaque[MESH_COUNT];
  std::vector<Instance> alpha[MESH_COUNT];   // blended, no depth write, drawn after opaque
  std::vector<UIVertex> ui;                  // pixel-space triangles, drawn last
  std::vector<ModelReq> models;              // loaded 3D models drawn instead of primitive rigs (see model_hooks.h)
  std::vector<GroundQuad> ground;            // textured ground (only when Game::groundTextured; else ground boxes go to opaque)
  float time = 0;                            // game clock (s): scrolls the water texture
  float animDt = 0;                          // animation time step for this frame (0 while paused)
  Mat4 view = Mat4::identity(), proj = Mat4::identity();
  Vec3 eye;
  Color sky = hexColor(0x9fd4ff), fog = hexColor(0xbfe3ff);
  float fogNear = 110, fogFar = 260;
  Vec3 lightDir = Vec3(-120, -200, -80);     // direction the sunlight travels (normalized by the renderer)
  Color sunColor = hexColor(0xffd8c0); float sunIntensity = 0.75f;      // prototype: DirectionalLight(0xffd8c0, 0.75) at (120,200,80)
  Color hemiSky = hexColor(0xc8d4ff), hemiGround = hexColor(0x2a1424); float hemiIntensity = 0.85f;  // HemisphereLight
  int width = 1280, height = 720;
  float uiScale = 0;                         // UIDraw base scale override (0 = from the height; split-screen halves use a smaller one)
  int vpX = 0, vpW = 0;                      // 3D viewport (pixels) inside the window: vpW 0 = full width (split screen: left / right half)
  // after-credits video frame (BGRX 8-bit, top-down), drawn full-screen (letterboxed) under the UI when set
  const uint8_t* video = nullptr; int videoW = 0, videoH = 0, videoStride = 0; uint64_t videoSerial = 0;
  void clear() { for (auto& v : opaque) v.clear(); for (auto& v : alpha) v.clear(); ui.clear(); models.clear(); ground.clear(); video = nullptr; }
  size_t instanceCount() const { size_t n = 0; for (auto& v : opaque) n += v.size(); for (auto& v : alpha) n += v.size(); return n; }
  void add(MeshId m, const Mat4& w, const Color& c, float unlit = 0) { (c.a < 0.999f ? alpha : opaque)[m].push_back(makeInstance(w, c, unlit)); }
};
}  // namespace tz
