#include "config.h"
#include "../core/json.h"
namespace tz {
Config C;
int Config::upgradeCost(int level) const {
  const float costs[5] = {upgradeCost1, upgradeCost2, upgradeCost3, upgradeCost4, upgradeCost5};
  int steps = (int)upgradeSteps; if (steps > 5) steps = 5;
  if (level < 1 || level > steps) return -1;
  return (int)costs[level - 1];
}
bool Config::loadJson(const std::string& text, std::string* err) {
  Json j; if (!Json::parse(text, j, err)) return false;
#define TZ_LOAD(n, d) if (j.has(#n)) n = (float)j[#n].numOr(n);
  TZ_CONFIG_FIELDS(TZ_LOAD)
#undef TZ_LOAD
  // "models": {"enabled": false, "peopleDist": 110, "soloMax": 32, "lodNear": 50, "lodFar": 130,
  //            "buildings": true, "buildingDist": 0} (loaded 3D models; see data/models.json)
  if (j["models"].type == Json::Object) {
    const Json& m = j["models"];
    if (m.has("enabled")) modelsEnabled = m["enabled"].boolOr(modelsEnabled > 0.5f) ? 1.f : 0.f;
    if (m.has("peopleDist")) modelPeopleDist = (float)m["peopleDist"].numOr(modelPeopleDist);
    if (m.has("soloMax")) modelSoloMax = (float)m["soloMax"].numOr(modelSoloMax);
    if (m.has("lodNear")) modelLodNear = (float)m["lodNear"].numOr(modelLodNear);
    if (m.has("lodFar")) modelLodFar = (float)m["lodFar"].numOr(modelLodFar);
    if (m.has("buildings")) modelBuildings = m["buildings"].boolOr(modelBuildings > 0.5f) ? 1.f : 0.f;
    if (m.has("buildingDist")) modelBuildingDist = (float)m["buildingDist"].numOr(modelBuildingDist);
  }
  if (j["schoolLandmarks"].type == Json::Array) for (size_t i = 0; i < 4 && i < j["schoolLandmarks"].size(); i++) schoolLandmarks[i] = j["schoolLandmarks"][i].strOr(schoolLandmarks[i]);
  return true;
}
}  // namespace tz
