import * as THREE from 'three';

const ARM_DOWN_Z = Math.PI / 2;

/**
 * Procedural kaiju rig whose bind pose is a T-pose.
 * Clip order matches a common GLTF export: static TPose first, then an
 * unrelated attack, then the clips locomotion should actually play.
 */
export function createCharacterModel() {
  const scene = new THREE.Group();
  scene.name = 'Character';

  const hips = bone('Hips', 0, 1.02, 0);
  const spine = bone('Spine', 0, 0.18, 0);
  const chest = bone('Chest', 0, 0.22, 0);
  const head = bone('Head', 0, 0.18, 0);
  const leftUpper = bone('LeftUpperArm', -0.34, 0.06, 0);
  const leftLower = bone('LeftLowerArm', -0.36, 0, 0);
  const leftHand = marker('LeftHand', -0.34, 0, 0);
  const rightUpper = bone('RightUpperArm', 0.34, 0.06, 0);
  const rightLower = bone('RightLowerArm', 0.36, 0, 0);
  const rightHand = marker('RightHand', 0.34, 0, 0);
  const leftThigh = bone('LeftThigh', -0.18, -0.02, 0);
  const leftShin = bone('LeftShin', 0, -0.46, 0);
  const leftFoot = marker('LeftFoot', 0, -0.44, 0.06);
  const rightThigh = bone('RightThigh', 0.18, -0.02, 0);
  const rightShin = bone('RightShin', 0, -0.46, 0);
  const rightFoot = marker('RightFoot', 0, -0.44, 0.06);
  const tail = bone('Tail', 0, 0.04, -0.22);
  const tailTip = bone('TailTip', 0, 0.04, -0.36);

  hips.add(spine, leftThigh, rightThigh, tail);
  spine.add(chest);
  chest.add(head, leftUpper, rightUpper);
  leftUpper.add(leftLower);
  leftLower.add(leftHand);
  rightUpper.add(rightLower);
  rightLower.add(rightHand);
  leftThigh.add(leftShin);
  leftShin.add(leftFoot);
  rightThigh.add(rightShin);
  rightShin.add(rightFoot);
  tail.add(tailTip);

  const torso = createBoundTorso(hips);
  scene.add(torso);

  addLimbMesh(leftUpper, new THREE.Vector3(-0.18, 0, 0), [0.4, 0.18, 0.18], '#2a9d8f');
  addLimbMesh(leftLower, new THREE.Vector3(-0.16, 0, 0), [0.36, 0.16, 0.16], '#1f7a70');
  addLimbMesh(rightUpper, new THREE.Vector3(0.18, 0, 0), [0.4, 0.18, 0.18], '#4c5d73');
  addLimbMesh(rightLower, new THREE.Vector3(0.16, 0, 0), [0.36, 0.16, 0.16], '#3d4c63');
  addLimbMesh(leftThigh, new THREE.Vector3(0, -0.24, 0), [0.22, 0.5, 0.22], '#2a9d8f');
  addLimbMesh(leftShin, new THREE.Vector3(0, -0.22, 0), [0.18, 0.46, 0.18], '#1f7a70');
  addLimbMesh(rightThigh, new THREE.Vector3(0, -0.24, 0), [0.22, 0.5, 0.22], '#4c5d73');
  addLimbMesh(rightShin, new THREE.Vector3(0, -0.22, 0), [0.18, 0.46, 0.18], '#3d4c63');
  addLimbMesh(head, new THREE.Vector3(0, 0.16, 0.04), [0.5, 0.4, 0.46], '#5c6b82');

  const eyeMat = new THREE.MeshBasicMaterial({ color: '#3ee0c5' });
  const leftEye = new THREE.Mesh(new THREE.SphereGeometry(0.05, 12, 8), eyeMat);
  leftEye.position.set(-0.12, 0.16, 0.2);
  const rightEye = leftEye.clone();
  rightEye.position.x = 0.12;
  head.add(leftEye, rightEye);

  return {
    scene,
    animations: [createTPoseClip(), createAttackClip(), createWalkClip(), createIdleClip()],
  };
}

function createBoundTorso(hips) {
  const geometry = new THREE.BoxGeometry(0.78, 0.72, 0.42);
  geometry.translate(0, 1.38, 0);
  const count = geometry.attributes.position.count;
  const skinIndex = new Uint16Array(count * 4);
  const skinWeight = new Float32Array(count * 4);
  for (let i = 0; i < count; i += 1) {
    skinWeight[i * 4] = 1;
  }
  geometry.setAttribute('skinIndex', new THREE.Uint16BufferAttribute(skinIndex, 4));
  geometry.setAttribute('skinWeight', new THREE.Float32BufferAttribute(skinWeight, 4));

  const torso = new THREE.SkinnedMesh(
    geometry,
    new THREE.MeshLambertMaterial({ color: '#4d5d73' }),
  );
  torso.name = 'Torso';
  torso.add(hips);
  torso.updateMatrixWorld(true);
  const bones = [];
  hips.traverse((obj) => {
    if (obj.isBone) bones.push(obj);
  });
  const skeleton = new THREE.Skeleton(bones);
  torso.bind(skeleton);
  return torso;
}

