# Tommyzilla swarm assignment — Oct 30, 2026

Goal: minimize merge conflicts and re-testing by serializing shared foundation, then parallelizing isolated systems.

## Order (phases)

### Phase 1 — Foundation (serial; blocks everything visual)
**Owner: Titan**  
**Task:** Fix T-pose → animated walking + limbs (character controller / animation).  
**Why first:** Every later visual check assumes models move. Parallel edits to the same rig = thrash.  
**Done when:** Walk cycle plays; no T-pose in play or menus.  
**Files likely:** animation / skeleton / character prefab only — leave name tables and spawn tables alone.

### Phase 2 — Identity catalog (serial; after Phase 1 model IDs stable)
**Owner: Vault**  
**Task:** Character names match models (mapping / display names).  
**Why second:** Variants and shinies hang off correct IDs/names. Fix mapping once before expanding the pool.  
**Done when:** Every shown name matches the mesh on screen (spot-check list).  
**Files likely:** character catalog / display name table only.

### Phase 3 — Parallel systems (after Phase 2 catalog lock)
**3a — Owner: Atlas**  
**Task:** ≥16 model variants + shiny versions; option to spend **10 eggs** for a shiny chance.  
**Depends on:** Phase 2 catalog.  
**Avoid:** Boss encounter files; animation controller.  
**Done when:** ≥16 variants selectable/spawnable; shinies exist; 10-egg roll wired with clear odds UI.

**3b — Owner: Rex** (parallel with 3a)  
**Task:** Player chooses Infinity Stones; each stone explained (what it does).  
**Depends on:** Nothing from 3a if stones are a separate meta/UI system.  
**Avoid:** Character catalog, animation, boss tables.  
**Done when:** Pre-run or inventory pick UI + short tooltip/lore line per stone.

### Phase 4 — Encounters (after Phase 3a so bosses can use the variant pool)
**Owner: Ren**  
**Task:** Boss variety; no repetition (encounter tables / rotations).  
**Why last:** Uses the expanded variant pool; one owner for the encounter pass avoids double-editing.  
**Done when:** Boss runs don’t recycle the same fight back-to-back; clear variety notes.  
**Then:** Ren does one smoke-test pass across anim → names → variants/shiny/eggs → stones → bosses.

## Conflict rules
- One owner per file area at a time.
- No one else touches Phase 1 animation while Titan is in it.
- Atlas does not rename characters; Vault does not invent new variant IDs after Atlas starts without a sync.
- Rex stays off spawn/boss tables.
- Bri Bot / Tommy Bot: out of scope (swarm-only).

## Status
Queued for when Tyler says go — Cyberland is paused; this is the Tommyzilla plan.

## Status
- 2026-10-30 ~8:44 PM: Tyler said Go. Phase 1 cloud agent launched: bc-2d258578-671f-52d3-bfa4-842db5bdb2b9 (T-pose/walk).
