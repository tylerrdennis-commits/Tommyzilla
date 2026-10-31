/**
 * Playable variant pool. These ids are not the title-lineup heroes.
 * Each record has its own display name, silhouette, and palette.
 * Shiny palettes flip the hue so the alternate form does not match the regular skin.
 */

function box(name, bone, size, position, rotation = [0, 0, 0]) {
  return { name, bone, size, position, rotation };
}

function look(body, limb, limbShade, eye, mark) {
  return { body, limb, limbShade, eye, mark };
}

function clamp01(value) {
  return Math.min(1, Math.max(0, value));
}

function hexToRgb(hex) {
  const h = hex.replace('#', '');
  return {
    r: parseInt(h.slice(0, 2), 16) / 255,
    g: parseInt(h.slice(2, 4), 16) / 255,
    b: parseInt(h.slice(4, 6), 16) / 255,
  };
}

function rgbToHex({ r, g, b }) {
  const channel = (value) => Math.round(clamp01(value) * 255).toString(16).padStart(2, '0');
  return `#${channel(r)}${channel(g)}${channel(b)}`;
}

function rgbToHsl({ r, g, b }) {
  const max = Math.max(r, g, b);
  const min = Math.min(r, g, b);
  const l = (max + min) / 2;
  if (max === min) return { h: 0, s: 0, l };
  const d = max - min;
  const s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
  let h;
  if (max === r) h = (g - b) / d + (g < b ? 6 : 0);
  else if (max === g) h = (b - r) / d + 2;
  else h = (r - g) / d + 4;
  return { h: h / 6, s, l };
}

function hslToRgb({ h, s, l }) {
  if (s === 0) return { r: l, g: l, b: l };
  const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
  const p = 2 * l - q;
  const channel = (t) => {
    let x = t;
    if (x < 0) x += 1;
    if (x > 1) x -= 1;
    if (x < 1 / 6) return p + (q - p) * 6 * x;
    if (x < 1 / 2) return q;
    if (x < 2 / 3) return p + (q - p) * (2 / 3 - x) * 6;
    return p;
  };
  return { r: channel(h + 1 / 3), g: channel(h), b: channel(h - 1 / 3) };
}

export function toShinyPalette(palette) {
  const map = (hex) => {
    const hsl = rgbToHsl(hexToRgb(hex));
    hsl.h = (hsl.h + 0.5) % 1;
    hsl.s = 0.82;
    hsl.l = 0.62;
    return rgbToHex(hslToRgb(hsl));
  };
  return {
    body: map(palette.body),
    limb: map(palette.limb),
    limbShade: map(palette.limbShade),
    eye: '#fff7d6',
    mark: '#ffe56a',
  };
}

function variant(id, displayName, modelKind, palette, features) {
  return {
    id,
    displayName,
    modelKind,
    palette,
    shinyPalette: toShinyPalette(palette),
    features,
  };
}

const wing = (name, side) => box(
  name,
  'chest',
  [0.9, 0.08, 0.46],
  [side * 0.72, 0.26, -0.1],
  [0.15, side * -0.45, side * -0.75],
);

