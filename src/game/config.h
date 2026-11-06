// Tuning values (defaults match the browser prototype). Loaded from data/config.json at startup.
#pragma once
#include <string>
namespace tz {
#define TZ_CONFIG_FIELDS(X) \
  X(worldHalf, 400) X(chunkSize, 100) X(activeRadius, 2) X(fogNear, 110) X(fogFar, 260) \
  X(brick, 2) X(rebuildDelay, 300) X(rebuildBrickInterval, 0.03f) \
  X(blockKaijuHealth, 0.45f) X(decorFallHealth, 0.75f) X(roofFallHealth, 0.6f) X(smashedHealth, 0.6f) X(landmarkSmashHealth, 0.7f) \
  X(collapseChance, 0.78f) X(collapseBonusCoins, 10) \
  X(maxDebris, 2500) X(gravity, 32) X(debrisLife, 7) X(debrisBounce, 0.3f) X(debrisFriction, 0.8f) \
  X(kaijuSpeed, 22) X(kaijuSprint, 1.8f) X(kaijuTurn, 9) X(waterSlow, 0.6f) X(kaijuRadius, 4.5f) X(kaijuMaxHp, 100) X(kaijuRegen, 2) \
  X(camDist, 34) X(camHeight, 12) X(camStickSpeed, 2.6f) X(camMouseSpeed, 0.004f) X(camAutoFollowDelay, 2.0f) X(camAutoFollowRate, 1.2f) \
  X(deadzone, 0.2f) \
  X(coinsPerBrick, 1) X(coinsPerBrickOther, 0) \
  X(damagePerLevel, 0.35f) X(rangePerLevel, 0.15f) X(speedPerLevel, 0.2f) X(upgradeSteps, 5) \
  X(upgradeCost1, 60) X(upgradeCost2, 140) X(upgradeCost3, 260) X(upgradeCost4, 420) X(upgradeCost5, 650) \
  X(startSlots, 2) X(slot2Smashes, 8) X(schoolUnlockCount, 4) \
  X(maxPeople, 260) X(peoplePerChunk, 12) X(fleeRadius, 34) X(walkSpeed, 2.2f) X(runSpeed, 9) X(peopleRespawnInterval, 1.5f) \
  X(maxSplats, 140) X(splatFadeRadius, 80) X(splatMaxAge, 360) \
  X(treePickRange, 9) X(throwSpeed, 46) X(rockRespawn, 300) \
  X(heatPerBuilding, 0.45f) X(heatPerPerson, 0.12f) X(heatPerBrick, 0.002f) X(heatDecay, 0.08f) X(heatDecayDelay, 6) \
  X(carsPerStar, 2) X(maxCars, 8) X(carSpeed, 26) X(carDps, 1.2f) X(carRange, 22) \
  X(heliStars, 3) X(heliDps, 2.0f) X(heliHp, 3) X(tankStars, 4) X(maxTanks, 3) X(tankSpeed, 9) X(tankHp, 3) X(tankShotDamage, 5) X(tankReload, 3.5f) \
  X(jetSpeed, 90) X(jetHp, 2) X(jetBombDamage, 6) X(jeepSpeed, 30) X(jeepDps, 1.5f) \
  X(nukeStars, 5) X(nukeCountdown, 20) X(nukeCancelCost, 150) X(nukeCancelHeat, 2.5f) X(spawnDist, 110) \
  X(masterVolume, 0.6f) X(screamMinGap, 0.7f) X(screamRadius, 30) \
  X(eggGlowHits, 6) X(revealTime, 4.0f) \
  X(tutorialRainbows, 10) X(tutorialBuildings, 5) X(tutorialRewardKeys, 1) X(tutorialRewardCoins, 50) X(tutorialMinCivilians, 32) X(tutorialRespawnInterval, 0.4f) X(tutorialSpawnMinDist, 25) \
  X(roarUnlockLandmarks, 1) X(roarUnlockHits, 15) X(hitCountGap, 0.75f) \
  X(rapidProjectileCap, 10) \
  X(blasterAimRadius, 1.8f) \
  X(eatRange, 14) X(eatTweenTime, 0.3f) \
  X(stompStunRadius, 24) X(stompStunTime, 3.0f) \
  X(keyCost, 100) X(coinsPerRainbow, 2) X(chestRange, 12) X(chestDist, 24) \
  X(bossHp, 150) X(bossGrowPerHit, 0.065f) X(bossMaxScale, 2.5f) X(bossBaseScale, 1.6f) X(bossRadius, 5.5f) X(bossSpeed, 7) \
  X(bossDamagePerBrick, 0.5f) X(bossHitGap, 0.15f) X(bossPoundDamage, 3) X(bossPoundRadius, 16) X(bossRushDamage, 3.5f) X(bossRushSpeed, 28) \
  X(bossHardness, 1.4f) X(bossThrowDamage, 2.5f) X(bossAttackGap, 3.2f) X(bossIntroTime, 3.5f) \
  X(introEnabled, 1) X(gloveStep, 8) X(gloveTargets, 10) X(gloveDropFraction, 0.5f) X(glovePickupRange, 8) X(policeVolume, 0.4f) X(policeMaxVoices, 3) \
  X(crazyEnemyHp, 2) X(crazyEnemyDamage, 2) X(crazyBossHp, 2) X(crazySuperCost, 1.75f) \
  X(eatCostStart, 2) X(eatCostEnd, 14) X(eatCostMinCharge, 0.5f) X(eatCostFullCharge, 1.0f) X(levelMinCivilians, 26) X(levelRespawnInterval, 0.6f) \
  X(flameRange, 11) X(flameDps, 1.2f) \
  X(copHp, 1) X(copSpeed, 9) X(copRange, 26) X(copReload, 2.0f) X(copShotDamage, 0.35f) \
  X(soldierHp, 1) X(soldierSpeed, 10) X(soldierRange, 30) X(soldierReload, 1.8f) X(soldierShotDamage, 0.45f) \
  X(tier1Hp, 1.6f) X(tier1Damage, 1.15f) X(tier1Speed, 1.1f) X(tier1Count, 1.0f) X(tier2Hp, 2.2f) X(tier2Damage, 1.3f) X(tier2Speed, 1.2f) X(tier2Count, 1.25f) \
  X(mbossHp, 24) X(mbossShotDamage, 1.5f) X(mbossReload, 3.0f) X(mbossSpeed, 6) X(kaijuPickLevel, 6) X(adventureEnemyDamage, 0.5f) \
  X(modelsEnabled, 1) X(modelPeopleDist, 110) X(modelSoloMax, 32) X(modelLodNear, 50) X(modelLodFar, 130) X(modelBuildings, 1) X(modelBuildingDist, 0)

struct Config {
#define TZ_DECL(n, d) float n = (float)(d);
  TZ_CONFIG_FIELDS(TZ_DECL)
#undef TZ_DECL
  // the 4 landmarks that unlock Ruth King Elementary
  std::string schoolLandmarks[4] = {"empress", "parliament", "museum", "ferry"};
  int upgradeCost(int level) const;  // cost to go from `level` to level+1 (level 1..5), -1 if maxed
  bool loadJson(const std::string& text, std::string* err);
};
extern Config C;  // global tuning
}  // namespace tz
