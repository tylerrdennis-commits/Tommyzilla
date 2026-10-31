import { choiceView } from './view.js';
import { equippedLines } from './effects.js';
import './panel.css';

export function mountStonePanel(menu, loadout, onToggle) {
  const section = document.createElement('section');
  section.id = 'infinity-stones';
  section.setAttribute('aria-labelledby', 'infinity-stones-heading');

  const heading = document.createElement('h2');
  heading.id = 'infinity-stones-heading';
  heading.textContent = 'Infinity Stones';

  const intro = document.createElement('p');
  intro.id = 'stone-intro';
  intro.textContent = 'Bring any set. The line under each name is what that stone does.';

  const list = document.createElement('div');
  list.id = 'stone-choices';
  section.append(heading, intro, list);
  const play = menu.querySelector('#play');
  const panel = play?.parentElement ?? menu;
  if (play) {
    panel.insertBefore(section, play);
    const scroller = document.createElement('div');
    scroller.id = 'menu-scroll';
    while (panel.firstChild && panel.firstChild !== play) {
      scroller.append(panel.firstChild);
    }
    panel.insertBefore(scroller, play);
  } else {
    panel.append(section);
  }

  for (const row of choiceView(loadout)) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'stone-choice';
    button.dataset.stoneId = row.id;
    button.title = row.explanation;

    const gem = document.createElement('span');
    gem.className = 'stone-gem';
    gem.style.background = row.color;

    const copy = document.createElement('span');
    copy.className = 'stone-copy';

    const nameRow = document.createElement('span');
    nameRow.className = 'stone-name-row';
    const name = document.createElement('span');
    name.className = 'stone-name';
    name.textContent = row.name;
    const state = document.createElement('span');
    state.className = 'stone-state';
    nameRow.append(name, state);

    const effect = document.createElement('span');
    effect.className = 'stone-effect';
    effect.textContent = row.explanation;

    copy.append(nameRow, effect);
    button.append(gem, copy);
    button.addEventListener('click', () => onToggle(row.id));
    list.append(button);
  }

  function refresh() {
    for (const row of choiceView(loadout)) {
      const button = list.querySelector(`[data-stone-id="${row.id}"]`);
      button.setAttribute('aria-pressed', String(row.pressed));
      button.querySelector('.stone-state').textContent = row.pressed ? 'Equipped' : 'Off';
    }
  }

  refresh();
  return { refresh };
}

export function mountPlayReadout(playHint) {
  const readout = document.createElement('div');
  readout.id = 'stone-readout';
  const sense = document.createElement('div');
  sense.id = 'stone-sense';
  sense.classList.add('hidden');
  playHint.append(readout, sense);

  function refresh(loadout) {
    const lines = equippedLines(loadout.ids());
    readout.replaceChildren();
    if (lines.length === 0) {
      readout.textContent = 'No Infinity Stones equipped.';
      return;
    }
    for (const line of lines) {
      const row = document.createElement('div');
      row.className = 'stone-readout-line';
      row.textContent = line;
      readout.append(row);
    }
  }

  return { readout, sense, refresh };
}