export const VARIANTS = [
  variant(
    'moss-titan',
    'Moss Titan',
    'moss-titan',
    look('#e4f25a', '#f4ff8a', '#8aaa22', '#243018', '#f7ffb0'),
    [0.28, 0.46, 0.62].map((y, index) => box(
      'SpineSpike',
      'spine',
      [0.14, 0.28 - index * 0.04, 0.1],
      [0, y, -0.36],
    )),
  ),
  variant(
    'frost-warden',
    'Frost Warden',
    'frost-warden',
    look('#7f93a8', '#d5e4ef', '#5d7386', '#1c3a44', '#c5d7e4'),
    [box('IceBrow', 'head', [0.56, 0.16, 0.2], [0, 0.34, 0.2])],
  ),
  variant(
    'vine-wyrm',
    'Vine Wyrm',
    'vine-wyrm',
    look('#5dce6a', '#8aee96', '#2e8a45', '#f6ffd2', '#d8ffb0'),
    [
      box('VineSnout', 'head', [0.18, 0.14, 0.4], [0, 0.06, 0.36]),
      wing('LeafWing', -1),
      wing('LeafWing', 1),
    ],
  ),
  variant(
    'copper-raptor',
    'Copper Raptor',
    'copper-raptor',
    look('#b86a3d', '#d4895a', '#8a4b28', '#ffe08a', '#f0c27a'),
    [box('HeadCrest', 'head', [0.16, 0.42, 0.28], [0, 0.42, 0])],
  ),
  variant(
    'basalt-golem',
    'Basalt Golem',
    'basalt-golem',
    look('#4e555b', '#6a7278', '#33383c', '#ffb25a', '#2a2e32'),
    [
      box('ShoulderBoulder', 'chest', [0.36, 0.3, 0.32], [-0.46, 0.14, 0]),
      box('ShoulderBoulder', 'chest', [0.36, 0.3, 0.32], [0.46, 0.14, 0]),
    ],
  ),
  variant(
    'luna-moth',
    'Luna Moth',
    'luna-moth',
    look('#6e5b8a', '#8d78ad', '#4c3d64', '#f7e27a', '#c9b6e8'),
    [
      box('MothWing', 'chest', [0.95, 0.08, 0.42], [-0.78, 0.2, -0.18], [0.15, -0.25, 0.95]),
      box('MothWing', 'chest', [0.95, 0.08, 0.42], [0.78, 0.2, -0.18], [0.15, 0.25, -0.95]),
    ],
  ),
  variant(
    'tide-serpent',
    'Tide Serpent',
    'tide-serpent',
    look('#ef6b4a', '#ff8d70', '#b84428', '#fff1c9', '#ffd0c2'),
    [box('Hood', 'head', [0.78, 0.52, 0.12], [0, 0.3, -0.06])],
  ),
  variant(
    'amber-beetle',
    'Amber Beetle',
    'amber-beetle',
    look('#c4842a', '#e0a84a', '#8f5e16', '#1a1a1a', '#f2d48a'),
    [box('Shell', 'spine', [0.72, 0.3, 0.56], [0, 0.4, -0.02])],
  ),
  variant(
    'cinder-phoenix',
    'Cinder Phoenix',
    'cinder-phoenix',
    look('#c4482a', '#e26a3a', '#8d2e18', '#ffe9a0', '#ffb15a'),
    [box('FlameCrest', 'head', [0.3, 0.48, 0.22], [0, 0.44, -0.02])],
  ),
  variant(
    'ink-kraken',
    'Ink Kraken',
    'ink-kraken',
    look('#243044', '#3d5168', '#17202d', '#7ee0ff', '#1a2433'),
    [
      box('Tentacle', 'chest', [0.12, 0.12, 0.72], [-0.3, -0.02, 0.2], [0.9, 0.2, 0.5]),
      box('Tentacle', 'chest', [0.12, 0.12, 0.72], [0.3, -0.02, 0.2], [0.9, -0.2, -0.5]),
    ],
  ),
  variant(
    'ivory-mammoth',
    'Ivory Mammoth',
    'ivory-mammoth',
    look('#c8beb0', '#e4dcd0', '#9e9488', '#3a2a22', '#f4efe6'),
    [
      box('Tusk', 'head', [0.08, 0.08, 0.46], [-0.14, 0.02, 0.32]),
      box('Tusk', 'head', [0.08, 0.08, 0.46], [0.14, 0.02, 0.32]),
    ],
  ),
  variant(
    'gale-griffin',
    'Gale Griffin',
    'gale-griffin',
    look('#c4a15a', '#e2c27a', '#8d7038', '#2a2418', '#f0e2b8'),
    [
      box('Beak', 'head', [0.16, 0.14, 0.42], [0, 0.08, 0.38]),
      wing('GriffinWing', -1),
      wing('GriffinWing', 1),
    ],
  ),
  variant(
    'crimson-oni',
    'Crimson Oni',
    'crimson-oni',
    look('#8e2430', '#b83a48', '#641820', '#f6e27a', '#e8c07a'),
    [
      box('OniHorn', 'head', [0.1, 0.38, 0.1], [-0.18, 0.4, 0.04], [0.15, 0, 0.4]),
      box('OniHorn', 'head', [0.1, 0.38, 0.1], [0.18, 0.4, 0.04], [0.15, 0, -0.4]),
    ],
  ),
  variant(
    'reef-shark',
    'Reef Shark',
    'reef-shark',
    look('#3d7ea6', '#5aa0c4', '#2a5874', '#101820', '#d8e8f0'),
    [box('DorsalFin', 'spine', [0.08, 0.58, 0.32], [0, 0.52, -0.16])],
  ),
  variant(
    'prism-warden',
    'Prism Warden',
    'prism-warden',
    look('#5b6fd6', '#7f93ee', '#3c4b9a', '#f8fbff', '#d0d8ff'),
    [0.24, 0.44, 0.62].map((y, index) => box(
      'CrystalShard',
      'spine',
      [0.14, 0.34 - index * 0.04, 0.14],
      [(index - 1) * 0.08, y, -0.28],
      [0.35, 0, 0.2],
    )),
  ),
  variant(
    'thorn-stag',
    'Thorn Stag',
    'thorn-stag',
    look('#6a4a32', '#8b6548', '#4a3222', '#f0e2c0', '#c4a06a'),
    [
      box('Antler', 'head', [0.12, 0.5, 0.12], [-0.2, 0.46, 0], [0, 0, 0.45]),
      box('Antler', 'head', [0.12, 0.5, 0.12], [0.2, 0.46, 0], [0, 0, -0.45]),
    ],
  ),
];

export function listVariants() {
  return VARIANTS;
}

export function findVariant(id) {
  return VARIANTS.find((entry) => entry.id === id) ?? null;
}

export function getVariant(id) {
  const variantRecord = findVariant(id);
  if (!variantRecord) {
    throw new Error(`Unknown variant id: ${id}`);
  }
  return variantRecord;
}
