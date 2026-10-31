import { startLocomotion } from '../animation/locomotion.js';

const SPEED = 4;

export function createCharacterController(model) {
  const loco = startLocomotion(model, { moving: false });

  return {
    clipName() {
      return loco.clipName();
    },
    setMoving(moving) {
      loco.setMoving(moving);
    },
    update(dt, input, { slide = true } = {}) {
      const forward = input.forward ? 1 : 0;
      const back = input.back ? 1 : 0;
      const left = input.left ? 1 : 0;
      const right = input.right ? 1 : 0;
      const moving = Boolean(forward || back || left || right);
      loco.setMoving(moving);

      if (slide && moving) {
        const dx = (right - left) * SPEED * dt;
        const dz = (forward - back) * SPEED * dt;
        model.scene.position.x += dx;
        model.scene.position.z += dz;
        model.scene.rotation.y = Math.atan2(dx, dz);
      }

      loco.update(dt);
    },
  };
}
