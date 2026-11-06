#!/usr/bin/env python3
"""Regenerates data/models.json (the model manifest). Edit the tables below, then run: python3 tools/gen_models_json.py
Every id here is looked up by the game (src/game/models.cpp); ids without an entry (or whose file is missing) keep
their primitive rig. Heights are world units at entity scale 1 (they match the primitive rigs' sizes)."""
import json, os
HERE = os.path.dirname(os.path.abspath(__file__))
PP = "assets/polypizza/"
KC = "assets/kenney/kenney_car-kit/Models/GLB format/"
KM = "assets/kenney/kenney_city-kit-commercial_2.1/Models/GLB format/"
KI = "assets/kenney/kenney_city-kit-industrial_2.0/Models/GLB format/"
KS = "assets/kenney/kenney_city-kit-suburban_20/Models/GLB format/"
KR = "assets/kenney/kenney_city-kit-roads/Models/GLB format/"
KP = "assets/kenney/kenney_mini-characters/GLB format/"
A = "CharacterArmature|"
# Quaternius clip sets
BIG = {"idle": A+"TZ_Idle", "walk": A+"Walk", "run": A+"Run", "attack": A+"Punch", "death": A+"Death", "hit": A+"HitReact",
       "eat": A+"Weapon", "stun": A+"HitReact", "jump": A+"Jump", "cheer": A+"Wave"}
SMALL = {"idle": A+"Idle", "walk": A+"Walk", "run": A+"Walk", "attack": A+"Bite_Front", "death": A+"Death", "hit": A+"HitRecieve",
         "eat": A+"Bite_Front", "stun": A+"HitRecieve", "jump": A+"Jump", "cheer": A+"Dance"}
DRAGON = {"idle": A+"Flying_Idle", "walk": A+"Flying_Idle", "run": A+"Fast_Flying", "attack": A+"Headbutt", "death": A+"Death",
          "hit": A+"HitReact", "eat": A+"Punch", "stun": A+"HitReact", "jump": A+"Fast_Flying", "cheer": A+"Yes"}
R = "RobotArmature|"
MECH = {"idle": R+"Idle", "walk": R+"Walk", "run": R+"Run", "attack": R+"Shoot_Big", "death": R+"Death", "hit": R+"HitRecieve_1",
        "eat": R+"Pickup", "stun": R+"HitRecieve_2", "jump": R+"Jump", "cheer": R+"Dance"}
CIV = {"idle": "idle", "walk": "walk", "run": "sprint", "attack": "attack-melee-right", "death": "die", "hit": "fall",
       "eat": "pick-up", "stun": "die", "jump": "jump", "cheer": "emote-yes"}
def rc(mat, col, mode="replace"): return {"material": mat, "color": col, "mode": mode}
M = {}
def add(id, file, **kw):
    e = {"file": file}; e.update(kw); M[id] = e
# ---- playable monsters (ids = data/monsters.json ids). Recolours give each model its monster's body/accent colours.
monsters = [
  ("tommyzilla",    "Dino.glb",             16, BIG,    [rc("Dino_Main", "#2b2f3a"), rc("Dino_Secondary", "#3ad7ff")]),
  ("mossback",      "Yeti.glb",             15, BIG,    [rc("Yeti_Main", "#6a7a5a"), rc("Yeti_Secondary", "#9ad04a")]),
  ("sparkleraptor", "Dragon_Evolved.glb",   15, DRAGON, [rc("Dragon_Main", "#7a4ad0"), rc("Dragon_Secondary", "#ffd36b")]),
  ("gloopzilla",    "Green_Blob.glb",       13, SMALL,  [rc("Green_Main", "#7ad04a")]),
  ("hootzooka",     "Birb.glb",             15, SMALL,  [rc("Birb_Main", "#2fa89a"), rc("Birb_Secondary", "#ffd36b")]),
  ("bonkbeak",      "Pigeon.glb",           14, SMALL,  [rc("Pigeon_Main", "#8a4ad0"), rc("Pigeon_Secondary", "#ffa040")]),
  ("fizzlefin",     "Fish.glb",             15, BIG,    [rc("Fish_Main", "#2a6fcf")]),
  ("noodlewyrm",    "Dragon.glb",           11, DRAGON, [rc("Dragon_Main", "#ff9ac8"), rc("Dragon_Secondary", "#7affc8")]),
  ("jellybellie",   "Pink_Slime.glb",       13, SMALL,  [rc("Pink_Main", "#ff6bd5"), rc("Pink_Secondary", "#3ad7ff")]),
  ("puffernaut",    "Green_Spiky_Blob.glb", 12, SMALL,  [rc("Green_Main", "#ff8a7a")]),
  ("wobblewump",    "Mushnub_Evolved.glb",  19, SMALL,  [rc("MushroomKing_Main", "#ffe03a")]),
  ("thunderhorn",   "Demon.glb",            15, BIG,    [rc("Demon_Main", "#3a3a8a")]),
  ("roborumble",    "Mech_Fox.glb",         16, MECH,   []),
]
# Three more meshes so at least 16 heroes have a model, and the on-screen name matches that mesh
# (Bee Mech, Blue Demon, Block Kid). Clamshell (crab) and Slinkysnoot (serpent) use the two models we author ourselves
# with tools/gen_creatures.py (assets/polypizza/Crab.glb, Serpent.glb).
for id, f, h, clips, recolor in monsters:
    e = {"height": h, "fallback": "monster", "clips": clips}
    if recolor: e["recolor"] = recolor
    add(id, PP + f, **e)
