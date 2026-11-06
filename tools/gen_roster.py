#!/usr/bin/env python3
"""Roster expansion: 18 -> 50 kaiju. Edit NEW below, then run:  python3 tools/gen_roster.py  &&  python3 tools/gen_models_json.py

Writes (idempotently - it always rebuilds from the first 18 original monsters):
  data/monsters.json  the 18 originals + the NEW species below
  data/eggs.json      tiers (who hatches at which rarity), one palette set per new species, and the tier rates
Every new species reuses one of the game's real 3D models ("mesh") with its own colours, size and stats, like a
"Frost" or "Lava" version of that creature. tools/gen_models_json.py imports ROSTER / MESH from here for the manifest.
"""
import colorsys, json, os
HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "..", "data")

# mesh key -> (file, height, clip set, recolour materials [main, secondary]); clip sets live in gen_models_json.py
MESH = {
    "dino":     ("Dino.glb",             16, "BIG",    ("Dino_Main", "Dino_Secondary"), "biped"),
    "yeti":     ("Yeti.glb",             15, "BIG",    ("Yeti_Main", "Yeti_Secondary"), "quad"),
    "dragon":   ("Dragon.glb",           11, "DRAGON", ("Dragon_Main", "Dragon_Secondary"), "biped"),
    "dragon2":  ("Dragon_Evolved.glb",   15, "DRAGON", ("Dragon_Main", "Dragon_Secondary"), "biped"),
    "blob":     ("Green_Blob.glb",       13, "SMALL",  ("Green_Main", None), "blob"),
    "birb":     ("Birb.glb",             15, "SMALL",  ("Birb_Main", "Birb_Secondary"), "bird"),
    "pigeon":   ("Pigeon.glb",           14, "SMALL",  ("Pigeon_Main", "Pigeon_Secondary"), "bird"),
    "fish":     ("Fish.glb",             15, "BIG",    ("Fish_Main", None), "biped"),
    "slime":    ("Pink_Slime.glb",       13, "SMALL",  ("Pink_Main", "Pink_Secondary"), "blob"),
    "mush":     ("Mushnub_Evolved.glb",  19, "SMALL",  ("MushroomKing_Main", None), "blob"),
    "demon":    ("Demon.glb",            15, "BIG",    ("Demon_Main", None), "biped"),
    "bdemon":   ("BlueDemon.glb",        15, "BIG",    ("BlueDemon_Main", "BlueDemon_Secondary"), "biped"),
    "mechfox":  ("Mech_Fox.glb",         16, "MECH",   (None, None), "robot"),
    "mechbee":  ("Mech_Bee.glb",         16, "MECH",   (None, None), "robot"),
    "crab":     ("Crab.glb",             10, "CRAB",   ("Crab_Main", "Crab_Secondary"), "crab"),
    "serpent":  ("Serpent.glb",          16, "SERPENT", ("Snoot_Main", "Snoot_Secondary"), "serpent"),
}

