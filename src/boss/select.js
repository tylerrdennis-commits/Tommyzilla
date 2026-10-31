/**
 * Shuffle-bag rotation. A cycle uses every encounter once. The next cycle
 * cannot open on the encounter that just closed, so back-to-back fights differ.
 */
function shuffle(items, random) {
  const bag = items.slice();
  for (let i = bag.length - 1; i > 0; i -= 1) {
    const j = Math.floor(random() * (i + 1));
    const swap = bag[i];
    bag[i] = bag[j];
    bag[j] = swap;
  }
  return bag;
}

export function createEncounterRotation(encounters, random = Math.random) {
  if (!encounters || encounters.length < 2) {
    throw new Error('Need at least two encounters');
  }
  let bag = [];
  let last = null;

  function refill() {
    bag = shuffle(encounters, random);
    if (last && bag[0].id === last.id) {
      const swapAt = bag.findIndex((entry, index) => index > 0 && entry.id !== last.id);
      const first = bag[0];
      bag[0] = bag[swapAt];
      bag[swapAt] = first;
    }
  }

  return {
    next() {
      if (bag.length === 0) refill();
      last = bag.shift();
      return last;
    },
  };
}
