import assert from 'node:assert/strict';
import { test } from 'node:test';
import { getVariant, listVariants } from '../src/character/variants.js';
import { listEncounters } from '../src/boss/encounters.js';
import { patternLabel, samplePattern } from '../src/boss/patterns.js';
import { createEncounterRotation } from '../src/boss/select.js';
import { bossBanner, createBossRun } from '../src/boss/fight.js';

const SAMPLE_TIMES = [0, 0.35, 0.8, 1.4, 2.2, 3.3, 5, 7.5, 8, 11.2];

function sequenceRandom(seed) {
  let state = seed >>> 0;
  return () => {
    state = (Math.imul(1664525, state) + 1013904223) >>> 0;
    return state / 4294967296;
  };
}

function samplesFor(pattern) {
  return SAMPLE_TIMES.map((time) => samplePattern(pattern, time));
}

test('the encounter table gives every variant its own boss and pattern', () => {
  const encounters = listEncounters();
  const variants = listVariants();
  assert.ok(encounters.length >= variants.length, `expected a boss for each of ${variants.length} variants`);
  assert.equal(encounters.length, variants.length);

  const ids = encounters.map((encounter) => encounter.id);
  const variantIds = encounters.map((encounter) => encounter.variantId);
  const patterns = encounters.map((encounter) => encounter.pattern);
  const labels = patterns.map((pattern) => patternLabel(pattern));
  assert.equal(new Set(ids).size, encounters.length);
  assert.equal(new Set(variantIds).size, encounters.length);
  assert.equal(new Set(patterns).size, encounters.length);
  assert.equal(new Set(labels).size, encounters.length);
  assert.deepEqual(new Set(variantIds), new Set(variants.map((variant) => variant.id)));

  for (const encounter of encounters) {
    const variant = getVariant(encounter.variantId);
    assert.equal(encounter.displayName, variant.displayName);
    assert.equal(typeof encounter.shiny, 'boolean');
    assert.ok(patternLabel(encounter.pattern).length > 12, encounter.pattern);
  }
  assert.ok(encounters.some((encounter) => encounter.shiny));
  assert.ok(encounters.some((encounter) => !encounter.shiny));
});

test('fight scripts trace different paths inside the arena', () => {
  const encounters = listEncounters();
  const signatures = new Map();
  let alwaysMoving = 0;
  let sometimesStill = 0;

  for (const encounter of encounters) {
    const samples = samplesFor(encounter.pattern);
    for (const sample of samples) {
      assert.ok(sample.x <= 1.2 && sample.x >= -3.2, `${encounter.pattern} x ${sample.x}`);
      assert.ok(sample.z <= 2.5 && sample.z >= 0.2, `${encounter.pattern} z ${sample.z}`);
      assert.equal(typeof sample.moving, 'boolean');
      assert.equal(Number.isFinite(sample.yaw), true);
    }
    const moved = samples.some((sample, index) => (
      index > 0 && (sample.x !== samples[0].x || sample.z !== samples[0].z)
    ));
    assert.equal(moved, true, `${encounter.pattern} never leaves its start`);
    if (samples.every((sample) => sample.moving)) alwaysMoving += 1;
    if (samples.some((sample) => !sample.moving)) sometimesStill += 1;

    const signature = samples
      .map((sample) => `${sample.x.toFixed(3)},${sample.z.toFixed(3)},${sample.moving ? 1 : 0}`)
      .join('|');
    assert.equal(signatures.has(signature), false, `${encounter.pattern} matches ${signatures.get(signature)}`);
    signatures.set(signature, encounter.pattern);
  }

  assert.ok(alwaysMoving >= 4, `only ${alwaysMoving} patterns keep moving`);
  assert.ok(sometimesStill >= 2, `only ${sometimesStill} patterns pause`);
});

test('unknown patterns are rejected', () => {
  assert.throws(() => samplePattern('not-a-pattern', 0), /Unknown pattern/);
  assert.throws(() => patternLabel('not-a-pattern'), /Unknown pattern/);
});

test('a short run never repeats the same encounter back to back', () => {
  const encounters = listEncounters();
  assert.ok(encounters.length >= 16, `roster too small to prove variety (${encounters.length})`);
  const rotation = createEncounterRotation(encounters, sequenceRandom(1));
  const drawn = [];
  for (let i = 0; i < encounters.length * 3; i += 1) drawn.push(rotation.next());

  for (let i = 1; i < drawn.length; i += 1) {
    assert.notEqual(drawn[i].id, drawn[i - 1].id, `id repeated at ${i}`);
    assert.notEqual(drawn[i].pattern, drawn[i - 1].pattern, `pattern repeated at ${i}`);
    assert.notEqual(drawn[i].variantId, drawn[i - 1].variantId, `variant repeated at ${i}`);
  }

  for (let cycle = 0; cycle < 3; cycle += 1) {
    const window = drawn.slice(cycle * encounters.length, (cycle + 1) * encounters.length);
    assert.equal(new Set(window.map((encounter) => encounter.id)).size, encounters.length);
  }

  const again = [];
  const replay = createEncounterRotation(encounters, sequenceRandom(1));
  for (let i = 0; i < encounters.length; i += 1) again.push(replay.next().id);
  assert.deepEqual(again, drawn.slice(0, encounters.length).map((encounter) => encounter.id));
});

test('different seeds open a run on different bosses', () => {
  const openers = new Set();
  for (let seed = 1; seed <= 8; seed += 1) {
    openers.add(createEncounterRotation(listEncounters(), sequenceRandom(seed)).next().id);
  }
  assert.ok(openers.size >= 3, `only ${[...openers].join(', ')} opening bosses`);
});

test('the fight script stays on one boss until the round ends', () => {
  const run = createBossRun({ random: sequenceRandom(4), fightSeconds: 2 });
  const opening = run.current();
  assert.equal(opening.elapsed, 0);
  assert.deepEqual(run.history(), [opening.id]);

  const held = run.update(1.25);
  assert.equal(held.id, opening.id);
  assert.ok(Math.abs(held.elapsed - 1.25) < 1e-9);
  const pose = samplePattern(opening.pattern, 1.25);
  assert.equal(held.x, pose.x);
  assert.equal(held.z, pose.z);
  assert.equal(held.yaw, pose.yaw);
  assert.equal(held.moving, pose.moving);
  assert.equal(held.patternLabel, patternLabel(opening.pattern));
  const name = opening.shiny ? `Shiny ${opening.displayName}` : opening.displayName;
  assert.equal(bossBanner(opening), `Boss ${opening.index}: ${name} — ${opening.patternLabel}`);
});

test('ending a round or skipping ahead switches to a different boss', () => {
  const run = createBossRun({ random: sequenceRandom(4), fightSeconds: 2 });
  const opening = run.current();
  const next = run.update(2);
  assert.notEqual(next.id, opening.id);
  assert.equal(next.elapsed, 0);
  assert.notEqual(next.pattern, opening.pattern);
  assert.deepEqual(run.history(), [opening.id, next.id]);

  const carried = run.update(2.4);
  assert.notEqual(carried.id, next.id);
  assert.ok(Math.abs(carried.elapsed - 0.4) < 1e-9);

  const forced = run.advance();
  assert.notEqual(forced.id, carried.id);
  assert.equal(forced.elapsed, 0);
  assert.deepEqual(run.history(), [opening.id, next.id, carried.id, forced.id]);
});
