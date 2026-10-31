import * as THREE from 'three';
import { createCharacterModel } from './character/model.js';
import { createCharacterController } from './character/controller.js';

const canvas = document.querySelector('#view');
const menu = document.querySelector('#menu');
const playHint = document.querySelector('#play-hint');
const playButton = document.querySelector('#play');

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

const model = createCharacterModel();
scene.add(model.scene);
const controller = createCharacterController(model);

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

function armDirection(fromName, toName) {
  const from = model.scene.getObjectByName(fromName);
  const to = model.scene.getObjectByName(toName);
  from.updateWorldMatrix(true, true);
  const origin = from.getWorldPosition(new THREE.Vector3());
  const tip = to.getWorldPosition(new THREE.Vector3());
  return tip.sub(origin).normalize();
}

function samplePose() {
  model.scene.updateMatrixWorld(true);
  const leftArm = armDirection('LeftUpperArm', 'LeftHand');
  const rightArm = armDirection('RightUpperArm', 'RightHand');
  const leftThigh = model.scene.getObjectByName('LeftThigh');
  const rightThigh = model.scene.getObjectByName('RightThigh');
  return {
    mode,
    clip: controller.clipName(),
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
  model.scene.position.set(0, 0, 0);
  model.scene.rotation.y = Math.PI / 2;
  controller.setMoving(true);
}

function enterMenu() {
  mode = 'menu';
  autoWalk = false;
  menu.classList.remove('hidden');
  playHint.classList.add('hidden');
  keys.clear();
  syncInput();
  menuTime = 0;
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
    controller.update(dt, { forward: true, back: false, left: false, right: false }, { slide: false });
    const pace = menuTime * 0.7;
    const anchorX = 0.35;
    model.scene.position.set(anchorX + Math.sin(pace) * 0.4, 0, 0);
    model.scene.rotation.y = Math.cos(pace) >= 0 ? Math.PI / 2 : -Math.PI / 2;
    camera.position.set(-1.15, 2.05, 7.2);
    camera.lookAt(-0.15, 1.55, 0);
    return;
  }

  const anyKey = input.forward || input.back || input.left || input.right;
  if (anyKey) autoWalk = false;
  const playInput = autoWalk
    ? { forward: true, back: false, left: false, right: false }
    : input;
  controller.update(dt, playInput);
  const yaw = model.scene.rotation.y;
  const forward = new THREE.Vector3(Math.sin(yaw), 0, Math.cos(yaw));
  const right = new THREE.Vector3(Math.cos(yaw), 0, -Math.sin(yaw));
  const desired = model.scene.position.clone()
    .addScaledVector(forward, -0.4)
    .addScaledVector(right, 6.2)
    .add(new THREE.Vector3(0, 1.65, 0));
  camera.position.lerp(desired, 1 - Math.exp(-4 * dt));
  camera.lookAt(model.scene.position.x, 1.2, model.scene.position.z);
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
  get mode() { return mode; },
};

tick();
