const TPOSE_NAME = /t[\s_-]?pose|bindpose|bind|reference/i;
const WALK_NAME = /walk|run|jog|locomotion/i;
const IDLE_NAME = /idle|stand|breath/i;
const ATTACK_NAME = /attack|hit|slash|roar|shoot|claw|stomp/i;

/**
 * GLTF rigs often export a static T-pose as animations[0]. Playing that clip
 * (or any bind/reference clip) leaves the character stuck in the bind pose.
 * Prefer a named locomotion clip, then a named idle, and never a T-pose when
 * another clip exists.
 */
export function selectLocomotionClip(clips, { moving }) {
  let best = null;
  let bestScore = -Infinity;

  for (const clip of clips) {
    const score = scoreClip(clip, moving);
    if (score > bestScore) {
      bestScore = score;
      best = clip;
    }
  }

  return best;
}

function scoreClip(clip, moving) {
  const name = clip?.name ?? '';
  let score = 0;

  if (TPOSE_NAME.test(name)) score -= 1000;
  if (moving && WALK_NAME.test(name)) score += 200;
  if (!moving && IDLE_NAME.test(name)) score += 200;
  if (!moving && WALK_NAME.test(name)) score += 20;
  if (ATTACK_NAME.test(name)) score -= 30;

  return score;
}
