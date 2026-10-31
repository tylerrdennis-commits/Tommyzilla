/**
 * Boss fight scripts. Each id is one motion. Encounters assign these 1:1 so
 * two different bosses never share a script.
 * Positions are offsets from the hero. The side camera only keeps a boss in
 * frame when that offset stays just ahead and on the far side of the hero.
 */

function charge(spec, time) {
  const along = Math.sin(time * spec.speed) * spec.amplitude;
  const ahead = Math.cos(time * spec.speed);
  if (spec.axis === 'z') {
    return { x: spec.x, z: spec.z + along, yaw: ahead >= 0 ? 0 : Math.PI, moving: true };
  }
  return {
    x: spec.x + along,
    z: spec.z,
    yaw: ahead >= 0 ? Math.PI / 2 : -Math.PI / 2,
    moving: true,
  };
}

function orbit(spec, time) {
  const angle = time * spec.speed + spec.phase;
  return {
    x: spec.cx + Math.cos(angle) * spec.radius,
    z: spec.cz + Math.sin(angle) * spec.radius,
    yaw: angle + (spec.speed >= 0 ? Math.PI / 2 : -Math.PI / 2),
    moving: true,
  };
}

function strafe(spec, time) {
  const along = Math.sin(time * spec.speed + spec.phase) * spec.amplitude;
  const ahead = Math.cos(time * spec.speed + spec.phase);
  return {
    x: spec.cx + along,
    z: spec.z,
    yaw: ahead >= 0 ? Math.PI / 2 : -Math.PI / 2,
    moving: true,
  };
}

function lunge(spec, time) {
  const cycle = spec.hold + spec.dash;
  const local = ((time % cycle) + cycle) % cycle;
  const moving = local >= spec.hold;
  const progress = moving ? (local - spec.hold) / spec.dash : 0;
  const offset = Math.sin(progress * Math.PI) * spec.reach;
  if (spec.axis === 'z') {
    return { x: spec.x, z: spec.z + offset, yaw: 0, moving };
  }
  return { x: spec.x + offset, z: spec.z, yaw: Math.PI / 2, moving };
}

function spiral(spec, time) {
  const angle = time * spec.speed + spec.phase;
  const radius = spec.base + Math.sin(time * spec.pulse) * spec.span;
  return {
    x: spec.cx + Math.cos(angle) * radius,
    z: spec.cz + Math.sin(angle) * radius,
    yaw: angle + Math.PI / 2,
    moving: true,
  };
}

function weave(spec, time) {
  const along = Math.sin(time * spec.along + spec.phase) * spec.reach;
  const side = Math.sin(time * spec.freq) * spec.amplitude;
  if (spec.axis === 'z') {
    return {
      x: spec.cx + side,
      z: spec.cz + along,
      yaw: Math.atan2(side, along || 0.001),
      moving: true,
    };
  }
  return {
    x: spec.cx + along,
    z: spec.cz + side,
    yaw: Math.atan2(along, side || 0.001),
    moving: true,
  };
}

function pincer(spec, time) {
  const size = spec.size;
  const loop = spec.diamond
    ? [[0, -size], [size, 0], [0, size], [-size, 0]]
    : [[-size, -size], [size, -size], [size, size], [-size, size]];
  const u = ((time * spec.speed + spec.phase) % 4 + 4) % 4;
  const edge = Math.floor(u) % 4;
  const f = u - Math.floor(u);
  const from = loop[edge];
  const to = loop[(edge + 1) % 4];
  const x = spec.cx + from[0] + (to[0] - from[0]) * f;
  const z = spec.cz + from[1] + (to[1] - from[1]) * f;
  return { x, z, yaw: Math.atan2(to[0] - from[0], to[1] - from[1]), moving: true };
}

function stomp(spec, time) {
  const cycle = spec.pause + spec.step;
  const local = ((time % cycle) + cycle) % cycle;
  const moving = local >= spec.pause;
  const progress = moving ? (local - spec.pause) / spec.step : 0;
  const laps = Math.floor(time / cycle) + progress;
  if (spec.axis === 'orbit') {
    const angle = laps * spec.turn;
    return {
      x: spec.cx + Math.cos(angle) * spec.reach,
      z: spec.cz + Math.sin(angle) * spec.reach,
      yaw: angle,
      moving,
    };
  }
  const span = spec.reach * 2;
  const travel = (laps * spec.stride) % span;
  return { x: spec.x, z: spec.z + travel - spec.reach, yaw: 0, moving };
}

