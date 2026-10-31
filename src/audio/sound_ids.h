// Sound effect identifiers shared by the game (emits events) and the audio backend (plays synthesized buffers).
#pragma once
#include <string>
namespace tz {
#define TZ_SOUNDS(X) \
  X(claw) X(tail) X(stomp) X(bonk) X(boing) X(slap) X(spin) X(slide) \
  X(breath) X(beam) X(bubble) X(sparkle) X(rainbow) X(frost) X(burp) X(peepers) X(zap) X(honk) \
  X(roar) X(roarCharge) X(quake) X(meteor) X(nova) X(tornado) \
  X(crumble) X(collapse) X(coin) X(pop) X(scream1) X(scream2) X(scream3) X(scream4) X(scream5) X(scream6) \
  X(hurt) X(shot) X(cannon) X(wreck) X(ui) X(unlock) X(boom) X(jet) X(bomb) X(eggCrack) X(hatch) X(fanfare) \
  X(back) X(error) X(splash) X(throwWhoosh) X(gulp) X(blaster) X(stun) X(tutorialDone)
enum SoundId {
#define TZ_ENUM(n) SND_##n,
  TZ_SOUNDS(TZ_ENUM)
#undef TZ_ENUM
  SND_COUNT
};
enum LoopId { LOOP_SIREN, LOOP_HELI, LOOP_NUKE, LOOP_JET, LOOP_COUNT };
const char* soundName(SoundId id);
SoundId soundByName(const std::string& n, SoundId fallback);
struct SoundEvent { SoundId id; float volume, pitch; };
}  // namespace tz
