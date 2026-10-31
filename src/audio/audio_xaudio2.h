// XAudio2 backend: plays the procedurally synthesized buffers from audio_synth.cpp. Never fatal: if XAudio2 is
// unavailable the game simply runs silent.
#pragma once
#include <string>
#include <vector>
#include "sound_ids.h"
namespace tz {
class XAudioPlayer {
 public:
  XAudioPlayer();
  ~XAudioPlayer();
  bool init(std::string& err);           // synthesizes every sound and creates the voices
  void play(const SoundEvent& e);
  void setLoopVolumes(const float* vol, float dt);  // LOOP_COUNT entries, smoothed
  void setMaster(float volume, bool muted);
  void shutdown();
  bool ok() const;
 private:
  struct Impl;
  Impl* d;
};
}  // namespace tz
