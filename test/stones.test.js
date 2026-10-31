import assert from 'node:assert/strict';
import { test } from 'node:test';
import { getStone, listStones } from '../src/stones/catalog.js';
import { createLoadout } from '../src/stones/loadout.js';
import {
  boostedPosition,
  equippedLines,
  headingLabel,
  scaledDt,
  senseLine,
  stoneModifiers,
} from '../src/stones/effects.js';
import { choiceView } from '../src/stones/view.js';

const EFFECT_PHRASE = {
  space: /covers more ground/,
  mind: /facing and pace/,
  reality: /Recolors the field/,
  power: /hero's size/,
  time: /Slows the run/,
  soul: /aura that follows/,
};

function simulateStep(ids, dt = 0.5) {
  const modifiers = stoneModifiers(ids);
  const step = scaledDt(dt, modifiers);
  const before = { x: 0, y: 0, z: 0 };
  const after = { x: 0, y: 0, z: 4 * step };
  return boostedPosition(before, after, modifiers.speedMultiplier);
}

test('the six Infinity Stones each explain a different effect', () => {
  const stones = listStones();
  assert.deepEqual(stones.map((stone) => stone.id), [
    'space',
    'mind',
    'reality',
    'power',
    'time',
    'soul',
  ]);

  const names = new Set();
  const colors = new Set();
  const explanations = new Set();
  for (const stone of stones) {
    assert.equal(getStone(stone.id), stone);
    assert.match(stone.name, /^[A-Z][a-z]+$/);
    assert.match(stone.color, /^#[0-9a-f]{6}$/);
    assert.ok(stone.explanation.length > 24, stone.id);
    assert.match(stone.explanation, EFFECT_PHRASE[stone.id]);
    names.add(stone.name);
    colors.add(stone.color);
    explanations.add(stone.explanation);
  }
  assert.equal(names.size, 6);
  assert.equal(colors.size, 6);
  assert.equal(explanations.size, 6);
});

test('an unknown stone id is rejected', () => {
  assert.throws(() => getStone('gauntlet'), /Unknown stone id: gauntlet/);
  const loadout = createLoadout();
  assert.throws(() => loadout.toggle('gauntlet'), /Unknown stone id: gauntlet/);
  assert.throws(() => createLoadout(['gauntlet']), /Unknown stone id: gauntlet/);
});

test('the pre-run loadout starts empty and toggles stones on and off', () => {
  const loadout = createLoadout();
  assert.deepEqual(loadout.ids(), []);
  assert.equal(loadout.has('space'), false);

  assert.equal(loadout.toggle('power'), true);
  assert.equal(loadout.toggle('soul'), true);
  assert.deepEqual(loadout.ids(), ['power', 'soul']);

  assert.equal(loadout.toggle('power'), false);
  assert.deepEqual(loadout.ids(), ['soul']);
  assert.equal(loadout.has('soul'), true);
});

test('choice rows show every stone explanation and the pressed set', () => {
  const loadout = createLoadout();
  loadout.toggle('time');
  loadout.toggle('mind');
  const rows = choiceView(loadout);

  assert.deepEqual(rows.map((row) => row.id), listStones().map((stone) => stone.id));
  for (const row of rows) {
    const stone = getStone(row.id);
    assert.equal(row.name, stone.name);
    assert.equal(row.color, stone.color);
    assert.equal(row.explanation, stone.explanation);
    assert.equal(row.pressed, row.id === 'time' || row.id === 'mind');
  }
});

test('an empty loadout leaves the run unchanged', () => {
  const modifiers = stoneModifiers([]);
  assert.equal(modifiers.speedMultiplier, 1);
  assert.equal(modifiers.timeScale, 1);
  assert.equal(modifiers.heroScale, 1);
  assert.equal(modifiers.realityField, null);
  assert.equal(modifiers.realityFog, null);
  assert.equal(modifiers.realityGround, null);
  assert.equal(modifiers.showSense, false);
  assert.equal(modifiers.showAura, false);
  assert.equal(scaledDt(0.2, modifiers), 0.2);
  assert.deepEqual(equippedLines([]), []);
});

test('each stone changes only its own part of the run', () => {
  const space = stoneModifiers(['space']);
  assert.equal(space.speedMultiplier, 1.65);
  assert.equal(space.timeScale, 1);
  assert.equal(space.heroScale, 1);
  assert.equal(space.showSense, false);
  assert.equal(space.showAura, false);
  assert.equal(space.realityField, null);

  const time = stoneModifiers(['time']);
  assert.equal(time.timeScale, 0.45);
  assert.equal(time.speedMultiplier, 1);
  assert.equal(time.heroScale, 1);

  const power = stoneModifiers(['power']);
  assert.equal(power.heroScale, 1.45);
  assert.equal(power.speedMultiplier, 1);
  assert.equal(power.timeScale, 1);

  const reality = stoneModifiers(['reality']);
  assert.equal(reality.realityField, '#3a1820');
  assert.equal(reality.realityFog, '#5c2832');
  assert.equal(reality.realityGround, '#6a3038');
  assert.equal(reality.heroScale, 1);
  assert.equal(reality.speedMultiplier, 1);

  const mind = stoneModifiers(['mind']);
  assert.equal(mind.showSense, true);
  assert.equal(mind.showAura, false);
  assert.equal(mind.speedMultiplier, 1);
  assert.equal(mind.timeScale, 1);
  assert.equal(mind.heroScale, 1);

  const soul = stoneModifiers(['soul']);
  assert.equal(soul.showAura, true);
  assert.equal(soul.showSense, false);
  assert.equal(soul.heroScale, 1);
  assert.equal(soul.speedMultiplier, 1);
});

test('space and time compose, and the other stones do not change stride', () => {
  const baseline = simulateStep([]);
  const space = simulateStep(['space']);
  const time = simulateStep(['time']);
  const both = simulateStep(['space', 'time']);

  assert.ok(Math.abs(space.z / baseline.z - 1.65) < 1e-9);
  assert.ok(Math.abs(time.z / baseline.z - 0.45) < 1e-9);
  assert.ok(Math.abs(both.z / baseline.z - (1.65 * 0.45)) < 1e-9);

  for (const id of ['mind', 'reality', 'power', 'soul']) {
    assert.equal(simulateStep([id]).z, baseline.z);
  }
});

test('mind names the facing and the pace', () => {
  assert.equal(headingLabel(0), 'north');
  assert.equal(headingLabel(Math.PI / 2), 'east');
  assert.equal(headingLabel(Math.PI), 'south');
  assert.equal(headingLabel(-Math.PI / 2), 'west');
  assert.equal(senseLine({ yaw: 0, pace: 6.6 }), 'Facing north · pace 6.6');
  assert.equal(senseLine({ yaw: Math.PI, pace: 0 }), 'Facing south · pace 0.0');
});

test('the play readout explains each equipped stone and skips the rest', () => {
  const lines = equippedLines(['soul', 'space']);
  assert.deepEqual(lines, [
    `Space: ${getStone('space').explanation}`,
    `Soul: ${getStone('soul').explanation}`,
  ]);
  assert.equal(lines.some((line) => line.startsWith('Power:')), false);
});
