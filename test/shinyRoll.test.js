import assert from 'node:assert/strict';
import { test } from 'node:test';
import {
  SHINY_CHANCE,
  SHINY_EGG_COST,
  SHINY_ODDS_COPY,
  STARTING_EGGS,
  applyShinyRoll,
  blockedReason,
  describeShinyRoll,
  rollShinyChance,
} from '../src/character/shinyRoll.js';

test('shiny chance costs 10 eggs at 1 in 10 and says it is not a guarantee', () => {
  assert.equal(SHINY_EGG_COST, 10);
  assert.equal(SHINY_CHANCE, 0.1);
  assert.equal(STARTING_EGGS >= SHINY_EGG_COST, true);
  assert.match(SHINY_ODDS_COPY, /10 eggs/);
  assert.match(SHINY_ODDS_COPY, /1 in 10/);
  assert.match(SHINY_ODDS_COPY, /chance/i);
  assert.match(SHINY_ODDS_COPY, /not a guarantee/i);
});

test('a low roll spends 10 eggs and awards a shiny', () => {
  const result = rollShinyChance(30, { variantId: 'moss-titan', random: () => 0 });
  assert.equal(result.ok, true);
  assert.equal(result.shiny, true);
  assert.equal(result.reason, 'shiny');
  assert.equal(result.spent, 10);
  assert.equal(result.eggs, 20);
  assert.equal(result.chance, 0.1);
  assert.match(describeShinyRoll({ ...result, variantName: 'Moss Titan' }), /Shiny Moss Titan/);
});

test('a roll on the boundary misses and still spends the eggs', () => {
  const result = rollShinyChance(10, { variantId: 'moss-titan', random: () => 0.1 });
  assert.equal(result.ok, true);
  assert.equal(result.shiny, false);
  assert.equal(result.reason, 'regular');
  assert.equal(result.eggs, 0);
  assert.equal(result.spent, 10);
  const copy = describeShinyRoll({ ...result, variantName: 'Moss Titan' });
  assert.match(copy, /Moss Titan/);
  assert.match(copy, /regular|no shiny/i);
  assert.match(copy, /spent/i);
});

test('just under the odds still hits', () => {
  const result = rollShinyChance(10, { variantId: 'reef-shark', random: () => 0.099999 });
  assert.equal(result.shiny, true);
  assert.equal(result.eggs, 0);
});

test('fewer than 10 eggs is refused and the balance stays put', () => {
  for (const eggs of [0, 9]) {
    const result = rollShinyChance(eggs, { variantId: 'moss-titan', random: () => 0 });
    assert.equal(result.ok, false);
    assert.equal(result.reason, 'insufficient-eggs');
    assert.equal(result.shiny, false);
    assert.equal(result.spent, 0);
    assert.equal(result.eggs, eggs);
    assert.match(blockedReason({ eggs, variantId: 'moss-titan' }), /Need 10 eggs/);
    assert.match(blockedReason({ eggs, variantId: 'moss-titan' }), new RegExp(`you have ${eggs}`));
  }
});

test('a shiny roll without a chosen variant does not spend eggs', () => {
  const result = rollShinyChance(30, { random: () => 0 });
  assert.equal(result.ok, false);
  assert.equal(result.reason, 'no-variant');
  assert.equal(result.eggs, 30);
  assert.equal(result.spent, 0);
  assert.equal(blockedReason({ eggs: 30, variantId: null }), 'Choose a variant first.');
  assert.equal(blockedReason({ eggs: 30, variantId: 'moss-titan' }), '');
});

test('a miss keeps a shiny already earned, and a hit turns a regular shiny', () => {
  const miss = rollShinyChance(20, { variantId: 'luna-moth', random: () => 0.95 });
  const hit = rollShinyChance(20, { variantId: 'luna-moth', random: () => 0 });
  const refused = rollShinyChance(3, { variantId: 'luna-moth', random: () => 0 });
  assert.equal(applyShinyRoll(true, miss), true);
  assert.equal(applyShinyRoll(false, miss), false);
  assert.equal(applyShinyRoll(false, hit), true);
  assert.equal(applyShinyRoll(true, refused), true);
});
