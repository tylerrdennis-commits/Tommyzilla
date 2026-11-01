# Tommyzilla City Smash — native Windows (Direct3D 12) edition

A kid-friendly kaiju sandbox set in a cartoon Victoria, BC. This is a C++17 / Win32 / Direct3D 12 rewrite of the
browser prototype (kept in `../tommyzilla_web_prototype/`). Everything is drawn from instanced primitives (boxes,
cylinders, spheres, cones…) with hemisphere lighting, a warm key light, a cool fill, fake contact darkening at the
ground, a dusk sky gradient and distance haze, a simple roughness/metal response on the existing colours, and a mild
bloom on top of an ACES tonemap. **Free CC0 3D models** (glTF/GLB +
FBX, see *3D models* below) are unchanged Kenney / Poly Pizza meshes for the monsters, The Bulk, police, army, vehicles, civilians, city buildings and street
props. The ground (pavement, streets with dashed centre lines, parks, harbour water, the Dallas Road promenade) is
textured with small procedural tiling textures, tinted by the original palette. Anything without a model (or with models switched off) uses the original primitive look. There are no audio
files: the HUD uses a built-in bitmap font and every sound is synthesized at startup.

## Building

### Visual Studio 2022 (MSVC)
Requirements: Visual Studio 2022 with the **Desktop development with C++** workload. That workload includes CMake
and the Windows 10/11 SDK, which provides d3d12, dxgi, d3dcompiler, xinput and xaudio2.

