// Loader dispatch, image decoding and small shared helpers.
#include "model_loader.h"
#include <cctype>
#include <cstdio>
#include <vector>
#include "../../third_party/stb/stb_image.h"

namespace tz {
namespace mdl {

static std::string lowerExt(const std::string& p) {
  size_t d = p.find_last_of('.'); if (d == std::string::npos) return "";
  std::string e = p.substr(d + 1); for (char& c : e) c = (char)std::tolower((unsigned char)c); return e;
}
bool fileExists(const std::string& path) { FILE* f = std::fopen(path.c_str(), "rb"); if (!f) return false; std::fclose(f); return true; }
std::string dirOf(const std::string& path) { size_t s = path.find_last_of("/\\"); return s == std::string::npos ? std::string() : path.substr(0, s + 1); }

bool loadModel(const std::string& path, Model& out, std::string& err, const LoadOptions& opt) {
  out = Model();
  if (!fileExists(path)) { err = "file not found: " + path; return false; }
  std::string e = lowerExt(path);
  bool ok;
  if (e == "gltf" || e == "glb") ok = loadGltf(path, out, err, opt);
  else if (e == "fbx") ok = loadFbx(path, out, err, opt);
  else { err = "unsupported model format '." + e + "': " + path; return false; }
  if (!ok) return false;
  out.path = path;
  finalizeModel(out);
  if (out.meshes.empty()) { err = "no meshes in " + path; return false; }
  return true;
}

bool decodeImage(const unsigned char* data, size_t size, Texture& out, std::string& err) {
  int w = 0, h = 0, n = 0;
  stbi_uc* px = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
  if (!px) { err = std::string("image decode failed: ") + (stbi_failure_reason() ? stbi_failure_reason() : "?"); return false; }
  out.width = w; out.height = h; out.rgba.assign(px, px + (size_t)w * h * 4);
  stbi_image_free(px);
  return true;
}
bool decodeImageFile(const std::string& path, Texture& out, std::string& err) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) { err = "texture not found: " + path; return false; }
  std::vector<unsigned char> buf; unsigned char tmp[65536]; size_t n;
  while ((n = std::fread(tmp, 1, sizeof tmp, f)) > 0) buf.insert(buf.end(), tmp, tmp + n);
  std::fclose(f);
  if (!decodeImage(buf.data(), buf.size(), out, err)) { err += " (" + path + ")"; return false; }
  size_t s = path.find_last_of("/\\"); out.name = s == std::string::npos ? path : path.substr(s + 1);
  return true;
}

std::string describeModel(const Model& m) {
  size_t s = m.path.find_last_of("/\\");
  std::string r = (s == std::string::npos ? m.path : m.path.substr(s + 1)) + ": ";
  char b[256];
  size_t joints = 0; for (auto& sk : m.skins) joints = std::max(joints, sk.joints.size());
  std::snprintf(b, sizeof b, "%zu nodes, %zu meshes (%zu skinned), %zu tris, %zu materials, %zu textures, %zu skins (max %zu joints), %zu clips",
                m.nodes.size(), m.meshes.size(), m.skinnedMeshCount(), m.triangleCount(), m.materials.size(), m.textures.size(), m.skins.size(), joints, m.clips.size());
  return r + b;
}

}  // namespace mdl
}  // namespace tz
