# MERGE NOTES — 3D model loader (glTF/GLB + FBX) for Tommyzilla

Built in a copy (`/workspace/tommyzilla_loader`) while gameplay work continued in
`/workspace/tommyzilla`. The loader is a self-contained module; the main tree only
needs the few small hooks listed under "Changed files".

## Added files
| Path | Purpose |
|---|---|
| `src/model/model.h/.cpp` | Core types (Model, Mesh, SubMesh, Node, Skin, Material, Texture, AnimationClip, Pose), quat/transform math, `computePose`, CPU `skinMesh`, `finalizeModel`, `linearToSrgb`, `hsvRecolor` |
| `src/model/model_loader.h/.cpp` | `loadModel(path)` dispatch by extension, image decode (stb_image), helpers, `describeModel` |
| `src/model/model_gltf.cpp` | glTF/GLB via cgltf: meshes, node hierarchy, materials (baseColorFactor + texture, alpha blend), vertex colours, skins, TRS animation channels |
| `src/model/model_fbx.cpp` | FBX via ufbx: meshes, materials/textures, skin clusters, baked animation stacks |
| `src/model/animator.h/.cpp` | `Animator`: play/loop/crossfade by clip index or name, speed, one-shot hold (death) |
| `src/model/model_manifest.h/.cpp` | Parser for `data/models.json` (file, scale/height, rotation, offset, groundAlign, tint/recolour, node hide/show, variants, clip map, fallback) |
| `src/model/model_library.h/.cpp` | `ModelLibrary` (lazy load by entity id, logs clips found + mapping), `ModelAsset` (offset matrix, logical clip map, looks/variants, `makeDraw`) |
| `src/model/model_draw.h` | `ModelDraw` / `ModelDrawList` — the only type the renderer needs from gameplay |
| `src/model/dx12_model_renderer.h/.cpp` | DX12 renderer for models (own root sig + 2 PSOs, SRV heap, lazy GPU upload, CPU-skinned per-frame upload ring) |
| `src/model/model_test_scene.h/.cpp` | `-modeltest` viewer scene (38 actors, 5 camera presets, cycling clips) |
| `src/model/third_party_impl.cpp`, `src/model/ufbx_impl.cpp` | Single TU implementations for cgltf/stb_image and ufbx (ufbx.c compiled as C++ because the mingw toolchain has no C compiler) |
| `src/model/model.cmake` | Defines `tz_model` static lib + `TZ_MODEL_WIN_SOURCES` |
| `third_party/cgltf/` (v1.15, MIT), `third_party/ufbx/` (v0.23.1, MIT/PD), `third_party/stb/` (stb_image 2.30, MIT/PD) | Vendored single-header libs + LICENSE files |
| `data/models.json` | Manifest: 20 entity ids → file, scale/height, rotation, tint/recolour, variants, clip mapping |
| `tools/copy_models.cmake` | Copies ONLY manifest-mapped models (+ extra textures, licences, sources.json) into `dist/assets` (CMake ≥ 3.19) |
| `tests/model_tests.cpp` | New `tz_model_tests` executable (360 checks) |
| `assets/polypizza/` | Quaternius + kazuma CC0 GLBs (+ `sources.json` with URLs/licence/creator), ~4.7 MB |
| `assets/kenney/` | Kenney Car Kit, City Kit Commercial/Suburban/Roads/Industrial (extracted, with License.txt), ~53 MB in repo; only ~1 MB of it ships |
| `MERGE_NOTES.md` | This file |

## Changed files (merge these hunks by hand)
1. **`CMakeLists.txt`**
   - `include(${CMAKE_SOURCE_DIR}/src/model/model.cmake)` after the tz_core options.
   - Game exe: add `${TZ_MODEL_WIN_SOURCES}` to sources, `target_link_libraries(... tz_model)`.
   - POST_BUILD: `cmake -DSRC_DIR=... -DDEST=$<TARGET_FILE_DIR> -P tools/copy_models.cmake`.
   - Non-WIN32 branch: `tz_model_tests` target + `add_test` (defines `TZ_SOURCE_DIR`, ASan link option like tz_tests).
