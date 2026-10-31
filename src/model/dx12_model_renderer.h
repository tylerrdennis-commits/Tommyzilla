// Direct3D 12 renderer for loaded models: static and skinned meshes (CPU-skinned per frame into an upload ring, 4 weights),
// base colour x vertex colour x base colour texture, the same hemisphere/Lambert lighting + fog as the primitive
// shader. GPU buffers/textures are created lazily the first time a model (by Model::uid) is drawn.
// Owned by Dx12Renderer; called inside its frame (after the primitive opaque pass / alpha pass).
#pragma once
#include <cstddef>
#include <string>
#include "model_draw.h"
struct ID3D12Device;
struct ID3D12GraphicsCommandList;
namespace tz {
class Dx12ModelRenderer {
 public:
  Dx12ModelRenderer();
  ~Dx12ModelRenderer();
  // colorFmt / depthFmt are DXGI_FORMAT values of the main render targets.
  bool init(ID3D12Device* device, int framesInFlight, int colorFmt, int depthFmt, std::string& err);
  // pass 0 = opaque materials (call first each frame), 1 = blended materials. frameCB = GPU VA of the shared
  // Frame constants (same layout as the primitive shader). Leaves its own root signature/PSO bound.
  // pass 2 / 3 = draw opaque / blended again with another frameCB (split screen right view), reusing pass 0's skinning + instances
  void draw(ID3D12GraphicsCommandList* cmd, unsigned long long frameCB, int frame, const ModelDrawList& list, int pass);
  void shutdown();
  bool ready() const;
  size_t uploadedModels() const;
  void skinCounts(int& skinned, int& reused) const;
  void drawCounts(int& draws, int& batches, int& calls) const;  // last frame: ModelDraws, instanced batches, draw calls  // last frame's CPU skinning work (reused = shared crowd poses)
 private:
  struct Impl;
  Impl* d;
};
}  // namespace tz
