#include "audio_xaudio2.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xaudio2.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "audio_synth.h"

namespace tz {
namespace {
constexpr int kVoices = 28;
WAVEFORMATEX monoFormat() {
  WAVEFORMATEX f = {};
  f.wFormatTag = WAVE_FORMAT_PCM; f.nChannels = 1; f.nSamplesPerSec = SYNTH_RATE; f.wBitsPerSample = 16;
  f.nBlockAlign = 2; f.nAvgBytesPerSec = SYNTH_RATE * 2;
  return f;
}
}  // namespace

struct XAudioPlayer::Impl {
  IXAudio2* xa = nullptr; IXAudio2MasteringVoice* master = nullptr;
  std::vector<PcmBuffer> sounds, loops;
  struct Voice { IXAudio2SourceVoice* v = nullptr; UINT64 started = 0; int id = -1; };
  Voice voices[kVoices]; UINT64 serial = 0;
  IXAudio2SourceVoice* loopVoices[LOOP_COUNT] = {}; float loopCur[LOOP_COUNT] = {};
  void release() {
    for (auto& v : voices) if (v.v) { v.v->DestroyVoice(); v.v = nullptr; }
    for (auto& v : loopVoices) if (v) { v->Stop(0); v->DestroyVoice(); v = nullptr; }
    if (master) { master->DestroyVoice(); master = nullptr; }
    if (xa) { xa->StopEngine(); xa->Release(); xa = nullptr; }
  }
};

XAudioPlayer::XAudioPlayer() : d(new Impl) {}
XAudioPlayer::~XAudioPlayer() { shutdown(); delete d; }
bool XAudioPlayer::ok() const { return d->xa != nullptr; }

bool XAudioPlayer::init(std::string& err) {
  auto fail = [&](const char* what, HRESULT hr) { char b[160]; std::snprintf(b, sizeof b, "%s failed (HRESULT 0x%08lX) - sound disabled", what, (unsigned long)hr); err = b; d->release(); return false; };
  HRESULT hr = XAudio2Create(&d->xa, 0, XAUDIO2_DEFAULT_PROCESSOR);
  if (FAILED(hr)) { d->xa = nullptr; return fail("XAudio2Create", hr); }
  hr = d->xa->CreateMasteringVoice(&d->master);
  if (FAILED(hr)) { d->master = nullptr; return fail("CreateMasteringVoice (no audio device?)", hr); }
  d->sounds.resize(SND_COUNT); d->loops.resize(LOOP_COUNT);
  for (int i = 0; i < SND_COUNT; i++) d->sounds[i] = synthSound((SoundId)i);
  for (int i = 0; i < LOOP_COUNT; i++) d->loops[i] = synthLoop((LoopId)i);
  WAVEFORMATEX fmt = monoFormat();
  for (auto& v : d->voices) {
    hr = d->xa->CreateSourceVoice(&v.v, &fmt, 0, 4.0f);
    if (FAILED(hr)) { v.v = nullptr; return fail("CreateSourceVoice", hr); }
  }
  for (int i = 0; i < LOOP_COUNT; i++) {
    hr = d->xa->CreateSourceVoice(&d->loopVoices[i], &fmt, 0, 2.0f);
    if (FAILED(hr)) { d->loopVoices[i] = nullptr; return fail("CreateSourceVoice(loop)", hr); }
    XAUDIO2_BUFFER b = {};
    b.AudioBytes = (UINT32)(d->loops[i].samples.size() * sizeof(int16_t)); b.pAudioData = (const BYTE*)d->loops[i].samples.data();
    b.LoopCount = XAUDIO2_LOOP_INFINITE;
    d->loopVoices[i]->SetVolume(0);
    d->loopVoices[i]->SubmitSourceBuffer(&b);
    d->loopVoices[i]->Start(0);
  }
  return true;
}

void XAudioPlayer::play(const SoundEvent& e) {
  if (!d->xa || e.id < 0 || e.id >= SND_COUNT) return;
  const PcmBuffer& pcm = d->sounds[e.id]; if (pcm.samples.empty()) return;
  // free voice, else steal the oldest one
  Impl::Voice* pick = nullptr; Impl::Voice* oldest = nullptr;
  for (auto& v : d->voices) {
    if (!v.v) continue;
    XAUDIO2_VOICE_STATE st; v.v->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    if (st.BuffersQueued == 0) { pick = &v; break; }
    if (!oldest || v.started < oldest->started) oldest = &v;
  }
  if (!pick) { pick = oldest; if (!pick) return; pick->v->Stop(0); pick->v->FlushSourceBuffers(); }
  XAUDIO2_BUFFER b = {};
  b.AudioBytes = (UINT32)(pcm.samples.size() * sizeof(int16_t)); b.pAudioData = (const BYTE*)pcm.samples.data(); b.Flags = XAUDIO2_END_OF_STREAM;
  pick->v->SetFrequencyRatio(std::min(3.9f, std::max(0.25f, e.pitch > 0 ? e.pitch : 1.f)));
  pick->v->SetVolume(std::min(2.f, std::max(0.f, e.volume)));
  if (SUCCEEDED(pick->v->SubmitSourceBuffer(&b))) { pick->v->Start(0); pick->started = ++d->serial; pick->id = e.id; }
}

void XAudioPlayer::setLoopVolumes(const float* vol, float dt) {
  if (!d->xa) return;
  float k = std::min(1.f, dt * 6);
  for (int i = 0; i < LOOP_COUNT; i++) {
    float target = std::min(1.5f, std::max(0.f, vol[i]));
    d->loopCur[i] += (target - d->loopCur[i]) * k;
    if (d->loopVoices[i]) d->loopVoices[i]->SetVolume(d->loopCur[i] < 0.002f ? 0.f : d->loopCur[i]);
  }
}

void XAudioPlayer::setMaster(float volume, bool muted) {
  if (d->master) d->master->SetVolume(muted ? 0.f : std::min(1.f, std::max(0.f, volume)));
}

void XAudioPlayer::shutdown() { if (d) d->release(); }
}  // namespace tz
