// Tommyzilla model module: CPU-side model data shared by the glTF/GLB (cgltf) and FBX (ufbx) loaders, the animator
// and the DX12 skinned-mesh renderer. Platform independent (no Windows headers), so the Linux tests can use it.
// Conventions match src/core/math.h: row vectors (p' = p * M), right-handed, +Y up, metres.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "../core/math.h"

namespace tz {
namespace mdl {

struct Quat { float x = 0, y = 0, z = 0, w = 1; };
Quat normalizeQ(Quat q);
Quat slerpQ(Quat a, Quat b, float t);  // shortest path, falls back to nlerp for tiny angles

struct Transform {           // node-local TRS
  Vec3 t; Quat r; Vec3 s = Vec3(1, 1, 1);
};
Mat4 toMat4(const Transform& x);  // S * R * T (row-vector convention)
Mat4 quatToMat4(const Quat& q);
Transform lerpTransform(const Transform& a, const Transform& b, float k);
bool invertAffine(const Mat4& m, Mat4& out);

struct Texture {             // decoded RGBA8 (stb_image); sRGB-encoded as stored in the PNG/JPG
  std::string name;
  int width = 0, height = 0;
  std::vector<uint8_t> rgba;
};

struct Material {
  std::string name;
  Color baseColor;           // display (sRGB) colour; glTF linear factors are converted on load
  int texture = -1;          // index into Model::textures, or -1
  bool doubleSided = true;
  bool blend = false;        // alpha mode BLEND (drawn without depth write)
  float roughness = 1.f;     // 1 = fully rough; the shader also glosses saturated albedo a little
  float metallic = 0.f;
};

struct Vertex {              // 64 bytes, matches the HLSL input layout in dx12_model_renderer.cpp
  float pos[3];
  float nrm[3];
  float uv[2];
  uint8_t color[4];          // sRGB vertex colour (white if the mesh has none)
  uint16_t joints[4];        // indices into the node's Skin::joints
  float weights[4];
  float pad;
};
static_assert(sizeof(Vertex) == 64, "Vertex layout");

struct SubMesh { uint32_t firstIndex = 0, indexCount = 0; int material = -1; };
struct Mesh {
  std::string name;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;  // triangle list
  std::vector<SubMesh> parts;     // one per material
  bool hasVertexColors = false;
  bool skinned = false;           // vertices carry joints/weights (draw with a Skin palette)
};

struct Node {
  std::string name;
  int parent = -1;
  std::vector<int> children;
  Transform local;           // rest pose
  int mesh = -1;             // index into Model::meshes
  int skin = -1;             // index into Model::skins (only when mesh is skinned)
};

struct Skin {
  std::string name;
  std::vector<int> joints;           // node indices
  std::vector<Mat4> inverseBind;     // model space -> joint space, one per joint
};

enum class Path : uint8_t { Translation, Rotation, Scale };
struct Channel {
  int node = -1;
  Path path = Path::Translation;
  bool step = false;                 // STEP interpolation (else linear / slerp)
  std::vector<float> times;          // seconds, ascending
  std::vector<float> values;         // 3 floats per key (T, S) or 4 (R, xyzw)
};
struct AnimationClip {
  std::string name;
  float duration = 0;
  std::vector<Channel> channels;
};

struct Model {
  std::string path, format;          // format: "gltf" | "glb" | "fbx"
  uint32_t uid = 0;                  // unique per loaded model (the renderer caches GPU buffers by uid)
  std::vector<Node> nodes;
  std::vector<int> order;            // node indices, parents before children
  std::vector<Mesh> meshes;
  std::vector<Material> materials;
  std::vector<Texture> textures;
  std::vector<Skin> skins;
  std::vector<AnimationClip> clips;
  Vec3 boundsMin, boundsMax;         // rest-pose bounds in model space
  std::vector<std::string> warnings;

  int findClip(const std::string& name) const;            // exact, then case-insensitive, then suffix after '|'
  size_t triangleCount() const;
  size_t skinnedMeshCount() const;
  std::vector<Transform> restLocals() const;
};

// Per-instance pose: node globals and per-skin joint palettes (inverseBind * jointGlobal), model space.
struct Pose {
  std::vector<Mat4> global;
  std::vector<std::vector<Mat4>> skinMats;
};
void computePose(const Model& m, const std::vector<Transform>& locals, Pose& out);
// CPU skinning (used by the DX12 renderer): out[i] = mesh vertex i with position/normal skinned by the palette
// (model space, up to 4 weights). out must hold me.vertices.size() vertices. Joints outside the palette are ignored.
void skinMesh(const Mesh& me, const std::vector<Mat4>& palette, Vertex* out);
// Called by the loaders once nodes/meshes/skins are filled in: parent order, bounds, uid.
void finalizeModel(Model& m);

// colour helpers
float linearToSrgb(float c);
float srgbToLinear(float c);
Color hsvRecolor(Color src, Color target);  // keep value/saturation shape, take the target's hue

}  // namespace mdl
}  // namespace tz
