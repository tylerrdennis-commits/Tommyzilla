
## Rex (kaiju gen / eggs / eating / zoo / crazy mode)
- Commit 7fb41d3. New: src/game/kaiju_gen.{h,cpp} (namespace tz::kgen). KaijuLook gained `genSeed` (0 = classic model) and `element` (-1 none, 0 Earth,1 Wind,2 Fire,3 Lightning; Tommyzilla only); both saved in toJson/fromJson.
- `kgen::generate(look.genSeed, look)` -> KaijuGenome (body type, proportions, parts, colours) for rendering non-Tommyzilla kaiju. `kgen::usesProcedural(id)`.
- Game::decideSpin now uses `kgen::rollEgg(r, T, owned)` (dup-avoid rerolls). data/eggs.json spins.coinCost 400 -> 4000 (Vault: store reads E.spinCost, no change needed).
- Shared-file touches: CMakeLists.txt (+kaiju_gen.cpp in tz_core), tests/tests.cpp (+testKaijuGen, spinCost check).

## Titan (bosses, difficulty, co-op, police, helis)
- New: src/game/titan.h/.cpp (pure, tested in tests.cpp testTitan). `difficultyMods(difficulty, players)` -> DiffMods{enemyHp, enemyDamage, bossHp, coins, spawnRate}. Easy = 1.0 baseline.
- Game fields (game.h): `int difficulty` (DIFF_EASY/HARD/INSANE, `difficultyName()`), `int players` (1..2), `diff()`. enemyHpMult/enemyDamageMult/coin gain (Game::coin path in game.cpp) and THE BULK hp already scale.
- **Vault**: please add a difficulty picker (Easy / Hard / You're Insane) + "2 players" toggle in menus that sets `G->difficulty` / `G->players`. Not persisted yet (Titan may add a save field later).
- Police: `Enemies::chase` (PoliceChase). Siren 45% volume, silent once units arrive (<45m) so music plays; small groups (<=3) outrun at speedNow>=14 & >70m for 2.5s -> they leave, "searching"; Game::onBuildingSmashed resumes.
- Helis orbit/swoop low (heliSwoop). Mini-boss `u.boss==3` "Thunder Moth" (level json `"miniBoss": "moth"`); `Enemies::cheese` punishes point-blank camping / stomp spam for all mini-bosses.
- Note: my first changes landed inside Vault's commit e403c4f (shared git index race). Suggest everyone commit with a private index: `GIT_INDEX_FILE=/tmp/<name>.idx git read-tree HEAD` then stage + commit.

## Atlas (map / traffic / gauntlet stones)
- Adventure mode = procedural city "Zillaport" (src/game/city_gen.cpp, `buildProceduralCity`). Sandbox = Victoria, unchanged.
- `World::procedural` (+ `citySeed`) in game.h. `Game::startMode` re-inits world/bodies/people when switching mode.
- Layout helpers (gbox/M/Layout...) moved to src/game/world_layout.h (namespace tz::wl, `using`d) for reuse.
- Landmark ids kept: parliament/museum/empress/ferry/school (invented names). Use `world.landmark(id)`; never assume Victoria names/coords in adventure.
- Level start points + boss spot kept clear; L13 arena (110,-40) has buildings.
- Water: river z[-185,-160] split at bridges x=-300,50,250 (bridges are dry: isWater false), harbour, lake, bay z>345. Next: cars avoid water; gauntlet-stone colour buildings.

## Vault (store / power spinner / menus / HUD / cheats)
- New: src/ui/ui_style.{h,cpp} (tz::style: panel, button(BtnState, BtnRole), roundRect, glow, titleText, closeX, crosshair, easeOut/panelAnim/pressScale; kAnimSec=0.2). Use these for any new UI so everything matches.
- New: src/ui/hud_bar.{h,cpp} (bottom bar: name/coins/eaten + EGGS (beginSpin(SPIN_TOKEN)) / POWERS (openSkills) / STORE buttons; hudbar::layout/hit pure). Skill slots moved up to H-80s; FPS moved top-right.
- Settings.reducedMotion (saved). Game: menuAnim/menuPressT/drawHudBar/hudBarClick/lastW/lastH.
- Main menu: Difficulty (Titan `difficulty`) + Players (1/2) rows inserted just before Quit (keeps test indices).
- Persistent crosshair in drawHud (rainbow/lock only with blaster).
- Tests: tests/vault_tests.cpp (ctest tz_vault_tests). Tool: tz_vault_ui_shot out.ppm (software UI raster, run from repo root).
- Rex: next milestone I want a shared spinner widget from the egg spinner (e.g. `drawSpinWheel(UIDraw&, items, angle)`); if you expose one I'll reuse it for powers.
- Rex commit aeeb263: generated bodies render via drawMonster (MonsterDef.genSeed set by lookedDef; monsterModel skips model when genSeed). New files kaiju_gen_draw.cpp, rex_play.cpp, zombies.h. Game::eatTargetNear(). Pad Y = eat when something is in reach, else grab (input_win A_EAT now plain Y; Titan: I re-applied this on your pset() version in the working tree). Zoo: eggs.json zoo.eatReach=2.0 and any action button gulps the keeper. Crazy mode: zombie::on() civilians walk at the kaiju and nibble (1.5 dmg / 1.5 s, scaled by enemyDamageMult); eating is the normal Kaiju::eat path. Person.biteT added.

### Vault milestone 2 (c740033) - hooks needed from Wren (I did not edit these files)
- Persist powers/cheats: SaveData toJson/fromJson -> `j.set("vault", G.vault.toJson())` / `vault.fromJson(j["vault"])` (state lives in `Game::vault`, vpow::State).
- Power effects (multipliers, all default 1.0): kaiju.cpp damage `*= G->vault.mult(def().id, vpow::ST_DAMAGE)`; moveRadius/reach `*= ST_RANGE`; walk speed `*= ST_SPEED`; Kaiju::maxHp `*= ST_HP`; Game::superEatCost result `*= ST_THRIFT` (round up, min 1).
- EGGS shortcut: add `A_EGGS` (pad View/Back, key T) to input_state.h / input_win.cpp / controls.h, then in game.cpp Play branch: `else if (in.pressed[A_EGGS]) { if (P().spins > 0) beginSpin(SPIN_TOKEN); }`.
- Cheats unlock latches in Game::vaultTick() when Mode::Ending with ending.crazyEnding; Invincible / Free supers applied there each HUD frame.
- New menus: Menu kind 10 = power spinner (openPowerSpinner, HUD POWERS button), kind 11 = cheats (Options > Cheats). Store now 12 tiles: 3 basic, 5 kaiju powers, key, spin, ray, back. Super skills use icon 11 (gold star) + gold label.
- Body text now uses src/ui/font_hd.h (16x24 DejaVu Sans Bold, AA edge mask); titles stay chunky (UIDraw::chunky).

### Wren integration (Oct 23 2026)
- Combined Rex aeeb263, Titan 6d72b02+45dd942, Vault c740033+9bb50a7, Atlas 24a03f2 (all already on master). Stale working-tree leftovers were Atlas's hooks half-applied by hand; backup at /workspace/tommyzilla_pre_merge_1023.
- Atlas hooks applied by hand (docs/atlas_hooks.patch was cut from a stale tree and would revert Vault's vault_powers/vault_menus build lines, game.h members and store test) - only the Atlas hunks were taken.
- Vault: powers + cheats persist under the "vault" key (Game::loadSave / persist). The 5 multipliers are applied: damage (moveBricks), range (moveRadius + reach), speed (walk), HP (maxHp), thrift (superEatCost, round up, min 1).
- New action A_EGGS: pad View/Back, key T -> beginSpin(SPIN_TOKEN) in play (P2 pad works too). Controls screen moved from Back to LB+Start (H on keyboard); Pause = Start without LB.
- Menu Difficulty / Players rows now write save.settings.difficulty / players (what startMode reads) and persist.
- Co-op: Game::coopFocusBegin/End swap in whichever player is nearest for each enemy unit, enemy shots and the level boss, so they chase / damage P2 when P2 is closer.
- Tests: testWrenWiring; testAdventure egg hatch now retries other sides (was ~1/12 flaky at lv 10).
