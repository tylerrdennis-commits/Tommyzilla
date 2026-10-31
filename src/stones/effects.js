import { getStone, listStones } from './catalog.js';

const SPACE_SPEED = 1.65;
const TIME_SCALE = 0.45;
const POWER_SCALE = 1.45;
const REALITY_FIELD = '#3a1820';
const REALITY_FOG = '#5c2832';
const REALITY_GROUND = '#6a3038';

const COMPASS = [
  { yaw: 0, label: 'north' },
  { yaw: Math.PI / 2, label: 'east' },
  { yaw: Math.PI, label: 'south' },
  { yaw: -Math.PI / 2, label: 'west' },
];

export function stoneModifiers(ids) {
  for (const id of ids) getStone(id);
  const selected = new Set(ids);
  const reality = selected.has('reality');
  return {
    speedMultiplier: selected.has('space') ? SPACE_SPEED : 1,
    timeScale: selected.has('time') ? TIME_SCALE : 1,
    heroScale: selected.has('power') ? POWER_SCALE : 1,
    realityField: reality ? REALITY_FIELD : null,
    realityFog: reality ? REALITY_FOG : null,
    realityGround: reality ? REALITY_GROUND : null,
    showSense: selected.has('mind'),
    showAura: selected.has('soul'),
  };
}

export function scaledDt(dt, modifiers) {
  return dt * modifiers.timeScale;
}

export function boostedPosition(before, after, multiplier) {
  return {
    x: before.x + (after.x - before.x) * multiplier,
    y: before.y + (after.y - before.y) * multiplier,
    z: before.z + (after.z - before.z) * multiplier,
  };
}

export function headingLabel(yaw) {
  let best = COMPASS[0];
  let bestDelta = Infinity;
  for (const point of COMPASS) {
    const delta = Math.abs(Math.atan2(Math.sin(yaw - point.yaw), Math.cos(yaw - point.yaw)));
    if (delta < bestDelta) {
      bestDelta = delta;
      best = point;
    }
  }
  return best.label;
}

export function senseLine({ yaw, pace }) {
  return `Facing ${headingLabel(yaw)} · pace ${pace.toFixed(1)}`;
}

/** Catalog order, so the readout stays stable while the player toggles. */
export function equippedLines(ids) {
  for (const id of ids) getStone(id);
  const selected = new Set(ids);
  return listStones()
    .filter((stone) => selected.has(stone.id))
    .map((stone) => `${stone.name}: ${stone.explanation}`);
}
