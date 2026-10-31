import * as THREE from 'three';
import { createCharacterController } from './character/controller.js';
import { createCharacterModel } from './character/model.js';
import { spawnShownCharacters } from './character/spawn.js';
import { getVariant, listVariants } from './character/variants.js';
import {
  SHINY_ODDS_COPY,
  STARTING_EGGS,
  applyShinyRoll,
  blockedReason,
  describeShinyRoll,
  rollShinyChance,
} from './character/shinyRoll.js';

const canvas = document.querySelector('#view');
const menu = document.querySelector('#menu');
const playHint = document.querySelector('#play-hint');
const playName = document.querySelector('#play-name');
const playButton = document.querySelector('#play');
const rosterList = document.querySelector('#roster');
const variantList = document.querySelector('#variant-list');
const eggCount = document.querySelector('#egg-count');
const shinyOdds = document.querySelector('#shiny-odds');
const shinyButton = document.querySelector('#shiny-chance');
const eggBlock = document.querySelector('#egg-block');
const shinyResult = document.querySelector('#shiny-result');

const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.shadowMap.enabled = true;

const scene = new THREE.Scene();
scene.background = new THREE.Color('#141a1e');
scene.fog = new THREE.Fog('#141a1e', 12, 36);

const camera = new THREE.PerspectiveCamera(50, window.innerWidth / window.innerHeight, 0.1, 80);
camera.position.set(0, 2.4, 8);

scene.add(new THREE.HemisphereLight('#c5d4e0', '#1a2420', 1.15));
const sun = new THREE.DirectionalLight('#f4f7fb', 1.4);
sun.position.set(4, 8, 3);
sun.castShadow = true;
scene.add(sun);

const ground = new THREE.Mesh(
  new THREE.CircleGeometry(24, 48),
  new THREE.MeshLambertMaterial({ color: '#1b2428' }),
);
ground.rotation.x = -Math.PI / 2;
ground.receiveShadow = true;
scene.add(ground);
scene.add(new THREE.GridHelper(24, 24, '#1c6f66', '#243238'));

const roster = spawnShownCharacters();
const slots = { yeti: 4.2, tommyzilla: 6.25, dragon: 8.3 };
let active = roster[0];
let rosterActive = active;
let selectedVariantId = null;
let eggs = STARTING_EGGS;
const shinyOwned = new Map();
let variantActor = null;
const controllers = new Map();
for (const entry of roster) {
  scene.add(entry.scene);
  attachNameSprite(entry);
  controllers.set(entry, createCharacterController(entry));
}
renderRoster();
renderVariants();
renderEggs();

const keys = new Set();
const input = { forward: false, back: false, left: false, right: false };
let mode = 'menu';
let menuTime = 0;
let autoWalk = false;
const clock = new THREE.Clock();

function syncInput() {
  input.forward = keys.has('KeyW') || keys.has('ArrowUp');
  input.back = keys.has('KeyS') || keys.has('ArrowDown');
  input.left = keys.has('KeyA') || keys.has('ArrowLeft');
  input.right = keys.has('KeyD') || keys.has('ArrowRight');
}

function activeController() {
  return controllers.get(active);
}

function armDirection(fromName, toName) {
  const from = active.scene.getObjectByName(fromName);
  const to = active.scene.getObjectByName(toName);
  from.updateWorldMatrix(true, true);
  const origin = from.getWorldPosition(new THREE.Vector3());
  const tip = to.getWorldPosition(new THREE.Vector3());
  return tip.sub(origin).normalize();
}