const PATTERNS = {
  'lane-charge': {
    label: 'Charges straight down the lane',
    sample: (time) => charge({ axis: 'z', amplitude: 0.7, speed: 1.7, x: -1.7, z: 1.45 }, time),
  },
  'cross-charge': {
    label: 'Charges across the lane',
    sample: (time) => charge({ axis: 'x', amplitude: 1.15, speed: 1.15, x: -1.15, z: 1.7 }, time),
  },
  'wide-orbit': {
    label: 'Circles a wide ring',
    sample: (time) => orbit({ cx: -1.35, cz: 1.45, radius: 0.85, speed: 0.9, phase: 0.4 }, time),
  },
  'reverse-orbit': {
    label: 'Circles a tight ring backward',
    sample: (time) => orbit({ cx: -0.85, cz: 1.15, radius: 0.55, speed: -1.35, phase: 2.1 }, time),
  },
  'strafe-cut': {
    label: 'Cuts across the near edge',
    sample: (time) => strafe({ cx: -1.05, amplitude: 1.2, speed: 1.4, phase: 0.2, z: 0.7 }, time),
  },
  'deep-strafe': {
    label: 'Cuts across the far edge',
    sample: (time) => strafe({ cx: -1.55, amplitude: 0.95, speed: 0.8, phase: 1.7, z: 2.15 }, time),
  },
  'pause-lunge': {
    label: 'Holds still, then lunges',
    sample: (time) => lunge({ axis: 'z', hold: 1.15, dash: 0.55, reach: 1.15, x: -1.25, z: 0.85 }, time),
  },
  'side-lunge': {
    label: 'Holds still, then lunges sideways',
    sample: (time) => lunge({ axis: 'x', hold: 0.7, dash: 0.45, reach: 1.35, x: -2.15, z: 1.55 }, time),
  },
  'tight-spiral': {
    label: 'Spirals outward from the center',
    sample: (time) => spiral({ cx: -1.2, cz: 1.35, base: 0.55, span: 0.35, speed: 1.9, pulse: 0.8, phase: 0.3 }, time),
  },
  'open-spiral': {
    label: 'Spirals inward from the rim',
    sample: (time) => spiral({ cx: -0.9, cz: 1.45, base: 0.6, span: 0.35, speed: -1.05, pulse: 0.5, phase: 2.4 }, time),
  },
  'sine-weave': {
    label: 'Weaves forward along a sine path',
    sample: (time) => weave({
      axis: 'z', cx: -1.25, cz: 1.4, reach: 0.75, amplitude: 0.7, along: 1.1, freq: 1.7, phase: 0.2,
    }, time),
  },
  'cross-weave': {
    label: 'Weaves sideways along a sine path',
    sample: (time) => weave({
      axis: 'x', cx: -1.05, cz: 1.65, reach: 0.85, amplitude: 0.55, along: 1.3, freq: 0.85, phase: 1.15,
    }, time),
  },
  'corner-pincer': {
    label: 'Closes in along the corners',
    sample: (time) => pincer({ cx: -1.25, cz: 1.4, size: 0.7, speed: 0.7, phase: 0.3, diamond: false }, time),
  },
  'diamond-pincer': {
    label: 'Closes in along a diamond',
    sample: (time) => pincer({ cx: -0.9, cz: 1.7, size: 0.75, speed: 1.05, phase: 1.5, diamond: true }, time),
  },
  'beat-stomp': {
    label: 'Stomps, then steps ahead',
    sample: (time) => stomp({
      axis: 'line', pause: 0.9, step: 0.45, reach: 0.7, stride: 0.55, x: -1.45, z: 1.35,
    }, time),
  },
  'ring-stomp': {
    label: 'Stomps around a ring',
    sample: (time) => stomp({
      axis: 'orbit', pause: 0.75, step: 0.4, reach: 0.8, turn: 0.85, cx: -1.15, cz: 1.4,
    }, time),
  },
};

export function patternLabel(id) {
  const pattern = PATTERNS[id];
  if (!pattern) throw new Error(`Unknown pattern: ${id}`);
  return pattern.label;
}

export function samplePattern(id, time) {
  const pattern = PATTERNS[id];
  if (!pattern) throw new Error(`Unknown pattern: ${id}`);
  return pattern.sample(time);
}