function createWalkClip() {
  const times = [0, 0.25, 0.5, 0.75, 1];
  const leftThigh = [0.95, 0.1, -0.75, 0.1, 0.95];
  const rightThigh = [-0.75, 0.1, 0.95, 0.1, -0.75];
  const leftArm = [-0.75, 0, 0.75, 0, -0.75];
  const rightArm = [0.75, 0, -0.75, 0, 0.75];
  const leftShin = [0.2, 0.35, 1.05, 0.35, 0.2];
  const rightShin = [1.05, 0.35, 0.2, 0.35, 1.05];

  return new THREE.AnimationClip('Walk', 1, [
    eulerTrack('LeftThigh.quaternion', times, leftThigh.map((x) => [x, 0, 0])),
    eulerTrack('RightThigh.quaternion', times, rightThigh.map((x) => [x, 0, 0])),
    eulerTrack('LeftShin.quaternion', times, leftShin.map((x) => [x, 0, 0])),
    eulerTrack('RightShin.quaternion', times, rightShin.map((x) => [x, 0, 0])),
    eulerTrack('LeftUpperArm.quaternion', times, leftArm.map((x) => [x, 0, ARM_DOWN_Z])),
    eulerTrack('RightUpperArm.quaternion', times, rightArm.map((x) => [x, 0, -ARM_DOWN_Z])),
    eulerTrack('LeftLowerArm.quaternion', times, [0.4, 0.15, 0.55, 0.15, 0.4].map((x) => [x, 0, 0])),
    eulerTrack('RightLowerArm.quaternion', times, [0.55, 0.15, 0.4, 0.15, 0.55].map((x) => [x, 0, 0])),
    eulerTrack('Tail.quaternion', times, [0.25, 0, -0.25, 0, 0.25].map((y) => [0, y, 0])),
    eulerTrack('Spine.quaternion', times, [0.06, 0.02, -0.04, 0.02, 0.06].map((x) => [x, 0, 0])),
  ]);
}

function createIdleClip() {
  const times = [0, 1];
  const downLeft = [0, 0, ARM_DOWN_Z];
  const downRight = [0, 0, -ARM_DOWN_Z];
  return new THREE.AnimationClip('Idle', 1, [
    eulerTrack('LeftUpperArm.quaternion', times, [downLeft, downLeft]),
    eulerTrack('RightUpperArm.quaternion', times, [downRight, downRight]),
    eulerTrack('LeftLowerArm.quaternion', times, [[0.35, 0, 0], [0.35, 0, 0]]),
    eulerTrack('RightLowerArm.quaternion', times, [[0.35, 0, 0], [0.35, 0, 0]]),
    eulerTrack('Spine.quaternion', times, [[0.04, 0, 0], [-0.02, 0, 0]]),
    eulerTrack('Tail.quaternion', times, [[0, 0.08, 0], [0, -0.08, 0]]),
  ]);
}

function createTPoseClip() {
  const times = [0, 1];
  const identity = [0, 0, 0];
  const names = [
    'LeftUpperArm',
    'RightUpperArm',
    'LeftLowerArm',
    'RightLowerArm',
    'LeftThigh',
    'RightThigh',
    'LeftShin',
    'RightShin',
    'Spine',
    'Tail',
  ];
  return new THREE.AnimationClip(
    'TPose',
    1,
    names.map((name) => eulerTrack(`${name}.quaternion`, times, [identity, identity])),
  );
}

function createAttackClip() {
  const times = [0, 0.4, 1];
  return new THREE.AnimationClip('Attack', 1, [
    eulerTrack('LeftUpperArm.quaternion', times, [
      [0, 0, -ARM_DOWN_Z],
      [-0.4, 0, -ARM_DOWN_Z],
      [0, 0, -ARM_DOWN_Z],
    ]),
    eulerTrack('RightUpperArm.quaternion', times, [
      [0, 0, ARM_DOWN_Z],
      [-0.4, 0, ARM_DOWN_Z],
      [0, 0, ARM_DOWN_Z],
    ]),
  ]);
}

function eulerTrack(name, times, eulers) {
  const values = [];
  for (const [x, y, z] of eulers) {
    const q = new THREE.Quaternion().setFromEuler(new THREE.Euler(x, y, z, 'XYZ'));
    values.push(q.x, q.y, q.z, q.w);
  }
  return new THREE.QuaternionKeyframeTrack(name, times, values);
}

function bone(name, x, y, z) {
  const next = new THREE.Bone();
  next.name = name;
  next.position.set(x, y, z);
  return next;
}

function marker(name, x, y, z) {
  const next = new THREE.Object3D();
  next.name = name;
  next.position.set(x, y, z);
  return next;
}

function addLimbMesh(parent, offset, size, color) {
  const mesh = new THREE.Mesh(
    new THREE.BoxGeometry(size[0], size[1], size[2]),
    new THREE.MeshLambertMaterial({ color }),
  );
  mesh.position.copy(offset);
  mesh.castShadow = true;
  parent.add(mesh);
  return mesh;
}
