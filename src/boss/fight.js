import { listEncounters } from './encounters.js';
import { patternLabel, samplePattern } from './patterns.js';
import { createEncounterRotation } from './select.js';

export const FIGHT_SECONDS = 8;

export function bossBanner(snapshot) {
  const name = snapshot.shiny ? `Shiny ${snapshot.displayName}` : snapshot.displayName;
  return `Boss ${snapshot.index}: ${name} — ${snapshot.patternLabel}`;
}

export function createBossRun({
  encounters = listEncounters(),
  random = Math.random,
  fightSeconds = FIGHT_SECONDS,
} = {}) {
  if (!(fightSeconds > 0)) throw new Error('fightSeconds must be positive');
  const rotation = createEncounterRotation(encounters, random);
  let encounter = rotation.next();
  let elapsed = 0;
  const history = [encounter.id];

  function snapshot() {
    const pose = samplePattern(encounter.pattern, elapsed);
    return {
      index: history.length,
      id: encounter.id,
      variantId: encounter.variantId,
      displayName: encounter.displayName,
      shiny: encounter.shiny,
      pattern: encounter.pattern,
      patternLabel: patternLabel(encounter.pattern),
      elapsed,
      x: pose.x,
      z: pose.z,
      yaw: pose.yaw,
      moving: pose.moving,
    };
  }

  function switchTo(nextEncounter) {
    encounter = nextEncounter;
    elapsed = 0;
    history.push(encounter.id);
  }

  return {
    current: snapshot,
    history: () => history.slice(),
    advance() {
      switchTo(rotation.next());
      return snapshot();
    },
    update(dt) {
      if (!(dt >= 0)) throw new Error('dt must be >= 0');
      let remaining = dt;
      while (remaining > 0) {
        const room = fightSeconds - elapsed;
        if (remaining < room) {
          elapsed += remaining;
          remaining = 0;
        } else {
          remaining -= room;
          switchTo(rotation.next());
        }
      }
      return snapshot();
    },
  };
}
