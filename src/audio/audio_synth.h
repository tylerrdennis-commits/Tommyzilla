// Procedural sound synthesis: every sound in the game is generated in code at startup (no audio files).
// Platform independent (the Linux tests check it); the XAudio2 backend just plays these buffers.
#pragma once
#include <cstdint>
#include <vector>
#include "sound_ids.h"
namespace tz {
constexpr int SYNTH_RATE = 44100;
struct PcmBuffer { std::vector<int16_t> samples; bool loop = false; };
PcmBuffer synthSound(SoundId id);
PcmBuffer synthLoop(LoopId id);
}  // namespace tz
