/**
 * Base identity catalog. One record owns the display name, the model kind,
 * and the skin. Variants and shinies are later phases and are not listed here.
 */
export const SHOWN_CHARACTERS = [
  {
    id: 'tommyzilla',
    displayName: 'Tommyzilla',
    role: 'hero',
    modelKind: 'dinosaur',
    palette: {
      body: '#1f8a7a',
      limb: '#2a9d8f',
      limbShade: '#18756a',
      eye: '#3ee0c5',
      mark: '#0e3d36',
    },
  },
  {
    id: 'yeti',
    displayName: 'Yeti',
    role: 'hero',
    modelKind: 'yeti',
    palette: {
      body: '#d5dee6',
      limb: '#f2f5f8',
      limbShade: '#b7c5d1',
      eye: '#16343c',
      mark: '#8ea3b5',
    },
  },
  {
    id: 'dragon',
    displayName: 'Dragon',
    role: 'hero',
    modelKind: 'dragon',
    palette: {
      body: '#2f6b3a',
      limb: '#3d8f4a',
      limbShade: '#245630',
      eye: '#f0c14a',
      mark: '#c4a35a',
    },
  },
];

export function listShownCharacters() {
  return SHOWN_CHARACTERS;
}

export function getCharacter(id) {
  const character = SHOWN_CHARACTERS.find((entry) => entry.id === id);
  if (!character) {
    throw new Error(`Unknown character id: ${id}`);
  }
  return character;
}
