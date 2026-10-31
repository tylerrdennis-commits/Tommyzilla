// Model loading: glTF 2.0 (.gltf / .glb, via cgltf) and FBX (.fbx, via ufbx) into tz::mdl::Model.
// Textures (PNG/JPG, embedded or external) are decoded to RGBA8 with stb_image.
#pragma once
#include <string>
#include "model.h"
namespace tz {
namespace mdl {
struct LoadOptions {
  bool loadTextures = true;
  bool loadAnimations = true;
};
// Picks the loader from the file extension. Returns false and fills err on failure (missing file, parse error).
bool loadModel(const std::string& path, Model& out, std::string& err, const LoadOptions& opt = LoadOptions());
bool loadGltf(const std::string& path, Model& out, std::string& err, const LoadOptions& opt);
bool loadFbx(const std::string& path, Model& out, std::string& err, const LoadOptions& opt);
// stb_image wrappers (PNG / JPEG). Output is always 4 channels.
bool decodeImage(const unsigned char* data, size_t size, Texture& out, std::string& err);
bool decodeImageFile(const std::string& path, Texture& out, std::string& err);
bool fileExists(const std::string& path);
std::string dirOf(const std::string& path);   // "a/b/c.glb" -> "a/b/"
// One-line summary for logs: "Dino.glb: 1 mesh, 6 materials, 0 textures, 1 skin (43 joints), 14 clips".
std::string describeModel(const Model& m);
}  // namespace mdl
}  // namespace tz