function samplePose() {
  active.scene.updateMatrixWorld(true);
  const leftArm = armDirection('LeftUpperArm', 'LeftHand');
  const rightArm = armDirection('RightUpperArm', 'RightHand');
  const leftThigh = active.scene.getObjectByName('LeftThigh');
  const rightThigh = active.scene.getObjectByName('RightThigh');
  return {
    mode,
    displayName: active.displayName,
    clip: activeController().clipName(),
    leftArmY: leftArm.y,
    rightArmY: rightArm.y,
    leftArmHorizontal: Math.abs(leftArm.y) < 0.35 && Math.abs(leftArm.x) > 0.75,
    rightArmHorizontal: Math.abs(rightArm.y) < 0.35 && Math.abs(rightArm.x) > 0.75,
    leftThighX: new THREE.Euler().setFromQuaternion(leftThigh.quaternion).x,
    rightThighX: new THREE.Euler().setFromQuaternion(rightThigh.quaternion).x,
  };
}

function enterPlay() {
  mode = 'play';
  autoWalk = true;
  menu.classList.add('hidden');
  playHint.classList.remove('hidden');
  if (selectedVariantId) {
    const shiny = shinyOwned.get(selectedVariantId) === true;
    if (!variantActor || variantActor.id !== selectedVariantId || variantActor.shiny !== shiny) {
      showVariantActor(selectedVariantId, shiny);
    }
    active = variantActor.model;
    for (const entry of roster) entry.scene.visible = false;
    active.scene.visible = true;
    active.scene.position.set(0, 0, 0);
    active.scene.rotation.y = Math.PI / 2;
    playName.textContent = shiny ? `Shiny ${active.displayName}` : active.displayName;
  } else {
    if (variantActor) variantActor.model.scene.visible = false;
    active = rosterActive;
    playName.textContent = active.displayName;
    for (const entry of roster) entry.scene.visible = entry === active;
    active.scene.position.set(0, 0, 0);
    active.scene.rotation.y = Math.PI / 2;
  }
  activeController().setMoving(true);
}

function enterMenu() {
  mode = 'menu';
  autoWalk = false;
  menu.classList.remove('hidden');
  playHint.classList.add('hidden');
  keys.clear();
  syncInput();
  menuTime = 0;
  active = rosterActive;
  for (const entry of roster) entry.scene.visible = true;
  if (selectedVariantId) {
    const shiny = shinyOwned.get(selectedVariantId) === true;
    if (!variantActor || variantActor.id !== selectedVariantId || variantActor.shiny !== shiny) {
      showVariantActor(selectedVariantId, shiny);
    }
    variantActor.model.scene.visible = false;
  }
}

playButton.addEventListener('click', enterPlay);
window.addEventListener('keydown', (event) => {
  if (event.code === 'Escape' && mode === 'play') {
    enterMenu();
    return;
  }
  keys.add(event.code);
  syncInput();
});
window.addEventListener('keyup', (event) => {
  keys.delete(event.code);
  syncInput();
});
window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});

function frame(dt) {
  if (mode === 'menu') {
    menuTime += dt;
    const pace = menuTime * 0.7;
    const face = Math.PI * 0.72;
    for (const entry of roster) {
      controllers.get(entry).update(
        dt,
        { forward: true, back: false, left: false, right: false },
        { slide: false },
      );
      const bob = entry === active ? Math.sin(pace) * 0.4 : 0;
      entry.scene.position.set(slots[entry.scene.userData.characterId] + bob, 0, 0);
      entry.scene.rotation.y = face;
    }
    if (variantActor) variantActor.model.scene.visible = false;
    camera.position.set(3.5, 2.35, 8.4);
    camera.lookAt(3.5, 1.15, 0);
    return;
  }

  const anyKey = input.forward || input.back || input.left || input.right;
  if (anyKey) autoWalk = false;
  const playInput = autoWalk
    ? { forward: true, back: false, left: false, right: false }
    : input;
  activeController().update(dt, playInput);
  const yaw = active.scene.rotation.y;
  const forward = new THREE.Vector3(Math.sin(yaw), 0, Math.cos(yaw));
  const right = new THREE.Vector3(Math.cos(yaw), 0, -Math.sin(yaw));
  const desired = active.scene.position.clone()
    .addScaledVector(forward, -0.4)
    .addScaledVector(right, 6.2)
    .add(new THREE.Vector3(0, 1.65, 0));
  camera.position.lerp(desired, 1 - Math.exp(-4 * dt));
  camera.lookAt(active.scene.position.x, 1.2, active.scene.position.z);
}

