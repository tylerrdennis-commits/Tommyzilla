// data/models.json: maps game entity ids to model files + placement (scale / target height, rotation offset,
// ground alignment), a look (tint + per-material recolours), named look variants (e.g. enemy difficulty tiers)
// and the logical clip names used by the game (idle, walk, run, attack, death -> clip names in the file).
#pragma once
#include <map>
#include <string>
#include <vector>
#include "../core/math.h"
namespace tz {
namespace mdl {
struct Recolor {
  enum Mode { Multiply, Replace, Hue };
  std::string material;      // exact material name, a prefix ending in '*', or "*" for all
  Color color;
  Mode mode = Hue;
};
struct Look {
  Color tint = {1, 1, 1, 1};       // multiplies every material (and texture)
  std::vector<Recolor> recolor;    // applied in order to matching materials' base colours
  std::vector<std::string> hide;   // node-name patterns ("Knife*", "*") whose meshes (and children) are not drawn
  std::vector<std::string> show;   // patterns that stay visible even if matched by hide (e.g. pick one weapon)
};
// Variants start from the base look; each field present in the variant replaces the base one.
struct ModelDesc {
  std::string id, file;            // file is relative to the asset root (the exe folder / repo root)
  std::vector<std::string> extraFiles;  // external textures etc. to ship with the model
  float scale = 1;                 // uniform scale (ignored when height > 0)
  float height = 0;                // > 0: scale so the rest-pose bounds are this tall (world units)
  Vec3 rotationDeg;                // rotation offset applied before scale/placement (yaw = y)
  Vec3 offset;                     // extra translation after ground alignment (world units)
  bool groundAlign = true;         // move the rest-pose bounds so minY = 0 and centre x/z on the origin
  float animSpeed = 1;
  Look look;
  std::map<std::string, Look> variants;        // name -> look (replaces the base look)
  std::map<std::string, std::string> clips;    // logical name -> clip name in the file
  std::string fallback;            // hint for the primitive fallback (e.g. "biped", "car", "building")
};
struct Manifest {
  bool enabled = true;             // top-level "enabled": false = draw every entity with its primitive rig
  std::vector<ModelDesc> models;
  const ModelDesc* find(const std::string& id) const;
};
bool parseManifest(const std::string& jsonText, Manifest& out, std::string& err);
bool parseColor(const std::string& s, Color& out);
bool matchPattern(const std::string& pattern, const std::string& name);  // exact, prefix*, or *  // "#rrggbb", "#rrggbbaa" or "rrggbb"
}  // namespace mdl
}  // namespace tz