CRAB = {"idle": "CrabArmature|Idle", "walk": "CrabArmature|Walk", "run": "CrabArmature|Run", "attack": "CrabArmature|Attack", "death": "CrabArmature|Death",
        "hit": "CrabArmature|Hit", "eat": "CrabArmature|Eat", "stun": "CrabArmature|Stun", "jump": "CrabArmature|Jump", "cheer": "CrabArmature|Cheer"}
SERPENT = {k: v.replace("CrabArmature", "SerpentArmature") for k, v in CRAB.items()}
add("clamshell", PP + "Crab.glb", height=10, fallback="monster", clips=CRAB,
    recolor=[rc("Crab_Main", "#f0e0c0"), rc("Crab_Secondary", "#2fa89a"), rc("Crab_Belly", "#c8b890")])
add("slinkysnoot", PP + "Serpent.glb", height=16, fallback="monster", clips=SERPENT,
    recolor=[rc("Snoot_Main", "#ff9a1f"), rc("Snoot_Secondary", "#9ad04a"), rc("Snoot_Belly", "#ffd8a0")])
add("clanksworth", PP + "Mech_Bee.glb", height=16, fallback="monster", clips=MECH)
add("stompasaurus", PP + "BlueDemon.glb", height=15, fallback="monster", clips=BIG,
    recolor=[rc("BlueDemon_Main", "#3a6adf"), rc("BlueDemon_Secondary", "#e0a030")])
add("pinchington", KP + "character-male-a.glb", height=14, fallback="monster", clips=CIV,
    extra=[KP + "Textures/colormap.png"])
# ---- THE BULK: the Blue Demon in purple (game scale 1.6 .. 4.0 multiplies this 15 = Boss::height() at scale 1)
add("the_bulk", PP + "BlueDemon.glb", height=15, fallback="boss", clips=BIG,
    recolor=[rc("BlueDemon_Main", "#8a3fd6", "hue"), rc("BlueDemon_Secondary", "#e0a030")])
# ---- police + military (heights match the ~4 m cartoon infantry and the primitive vehicles)
add("cop", PP + "SWAT.glb", height=4.2, fallback="person",
    clips={"idle": A+"Idle_Gun", "walk": A+"Walk", "run": A+"Run", "attack": A+"Gun_Shoot", "death": A+"Death",
           "hit": A+"HitRecieve", "stun": A+"HitRecieve_2", "jump": A+"Roll", "cheer": A+"Wave"})
WEAPONS = ["Revolver*", "Sniper*", "Pistol", "SMG", "GrenadeLauncher", "ShortCannon", "Shotgun", "RocketLauncher", "AK", "Shovel", "Knife*"]
add("soldier", PP + "Character_Soldier.glb", height=4.2, fallback="person", hide=WEAPONS, show=["AK"],
    clips={"idle": A+"Idle", "walk": A+"Run", "run": A+"Run_Gun", "attack": A+"Idle_Shoot", "death": A+"Death",
           "hit": A+"HitReact", "stun": A+"HitReact", "jump": A+"Jump", "cheer": A+"Wave"},
    variants={  # enemy tiers: 1 = crimson, 2 = gold (game tierTintHex)
      "tier1": {"show": ["Shotgun"], "recolor": [rc("Character_Main", "#9e2a2a"), rc("Pants", "#5a1a1a")]},
      "tier2": {"show": ["RocketLauncher"], "recolor": [rc("Character_Main", "#e0a830"), rc("Pants", "#8a6a20"), rc("Grey", "#2a2a30")]}})
CM = [KC + "Textures/colormap.png"]
add("police_car", KC + "police.glb", height=2.7, fallback="car", extra=CM)
add("car_suv", KC + "suv.glb", height=2.7, fallback="car", extra=CM, variants={"army": {"tint": "#9aaa6a"}})
add("ambulance", KC + "ambulance.glb", height=3.8, fallback="car", extra=CM)
add("fire_truck", "assets/kenney/kenney_car-kit/Models/FBX format/firetruck.fbx", height=3.6, fallback="car",
    extra=["assets/kenney/kenney_car-kit/Models/FBX format/Textures/colormap.png"])
