import assert from 'node:assert/strict';
import { test } from 'node:test';
import { listShownCharacters } from '../src/character/catalog.js';
import { createCharacterModel } from '../src/character/model.js';
import { spawnVariant } from '../src/character/spawn.js';
import { getVariant, listVariants } from '../src/character/variants.js';

function partColor(scene, part) {
  let color = null;
  scene.traverse((obj) => {
    if (obj.isMesh && obj.userData.part === part && color === null) {
      color = obj.material.color.getHexString();
    }
  });
  return color;
}

function marks(scene) {
  const names = [];
  scene.traverse((obj) => {
    if (obj.userData.identityMark) names.push(obj.name);
  });
  return names.sort();
}

function hasShinySpark(scene) {
  let found = false;
  scene.traverse((obj) => {
    if (obj.name === 'ShinySpark') found = true;
  });
  return found;
}

test('at least 16 variants are distinct looks a player can spawn', () => {
  const variants = listVariants();
  assert.ok(variants.length >= 16, `expected >= 16 variants, saw ${variants.length}`);

  const ids = variants.map((variant) => variant.id);
  const names = variants.map((variant) => variant.displayName);
  assert.equal(new Set(ids).size, variants.length);
  assert.equal(new Set(names).size, variants.length);

  const baseNames = new Set(listShownCharacters().map((character) => character.displayName));
  for (const name of names) {
    assert.equal(baseNames.has(name), false, `${name} collides with a base hero name`);
  }

  const signatures = [];
  for (const variant of variants) {
    const model = spawnVariant(variant.id);
    assert.equal(model.displayName, variant.displayName);
    assert.equal(model.scene.userData.characterId, variant.id);
    assert.equal(model.scene.userData.modelKind, variant.modelKind);
    assert.equal(model.scene.userData.shiny, false);
    assert.equal(model.scene.getObjectByName('Nameplate').userData.label, variant.displayName);
    assert.equal(partColor(model.scene, 'body'), variant.palette.body.slice(1));
    assert.equal(hasShinySpark(model.scene), false);
    assert.ok(marks(model.scene).length > 1, `${variant.id} needs a silhouette beyond a bare tail`);
    signatures.push(`${variant.palette.body}|${marks(model.scene).join('+')}|${variant.modelKind}`);
    assert.equal(model.animations.map((clip) => clip.name).join(','), 'TPose,Attack,Walk,Idle');
  }
  assert.equal(new Set(signatures).size, variants.length);
});

test('every variant has a shiny form that does not match its regular colors', () => {
  for (const variant of listVariants()) {
    assert.notEqual(variant.shinyPalette.body, variant.palette.body, variant.id);
    const shiny = createCharacterModel(variant.id, { shiny: true });
    assert.equal(shiny.displayName, variant.displayName);
    assert.equal(shiny.scene.userData.shiny, true);
    assert.equal(shiny.scene.userData.characterId, variant.id);
    assert.equal(shiny.scene.getObjectByName('Nameplate').userData.label, variant.displayName);
    assert.equal(partColor(shiny.scene, 'body'), variant.shinyPalette.body.slice(1));
    assert.notEqual(partColor(shiny.scene, 'body'), variant.palette.body.slice(1));
    assert.equal(hasShinySpark(shiny.scene), true);
    const body = shiny.scene.getObjectByName('Torso');
    assert.ok(body.material.emissive.getHex() > 0, `${variant.id} shiny body should glow`);
  }
});

test('known variants keep a stable name and silhouette', () => {
  const moss = getVariant('moss-titan');
  const moth = getVariant('luna-moth');
  const stag = getVariant('thorn-stag');
  assert.equal(moss.displayName, 'Moss Titan');
  assert.equal(moth.displayName, 'Luna Moth');
  assert.equal(stag.displayName, 'Thorn Stag');
  assert.ok(marks(createCharacterModel('moss-titan').scene).includes('SpineSpike'));
  assert.ok(marks(createCharacterModel('luna-moth').scene).includes('MothWing'));
  assert.ok(marks(createCharacterModel('thorn-stag').scene).includes('Antler'));
});

test('base heroes stay regular and keep their catalog identity when variants exist', () => {
  const tommy = createCharacterModel('tommyzilla');
  assert.equal(tommy.displayName, 'Tommyzilla');
  assert.equal(tommy.scene.userData.modelKind, 'dinosaur');
  assert.equal(tommy.scene.userData.shiny, false);
  assert.equal(hasShinySpark(tommy.scene), false);
  assert.deepEqual(marks(tommy.scene), ['DorsalPlate', 'DorsalPlate', 'DorsalPlate', 'TailMesh']);
});
