import { listShownCharacters } from './catalog.js';
import { createCharacterModel } from './model.js';

/** Spawn every catalog hero. Name and mesh come from the same record. */
export function spawnShownCharacters() {
  return listShownCharacters().map((character) => createCharacterModel(character.id));
}
