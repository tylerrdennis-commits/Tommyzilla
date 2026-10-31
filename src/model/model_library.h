// Loads data/models.json and the models it lists (lazily, cached). get() returns nullptr when an entity has no
// manifest entry or its file is missing/broken, so callers keep drawing the built-in primitive rig instead.
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "model.h"
#include "model_draw.h"
#include "model_manifest.h"
namespace tz {
namespace mdl {
// Logical clip names the game asks for. Unmapped names fall back to common Quaternius/Mixamo-style clip names.
extern const char* const kLogicalClips[5];  // "idle", "walk", "run", "attack", "death"

struct ModelAsset {
  ModelDesc desc;
  Model model;
  Mat4 offset = Mat4::identity();         // rotation offset * scale * ground alignment (model -> entity space)
  float scale = 1;
  Pose restPose;
  std::map<std::string, int> clipMap;     // logical name -> clip index (-1 = not available)
  std::map<std::string, std::vector<Color>> looks;  // "" = base look, plus each variant
  std::map<std::string, std::vector<uint8_t>> visibility;  // per look: node visible flags (missing = all visible)
  int clip(const std::string& logical) const;        // logical name, or a raw clip name; -1 if none
  const std::vector<Color>* look(const std::string& variant = "") const;
  const std::vector<uint8_t>* visible(const std::string& variant = "") const;
  // Convenience: a draw at an entity transform (yaw etc. already in entityWorld), with an optional animated pose.
  ModelDraw makeDraw(const Mat4& entityWorld, const Pose* pose = nullptr, const std::string& variant = "") const;
};

class ModelLibrary {
 public:
  // root: directory that manifest file paths are relative to ("" or ending in '/'). Returns false on a parse error.
  bool loadManifestFile(const std::string& manifestPath, const std::string& root, std::string& err);
  bool loadManifestText(const std::string& text, const std::string& root, std::string& err);
  const Manifest& manifest() const { return manifest_; }
  const ModelAsset* get(const std::string& id);      // loads on first use; nullptr = use the primitive fallback
  int loadAll();                                     // returns how many loaded
  const std::vector<std::string>& log() const { return log_; }
  std::string root() const { return root_; }
 private:
  Manifest manifest_;
  std::string root_;
  std::map<std::string, std::unique_ptr<ModelAsset>> cache_;  // null entry = tried and failed
  std::vector<std::string> log_;
};

// Applies a look to a model's materials / nodes (exposed for tests). nodeVisibility returns {} when nothing is hidden.
std::vector<Color> applyLook(const Model& m, const Look& l);
std::vector<uint8_t> nodeVisibility(const Model& m, const Look& l);
}  // namespace mdl
}  // namespace tz
