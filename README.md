# Tommyzilla

Browser prototype. Phase 1 covers character locomotion: the bind pose is a T-pose, and the exported clip list leads with a static `TPose` animation. Playing that first clip freezes the body. Locomotion selects `Walk` while moving and `Idle` while standing, binds the torso to the hip skeleton, and advances the animation mixer every frame.

## Run

```bash
npm install
npm test
npm run dev
```

Open the dev server. On the title screen the character walks in place (arms down, legs striding). Press Play and the character keeps walking. WASD or the arrow keys steer; release them to stand. Esc returns to the title.

`node scripts/verify-walk.mjs` checks the same path in a headless browser: menu walk, play walk, idle arms down, and Esc back to the walk cycle. The dev server must already be running.
