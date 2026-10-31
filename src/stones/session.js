import * as THREE from 'three';
import { listStones } from './catalog.js';
import { createLoadout } from './loadout.js';
import {
  boostedPosition,
  scaledDt as scaleStep,
  senseLine,
  stoneModifiers,
} from './effects.js';
import { mountPlayReadout, mountStonePanel } from './panel.js';

/**
 * Pre-run stone picker plus the field presentation for the current loadout.
 * Movement clips stay on the locomotion controller; stones only scale time,
 * stride, size, color, and the play hint.
 */
export function createStoneSession({ menu, playHint, scene, ground, getActive }) {
  const loadout = createLoadout();
  const base = {
    background: scene.background.clone(),
    fog: scene.fog.color.clone(),
    ground: ground.material.color.clone(),
  };
  const play = mountPlayReadout(playHint);
  let panelRefresh = () => {};
  const panel = mountStonePanel(menu, loadout, (id) => {
    loadout.toggle(id);
    panelRefresh();
    play.refresh(loadout);
  });
  panelRefresh = () => panel.refresh();
  play.refresh(loadout);

  const aura = new THREE.Mesh(
    new THREE.SphereGeometry(0.42, 20, 16),
    new THREE.MeshBasicMaterial({
      color: '#f08a2a',
      transparent: true,
      opacity: 0.55,
      depthWrite: false,
    }),
  );
  aura.name = 'SoulAura';
  aura.userData.stoneAura = true;
  aura.position.set(0, 1.25, 0);
  const auraLight = new THREE.PointLight('#f08a2a', 4, 7);
  aura.add(auraLight);

  let last = null;

  function modifiers() {
    return stoneModifiers(loadout.ids());
  }

  function applyField(mods) {
    if (mods.realityField) {
      scene.background.set(mods.realityField);
      scene.fog.color.set(mods.realityFog);
      ground.material.color.set(mods.realityGround);
      return;
    }
    scene.background.copy(base.background);
    scene.fog.color.copy(base.fog);
    ground.material.color.copy(base.ground);
  }

  function applyAura(hero, show) {
    if (!show) {
      aura.removeFromParent();
      return;
    }
    if (aura.parent !== hero) hero.add(aura);
  }

  function applyPresentation(hero, dt) {
    const mods = modifiers();
    hero.scale.setScalar(mods.heroScale);
    applyField(mods);
    applyAura(hero, mods.showAura);
    play.sense.classList.toggle('hidden', !mods.showSense);
    if (mods.showSense) {
      const moved = last
        ? Math.hypot(hero.position.x - last.x, hero.position.z - last.z)
        : 0;
      const pace = dt > 0 ? moved / dt : 0;
      play.sense.textContent = senseLine({ yaw: hero.rotation.y, pace });
    }
    last = { x: hero.position.x, z: hero.position.z };
  }

  return {
    list: () => listStones(),
    selected: () => loadout.ids(),
    toggle(id) {
      const pressed = loadout.toggle(id);
      panel.refresh();
      play.refresh(loadout);
      return pressed;
    },
    modifiers,
    scaledDt(dt) {
      return scaleStep(dt, modifiers());
    },
    boostStride(hero, before) {
      const { speedMultiplier } = modifiers();
      if (speedMultiplier === 1) return;
      const next = boostedPosition(before, hero.position, speedMultiplier);
      hero.position.set(next.x, next.y, next.z);
    },
    onEnterPlay() {
      const hero = getActive();
      last = { x: hero.position.x, z: hero.position.z };
      play.refresh(loadout);
      applyPresentation(hero, 0);
    },
    onEnterMenu() {
      const hero = getActive();
      hero.scale.setScalar(1);
      applyField(stoneModifiers([]));
      applyAura(hero, false);
      play.sense.classList.add('hidden');
      last = null;
    },
    syncPlay(hero, dt) {
      applyPresentation(hero, dt);
    },
    snapshot() {
      const hero = getActive();
      const shownAura = hero.getObjectByName('SoulAura');
      return {
        selected: loadout.ids(),
        background: scene.background.getHexString(),
        fog: scene.fog.color.getHexString(),
        ground: ground.material.color.getHexString(),
        heroScale: hero.scale.x,
        heroZ: hero.position.z,
        auraVisible: Boolean(shownAura),
        senseText: play.sense.classList.contains('hidden') ? '' : play.sense.textContent,
        readoutText: play.readout.innerText,
      };
    },
  };
}