2. **`src/renderer/dx12_renderer.h`** — forward-declare `struct ModelDrawList;`, change signature to
   `bool render(const RenderList& rl, const ModelDrawList* models = nullptr);` (default arg ⇒ existing callers unchanged), (the `Dx12ModelRenderer models;` member lives in the Impl struct in dx12_renderer.cpp).
3. **`src/renderer/dx12_renderer.cpp`**
   - `init`: after `createMeshes()`, `models.init(device, kFrames, colorFmt, depthFmt, err)`; failure is non-fatal (logged via OutputDebugString, game keeps primitive rendering).
   - `render`: after the opaque batches → `models.draw(cmd, frameCB, frame, *list, 0)` then re-set primitive root sig/CBV/PSO; after alpha batches → pass 1 (blended materials).
   - `shutdown`: `models.shutdown()`.
4. **`src/platform/main_win.cpp`** — `narrowAcp()`, `runModelTest(...)`, flags `-modeltest` and `-modelcam=N` (handled before audio init; writes `modeltest_log.txt` next to the exe). No change to the normal game path.
5. **`build_mingw.sh` / `build_windows.bat`** — after copying `data/`: `rm -rf dist/assets` then run `tools/copy_models.cmake` (bat checks errorlevel).

## Integration hooks (for wiring gameplay later — NOT done yet)
```cpp
ModelLibrary lib; std::string err; lib.loadManifestFile(exeDir + "data/models.json", exeDir, err);
const ModelAsset* a = lib.get("tommyzilla");      // nullptr ⇒ use existing primitive draw (fallback)
Animator anim; anim.bind(&a->model);               // one Animator per entity
anim.play(a->clip("walk"), /*loop*/true, /*fade*/0.25f);   // logical: idle/walk/run/attack/death
anim.update(dt);
ModelDrawList list; list.draws.push_back(a->makeDraw(world, &anim.pose(), "tier2" /*variant or ""*/));
renderer.render(renderList, &list);
```
- Suggested place: `Game::render` fills a `ModelDrawList` next to the `RenderList`; store an `Animator` per kaiju/unit; skip the primitive draw for entities whose `lib.get(id)` is non-null.
- Recolour: The Bulk = BlueDemon with `BlueDemon_Main` hue-shifted to purple; enemy tiers = `variants` (`tier1/2/3` for soldier, `police/army` for tank).

## Design notes / caveats
- **Skinning is on the CPU** (into a per-frame upload ring). Wine's vkd3d HLSL compiler has no StructuredBuffer support and a dynamically indexed joint cbuffer ran at 0.2–3 fps under lavapipe; CPU skinning gives ~25–55 fps for the full 37-model scene there. On real GPUs GPU skinning could be revisited; the interface would not change.
- Paths go through `narrowAcp` + `fopen` → non-ANSI install paths are not supported (same as existing data loading).
- FBX: right-handed Y-up metres, animation stacks baked with `ufbx_bake_anim`. Only verified on static Kenney FBX (fire truck) + a skinned FBX test that skips if the file is absent.
- The MSVC build (build_windows.bat / CMake + MSVC) was **not** compiled here (no MSVC on the box); vendored TUs use `/W0` there.
- `assets/_downloads/` zips were moved out of the tree to `/workspace/tommyzilla_loader_downloads/` (19 MB); unused Kenney OBJ/FBX folders are left in `assets/kenney` (repo-only, not shipped).

## Merge into the main tree + gameplay wiring (done, Oct 20 2026)
Merged into `/workspace/tommyzilla` (pre-merge snapshot: `/workspace/tommyzilla_premerge`, zip backup
`/workspace/tommyzilla_dx12_v3.zip`). All gameplay changes from the main tree were kept.

* **Layering:** `tz_core` does not link `tz_model`. Core has `src/game/model_hooks.h` (`ModelCatalog` interface,
  `ModelReq`, logical clips) and `src/game/models.cpp` (which entity -> which model id / look / clip). Draw code calls
  `G->kaijuModel / bossModel / unitModel / personModel / buildingModel / bodyModel / monsterModel`; each returns false
  (-> primitive rig) when models are off or missing. `src/model/game_models.*` (`GameModels : ModelCatalog`) turns the
  `RenderList::models` requests into a `ModelDrawList`, owning animators (shared crowd poses, solo cap, distance
  animation-rate LOD, frustum cull, GC). `main_win.cpp` loads the library, sets `game->models`, and per frame calls
  `gameModels.build(rl, draws)` then `renderer->render(rl, &draws)`.
