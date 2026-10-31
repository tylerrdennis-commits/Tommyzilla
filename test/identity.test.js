import assert from 'node:assert/strict';
import { test } from 'node:test';
import { listShownCharacters, getCharacter } from '../src/character/catalog.js';
import { createCharacterModel } from '../src/character/model.js';
import { spawnShownCharacters } from '../src/character/spawn.js';

function meshColor(object) {
  const mesh = object?.children?.find((child) => child.isMesh && child.userData.part);
  return mesh?.material?.color?.getHexString() ?? null;
}

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

const EXPECTED = {
  tommyzilla: {
    displayName: 'Tommyzilla',
    modelKind: 'dinosaur',
    marks: ['DorsalPlate', 'DorsalPlate', 'DorsalPlate', 'TailMesh'],
  },
  yeti: {
    displayName: 'Yeti',
    modelKind: 'yeti',
    marks: ['Brow'],
  },
  dragon: {
    displayName: 'Dragon',
    modelKind: 'dragon',
    marks: ['Snout', 'TailMesh', 'Wing', 'Wing'],
  },
};

test('every shown hero is spawned from one catalog record', () => {
  const shown = listShownCharacters();
  assert.deepEqual(shown.map((character) => character.id), ['tommyzilla', 'yeti', 'dragon']);
  const spawned = spawnShownCharacters();
  assert.equal(spawned.length, shown.length);

  spawned.forEach((model, index) => {
    const character = shown[index];
    assert.equal(model.displayName, character.displayName);
    assert.equal(model.scene.userData.characterId, character.id);
    assert.equal(model.scene.userData.modelKind, character.modelKind);
    assert.equal(model.scene.getObjectByName('Nameplate').userData.label, character.displayName);
  });
});

test('the name on a hero matches that mesh, including a uniform skin', () => {
  for (const [id, expected] of Object.entries(EXPECTED)) {
    const character = getCharacter(id);
    const model = createCharacterModel(id);
    const leftThigh = meshColor(model.scene.getObjectByName('LeftThigh'));
    const rightThigh = meshColor(model.scene.getObjectByName('RightThigh'));
    const leftArm = meshColor(model.scene.getObjectByName('LeftUpperArm'));
    const rightArm = meshColor(model.scene.getObjectByName('RightUpperArm'));
    const body = partColor(model.scene, 'body');

    assert.equal(character.displayName, expected.displayName);
    assert.equal(character.modelKind, expected.modelKind);
    assert.equal(model.displayName, expected.displayName);
    assert.equal(model.scene.userData.modelKind, expected.modelKind);
    assert.equal(leftThigh, character.palette.limb.slice(1));
    assert.equal(rightThigh, leftThigh);
    assert.equal(leftArm, rightArm);
    assert.equal(leftArm, character.palette.limb.slice(1));
    assert.equal(body, character.palette.body.slice(1));
    assert.deepEqual(marks(model.scene), expected.marks);
    assert.equal(model.scene.getObjectByName('Nameplate').userData.label, expected.displayName);
  }
});
