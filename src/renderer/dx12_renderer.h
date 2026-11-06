// Minimal Direct3D 12 renderer: instanced unit primitives with hemisphere + wrapped key light, contact darkening,
// a dusk sky gradient, distance haze, ACES tonemap and a mild half-resolution bloom. Fog matches the sky.
// plus 2D UI triangles, and the textured ground pass (Texture2DArray, world-space UVs; see game/ground.h). Loaded models
// are drawn by src/model. Shaders are embedded HLSL compiled at startup with D3DCompile.
#pragma once
#include <string>
#include "../game/render_types.h"
struct HWND__;
namespace tz {
struct ModelDrawList;  // src/model/model_draw.h (optional loaded-model draws)
class Dx12Renderer {
 public:
  Dx12Renderer();
  ~Dx12Renderer();
  bool init(HWND__* hwnd, int width, int height, std::string& err);
  void resize(int width, int height);
  // Draws and presents one frame. Returns false if the device was lost (an error box has been shown).
  // models (optional): loaded glTF/FBX models, drawn after the opaque primitives and again for blended materials.
  // right (optional, split screen): a second 3D view drawn into right->vpX..vpW; rl then covers rl.vpX..vpW and still
  // carries the fullscreen UI / video. Loaded models are one shared list drawn in each view.
  bool render(const RenderList& rl, const ModelDrawList* models = nullptr, const RenderList* right = nullptr);
  // Uploads the ground textures (one array slice per GroundSurface, full mip chain). False = keep the flat ground.
  bool setGroundTextures(const GroundTextureSet& set, std::string& err);
  bool groundReady() const;
  void shutdown();
  const std::string& adapterName() const;
  void modelSkinCounts(int& skinned, int& reused) const;
  void modelDrawCounts(int& draws, int& batches, int& calls) const;  // last frame's CPU-skinned mesh instances / shared-pose reuses
  bool usingWarp() const;
 private:
  struct Impl;
  Impl* d;
};
}  // namespace tz