function tick() {
  const dt = Math.min(clock.getDelta(), 0.05);
  frame(dt);
  renderer.render(scene, camera);
  requestAnimationFrame(tick);
}

window.__tommyzilla = {
  samplePose,
  enterPlay,
  enterMenu,
  identities,
  variants,
  selectVariant,
  spendShiny,
  setEggs,
  eggState,
  preview,
  get mode() { return mode; },
};

function identities() {
  return roster.map((entry) => {
    const torso = entry.scene.getObjectByName('Torso');
    const thigh = entry.scene.getObjectByName('LeftThigh').children.find((child) => child.userData.part === 'limb');
    return {
      id: entry.scene.userData.characterId,
      displayName: entry.displayName,
      modelKind: entry.scene.userData.modelKind,
      nameplate: entry.scene.getObjectByName('Nameplate').userData.label,
      body: torso.material.color.getHexString(),
      limb: thigh.material.color.getHexString(),
      visible: entry.scene.visible,
    };
  });
}

function renderRoster() {
  rosterList.replaceChildren();
  for (const entry of roster) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'roster-item';
    button.dataset.characterId = entry.scene.userData.characterId;
    button.setAttribute('aria-pressed', String(!selectedVariantId && entry === rosterActive));
    const swatch = document.createElement('span');
    swatch.className = 'swatch';
    swatch.style.background = `#${entry.scene.getObjectByName('Torso').material.color.getHexString()}`;
    const label = document.createElement('span');
    label.textContent = entry.displayName;
    button.append(swatch, label);
    button.addEventListener('click', () => {
      rosterActive = entry;
      active = entry;
      selectedVariantId = null;
      clearVariantActor();
      renderRoster();
      renderVariants();
      renderEggs();
    });
    rosterList.append(button);
  }
}

shinyButton.addEventListener('click', () => {
  spendShiny();
});

function renderVariants() {
  variantList.replaceChildren();
  for (const variant of listVariants()) {
    const shiny = shinyOwned.get(variant.id) === true;
    const button = document.createElement('button');
    button.type = 'button';
    button.className = shiny ? 'variant-item shiny' : 'variant-item';
    button.dataset.variantId = variant.id;
    button.setAttribute('aria-pressed', String(variant.id === selectedVariantId));
    const swatch = document.createElement('span');
    swatch.className = 'swatch';
    swatch.style.background = (shiny ? variant.shinyPalette : variant.palette).body;
    const label = document.createElement('span');
    label.textContent = shiny ? `★ ${variant.displayName}` : variant.displayName;
    button.append(swatch, label);
    button.addEventListener('click', () => {
      selectVariant(variant.id);
    });
    variantList.append(button);
  }
}

function renderEggs(result = null, variant = null) {
  eggCount.textContent = `Eggs: ${eggs}`;
  shinyOdds.textContent = SHINY_ODDS_COPY;
  const blocked = blockedReason({ eggs, variantId: selectedVariantId });
  shinyButton.disabled = blocked !== '';
  shinyButton.setAttribute('aria-disabled', String(blocked !== ''));
  eggBlock.textContent = blocked;
  eggBlock.classList.toggle('hidden', blocked === '');
  if (result && variant) {
    shinyResult.textContent = describeShinyRoll({
      shiny: result.shiny,
      variantName: variant.displayName,
      eggs: result.eggs,
    });
  }
}

function selectVariant(id) {
  selectedVariantId = id;
  showVariantActor(id, shinyOwned.get(id) === true);
  renderRoster();
  renderVariants();
  renderEggs();
  return preview();
}