# tier, id, display name, mesh, body, accent, belly, scale, speed, hp, melee, power, variant, description
NEW = [
    # ---- common (+4)
    ("common",    "mudstomp",     "Mud Dino",         "dino",    "7a6a3a", "c8a050", "b8a878", 1.00, 0.95, 1.10, 1.10, 0.90, "",        "A muddy dinosaur that loves puddles"),
    ("common",    "sunnybird",    "Sunny Birb",       "birb",    "ffd23a", "ff8a3a", "fff0b0", 0.95, 1.20, 0.90, 0.90, 1.10, "owl",     "A sunshine-yellow birb with a loud chirp"),
    ("common",    "mintslime",    "Mint Slime",       "slime",   "6bffb0", "3ad7ff", "d8fff0", 1.05, 0.90, 1.10, 1.00, 0.95, "drip",    "A cool minty slime that never melts"),
    ("common",    "grumpfish",    "Grumpfish",        "fish",    "8aa0b0", "e0e8f0", "c8d4dc", 1.00, 0.95, 1.15, 1.10, 0.90, "fins",    "A very grumpy fish on two legs"),
    # ---- uncommon (+5)
    ("uncommon",  "frostyeti",    "Snow Yeti",        "yeti",    "e8f4ff", "7ad8ff", "ffffff", 1.15, 0.85, 1.35, 1.15, 0.90, "shell",   "A fluffy white yeti from the mountain tops"),
    ("uncommon",  "lavacrab",     "Lava Crab",        "crab",    "d04a1a", "ffb020", "f08a50", 1.05, 1.00, 1.25, 1.20, 1.00, "shell",   "A red-hot crab with glowing claws"),
    ("uncommon",  "stormpigeon",  "Storm Pigeon",     "pigeon",  "3a5a9a", "ffe03a", "a8c0e8", 1.00, 1.25, 0.95, 1.00, 1.20, "beak",    "A thundercloud-grey pigeon with lightning feathers"),
    ("uncommon",  "zapblob",      "Zap Blob",         "blob",    "ffe03a", "3ad7ff", "fff4a0", 1.10, 0.95, 1.20, 1.00, 1.20, "drip",    "A crackling yellow blob full of sparks"),
    ("uncommon",  "toadstool",    "Toadstool Prince", "mush",    "d04a4a", "ffffff", "f8e0d0", 1.10, 0.85, 1.30, 1.00, 1.15, "tall",    "A polka-dot mushroom prince with a spotty crown"),
    # ---- rare (+8)
    ("rare",      "sandserpent",  "Sand Serpent",     "serpent", "e0c070", "b0602a", "f8e8b8", 1.15, 1.20, 1.20, 1.10, 1.10, "long",     "A desert serpent that slithers fast"),
    ("rare",      "seaserpent",   "Sea Serpent",      "serpent", "2a9ad0", "9affea", "b8f0ff", 1.20, 1.15, 1.30, 1.10, 1.15, "long",     "A sea serpent with shimmering teal fins"),
    ("rare",      "emberdragon",  "Ember Dragon",     "dragon",  "e84a2a", "ffd23a", "ffb080", 1.15, 1.20, 1.15, 1.20, 1.25, "",         "A fiery dragon with glowing ember wings"),
    ("rare",      "jadedragon",   "Jade Dragon",      "dragon",  "2fb070", "ffe08a", "b8f0c8", 1.15, 1.15, 1.25, 1.15, 1.20, "slim",     "A graceful green dragon, wise and fast"),
    ("rare",      "ironyeti",     "Iron Yeti",        "yeti",    "7a8494", "ff8a3a", "b0b8c4", 1.25, 0.80, 1.60, 1.30, 0.90, "shell",    "A yeti in rusty armour. Really hard to knock down"),
    ("rare",      "rustmech",     "Rust Mech",        "mechbee", "a0502a", "3a3a3a", "6a3a20", 1.15, 0.90, 1.45, 1.25, 1.00, "hat",      "A rusty old mech that still hits like a truck"),
    ("rare",      "ghostbirb",    "Ghost Birb",       "birb",    "c8d0ff", "7a8aff", "f0f4ff", 1.00, 1.30, 0.95, 1.00, 1.35, "owl",      "A spooky pale birb that glows in the dark"),
    ("rare",      "bonecrab",     "Bone Crab",        "crab",    "e8e4d0", "8a2a2a", "c8c4b0", 1.10, 1.00, 1.30, 1.35, 1.00, "shell",    "A skeleton crab with scary red eyes"),
    # ---- epic (+9)
    ("epic",      "infernodemon", "Inferno Demon",    "demon",   "d02a1a", "ffb020", "f08060", 1.25, 1.00, 1.45, 1.30, 1.30, "horn",     "A blazing demon wreathed in fire"),
    ("epic",      "frostdemon",   "Frost Demon",      "demon",   "8ad8ff", "ffffff", "d8f0ff", 1.25, 1.00, 1.40, 1.25, 1.35, "horn",     "An icy demon with a freezing roar"),
    ("epic",      "shadowdemon",  "Shadow Demon",     "bdemon",  "2a1a4a", "b06bff", "5a3a8a", 1.25, 1.10, 1.35, 1.35, 1.30, "neck",     "A demon that lurks in the dark"),
    ("epic",      "golddemon",    "Gold Demon",       "bdemon",  "e0b030", "3a1a6a", "ffe890", 1.30, 0.95, 1.55, 1.35, 1.20, "neck",     "A shining gold demon. Very showy"),
    ("epic",      "crystaldragon","Crystal Dragon",   "dragon2", "7ae8ff", "ff6bd5", "d8f8ff", 1.25, 1.20, 1.35, 1.25, 1.45, "slim",     "A sparkling crystal dragon"),
    ("epic",      "nightdragon",  "Night Dragon",     "dragon2", "1a1a3a", "5a6bff", "3a3a6a", 1.25, 1.25, 1.40, 1.30, 1.40, "slim",     "A midnight dragon with starry wings"),
    ("epic",      "voltfox",      "Volt Fox Mech",    "mechfox", "ffe03a", "3a6aff", "e8c820", 1.20, 1.15, 1.35, 1.25, 1.45, "",         "A lightning-fast fox mech"),
    ("epic",      "magmayeti",    "Magma Yeti",       "yeti",    "3a2a2a", "ff6a1a", "6a3a2a", 1.35, 0.85, 1.65, 1.40, 1.25, "shell",    "A volcano yeti oozing hot lava"),
    ("epic",      "sporeking",    "Spore King",       "mush",    "8a4ad0", "7affc8", "d8b8f8", 1.30, 0.90, 1.50, 1.20, 1.45, "tall",     "A royal purple mushroom that puffs spore clouds"),
    # ---- legendary (+6)
    ("legendary", "titanrex",     "Titan Rex",        "dino",    "c8a020", "ffffff", "f0d878", 1.50, 0.95, 1.80, 1.50, 1.30, "",         "The golden king of all dinosaurs"),
    ("legendary", "stormdragon",  "Storm Dragon",     "dragon2", "5a8aff", "ffffff", "c8d8ff", 1.40, 1.30, 1.55, 1.40, 1.60, "slim",     "A dragon that rides the thunderstorm"),
    ("legendary", "overmech",     "Overlord Mech",    "mechbee", "1a1a22", "ff2a4a", "3a3a44", 1.45, 0.95, 1.85, 1.50, 1.50, "hat",      "A black battle mech with red eyes. Ruler of the machines"),
    ("legendary", "starserpent",  "Star Serpent",     "serpent", "1a1a5a", "ffe03a", "3a3a8a", 1.45, 1.25, 1.55, 1.40, 1.60, "long",     "A cosmic serpent covered in twinkling stars"),
    ("legendary", "phoenixbirb",  "Phoenix Birb",     "birb",    "ff7a1a", "ffe03a", "ffd080", 1.35, 1.40, 1.45, 1.35, 1.70, "owl",      "A fiery birb that rises again and again"),
    ("legendary", "leviathan",    "Leviathan",        "fish",    "2a4aa0", "7affff", "6a8ae0", 1.55, 1.00, 1.90, 1.55, 1.40, "fins",     "A giant deep-sea monster of the dark ocean"),
]
ROSTER = NEW

