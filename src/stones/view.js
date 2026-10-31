import { listStones } from './catalog.js';

/** Rows for the pre-run picker. Explanations stay visible whether equipped or not. */
export function choiceView(loadout) {
  return listStones().map((stone) => ({
    id: stone.id,
    name: stone.name,
    color: stone.color,
    explanation: stone.explanation,
    pressed: loadout.has(stone.id),
  }));
}
