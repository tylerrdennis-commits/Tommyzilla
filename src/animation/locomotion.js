import * as THREE from 'three';
import { selectLocomotionClip } from './selectClip.js';

/**
 * Plays a walk or idle clip on the character root and advances the mixer.
 * Without update(), or with the static T-pose clip, bones stay at the bind pose.
 */
export function startLocomotion(model, { moving = false } = {}) {
  const mixer = new THREE.AnimationMixer(model.scene);
  let current = null;
  let action = null;
  let movingIntent = moving;

  function play(nextMoving) {
    movingIntent = nextMoving;
    const clip = selectLocomotionClip(model.animations, { moving: nextMoving });
    if (!clip || clip === current) return;

    const next = mixer.clipAction(clip);
    next.reset();
    next.setLoop(THREE.LoopRepeat, Infinity);
    next.enabled = true;
    next.setEffectiveWeight(1);
    next.play();

    if (action) {
      action.crossFadeTo(next, 0.12, false);
    }

    action = next;
    current = clip;
  }

  play(movingIntent);

  return {
    mixer,
    root: model.scene,
    clipName() {
      return current?.name ?? null;
    },
    setMoving(value) {
      play(Boolean(value));
    },
    update(dt) {
      mixer.update(dt);
    },
  };
}