function spendShiny({ random } = {}) {
  const result = rollShinyChance(eggs, { variantId: selectedVariantId, random });
  const variant = selectedVariantId ? getVariant(selectedVariantId) : null;
  if (result.ok && variant) {
    eggs = result.eggs;
    shinyOwned.set(variant.id, applyShinyRoll(shinyOwned.get(variant.id) === true, result));
    showVariantActor(variant.id, shinyOwned.get(variant.id) === true);
  }
  renderVariants();
  renderEggs(result.ok ? result : null, variant);
  return {
    ...result,
    eggs,
    blocked: blockedReason({ eggs, variantId: selectedVariantId }),
    oddsCopy: SHINY_ODDS_COPY,
    canSpend: blockedReason({ eggs, variantId: selectedVariantId }) === '',
    preview: preview(),
  };
}

function setEggs(count) {
  eggs = count;
  renderEggs();
  return eggState();
}

function eggState() {
  const blocked = blockedReason({ eggs, variantId: selectedVariantId });
  return {
    eggs,
    selectedVariantId,
    blocked,
    oddsCopy: SHINY_ODDS_COPY,
    canSpend: blocked === '',
    result: shinyResult.textContent,
  };
}

function variants() {
  return listVariants().map((variant) => ({
    id: variant.id,
    displayName: variant.displayName,
    modelKind: variant.modelKind,
    shiny: shinyOwned.get(variant.id) === true,
    body: variant.palette.body,
    shinyBody: variant.shinyPalette.body,
  }));
}

function preview() {
  if (!variantActor) return null;
  const sceneRoot = variantActor.model.scene;
  const torso = sceneRoot.getObjectByName('Torso');
  return {
    id: variantActor.id,
    displayName: variantActor.model.displayName,
    modelKind: sceneRoot.userData.modelKind,
    shiny: sceneRoot.userData.shiny === true,
    body: torso.material.color.getHexString(),
    visible: sceneRoot.visible,
    nameplate: sceneRoot.getObjectByName('Nameplate').userData.label,
    spark: Boolean(sceneRoot.getObjectByName('ShinySpark')),
    clip: variantActor.controller.clipName(),
  };
}

function showVariantActor(id, shiny) {
  clearVariantActor();
  const model = createCharacterModel(id, { shiny });
  scene.add(model.scene);
  attachNameSprite(model);
  const controller = createCharacterController(model);
  controllers.set(model, controller);
  variantActor = { model, controller, id, shiny };
  model.scene.visible = mode === 'play';
}

function clearVariantActor() {
  if (!variantActor) return;
  controllers.delete(variantActor.model);
  scene.remove(variantActor.model.scene);
  disposeScene(variantActor.model.scene);
  variantActor = null;
}

function disposeScene(root) {
  root.traverse((obj) => {
    if (obj.geometry) obj.geometry.dispose();
    const materials = obj.material
      ? (Array.isArray(obj.material) ? obj.material : [obj.material])
      : [];
    for (const material of materials) {
      if (material.map) material.map.dispose();
      material.dispose();
    }
  });
}

function attachNameSprite(entry) {
  const plate = entry.scene.getObjectByName('Nameplate');
  const label = plate.userData.label;
  const shown = entry.scene.userData.shiny ? `★ ${label}` : label;
  const wide = shown.length > 12;
  const nameCanvas = document.createElement('canvas');
  nameCanvas.width = wide ? 640 : 512;
  nameCanvas.height = 128;
  const ctx = nameCanvas.getContext('2d');
  ctx.font = wide
    ? '700 52px Impact, Arial Black, sans-serif'
    : '700 72px Impact, Arial Black, sans-serif';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.lineWidth = 12;
  ctx.strokeStyle = '#08211c';
  ctx.fillStyle = entry.scene.userData.shiny ? '#ffe56a' : '#ffffff';
  const center = wide ? 320 : 256;
  ctx.strokeText(shown, center, 68);
  ctx.fillText(shown, center, 68);
  const texture = new THREE.CanvasTexture(nameCanvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  const sprite = new THREE.Sprite(new THREE.SpriteMaterial({
    map: texture,
    transparent: true,
    depthTest: false,
  }));
  sprite.name = 'NameSprite';
  sprite.scale.set(1.55, 0.39, 1);
  plate.add(sprite);
}

tick();