* **Command line:** run `build_windows.bat` (Release) or `build_windows.bat Debug`. The output is `dist\Tommyzilla.exe` plus `dist\data\`, and the exe is also copied to the project root (next to `data\`) so double-clicking `Tommyzilla.exe` there works.
* **IDE:** *File > Open > Folder…* and pick this folder. VS reads `CMakeLists.txt`. Choose the `Tommyzilla.exe` target and press F5.
* **Manual:** `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`, then open `build\Tommyzilla.sln`.

Debug builds turn on the **D3D12 debug layer**. When a debugger is attached they also break on D3D errors.

### mingw-w64 cross-compile (from Linux)
Run `./build_mingw.sh`. It needs `g++-mingw-w64-x86-64-posix` and CMake, and produces `dist/Tommyzilla.exe` (+ `dist/data/`)
and a copy at `./Tommyzilla.exe`. It is a static exe that depends only on system DLLs.

**Shader check.** The exe compiles its HLSL at startup with D3DCompile, which follows the strict fxc rules. Wine's vkd3d
is more forgiving: a variable named `line` (a reserved geometry-shader word) once ran fine under Wine and failed on real
Windows. Before shipping a shader change, run `python3 tools/check_shaders.py --dxc <dxc> --fxc <fxc.exe>`. It compiles
every entry point with the Microsoft compilers:
* `dxc`: the Linux build from the DirectXShaderCompiler GitHub releases (SM 6.0 and 6.6).
* `fxc.exe`: with its `d3dcompiler_47.dll` from the Windows SDK (the `Microsoft.Windows.SDK.CPP` NuGet package,
  `c/bin/<ver>/x64/`). On Linux it runs under Wine with the native DLL (vs/ps_5_0, as in the game).

`build_mingw.sh` runs the check first whenever `DXC` / `FXC` are set (or `dxc` is on PATH), and `ctest` adds an
`hlsl_compile` test when CMake is given `-DDXC=` / `-DFXC=`. Without either, the unit tests still scan both shaders for
reserved words used as names.

### Linux test harness (game logic only)
```
cmake -S . -B build-tests && cmake --build build-tests -j && ./build-tests/tz_tests
cmake -S . -B build-asan -DTZ_ASAN=ON -DCMAKE_BUILD_TYPE=Debug && cmake --build build-asan -j && ./build-asan/tz_tests
```
The harness checks:
* the data tables;
* the move-unlock table;
* the campaign shuffle;
* the economy and police escalation;
* the save format;
* the sound synth and meshes.

It also runs headless full-game simulations: the tutorial (including the rainbow + building challenge), smashing,
coins, the police and the nuke; the school lock, the 78% collapse odds and rebuilding; every move on every monster;
the full 20-level adventure with the egg spinner, the gauntlet arc and the three-stage BULK; the menus and HUD; and the newer systems (rapid fire
caps, roar charge, blaster aiming, eat, stomp stun + ring, skill slots, chests/keys, save slots, Crazy mode,
police audio, walk/run speeds and the startup intro).

## Running
Run `Tommyzilla.exe` on Windows 10/11 with a DirectX 12 GPU. If no hardware device is found, it falls back to the
WARP software renderer. Keep `data\` next to the exe if you want to tweak values; otherwise the copies embedded in
the exe are used.

* **Save slots:** 3 players, one JSON file each: `%APPDATA%\Tommyzilla\save_slot1.json` … `save_slot3.json`. If that
  folder isn't writable the game uses `saves\` beside the exe. Files are written atomically (temp file + `MoveFileEx`).
  A damaged file is kept as `save_slotN.json.bad` and that slot starts fresh. An old single `save.json` is moved into slot 1.
  * The slot screen (shown after the intro) lists each slot's name, adventure level, coins and last-played time, or *Empty*.
    Pick a slot to load or start it; *Delete a save slot…* asks for confirmation. *Switch player / save slot* on the main menu goes back to it.
  * Every slot has separate **Sandbox**, **Adventure** and **Crazy** sections (coins, keys, skills, loadout, eaten count,
    chests, progress). Monsters you own and settings are shared by the slot.
  * Autosaves: level complete, tutorial complete, chest opened, key bought, ability unlocked, back to menu, quit.
* **Fullscreen:** F11 or Alt+Enter toggles borderless fullscreen.
* **`Tommyzilla.exe -nosound`** skips XAudio2 entirely (for machines with no audio device).
* **`Tommyzilla.exe -devgoal`** (testing only): each Adventure level's goal completes after 3 s. Boss fights still have to be won.
* **`Tommyzilla.exe -nointro`** skips the startup cinematic (also `"introEnabled": 0` in `data/config.json`).

* **`Tommyzilla.exe -nomodels`** draws everything with the original primitives (same as `"models": {"enabled": false}`
  in `data/config.json` or `"enabled": false` at the top of `data/models.json`).
* Testing-only flags: `-devstart=sandbox` / `-devstart=N` (jump straight into Sandbox or Adventure level N, nothing is
  saved), `-devlookat` (keep the monster and camera turned toward the boss / mini-boss / nearest enemy), `-fpslog`
  (writes `fps_log.txt` beside the exe every 2 s: fps, model requests / draws / batches, CPU skinning and per-frame ms).

## 3D models
`data/models.json` maps game ids to model files under `assets/` (height, rotation, clip names, recolours and named
looks such as enemy tiers). `models_log.txt` beside the exe lists what loaded, the clips found and any fallbacks.
* **Monsters:** Tommyzilla = Dino, Mossback = Yeti, Sparkleraptor = Dragon Evolved; Gloopzilla, Hootzooka, Bonkbeak,
  Fizzlefin, Noodlewyrm, Jellybellie, Puffernaut, Wobblewump, Thunderhorn and Roborumble use the nearest Ultimate
  Monsters / mech model recoloured to their colours. Pinchington, Clamshell, Clanksworth, Stompasaurus and Slinkysnoot
  have no fitting model and keep their primitive rigs. The character select, intro and egg hatch use the models too.
* **The Bulk:** Blue Demon recoloured purple with gold horns; the model scale follows his growth (up to 4x).
* **Police / army:** cops = SWAT, soldiers = Soldier (AK; red tier = shotgun + crimson, gold tier = rocket launcher +
  gold), police car, army jeep (SUV, olive), helicopters (police / army looks, searchlight cone kept), tanks
  (police / army; tier paint), Giant Super-Tank = the tank at 2.2x in gold/black, Mega Mech = Mech Bee.
* **City:** houses / shops / offices / towers / factories from the Kenney city kits, fitted to each building's brick
  footprint. The first hit swaps the building to its destructible bricks, so smashing, collapse, rubble and rebuild
  work as before (and the model comes back after a rebuild). Landmarks and the school stay primitive.
  Trees, plus new street props: street lights, barrels, barriers, containers and parked ambulances / fire trucks /
  sedans / taxis / vans (all knockable and throwable).
* **Civilians:** Kenney Mini Characters near the camera (`peopleDist` 110), stick figures further away.
* **Animation from game state:** idle / walk / run (by speed), attack (restarted per move, stretched to the move's
  length), roar = cheer, stomp = jump, eat, hit react, death; stunned civilians play *die* and hold it, tumbling ones
  play the fall clip. Rainbow, stomp ring, chests, glove, stun stars and all other effects are unchanged.
* **Performance (CPU skinning):** civilians share one animated pose per (model, clip, phase bucket); at most
  `soloMax` (32) entities get their own animator (nearest first, the rest share); animation updates every frame
  within `lodNear` (50), every 2nd frame to `lodFar` (130), every 4th beyond; frustum culling by model bounds; each
  shared pose is skinned once per frame; draws of the same model + look + pose are GPU-instanced (about 210 model
  draws -> about 30 batches / 60 draw calls in Old Town).
* Config: `"models": {"enabled", "peopleDist", "soloMax", "lodNear", "lodFar", "buildings", "buildingDist"}` in
  `data/config.json` (`buildings: false` keeps brick buildings; `buildingDist` > 0 draws models only that close).

### Credits (3D models, all CC0 / public domain)
* **Quaternius** (quaternius.com, via poly.pizza): Ultimate Monsters, Blue Demon, Mech Bee, Mech Fox, SWAT, Soldier,
  Tank, Street Light, Barrel, Barrier, Container.
* **Kenney** (kenney.nl): Car Kit, City Kit Commercial / Suburban / Industrial / Roads, Mini Characters.
* **kazuma** (poly.pizza): Helicopter.

### Ground textures (procedural, CC0)
The five ground textures in `data/textures/` (`ground_concrete`, `ground_asphalt`, `ground_grass`, `ground_water`,
`ground_paving`, 256x256 greyscale PNG, about 140 KB in total) were generated for this game by
`tools/gen_ground_textures.py`: tileable FFT-filtered noise, slab and paver joints and integer-frequency wave sines,
with fixed seeds, so the output is the same every time. No third-party images are used. They are released CC0 with the
game, and the end credits list them under *Ground Textures*.

How they're drawn:
* **Rendering:** `Dx12Renderer::setGroundTextures` uploads them into one `Texture2DArray` (one slice per surface,
  full mip chain) with an anisotropic, wrapping sampler.
* **Tiling:** UVs are world-space (`x / tile`, `z / tile`; 8 m concrete and asphalt, 10 m grass, 16 m water,
  4 m paving), so tiles never stretch with the size of a surface.
* **Colour:** each texel is multiplied by the surface's old flat colour and divided by the texture's mean brightness,
  so the palette, the hemisphere and sun lighting, the fog and the L20 storm darkening look the same as before, now
  with detail.
* **Water:** two layers drift slowly in opposite directions.
* **Streets:** a 3 m pale-yellow centre dash every 6 m, left out at crossings.
* **Fallback:** if any texture is missing or unreadable, or the GPU upload fails, the game keeps the original
  flat-coloured ground boxes. `ground_log.txt` beside the exe says which; `-flatground` forces this.

Per-file sources: `assets/polypizza/sources.json`, `assets/kenney/*/License.txt`, `assets/CREDITS.txt`. The same
credits are in the game under *Options > Credits* and on the victory screen.

### Startup intro
Black screen, *"Tommy D Productions presents"* fades in (1 s), holds (2 s) and fades out; the dusk city fades in with a
slow camera push-in; Tommyzilla rises from below with a rumble and a roar while **TOMMYZILLA** fades and grows in;
then it fades to the save-slot screen. About 13.5 s, once per launch. Any key, mouse click or controller button skips
straight to the menu.

## Controls
Every keyboard action has a controller button. The same table is shown in game on the **Controls** screen
(**Pause > Controls**, **Options > Controls** from the main or pause menu, or **Back** / **H** while playing); it is
generated from `src/game/controls.h`, and `tests/tests.cpp` checks it against the live bindings in `src/input/input_win.cpp`.

| Action | Controller (XInput) | Keyboard + mouse |
|---|---|---|
| Move | Left stick | W A S D (arrows also work) |
| Look | Right stick | Mouse. Click the game to capture the cursor (raw input); Esc releases it |
| Melee 1 / 2 / 3 | X / B / A | J / K / L (or left click / right click / Space) |
| Alt melee modifier | LB (hold) | (use U / I / O) |
| Melee 4 / 5 / 6 | LB + X / B / A | U / I / O |
| Skill slot 1 (close range) | RT | F |
| Skill slot 2 (ranged / super) | LT | Q |
| Quick-cycle slot 1 / slot 2 | D-pad left / right | 1 / 2 |
| Skills list (overlay, pauses) | D-pad down | Tab |
| Store (upgrades + chest keys) | D-pad up | B |
| Stomp: hold to show the stun ring, release to stomp | A | L or Space |
| Eat nearest civilian / open a chest | LB + Y | E |
| Grab / throw tree or rock | Y | G (**moved from E**) |
| Run (1.8x walk) | L3 | Shift (hold) |
| Reset camera | R3 | C (or middle click) |
| Pay 150 coins to cancel a nuke (also the Snap / Rainbow wave) | RB | R |
| Pause | Start | Esc / P (**Tab now opens Skills**) |
| Controls screen | Back | H |
| Mute | **LB + Back** (new) | M |
| Menu move | D-pad / left stick | Arrows (W A S D in menus) |
| Menu select / buy | A | Enter / Space |
| Menu back / close | B | Esc / Backspace |

Menus can be driven with the D-pad or stick plus A/B, with the arrows plus Enter/Esc, or with the mouse.

Rebinds: E used to be grab/throw; it is now **eat / open chest**, and grab/throw moved to **G**. Tab used to pause; it now
opens the **Skills** list (pause is Esc / P / Start). On the pad, Y alone is grab and LB+Y is eat.

### Weapons and actions
* **Two active skill slots.** Slot 1 = close range (melee smashes and Glow Breath), slot 2 = ranged / super (Rainbow
  Blaster, Charged Roar and the other beams/supers). Defaults: slot 1 Glow Breath, slot 2 Rainbow Blaster. Only slotted
  skills fire. Swap instantly with 1 / 2 (D-pad left / right) or pick from the Skills list (that list pauses the game).
  Stomp and Eat are always available. Loadout and owned skills are saved per mode.
* **Rainbow Blaster** (slot 2): a cartoony rainbow pistol. The crosshair sits above Tommyzilla's head and lights up with
  *RAINBOW TIME!* when a civilian is in the line of fire (camera ray through the crosshair). Bolts turn civilians into rainbows.
* **Glow Breath and Rainbow Blaster** have no cooldown: every press fires, up to `rapidProjectileCap` (10) on screen.
* **Melee** is instant with no cooldown.
* **Eat-to-fuel supers (Rampage style).** Every civilian you gulp with **E** adds 1 to **Eaten: N** (HUD, top left,
  under coins/keys). Super attacks (Charged Roar, Mega Quake, Meteor Sprinkle, Glow Nova, Twister Tantrum) have **no
  cooldown**; each one **costs eaten civilians**, checked before it fires and spent when it does. Not enough = it doesn't
  fire, a red *Need N eaten – press E to gulp people!* hint flashes, and the Eaten counter blinks.
  * Cost = `eatCostStart` (2) on level 1 rising evenly to `eatCostEnd` (14) on level 20 (level 8: 6, level 15: 11). The Sandbox uses the level-1 cost.
  * The roar's cost scales with charge: a quick release costs `eatCostMinCharge` (0.5x), a full charge
    `eatCostFullCharge` (1x), so a full roar is 2x a quick one (level 1: 1–2, level 8: 3–6, level 20: 7–14).
  * Crazy multiplies the **eat cost** by `crazySuperCost` (1.75x, e.g. level-1 roar 2–4). Charge time and cooldowns are not changed.
  * The cost is shown on a tab above the slot (e.g. *1-2 eaten*, green if you can afford it, red if not), and under the
    roar meter while charging (*cost 2 eaten (you have 6)*). The gauntlet costs nothing.
  * Every level (and the Sandbox) keeps at least `levelMinCivilians` (26) people around, respawning every
    `levelRespawnInterval` (0.6 s), so there is always fuel.
* **Charged Roar** (slot 2): hold to charge (meter on screen, stays at full while held), release to roar. Stronger at
  full charge (radius and damage scale from 35% to 100%), no cooldown (it costs eaten civilians, see above). It starts **locked**: smash 1 landmark **or**
  survive 15 hits (`roarUnlockLandmarks` / `roarUnlockHits`). Then a locked chest appears; open it with a key and you get
  *Charged Roar unlocked!*. The HUD shows the locked state and progress.
* **Eat (E / LB+Y):** gulps the nearest civilian within `eatRange` (14 m, scaled with monster size): lift-to-mouth
  tween, gulp, rainbow sparkle. Nobody in range = nothing happens. Counts as a rainbow.
* **Stomp:** hold to show a ground ring that is exactly the stun radius, release to stomp. Everyone inside is stunned
  for `stompStunTime` (3 s): flat on the ground with circling cartoon stars, then they get up. Stunned civilians can be
  eaten. Normal stomp damage is unchanged.
* **Skill chests + keys:** a locked chest says *Locked — buy a key at the store (100 coins)* when you touch it or
  press E. A key (`keyCost` 100 coins, in the Store) opens it. The HUD shows coins and keys. Finishing the tutorial
  gives 1 free key + 50 coins, so the first (roar) chest is always affordable.
* **Moving:** walk speed is `kaijuSpeed` 22 (was 16, +37.5%, for every monster via its speed multiplier; the walk cycle
  scales with speed, so feet don't slide). Hold Shift / L3 to run at `kaijuSprint` 1.8x (was 1.7x). Walking pushes small
  ground units (cops, jeeps, soldiers) out of the way; civilians never block you.

## Modes
* **Sandbox:** free smashing with its own coins and upgrades.
  * The tutorial walks you through moving, claw, tail, stomp, Glow Breath and grabbing, then a short challenge:
    **Rainbows 10** (smash, shoot or eat civilians) and **Buildings 5** (`tutorialRainbows` / `tutorialBuildings`),
    with on-screen counters *Rainbows 4/10*, *Buildings 2/5*. About 3–5 minutes. Extra civilians spawn while it runs.
    Finishing shows *TUTORIAL COMPLETE!* with a jingle and the reward, then the *police yes/no* prompt (default answer: *No*).
  * **Police toggle:** Sandbox police are **OFF by default**. Turn them on or off any time from the pause menu (Esc / Start)
    > **Police: ON/OFF**; the choice is saved in the slot's `settings.police`. OFF means no police spawn, no wanted stars or
    heat, no sirens, and any police on the map despawn immediately. Adventure / Crazy police and army are unaffected.
  * **Ground:** textured in Sandbox and Adventure alike (see *Ground textures*); `-flatground` shows the old flat colours.
  * **Police stars** escalate from cars to a helicopter (3★), tanks (4★) and a nuke countdown (5★). Pay 150 coins or it's game over; you keep your coins.
  * Both skill slots are available from the start. **Ruth King Elementary** can't be damaged until 4 landmarks are smashed: Empress, Parliament, Museum and Ferry.
  * Police (cop) audio is quieter: sirens, police helicopters, cop shots and police-tank cannons play at `policeVolume`
    (0.4 = 40%), with at most `policeMaxVoices` (3) cop sounds overlapping. Other sounds are unchanged.
* **Adventure:** locked until the sandbox has 8 smashed buildings (`slot2Smashes`, was 20; the tutorial's own 10 rainbows + 5 buildings are unchanged), a store purchase and a smashed school. The main menu shows a checklist.
  When it unlocks during play you get a choice: *Keep playing Sandbox to unlock more powers* or *Go to Main Menu and start Adventure* (both save first).
  * It has **20 levels** and a separate economy and move set. Level 20 is the final boss, **THE BULK**. Each level lasts a few minutes. Older 15-level saves migrate and keep their progress.
  * **Enemy ladder** (data in `levels.json` `enemies`; per-level table below):
    * **Cops** (L1+): on foot with green pea-shooters, plus police cars.
    * **Riot cops** (L2+): a big see-through blue **front shield** that blocks the blaster, breath and beams from the
      front. Go round them, or use melee / stomps / supers (they get round the shield).
    * **Military** (L2+): soldiers, jeeps; tanks and helicopters from L3; jets from L4.
    * **Armoured tanks** (L4+): shiny chrome tanks. Normal hits just **clang** ("needs a super!"); only supers dent them
      (3 HP, 2-3 super hits). At most 2 alive, never part of a goal, and not scaled up in Crazy, so they are always
      beatable and never required.
    * **Flamethrower troops** (L6+): red fuel tanks and a short orange flame cone (`flameRange` 11 m, `flameDps` 1.2). Step back out of the cone.
    * **Tier 1** (L6-10: red paint, x1.6 HP, x1.15 damage, x1.1 speed) and **tier 2** (L11-19: gold paint, x2.2 HP, x1.3
      damage, x1.2 speed, x1.25 count). Mini-bosses (Giant Super-Tank / Mega Mech) end L11, L12 and L14-19.
    * **Level 5** is the **SUPPLY DROP**: every chest skill you don't own yet falls from the sky in a rainbow chest that opens **without a key**.
    * Crazy runs the same ladder with the Crazy HP/damage multipliers (armoured tanks excepted, see above).
  * Kid-friendly damage: every enemy hit in Adventure is scaled by `adventureEnemyDamage` (0.5). Crazy doubles that.
  * **Level-6 champion:** at the start of level 6 in normal Adventure you **PICK YOUR CHAMPION** from your hatched
    monsters. It's locked in for the rest of that campaign and saved per slot (`lockedMonster`). Hatching later eggs still
    adds them to your roster, and the Sandbox stays free. A new campaign clears the lock. Crazy keeps free choice anytime.
  * Each win earns an **egg spin** (see *Eggs* below). The egg still hatches in the world with glow moves; the monster grows during the reveal and is announced.
  * The 18-monster roster is shuffled per campaign seed. Free play continues until you choose **Next Level** in the pause menu.
* **THE BULK (level 20) - three stages.** Place all six stones on the pedestals with the gauntlet on (walk up + E). The sky
  darkens (dim purple light and fog, never too dark to see him), the ground rumbles and THE BULK arrives.
  1. **Stage 1:** the first gauntlet hit (with six stones) knocks him to **exactly 1 HP** - not dead! He gets **angry**
     (red flash, roar, rumble), **grows to 2.2x** (the camera pulls back) and his HP refills to the stage-2 pool (`stage2Hp` 220).
  2. **Stage 2:** a real fight. Telegraphed ground-pound slam, charge and rock throws do real damage (`stage2Damage` 6),
     so you **can** be knocked out (normal fail screen). No one-shot here: the gauntlet only does normal damage.
     Retry choices: **Retry from stage 2 (THE BULK)** or **Retry level** (from the pedestals). Emptying his stage-2 HP starts...
  3. **FINAL FORM (stage 3):** a transformation (flash, spin, glow) and a new HP pool (`stage3Hp` 200). He can't drop below
     1 HP. At **50% HP or less** the prompt **DO THE SNAP! Press R** (pad: RB) appears (locked above 50%). The snap plays
     a big cinematic (white flash, rainbow wave) that destroys THE BULK and shows the *peace to the land* ending.
  * **Aftermath, normal Adventure:** the stones shatter and the gauntlet crumbles. They are removed from the Adventure
    save (the slot shows an empty, cracked gauntlet) and the ending is recorded. Crazy unlocks.
  * **Aftermath, Crazy:** the gauntlet survives (a twist beat). It goes, with all six stones, to that slot's **Sandbox**
    ("Future Sandbox"), and the victory screen says *The Gauntlet is yours in Sandbox!*. In Sandbox it pulses at full
    power and **R / RB** fires the **rainbow wave** ultimate (`sandboxUltimateRadius` 120 m, `sandboxUltimateCooldown` 20 s).
  * Crazy scales the stage HP by `crazyBossHp`. Then the full credits, THE END and the after-credits scene play (see *Ending*).
* **Crazy (unlocks after beating THE BULK on level 20, per save slot):** all 20 levels with enemy HP and damage x2
  (`crazyEnemyHp`, `crazyEnemyDamage`), THE BULK x2 HP (300) and x2 damage (`crazyBossHp`), and super attacks costing
  1.75x more **eaten civilians** (`crazySuperCost`; charge time and cooldowns unchanged). Pick **any** hero kaiju, at the start and between levels.
  Crazy has its own save section in each slot.
* **Character select:** every unlocked monster, with stats from `data/monsters.json`.
* **Options:** sound on/off, volume, mouse sensitivity, invert Y, the Controls screen, and reset save (with confirmation).
* **Store:** an icon grid (4 per row): Mighty Claws, Big Reach, Stompy Speed, Chest key, Egg spin, De-Evolution Ray, Back.
  Each tile shows a drawn icon, the name, level / count and coin price; maxed upgrades show *MAX*, unaffordable tiles are
  dimmed with a red struck-through price. D-pad / stick / arrows move (left/right wrap through the tiles, up/down wrap in the
  column, a short last row clamps to its last tile), A / Enter / click buys, B / Esc or the Back tile closes. Prices and
  purchases are unchanged.

### Monsters (18)
Tommyzilla (starter), Gloopzilla, Sir Pinchington, Hootzooka, Robo-Rumble, Noodlewyrm, Stompasaurus, Mega Mossback,
Fizzlefin, Bonkbeak, Jellybellie, Clanksworth, Captain Clamshell, Sparkleraptor, Wobblewump, Thunderhorn,
Slinkysnoot, Puffernaut.

They use 7 procedural body plans (biped, quad, blob, crab, bird, robot and serpent) plus variants such as shells,
horns, fins, tentacles and hats.

### Moves (27). Slot 1 = close range, slot 2 = ranged / super.
* **Melee (10)**
  * Starters: Claw Swipe, Tail Swipe, Stomp.
  * Smash count: Headbutt Bonk (3 smashes), Belly Bounce (8), Ground Pound Hop (15), Uppercut Pop (30).
  * Landmarks: Double Slap (Chinatown gate), Spin-Tail Twister (Empress), Body Slam Slide (Ferry).
* **Direct (11)**
  * Starters: Glow Breath (slot 1), Rainbow Blaster (slot 2).
  * Smash count: Bubble Blaster (5), Plate-Glow Beam (12), Frost Puff (18), Rainbow Ray (25), Honk Wave (35), Boulder Burp (40), Laser Peepers (50), Static Zap (60).
  * Landmark: Sparkle Spit (Museum).
* **Super (5 + 1 secret)**
  * Charged Roar: milestone (1 landmark or 15 hits) + skill chest.
  * Landmark: Mega Quake (school).
  * Smash count: Meteor Sprinkle (75), Glow Nova (100), Twister Tantrum (120).
  * All supers arrive in a **skill chest** (key needed; free on Adventure level 5) and are fuelled by **eaten civilians** (no cooldowns).

The *glow* moves hatch eggs: Glow Breath, Plate-Glow Beam, Sparkle Spit, Rainbow Ray, Laser Peepers, Meteor Sprinkle and Glow Nova.

### Adventure levels (20)
| # | Level | Goal | Enemies |
|---|---|---|---|
| 1 | Cop Chaos | smash 8 buildings | cops + police cars |
| 2 | The Army Arrives | survive 90 s by the Inner Harbour | cops, **riot cops**, soldiers, jeeps |
| 3 | Heavy Metal | destroy 3 tanks | riot, soldiers, jeeps, tanks, helicopters |
| 4 | Everybody Attacks! | reach the Ferry Terminal in 2 min | all types + first **armoured tank** |
| 5 | Supply Drop Showdown | smash the Royal BC Museum + 6 more | all + 2 riot + armoured + free skill chests |
| 6 | Red Alert Park | survive 2 min in Beacon Hill Park (pick your champion first) | tier 1 (red) + **flamethrowers** |
| 7 | Old Town Overload | smash 12 Old Town buildings | tier 1, riot x2, armoured, flamer |
| 8 | Jet Stream | knock 4 jets out of the sky | tier 1, riot, armoured, flamer |
| 9 | Landmark Legend | smash the Empress Hotel and the Parliament Buildings | tier 1, riot x2, armoured, flamer x2 |
| 10 | The Big Showdown | defeat 10 enemies | tier 1, everything |
| 11 | Downtown Demolition | smash 15 buildings, then the boss | tier 2 (gold) + Giant Super-Tank |
| 12 | Chopper Chaos | 3 helicopters, then the boss | tier 2 + Mega Mech |
| 13 | The Zookeeper's Box | buildings stay smashed: flatten the whole district (counter on screen) | tier 2 cops, riot, soldier |
| 14 | Harbour Hold-Out | Storm Stone (purple), then the boss | tier 2 + Giant Super-Tank |
| 15 | Victory Lap | Tide Stone (blue), then the boss | tier 2 + Mega Mech |
| 16 | Park Patrol | Grove Stone (green), then the boss | tier 2 + Giant Super-Tank |
| 17 | Old Town Embers | Ember Stone (red), then the boss | tier 2 + Mega Mech |
| 18 | Sunny Showdown | Sun Stone (orange), then the boss | tier 2 + Giant Super-Tank |
| 19 | Lightning Lane | Spark Stone (yellow), then the boss | tier 2 + Mega Mech, armoured x2 |
| 20 | THE BULK | place the six stones, then three stages | - |

### Eggs, rarities and colours (`data/eggs.json`)
* **Egg spinner** (CS:GO style). The weighted roll picks the result **first**; then the reel (`reelLength` 40 eggs,
  `cellsPerSecond` 14) spins and lands on it with an eased stop when you press **Space / Enter / click / pad A**
  (or by itself after `autoStop` 7 s). Rarity glow, the reveal card, then the egg drops and **hatches** in the world.
  The first spin on a new slot is always a playable kaiju.
* **Rarity table** (rate = percent weight; rarer = bigger and tougher at the start):

| Rarity | Rate | Size | HP | Extras | Duplicate gives | Kaiju |
|---|---|---|---|---|---|---|
| Common | 50% | x1.00 | x1.00 | - | 40 coins + 40 XP | Gloopzilla, Jellybellie, Puffernaut, Bonkbeak, Sir Pinchington |
| Uncommon | 27% | x1.05 | x1.10 | - | 60 + 60 | Hootzooka, Captain Clamshell, Wobblewump, Fizzlefin |
| Rare | 14% | x1.10 | x1.20 | 1 passive | 100 + 90 | Mega Mossback, Noodlewyrm, Slinkysnoot, Clanksworth |
| Epic | 6.5% | x1.15 | x1.30 | 1 bonus super | 160 + 130 | Sparkleraptor, Thunderhorn, Stompasaurus |
| Legendary | 2% | x1.20 | x1.45 | passive + super | 260 + 200 | Robo-Rumble |
| TOMMYZILLA | 0.5% | x1.30 | x1.60 | 2 passives + super | 500 + 300 | Tommyzilla (also the starter) |

  Passives: Healing Hugs (slow regen even in fights), Tough Scales (-20% damage), Coin Magnet (+25% coins), Big Belly (gulp
  from 35% further away). Bonus supers: Mega Quake, Meteor Sprinkle, Glow Nova, Twister Tantrum. A Common kaiju can still
  beat the whole adventure (tested with base stats through all 20 levels).
* **How to get extra spins:** 1 free spin when a slot is created; +1 for every Adventure / Crazy level won; in Sandbox +1
  every 30 buildings smashed; a 50% chance in each skill chest; or buy one in the store for 400 coins. Spare spins are used
  from the pause menu (**Egg Spinner**).
* **Duplicates:** spinning a kaiju you already own gives coins + XP by rarity (table above). If the duplicate rolled a
  rarer **finish** than the one you have, your kaiju is upgraded to it. A duplicate never takes anything away.
* **Colours:** every kaiju has data-driven palettes (primary / secondary / accent / eyes, 18 sets). Each spin picks one by
  weight, jitters the hues (+-22 degrees) and may swap secondary/accent, so two hatchlings rarely look the same.
  **Finishes** roll separately: **GOLD 1%**, **CHROME 0.5%**, **HOLOGRAPHIC 0.25%** (shaders in both the model renderer and
  the primitive renderer). The look is saved per monster and shows on the egg, the hatchling, the grown kaiju, the HUD bars and
  frames (accent colour), the save-slot screen and character select.
* **Growth and XP:** a hatchling starts at about **0.42x** size and grows to full size (times the rarity size bonus,
  capped) over `fullXp` 600 XP. The camera pulls back smoothly and reach / eat / stomp all scale with size. While small it is
  treated fairly: it takes less damage (down to x0.55) and smashes bricks more easily. XP sources (`xp`): eating 6,
  buildings 10, landmarks 40, enemies 8/15, mini-boss 60, chests 25, level won 80, the zoo 180. **Healing at full HP turns
  into XP** (1 HP = `healToXp` 1 XP; a partial overflow only converts the extra). Natural regen never converts. HUD: an
  accent-coloured **XP** bar under the HP bar, with small throttled "+XP" pops. Saved per monster and per mode.
* **Melee lifesteal:** every block smashed with a melee move heals +1 HP (capped at max HP) with throttled "+1 heart" pops.
* **De-Evolution Ray** (store, 2500 coins, `deEvolution.price`): a sky beam, glow, shockwave ring and flash shrink your kaiju
  back into an egg, then the spinner rolls a new one at the normal rates. You **spend** the 2500 coins. The current kaiju's
  look, rarity and growth are reset, and the new kaiju starts at XP 0. You **keep** level progress, skills and slots, keys,
  unopened chests and the rest of your coins. It works only with enough coins, never during a boss fight, and only after the
  confirm *"Your kaiju will turn back into an egg. Are you sure?"*. After the level-6 lock the new kaiju becomes the champion.
  Per mode; saved.
  *Why:* it is a big but reachable goal (several levels of coins). A child who rolled a Common can re-roll without
  losing the campaign, and it costs enough to stay a special choice rather than a reroll button.
* **Kaiju Zoo opening:** after the first spin of a new game (once per mode), your hatchling starts in a cage with bars,
  a *KAIJU ZOO* sign and onlookers. The zookeeper walks up: **Press E to gulp!** (pad LB+Y). The gulp shows the eat
  tween, rainbow sparkles and a growth/XP burst; then the bars shatter (debris, shake) and you break out into the tutorial.
  No enemies, can't fail, a gentle hint if idle. **Tab** skips it. It counts as 1 eaten / 1 rainbow.

### The gauntlet arc (L12-L20)
* Winning L12 quietly adds an empty gauntlet outline to the HUD / skills screen (no text).
* L13: buildings don't rebuild (`noRebuild`). A building counter appears (this level only). When the whole district is
  flattened a locked box appears. Inside is **The Bulk's Gauntlet**, which goes into the gauntlet slot.
* L14-19: each level has one stone behind a locked gate: **Storm** (purple), **Tide** (blue), **Grove** (green),
  **Ember** (red), **Sun** (orange), **Spark** (yellow). Picking one up needs the gauntlet equipped. Otherwise you get a big,
  harmless knockback and *"Too much power! You need something to hold it..."*. Stones appear on the glove one by one.
* **Power scales with stones:** every 8 m walked with the gauntlet slotted, a rainbow pulse collapses the nearest intact
  buildings. 0 stones = cosmetic only, then 2 / 3 / 5 / 6 / 8 / **10** buildings for 1-6 stones (`pulseTargets`).
  The pulse never touches bosses. It replaces the old random-level glove easter egg: the gauntlet **is** the glove.
* **Infinity Stones (Skills menu):** with the gauntlet owned, *Infinity Stones* lists all six. Open one to read what
  the stone is and what each of its powers does, then assign those powers to different actions — **Pulse** (the walk
  shockwave), **Strike** (melee), **Blast** (ranged) and **Roar** (supers). One power per action; choosing another
  replaces it; choosing the assigned power again clears it. Effects apply only while the gauntlet is equipped and you
  still own that stone. The building-count table above is unchanged until a pulse power adds its own extra. Powers
  ship in code (`src/game/eggs.cpp`) and can be overridden per stone with a `powers` array in `eggs.json`.

### Ending, credits and after-credits
* **Credits** come from `data/credits.json` (editable: the exe reads `data\credits.json` beside it, with the embedded copy
  as fallback). Roles:
  * Directed by / Written by / Dedicated to: Tommy M. Dennis (in that order).
  * Game Design, Enemy Design, Boss Design, Art Direction: Tommy M. Dennis & Tyler Dennis.
  * Producer: Tyler Dennis. QA Testing: Tommy M. Dennis.
  * Programming and UI/UX Design: Computer.
  * 3D Modeling / Animation: Quaternius, Kenney, kazuma (CC0) + primitives by Computer.
  * Sound Design / Music: procedural audio synthesised in code by Computer.

  Then come **SPECIAL THANKS** (only the asset sources really used, per `assets/CREDITS.txt` and `data/models.json`) and **THE END**.
* **After-credits:** fade to black, *THE END*, then `data/video/after_credits.mp4` plays in-game. It is not skippable: all input
  is ignored until it ends, then any button returns to the title. Skipping the credits lands on the video.
  The sign text **"Do not feed people."** is a game overlay anchored to the blank sign in the video
  (measured in the final clip: x 0.19-0.36, y 0.40-0.57 of the frame). The sign holds still until 5.0 s, then the overlay follows the 1.0x -> 1.8x
  smoothstep push-in toward the egg at (0.51, 0.62), so it slides off to the left with the sign.
* **Playback:** Windows Media Foundation (`IMFSourceReader`, H.264 -> RGB32 on the CPU) feeds a D3D12 texture drawn
  full screen (letterboxed) under the UI. If Media Foundation or the H.264 decoder is missing, the game plays its
  **in-engine closing shot** (new enclosure, sign, a new egg) instead, for the same length as the MP4 (read from its header).
  There the sign text sits on the in-engine sign for 0-5 s and then becomes a caption.
  **Under Wine:** Media Foundation goes through winegstreamer, so it needs a GStreamer H.264 decoder on the host
  (`gstreamer1.0-libav`). Without it, `start()` fails and the in-engine fallback runs. With it (tested in a brand-new
  prefix from a fresh unzip), the real MP4 played through Media Foundation, and the game reported a length of 10.625 s.
  The run is logged in `docs/fresh_after_credits_*.png` and `video_log.txt`, which is written beside the exe on every ending.

## Configuration (`data/`)
* `config.json`: about 150 tuning values. Examples: `collapseChance` 0.78, `rebuildDelay` 300 s, `schoolUnlockCount` 4,
  `nukeCancelCost` 150, upgrade costs, police heat and unit stats, egg and reveal timings, and the newer values above
  (`tutorialRainbows`, `rapidProjectileCap`, `eatRange`, `stompStunRadius`, `keyCost`, `boss*`, `crazy*`,
  `policeVolume`, `kaijuSpeed`, `kaijuSprint`, `introEnabled`…). `//` comments are allowed.
* `moves.json`: the data-driven move and unlock table (kind, shape, radius, reach, damage, cooldown, glow…).
* `monsters.json`: name, body plan, variant, colours, scale, speed, HP and power multipliers.
* `eggs.json`: rarities, species per rarity, palettes, finishes, passives, spins, growth, XP, De-Evolution price, zoo, gates, stones, gauntlet and Bulk stages.
* `credits.json`: the end credits, Special Thanks and the after-credits settings (video path, sign text, hold times).
* `textures/ground_*.png`: the tileable ground detail maps (see *Ground textures*). Optional: without them the ground is flat-coloured.
* `levels.json`: objective, start position, enemy mix (`cops`, `cars`, `soldiers`, `jeeps`, `helis`, `tanks`, `jets`, `riots`, `armored`, `flamers`:
  max alive of each, one spawn per `interval`, rotating through the types), `tier` (0–2), `miniboss`
  (`supertank` / `mech`), `chests` (level-5 supply drop), `noRebuild`, `unlockGauntletSlot`, `pedestals` and coin reward.

The build embeds all six JSON files into the exe, so `data\` is optional. A broken file falls back to the built-in copy with a warning.

## Source layout
```
src/core      math (row-vector matrices, RH view/proj), json, rng
src/game      config, tables, progress (pure, unit-tested), world, effects, people, enemies, kaiju, rigs, game
src/renderer  meshes (platform independent), dx12_renderer (device, swapchain, PSOs, embedded HLSL)
src/audio     sound ids, audio_synth (procedural PCM), audio_xaudio2 (voice pool + loops)
src/input     input_win (raw input, cursor capture, XInput + rumble)
src/ui        ui_draw (bitmap-font text, rects), menus (all menus + HUD), font_data.h (generated)
src/platform  main_win (window, loop, data/save files, model library + presenter)
src/model     glTF/FBX loader, animator, manifest, model library, GameModels presenter, DX12 model renderer, -modeltest scene
src/game/models.cpp  which entity uses which model + clip (tz_core side, via the ModelCatalog interface)
data/         JSON game data       tools/  embed.cmake, gen_font.py       tests/  Linux harness
```

## Developer notes / spoilers (not for players)
**The Zookeeper's Key (hidden).** Gulping the zookeeper in the Kaiju Zoo opening silently grants the key: no toast,
popup or tutorial. It's also granted if the zoo is skipped in a slot+mode that completed it before. It shows only in the
Skills / collection list, with a key-ring icon and the line *"A jingly key ring from the Kaiju Zoo. It smells like
peanuts."*. Saved per slot + mode.
* **Zoo Gates** (`eggs.json` `zooGates`): a fence with a padlock sign. One is in Sandbox (spin token) and four are in Adventure
  levels 2-12 (L3 300 coins, L6 spin token, L9 a rare-finish roll, L11 500 coins). With the key, walking up opens it. Without
  it the gate just stays shut, with no hint. The key is never used up, and each reward is granted once (saved per gate).
* **L13 box:** hidden in a random building of the district; it appears when 100% is destroyed and opens with the key. A
  player who skipped the zoo isn't stuck: a zookeeper walks over to the box, and gulping him grants the key silently.
* **L14-19:** each stone sits behind a key gate of its own (same rule; the L13 zookeeper guarantees every player has the
  key by then).
* The old random-level glove easter egg (`gloveLevel`) is gone. Old saves keep a collected glove as the gauntlet.

**Testing flags** (all hidden): `-devstart=sandbox|N`, `-devgoal`, `-devlookat[=riot|armor|flame]`, `-devspin[=s]`,
`-devzoo`, `-devgrow=XP`, `-devfinish=gold|chrome|holo|roll`, `-devbulk=1|2|3`, `-devending` / `-devcredits`, `-devvideopattern`,
`-fpslog`, `-nointro`, `-nomodels`, `-nosound`, `-flatground` (flat-coloured ground, no textures).

## What was verified
* The mingw-w64 GCC 14 cross-compile is clean with `-Wall`. A clang 19 `--target=x86_64-w64-windows-gnu -Wall -Wextra` syntax check of every source file is also clean.
* The Linux harness passes 1274/1274 checks, including under ASan and UBSan. New sections cover eat costs (per
  level, charge and Crazy; no fire when short; spending; Sandbox = level 1), the roster per level (level 1 police only,
  cop pea shots hit), tiers, the boss phase on 11–14, the level-5 free chests, the level-6 pick and lock (saved; Crazy
  free), the easier Bulk (17 claw hits, ends at max size), and a difficulty curve (how long an idle kaiju lasts per level).
* The mingw exe was smoke-tested under **Wine 10 + vkd3d + lavapipe** (software Vulkan, headless Xvfb) with `-nosound`.
  * The D3D12 device, PSOs, runtime HLSL compile, lighting, fog, the instanced world and the bitmap-font HUD and menus all render (about 50 FPS in software).
  * Keyboard input started Sandbox, moved the monster and advanced the tutorial. Screenshots are in `docs/`.
  * Later run: the intro (studio card, city + TOMMYZILLA title), the save-slot screen, the main menu with the Crazy
    item, the tutorial HUD counters, skill slots, the stomp ring, blaster shots and the Skills list were all checked
    on screen (`docs/wine_intro_*.png`, `wine_slot_menu.png`, `wine_main_menu.png`, `wine_hud_tutorial.png`,
    `wine_stomp_ring.png`, `wine_blaster.png`, `wine_skills.png`).
  * Eat-to-fuel / roster run: `docs/wine_hud_eaten_cost.png` (*Eaten: 6*, the *1-2 eaten* slot tag, the roar meter with
    *cost 2 eaten (you have 6)*; releasing spent 2), `docs/wine_cop_level.png` (level 1: a cop with a pea-shooter, a
    green pea in flight, a police car), `docs/wine_miniboss.png` (level 11 Giant Super-Tank with its HP bar and gold
    tier-2 soldiers). The hidden `-devgoal` testing flag completes each level's goal after 3 s so the boss could be reached.

* **3D model merge (Oct 2026):** Linux harness 1293/1293 (1274 earlier checks + 19 new model-hook + credits checks with a fake
  catalog) and the model suite 582/582 (every mapped model loads, every mapped clip exists, looks/tiers, the
  GameModels presenter driving a real game for 600 frames), both also clean under ASan + UBSan. Under Wine + lavapipe
  (software rendering, 1024x576): Old Town / cop level ~29-30 fps (same as `-nomodels`), Sandbox waterfront ~46 fps,
  The Bulk ~45 fps, level 10 with 10 enemies ~60 fps. Screenshots: `docs/models_*.png`.

* **Eggs / gauntlet / Bulk / ending batch (Oct 2026):** Linux harness **2441/2441** checks (2440 + 1 L20 lighting
  regression check) and the model suite **588/588** (+6 palette/finish checks); the full core suite is also clean under ASan.
  Wine + lavapipe screenshots in `docs/`: `eggs_spinner_mid_spin.png`, `eggs_reveal.png`, `growth_hatchling_small.png`
  vs `growth_grown.png`, `ladder_riot_cop*.png`, `ladder_armoured_tank.png`, `ladder_flamethrower*.png`,
  `finish_gold.png`, `finish_holo.png`, `zoo_opening.png`, `bulk_stage2.png`, `bulk_stage3_snap.png`,
  `ending_credits.png`, `ending_after_credits_wine_fallback*.png` and `ending_video_pass_testpattern*.png`.
  Software-rendered fps: Sandbox ~45-49, L2 ~33-45, L5 ~32-36, L9 ~45-57, L20 Bulk ~58-67.

## Known limitations of this first draft
* After-credits video: Media Foundation playback was verified only under Wine 10 + winegstreamer + gst-libav, not on real Windows. There is no audio track in the clip, and none is played.
* 3D models: orientation, scale and clip choice were checked only in software-rendered Wine screenshots; real GPU and
  MSVC builds are untested. Helicopter rotors are part of the static model (they don't spin). Kenney building models are
  stretched to the existing brick footprints, so some look a little squashed or tall.
* Nothing has run on real Windows or GPU hardware yet; the only run was under Wine.
* MSVC was not available, so the VS2022 build is untested (it was written for MSVC, and the code compiles with GCC and clang).
* XAudio2 was never heard. Under Wine with **no audio device**, Wine's FAudio crashed inside `CreateSourceVoice`; `-nosound` avoids this. Expected to be fine on real Windows, but untested.
* XInput, rumble and raw mouse capture are untested on hardware.
* Under Wine/vkd3d a fully opaque full-screen black quad hid the UI text drawn over it, so the intro's black card is a 98.5% veil. Real Windows behaviour is untested.
* No shadows, anti-aliasing or post effects. The monster shadow is a flat disc. UI text is made of per-pixel quads (simple, but thousands of triangles).
* Vsync is always on. There's no frame-rate cap option and no key rebinding (the controls are fixed and listed above).
* Only the first connected XInput pad is used. DirectInput-only controllers are not supported.
* Level balance (enemy counts, timers, rewards) is a first pass.
