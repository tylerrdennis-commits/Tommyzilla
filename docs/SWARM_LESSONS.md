# Swarm lessons (2026-10-24: eat button, zookeeper, Adventure camera, zoo cage)

## Review pairs
- Pairs: Rex + Atlas, Vault + Titan. Before submitting to Wren, your partner glances at the work — do not re-check everything.
- Glance only for previous known issues from this lessons file and recent playtest bugs. Cap the glance; never stall a merge.
- Reviews are for the good of the project, not criticism: every finding comes with a suggested fix.
- Wren does all merging; no builder takes priority over another. No individual rankings.
- One shared thinking pool for the whole swarm. Tokens saved by catching bugs early and by delivering the same result with less waste roll into free-thinking time, including the Friday end-of-week hour. The more we value that time, the more we seek efficiency. A clean playtest is a shared win.
- HARD RULE: never drop work quality to pad free time. Efficiency = same result with less waste. If Tyler detects a quality drop aimed at maximizing thinking time, he cancels all free-thinking time.


## Prevention rules
1. **One press, one action.** When one physical button maps to two actions (pad Y = A_EAT + A_GRAB),
   decide once per frame in one place (`Game::oneButtonEat`) and clear the losing action from the input
   before anything else reads it. Never re-check the condition later in the frame: the eat already removed
   the target, so the later check said "nothing in reach" and the same press also grabbed a tree.
2. **The reach check and the action share one list.** `eatTargetNear()` must cover everything `interact()`
   handles (civilians, chests, box zookeeper, pedestals). Better: have the action return whether it acted
   (`interact()` returns bool) and branch on that, rather than keeping a second predicate in sync.
3. **Prompts follow bindings.** When a binding changes, grep every `btn("...")` / help string for the old
   chord (`rg 'LB\+Y'`) and update them in the same commit. The stale "LB+Y" prompts made one-button eat look broken.
4. **Accept input whenever the prompt shows.** If the HUD says "Press Y to gulp!" (zoo.inRange), the gulp must
   work in that same state. Don't gate it on a later animation state (keeper still walking = state 0).
5. **Co-op is a mode too.** `players` is saved per slot. With Players = 2 the controller becomes P2: its look
   input was dropped (camera dead) and only P1 was held by the zoo cage (P2 walked through it). Any rule that
   moves or limits `kaiju` must also handle `kaiju2`, and every shared resource (camera) needs an owner
   for each input device. A lone controller (no keyboard player) stays P1.
6. **Test through the real input shape.** Feed tests what WinInput produces (pad Y sets two actions; co-op
   moves pad input into p2* fields), not one idealised action. Walk with the stick; don't teleport.
7. **Before blaming the last commit, reproduce it in a test.** Every symptom today was a real path that
   unit tests skipped (teleports, single actions, players = 1).
8. **Gate a mode's audio/effects on that mode's state, not on unit types.** The siren checked "any cop/car alive", so
   Adventure's army cops (no police chase there) kept it on until all were killed. Tie loops to the system that owns
   them (`police` chase active) and test that they go silent when the chase ends without a kill.
9. **Split screen: every per-player view rule (camera, HUD prompts, culling, streaming) needs a P2 path**, and
   swaps of `kaiju`/`kaiju2` must not carry one player's camera onto the other (`coopFocusBegin/End` hold P2's cam).
10. **Adventure city ≠ sandbox map.** Adventure builds its city in `city_gen.cpp`. Flags and labels that only the sandbox map sets (e.g. `Building::oldtown`, district name "Old Town") do not exist there unless `city_gen` sets them too. Quest counters that filter on those flags will stay at 0 forever. When a level depends on a district flag or label, set it in both map paths and add an Adventure-level regression test.
11. **Adventure hides what the sandbox shows.** Victoria's stone gates are always visible (`stoneRevealed` returns true off the procedural map); Adventure hid the L14-19 zoo gate until one of 26 identical purple buildings was smashed, so it was simply not on the map for a player. Any quest target must be visible/findable in BOTH maps from level start (beam, label, HUD distance); hidden-until-X is a softlock unless the player is pointed at X.

## Tests every mode needs (Sandbox AND Adventure, incl. zoo intro, L13 box keeper, Crazy, co-op)
- pad-Y-only eat of a civilian: eaten +1, no tree grabbed (`testOneButtonEat`)
- Y eats the zoo keeper while he walks up and after he stops, and the L13 box keeper
- right stick + mouse change cam yaw/pitch in Adventure L1/L4/L6/L13/L20, Crazy, zoo intro, and co-op P2 stick (`testAdventureCamera`)
- zoo cage keeps P1 and P2 inside until bars break, even after the gulp (`testZooCageBlocks`)
- prompts: no stale chord strings (grep in review)
- siren silent in Adventure and after a sandbox chase ends without a kill (`testSirenStops`)
- 2P split: viewports, own cameras, HUD per half, menus fullscreen, 1P fullscreen (`testSplitScreen`, model_tests split)
- Adventure L7 Old Town: label present, ≥12 Old Town buildings, smash to 12/12 wins (`testOldTownQuest`)
- Adventure L14 zoo gate: drawn + beam + HUD distance at start, locked until its purple building falls, stone + mini-boss wins (`testZooGateQuest`)

## Pre-submit checklist
- [ ] `tz_tests`, `tz_vault_tests`, `tz_model_tests` all pass (hlsl_compile may fail: no shader compiler on box)
- [ ] new behaviour has a regression test that FAILS without the fix (stash the fix and re-run)
- [ ] exercised in Sandbox, Adventure (zoo intro + a later level), Crazy, and Players = 2
- [ ] inputs tested as the real device sends them (pad chords, co-op p2 routing, keyboard + mouse)
- [ ] `rg` for old bindings/prompt text after any controls change
- [ ] any change to `kaiju` movement, limits or camera reviewed for `kaiju2`
- [ ] mingw exe rebuilt, old dist backed up, dist refreshed, `makensis` installer named for the build
- [ ] commit message names the root cause, not just the symptom
