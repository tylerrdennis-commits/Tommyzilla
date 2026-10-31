import { getVariant, listVariants } from '../character/variants.js';
import { patternLabel } from './patterns.js';

/**
 * One boss per variant, and each pattern id is used once. Shiny alternates so
 * neighboring looks stay easy to tell apart. select.js is what keeps the next
 * fight from repeating the one that just ended.
 */
const ASSIGNMENTS = [
  ['moss-titan', false, 'lane-charge'],
  ['frost-warden', true, 'cross-charge'],
  ['vine-wyrm', false, 'wide-orbit'],
  ['copper-raptor', true, 'reverse-orbit'],
  ['basalt-golem', false, 'strafe-cut'],
  ['luna-moth', true, 'deep-strafe'],
  ['tide-serpent', false, 'pause-lunge'],
  ['amber-beetle', true, 'side-lunge'],
  ['cinder-phoenix', false, 'tight-spiral'],
  ['ink-kraken', true, 'open-spiral'],
  ['ivory-mammoth', false, 'sine-weave'],
  ['gale-griffin', true, 'cross-weave'],
  ['crimson-oni', false, 'corner-pincer'],
  ['reef-shark', true, 'diamond-pincer'],
  ['prism-warden', false, 'beat-stomp'],
  ['thorn-stag', true, 'ring-stomp'],
];

function buildEncounters() {
  const variants = listVariants();
  if (ASSIGNMENTS.length !== variants.length) {
    throw new Error(`Boss table covers ${ASSIGNMENTS.length} variants, catalog has ${variants.length}`);
  }
  const seenVariants = new Set();
  const seenPatterns = new Set();
  return ASSIGNMENTS.map(([variantId, shiny, pattern]) => {
    const variant = getVariant(variantId);
    patternLabel(pattern);
    if (seenVariants.has(variantId)) throw new Error(`Duplicate boss variant ${variantId}`);
    if (seenPatterns.has(pattern)) throw new Error(`Duplicate boss pattern ${pattern}`);
    seenVariants.add(variantId);
    seenPatterns.add(pattern);
    return {
      id: `boss-${variantId}`,
      variantId,
      displayName: variant.displayName,
      shiny,
      pattern,
    };
  });
}

export const BOSS_ENCOUNTERS = buildEncounters();

export function listEncounters() {
  return BOSS_ENCOUNTERS;
}
