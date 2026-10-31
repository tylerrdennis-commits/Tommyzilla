/**
 * Infinity Stones are a pre-run meta choice. They are not character variants
 * and they do not belong on the hero catalog.
 */
export const INFINITY_STONES = [
  {
    id: 'space',
    name: 'Space',
    color: '#4c6fff',
    explanation: 'Each step covers more ground.',
  },
  {
    id: 'mind',
    name: 'Mind',
    color: '#f0d040',
    explanation: 'Shows facing and pace on screen.',
  },
  {
    id: 'reality',
    name: 'Reality',
    color: '#e23b4a',
    explanation: 'Recolors the field while equipped.',
  },
  {
    id: 'power',
    name: 'Power',
    color: '#a45cff',
    explanation: "Swells the hero's size for the run.",
  },
  {
    id: 'time',
    name: 'Time',
    color: '#3dbe5a',
    explanation: 'Slows the run, easing each step.',
  },
  {
    id: 'soul',
    name: 'Soul',
    color: '#f08a2a',
    explanation: 'An aura that follows the hero.',
  },
];

export function listStones() {
  return INFINITY_STONES;
}

export function getStone(id) {
  const stone = INFINITY_STONES.find((entry) => entry.id === id);
  if (!stone) {
    throw new Error(`Unknown stone id: ${id}`);
  }
  return stone;
}
