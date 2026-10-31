import { getStone, listStones } from './catalog.js';

/** Stones chosen before a run. Empty means the field plays as usual. */
export function createLoadout(initial = []) {
  const selected = new Set();
  for (const id of initial) {
    getStone(id);
    selected.add(id);
  }

  return {
    ids() {
      return listStones().filter((stone) => selected.has(stone.id)).map((stone) => stone.id);
    },
    has(id) {
      return selected.has(id);
    },
    toggle(id) {
      getStone(id);
      if (selected.has(id)) selected.delete(id);
      else selected.add(id);
      return selected.has(id);
    },
  };
}