* **Renderer:** `ModelDraw` gained `mix`/`mixK` (tier paint, hit flash). Skinning is deduplicated per shared pose, and
  draws with the same model + pose + look are drawn instanced (per-instance world/tint/mix/glow in input slot 1).
  This took Old Town from 12 fps to 30 fps under Wine/lavapipe.
* **Data:** `data/models.json` v2 is generated by `tools/gen_models_json.py` (76 entries, `"enabled": true`).
  New assets: more Quaternius monsters + Mech Bee/Fox, Kenney Mini Characters. `assets/` now holds only the mapped
  files + licences (the full extracted kits were moved out to `/workspace/tommyzilla_assets_full`, outside the zip;
  copy files back from there before mapping new ones).
* **World:** new `BODY_PROP` bodies (street lights, barrels, barriers, containers, parked vehicles) placed with their
  own RNG after the original layout, so the existing layout / RNG stream is unchanged.
* **Tests:** `tz_tests` +19 checks (`testModelHooks`, fake catalog); `tz_model_tests` updated ids + presenter tests.

---
# Eggs / colours / growth / ladder / gauntlet arc / three-stage Bulk / ending batch (Oct 20 2026)
Backups before this batch: `/workspace/tommyzilla_dx12_v4.zip`, `/workspace/tommyzilla_pre_eggs/`.

## Added
| Path | Purpose |
|---|---|
| `data/eggs.json` | rarities, species tiers, palettes, finishes, passives, spins, growth, XP, De-Evo price, zoo, gates, stones, gauntlet + Bulk stages |
| `data/credits.json` | end credits, Special Thanks, after-credits settings |
| `data/video/after_credits.mp4` | the after-credits clip (supplied; 10.625 s, 1280x720, 24 fps) |
| `src/game/eggs.h/.cpp` | `EggTables E`, weighted rolls, look rolling (palette + hue jitter + finish), duplicates |
| `src/game/features.cpp` | spinner, reveal/hatch, looks, growth, XP, De-Evolution Ray, Kaiju Zoo, Zoo Gates, key |
| `src/game/arc.cpp` | gauntlet slot/box/stones/pedestals, staged Bulk glue, snap, aftermath, credits, ending, MP4 duration, sign overlay |
| `src/platform/video_mf.h/.cpp` | Media Foundation `IMFSourceReader` player (H.264 -> RGB32 frames) |

## Changed
`CMakeLists.txt` (new sources, Media Foundation libs), `tools/embed.cmake` + `src/game/embedded.cpp` (eggs, credits),
`data/levels.json` (20 levels, ladder fields), `src/game/{boss,config,enemies,game,kaiju,models,progress,saveslots,tables,world}`,
`src/game/{game.h,model_hooks.h,render_types.h,progress.h,saveslots.h,tables.h}`, `src/model/{game_models,dx12_model_renderer,model_draw}`
(palette + finish shader), `src/renderer/dx12_renderer.cpp` (finish shader + video texture pass), `src/platform/main_win.cpp`
(video player, credits from disk, testing flags), `src/ui/menus.cpp` (spinner/reveal UI, store, XP bar, colours in HUD/slots/select,
skills/key, gauntlet slot, stage HP bars, snap prompt), `tests/tests.cpp`, `tests/model_tests.cpp`, `README.md`.

## Notes
* `tz_core` still doesn't depend on `tz_model`; the palette/finish travel in `ModelReq` like the earlier looks.
* `VideoPlayer` is an interface in `game.h`; the core plays the in-engine closing shot when `start()` fails.
* Bug found in the Wine pass and fixed: L20 darkening multiplied the reused RenderList's light intensities every frame
  (scene went black). Lights are now reset each frame; regression check in `testGauntletArc`.
