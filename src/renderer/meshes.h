// Unit primitive meshes (positions + normals), generated in code. Platform independent.
#pragma once
#include <cstdint>
#include <vector>
#include "../game/render_types.h"
namespace tz {
struct MeshVertex { float px, py, pz, nx, ny, nz; };
struct MeshData { std::vector<MeshVertex> v; std::vector<uint16_t> idx; };
MeshData buildMesh(MeshId id);
}  // namespace tz