add("car_sedan", KC + "sedan.glb", height=2.7, fallback="car", extra=CM)
add("car_taxi", KC + "taxi.glb", height=3.1, fallback="car", extra=CM)
add("car_van", KC + "van.glb", height=2.8, fallback="car", extra=CM)
add("tank", PP + "Tank.glb", height=5.0, fallback="tank", variants={
    "police": {"recolor": [rc("Tank_Main*", "#2f5fb3")]},
    "army": {"recolor": [rc("Tank_Main*", "#55703a")]},
    "super": {"recolor": [rc("Tank_Main*", "#d0a020"), rc("Grey", "#1a1d24"), rc("Black*", "#101216")]}})
add("police_heli", PP + "Helicopter_kazuma.glb", height=4.4, fallback="heli", variants={
    "army": {"recolor": [rc("Material.003", "#4a5a3a"), rc("Material.004", "#7a8a5a")]}})
add("mega_mech", PP + "Mech_Bee.glb", height=16, fallback="mech", clips=MECH)
# ---- civilians: Kenney Mini Characters (look % 12)
for g in ("male", "female"):
    for c in "abcdef":
        add("civilian_%s_%s" % (g, c), KP + "character-%s-%s.glb" % (g, c), height=2.4, fallback="person",
            extra=[KP + "Textures/colormap.png"], clips=CIV)
# ---- city buildings (fitted to each building's brick footprint; swapped for the bricks once damaged)
def bld(prefix, kit, names, tex):
    for i, n in enumerate(names):
        add("%s_%s" % (prefix, chr(97 + i)), kit + n + ".glb", height=1, fallback="building", extra=[kit + "Textures/" + tex])
bld("house", KS, ["building-type-%s" % c for c in "abcdefgh"], "colormap.png")
bld("shop", KM, ["building-%s" % c for c in "abcdefgh"], "colormap.png")
bld("office", KM, ["building-%s" % c for c in "ijklmn"], "colormap.png")
bld("tower", KM, ["building-skyscraper-%s" % c for c in "abcde"], "colormap.png")
bld("factory", KI, ["building-%s" % c for c in "abcd"], "colormap.png")
add("road_straight", KR + "road-straight.glb", scale=8, groundAlign=False, fallback="flat", extra=[KR + "Textures/colormap.png"])
# ---- props
add("tree_large", KS + "tree-large.glb", height=10, fallback="tree", extra=[KS + "Textures/colormap.png"])
add("tree_small", KS + "tree-small.glb", height=8, fallback="tree", extra=[KS + "Textures/colormap.png"])
add("street_light", PP + "Street_Light.glb", height=6, fallback="pole")
add("barrel", PP + "Exploding_Barrel.glb", height=1.3, fallback="barrel")
add("barrier", PP + "Barrier_Single.glb", height=1.2, fallback="box")
add("container", PP + "Container_Small.glb", height=2.6, fallback="box")
HEADER = """// Model manifest (generated by tools/gen_models_json.py - edit that and re-run): game entity id -> model file
// (relative to the exe folder / repo root), placement, look, clips. Ids are looked up by src/game/models.cpp.
//   height   : scale so the rest pose is this tall (world units at entity scale 1). Or "scale".
//   rotation : [x, y, z] degrees applied first (use y to make the model face +Z like the game rigs).
//   recolor  : [{material, color, mode: hue|replace|multiply}], tint multiplies everything; hide/show node patterns.
//   variants : named looks (enemy tiers, police/army) starting from the base look.
//   clips    : logical name (idle walk run attack death hit eat stun jump cheer) -> clip name in the file.
//   extra    : external files (textures) the model needs; tools/copy_models.cmake ships only these + "file".
// "enabled": false turns every model off (the game then draws its primitive rigs; also config.json models.enabled).
// A missing or broken file is logged (models_log.txt) and that entity keeps its built-in primitive rig.
"""
out = os.path.join(HERE, "..", "data", "models.json")
with open(out, "w") as f:
    f.write(HEADER)
    f.write("{\n  \"version\": 2,\n  \"enabled\": true,\n  \"models\": {\n")
    items = list(M.items())
    for i, (k, v) in enumerate(items):
        f.write("    %s: %s%s\n" % (json.dumps(k), json.dumps(v, separators=(", ", ": ")), "," if i + 1 < len(items) else ""))
    f.write("  }\n}\n")
print("wrote", out, len(M), "models")
