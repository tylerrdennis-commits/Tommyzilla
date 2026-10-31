# Tommyzilla

Browser prototype. Phase 1 covers character locomotion: the bind pose is a T-pose, and the exported clip list leads with a static `TPose` animation. Playing that first clip freezes the body. Locomotion selects `Walk` while moving and `Idle` while standing, binds the torso to the hip skeleton, and advances the animation mixer every frame.

Phase 2 locks identity. Each shown hero is one catalog record: display name, model kind, and skin are set together in `src/character/catalog.js`, and spawn copies that record onto the mesh and the nameplate. The title screen shows the base cast only. Variants, shinies, egg rolls, and boss encounters are later phases. Infinity Stones are a separate pre-run choice and do not change the catalog.

| Name on screen | Model | Skin to look for |
| --- | --- | --- |
| Tommyzilla | dinosaur | Teal body and limbs, three dorsal plates, a tail |
| Yeti | yeti | White body and limbs, heavy brow, no tail |
| Dragon | dragon | Green body and limbs, snout, two wings, a tail |

No NPCs are shown in this build. The game title in the panel is the title, not a character name.

## Run

```bash
npm install
npm test
npm run dev
```

Open the dev server. The title screen shows three walking heroes. Left to right they are Yeti (white, heavy brow, no tail), Tommyzilla (teal, three dorsal plates, tail), and Dragon (green, snout, wings, tail). The name above each body matches that mesh, and the roster buttons repeat those same names. Choose a row, press Play, and the hint uses that name for the mesh on the field. WASD or the arrow keys steer; release them to stand. Esc returns to the title.

`node scripts/verify-walk.mjs` checks locomotion in a headless browser: menu walk, play walk, idle arms down, and Esc back to the walk cycle. `node scripts/verify-names.mjs` checks the spot-check list: each roster label, nameplate, and play-mode hint matches the skin of that hero. `node scripts/verify-stones.mjs` checks the Infinity Stone picker, each explanation, and that an equipped stone changes the run. The dev server must already be running.

## Infinity Stones

The title screen lists the Infinity Stones in the roster panel, above Play. Every stone shows what it does. Click a stone to bring it, and click it again to leave it behind. Play uses that set. The field hint repeats the explanation for each stone you brought. Leaving the run with Esc keeps the same set, so you can change it and play again. Bringing none leaves movement, size, and the field as they are.

| Stone | What it does |
| --- | --- |
| Space | Each step covers more ground. |
| Mind | Shows facing and pace on screen. |
| Reality | Recolors the field while equipped. |
| Power | Swells the hero's size for the run. |
| Time | Slows the run, easing each step. |
| Soul | An aura that follows the hero. |