TIER_RATES = {"common": 36, "uncommon": 26, "rare": 20, "epic": 13, "legendary": 4.5, "ultra": 0.5}  # more rares and epics than before (was 50/27/14/6.5/2/0.5)

def hexs(c): return "%02x%02x%02x" % tuple(int(round(v * 255)) for v in c)
def rgb(h): return tuple(int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))
def shift(h, dh=0.0, ds=1.0, dv=1.0):
    r, g, b = rgb(h); hh, s, v = colorsys.rgb_to_hsv(r, g, b)
    return hexs(colorsys.hsv_to_rgb((hh + dh) % 1.0, max(0, min(1, s * ds)), max(0, min(1, v * dv))))
def palettes_for(body, accent, belly):
    return [
        {"name": "Classic", "weight": 4, "primary": body, "secondary": belly, "accent": accent, "eyes": "ffe03a"},
        {"name": "Berry",   "weight": 2, "primary": shift(body, 0.17), "secondary": shift(belly, 0.17), "accent": shift(accent, 0.33), "eyes": "ff4fd8"},
        {"name": "Ocean",   "weight": 2, "primary": shift(body, -0.19), "secondary": shift(belly, -0.19), "accent": shift(accent, 0.5), "eyes": "3ad7ff"},
        {"name": "Candy",   "weight": 1, "primary": shift(body, 0.42, 0.55, 1.35), "secondary": shift(belly, 0.42, 0.5, 1.1), "accent": shift(accent, 0.1), "eyes": "7aff6a"},
    ]

def main():
    # ---- monsters.json: keep header + the first 18 original lines, then the new species
    p = os.path.join(DATA, "monsters.json"); lines = open(p).read().split("\n")
    head = [l for l in lines if l.startswith("//")]
    ent = [l for l in lines if l.strip().startswith("{")]
    orig = ent[:18]
    assert len(orig) == 18, "expected the 18 original monsters first"
    out = []
    for tier, id, name, mesh, body, acc, belly, sc, sp, hp, me, po, var, desc in NEW:
        plan = MESH[mesh][4]
        out.append('  {"id":"%s","variant":"%s","name":"%s","plan":"%s","body":"%s","accent":"%s","belly":"%s","scale":%.2f,"speed":%.2f,"hp":%.2f,"melee":%.2f,"power":%.2f,"desc":"%s"}' % (id, var, name, plan, body, acc, belly, sc, sp, hp, me, po, desc))
    head = [h.replace("18 original monsters", "50 monsters (18 originals + 32 more from tools/gen_roster.py)") for h in head]
    body_lines = [l.rstrip(",") for l in orig] + out
    text = "\n".join(head) + "\n[\n" + ",\n".join(body_lines) + "\n]\n"
    open(p, "w").write(text)
    # ---- eggs.json
    ep = os.path.join(DATA, "eggs.json"); j = json.load(open(ep))
    for t in j["tiers"]: j["tiers"][t] = [m for m in j["tiers"][t] if m not in {n[1] for n in NEW}]
    for tier, id, *_ in NEW: j["tiers"][tier].append(id)
    for tier, id, name, mesh, body, acc, belly, *_ in NEW: j["palettes"][id] = palettes_for(body, acc, belly)
    for r in j["rarities"]: r["rate"] = TIER_RATES[r["id"]]
    open(ep, "w").write(json.dumps(j, indent=1, ensure_ascii=False))
    print("monsters:", len(orig) + len(NEW), {t: len(v) for t, v in j["tiers"].items()}, "rate sum", sum(TIER_RATES.values()))

if __name__ == "__main__":
    main()
