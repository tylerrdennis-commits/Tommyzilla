# Tommyzilla

Browser prototype. Phase 1 covers character locomotion: the bind pose is a T-pose, and the exported clip list leads with a static `TPose` animation. Playing that first clip freezes the body. Locomotion selects `Walk` while moving and `Idle` while standing, binds the torso to the hip skeleton, and advances the animation mixer every frame.

Phase 2 locks identity. Each shown hero is one catalog record: display name, model kind, and skin are set together in `src/character/catalog.js`, and spawn copies that record onto the mesh and the nameplate. The title lineup stays these three heroes. Infinity Stones are a separate pre-run choice and do not change the catalog.

Phase 3a adds 16 player variants in `src/character/variants.js`. Each variant has its own display name, silhouette, and palette, and a shiny palette that flips the hue. The nameplate keeps the variant name. A shiny adds a gold spark, a glow, and a star on the floating name. Players start with 30 eggs. Spending 10 eggs rolls a shiny for the selected variant at **1 in 10** (`random() < 0.1`). The odds line in the dock says the spend is a chance, not a guarantee. A miss still spends the 10 eggs and does not remove a shiny already earned. Fewer than 10 eggs disables the button and shows how many eggs are left. No variant selected disables it too, and does not spend eggs.

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

The variant dock lists 16 more looks. The title lineup stays Yeti, Tommyzilla, and Dragon. Pick a variant, then press Play to bring that mesh onto the field. The dock shows the egg count and the line "Spend 10 eggs for a 1 in 10 chance at a shiny. This is a chance, not a guarantee." Spend 10 eggs rolls the selected variant. A hit switches that variant to its shiny colors. With fewer than 10 eggs the button stays disabled and the dock says you need 10. A miss keeps a shiny you already earned.

`node scripts/verify-walk.mjs` checks locomotion in a headless browser: menu walk, play walk, idle arms down, and Esc back to the walk cycle. `node scripts/verify-names.mjs` checks the spot-check list: each roster label, nameplate, and play-mode hint matches the skin of that hero. `node scripts/verify-variants.mjs` checks the 16-variant list, the odds copy, a refused spend below 10 eggs, a miss, a shiny hit, and that the shiny still walks. `node scripts/verify-stones.mjs` checks the Infinity Stone picker, each explanation, and that an equipped stone changes the run. The dev server must already be running.

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
