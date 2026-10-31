/**
 * Shiny hatch odds.
 * Spending 10 eggs buys one roll. The roll hits when random() < 0.1,
 * so the chance is 1 in 10 (10%). A miss still spends the eggs.
 * A miss does not take away a shiny already earned for that variant.
 * Fewer than 10 eggs refuses the roll and leaves the balance unchanged.
 * Players start with 30 eggs (three attempts) and there is no egg income in this phase.
 */
export const SHINY_EGG_COST = 10;
export const SHINY_CHANCE = 0.1;
export const STARTING_EGGS = 30;
export const SHINY_ODDS_COPY =
  'Spend 10 eggs for a 1 in 10 chance at a shiny. This is a chance, not a guarantee.';

export function blockedReason({ eggs, variantId }) {
  if (!variantId) return 'Choose a variant first.';
  if (eggs < SHINY_EGG_COST) {
    return `Need 10 eggs for a shiny chance (you have ${eggs}).`;
  }
  return '';
}

export function rollShinyChance(eggs, { variantId = null, random = Math.random } = {}) {
  if (!variantId) {
    return {
      ok: false,
      reason: 'no-variant',
      eggs,
      shiny: false,
      spent: 0,
      cost: SHINY_EGG_COST,
      chance: SHINY_CHANCE,
    };
  }
  if (eggs < SHINY_EGG_COST) {
    return {
      ok: false,
      reason: 'insufficient-eggs',
      eggs,
      shiny: false,
      spent: 0,
      cost: SHINY_EGG_COST,
      chance: SHINY_CHANCE,
    };
  }
  const shiny = random() < SHINY_CHANCE;
  return {
    ok: true,
    reason: shiny ? 'shiny' : 'regular',
    eggs: eggs - SHINY_EGG_COST,
    shiny,
    spent: SHINY_EGG_COST,
    cost: SHINY_EGG_COST,
    chance: SHINY_CHANCE,
    variantId,
  };
}

export function applyShinyRoll(hadShiny, result) {
  if (!result.ok) return hadShiny;
  if (result.shiny) return true;
  return hadShiny;
}

export function describeShinyRoll({ shiny, variantName, eggs }) {
  if (shiny) {
    return `Shiny ${variantName}! The 1 in 10 chance hit. Eggs left: ${eggs}.`;
  }
  return `No shiny this time. ${variantName} stayed regular. The 10 eggs are spent either way. Eggs left: ${eggs}.`;
}
