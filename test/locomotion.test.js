import assert from 'node:assert/strict';
import { test } from 'node:test';
import * as THREE from 'three';
import { createCharacterModel } from '../src/character/model.js';
import { startLocomotion } from '../src/animation/locomotion.js';
import { createCharacterController } from '../src/character/controller.js';

function worldDirection(fromObject, toObject) {
  fromObject.updateWorldMatrix(true, true);
  const from = fromObject.getWorldPosition(new THREE.Vector3());
  const to = toObject.getWorldPosition(new THREE.Vector3());
  return to.sub(from).normalize();
}

function measurePose(scene) {
  const leftArm = worldDirection(
    scene.getObjectByName('LeftUpperArm'),
    scene.getObjectByName('LeftHand'),
  );
  const rightArm = worldDirection(
    scene.getObjectByName('RightUpperArm'),
    scene.getObjectByName('RightHand'),
  );
  const leftThigh = scene.getObjectByName('LeftThigh');
  const rightThigh = scene.getObjectByName('RightThigh');
  const leftSwing = new THREE.Euler().setFromQuaternion(leftArmObjectQuat(scene), 'XYZ');
  return {
    leftArm,
    rightArm,
    leftArmHorizontal: Math.abs(leftArm.y) < 0.35 && Math.abs(leftArm.x) > 0.75,
    rightArmHorizontal: Math.abs(rightArm.y) < 0.35 && Math.abs(rightArm.x) > 0.75,
    leftThighX: new THREE.Euler().setFromQuaternion(leftThigh.quaternion).x,
    rightThighX: new THREE.Euler().setFromQuaternion(rightThigh.quaternion).x,
    leftArmSwing: leftSwing.x,
  };
}

function leftArmObjectQuat(scene) {
  return scene.getObjectByName('LeftUpperArm').quaternion;
}

test('bind pose is a T-pose and the first clip freezes it', () => {
  const model = createCharacterModel();
  const bind = measurePose(model.scene);
  assert.equal(bind.leftArmHorizontal, true);
  assert.equal(bind.rightArmHorizontal, true);
  assert.equal(model.animations[0].name, 'TPose');

  const mixer = new THREE.AnimationMixer(model.scene);
  mixer.clipAction(model.animations[0]).play();
  mixer.update(0.5);
  model.scene.updateMatrixWorld(true);
  const frozen = measurePose(model.scene);
  assert.equal(frozen.leftArmHorizontal, true);
  assert.equal(frozen.rightArmHorizontal, true);
  assert.ok(Math.abs(frozen.leftThighX) < 0.05);
});

test('moving plays the walk clip and swings arms and legs off the T-pose', () => {
  const model = createCharacterModel();
  const loco = startLocomotion(model, { moving: true });
  assert.equal(loco.clipName(), 'Walk');

  const samples = [];
  for (let i = 0; i < 60; i += 1) {
    loco.update(1 / 60);
    model.scene.updateMatrixWorld(true);
    samples.push(measurePose(model.scene));
  }

  assert.ok(samples.every((sample) => sample.leftArmHorizontal === false));
  assert.ok(samples.every((sample) => sample.rightArmHorizontal === false));

  const thighRange = range(samples.map((sample) => sample.leftThighX));
  const armRange = range(samples.map((sample) => sample.leftArmSwing));
  assert.ok(thighRange > 0.8, `left thigh swing ${thighRange}`);
  assert.ok(armRange > 0.6, `left arm swing ${armRange}`);

  const opposite = samples.some(
    (sample) => Math.sign(sample.leftThighX) !== 0
      && Math.sign(sample.rightThighX) !== 0
      && Math.sign(sample.leftThighX) !== Math.sign(sample.rightThighX),
  );
  assert.equal(opposite, true);
});

test('standing still uses idle and keeps the arms down', () => {
  const model = createCharacterModel();
  const loco = startLocomotion(model, { moving: false });
  assert.equal(loco.clipName(), 'Idle');
  loco.update(0.4);
  model.scene.updateMatrixWorld(true);
  const pose = measurePose(model.scene);
  assert.equal(pose.leftArmHorizontal, false);
  assert.equal(pose.rightArmHorizontal, false);
  assert.ok(pose.leftArm.y < -0.7);
  assert.ok(pose.rightArm.y < -0.7);
});

test('a static T-pose clip is not used when a walk clip exists', () => {
  const model = createCharacterModel();
  model.animations = model.animations.filter((clip) => clip.name !== 'Idle');
  const standing = startLocomotion(model, { moving: false });
  assert.notEqual(standing.clipName(), 'TPose');
  standing.update(0.3);
  model.scene.updateMatrixWorld(true);
  const pose = measurePose(model.scene);
  assert.equal(pose.leftArmHorizontal, false);
});

test('torso skinned mesh is bound to the hip skeleton', () => {
  const model = createCharacterModel();
  const torso = model.scene.getObjectByName('Torso');
  const hips = model.scene.getObjectByName('Hips');
  assert.equal(torso.isSkinnedMesh, true);
  assert.ok(torso.skeleton.bones.includes(hips));
  assert.equal(torso.skeleton.boneInverses.length, torso.skeleton.bones.length);

  torso.skeleton.update();
  const before = torso.skeleton.boneMatrices[13];
  hips.position.y += 1.5;
  hips.updateWorldMatrix(true, false);
  torso.skeleton.update();
  assert.notEqual(torso.skeleton.boneMatrices[13], before);
});

test('controller walks forward on the walk cycle and idles without a T-pose', () => {
  const model = createCharacterModel();
  const controller = createCharacterController(model);
  assert.equal(controller.clipName(), 'Idle');

  controller.update(0.5, { forward: true, back: false, left: false, right: false });
  assert.equal(controller.clipName(), 'Walk');
  assert.ok(model.scene.position.z > 0.5);

  const first = new THREE.Euler().setFromQuaternion(
    model.scene.getObjectByName('LeftThigh').quaternion,
  ).x;
  controller.update(0.35, { forward: true, back: false, left: false, right: false });
  const second = new THREE.Euler().setFromQuaternion(
    model.scene.getObjectByName('LeftThigh').quaternion,
  ).x;
  assert.ok(Math.abs(second - first) > 0.2, `thigh delta ${second - first}`);

  controller.update(0.2, { forward: false, back: false, left: false, right: false });
  assert.equal(controller.clipName(), 'Idle');
  model.scene.updateMatrixWorld(true);
  const pose = measurePose(model.scene);
  assert.equal(pose.leftArmHorizontal, false);
  assert.equal(pose.rightArmHorizontal, false);
});

function range(values) {
  return Math.max(...values) - Math.min(...values);
}
