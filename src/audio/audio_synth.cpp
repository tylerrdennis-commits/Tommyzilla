#include "audio_synth.h"
#include <cmath>
#include <cstring>
#include "../core/rng.h"
namespace tz {
const char* soundName(SoundId id) {
  static const char* names[] = {
#define TZ_NAME(n) #n,
    TZ_SOUNDS(TZ_NAME)
#undef TZ_NAME
  };
  return id >= 0 && id < SND_COUNT ? names[id] : "?";
}
SoundId soundByName(const std::string& n, SoundId fb) {
  for (int i = 0; i < SND_COUNT; i++) if (n == soundName((SoundId)i)) return (SoundId)i;
  return fb;
}

namespace {
constexpr float TAU = 6.28318530718f;
enum Wave { SINE, SQUARE, SAW, TRI };
enum Filt { LP, HP, BP };
struct Synth {
  std::vector<float> buf; Rng rng;
  explicit Synth(float seconds, uint64_t seed = 7) : buf((size_t)(seconds * SYNTH_RATE) + 1, 0.f), rng(seed) {}
  static float env(float t, float a, float peak, float dec) {
    if (t < 0) return 0;
    if (t < a) return peak * (t / a);
    float k = (t - a) / dec; if (k >= 1) return 0;
    return peak * std::pow(0.0001f / peak, k) * (1 - k * k * k);  // exponential, forced to 0 at the end
  }
  static float wave(Wave w, float ph) {
    float f = ph - std::floor(ph);
    switch (w) {
      case SINE: return std::sin(f * TAU);
      case SQUARE: return f < 0.5f ? 0.7f : -0.7f;
      case SAW: return (2 * f - 1) * 0.8f;
      case TRI: return 4 * std::fabs(f - 0.5f) - 1;
    }
    return 0;
  }
  // oscillator with exponential pitch sweep f0 -> f1 and optional vibrato
  void osc(Wave w, float f0, float f1, float start, float dur, float vol, float att = 0.01f, float vibHz = 0, float vibDepth = 0) {
    double ph = 0; size_t s0 = (size_t)(start * SYNTH_RATE), n = (size_t)((dur + att) * SYNTH_RATE);
    for (size_t i = 0; i < n && s0 + i < buf.size(); i++) {
      float t = (float)i / SYNTH_RATE, k = std::min(1.f, t / std::max(0.001f, dur));
      float f = f1 > 0 ? f0 * std::pow(f1 / f0, k) : f0;
      if (vibHz > 0) f += std::sin(t * vibHz * TAU) * vibDepth;
      ph += f / SYNTH_RATE;
      buf[s0 + i] += wave(w, (float)ph) * env(t, att, vol, dur);
    }
  }
  // white noise through a swept state-variable filter
  void noise(float start, float dur, float vol, Filt ft, float f0, float f1, float q = 1, float att = 0.01f) {
    float low = 0, band = 0; size_t s0 = (size_t)(start * SYNTH_RATE), n = (size_t)((dur + att) * SYNTH_RATE);
    for (size_t i = 0; i < n && s0 + i < buf.size(); i++) {
      float t = (float)i / SYNTH_RATE, k = std::min(1.f, t / std::max(0.001f, dur));
      float fc = f1 > 0 ? f0 * std::pow(f1 / f0, k) : f0;
      float F = 2 * std::sin(3.14159f * std::min(fc, 16000.f) / SYNTH_RATE), Q = 1.f / std::max(0.5f, q);
      float in = rng.next() * 2 - 1;
      low += F * band; float high = in - low - Q * band; band += F * high;
      float o = ft == LP ? low : ft == HP ? high : band;
      buf[s0 + i] += o * env(t, att, vol, dur);
    }
  }
  // squeaky cartoon "aaah": sawtooth with pitch bend + vibrato through two formant bandpass filters
  void squeak(float start, float base, float dur, float vol, float f1c, float f2c) {
    double ph = 0; size_t s0 = (size_t)(start * SYNTH_RATE), n = (size_t)(dur * SYNTH_RATE);
    float l1 = 0, b1 = 0, l2 = 0, b2 = 0;
    for (size_t i = 0; i < n && s0 + i < buf.size(); i++) {
      float t = (float)i / SYNTH_RATE, k = t / dur;
      float f = k < 0.3f ? base * std::pow(1.8f, k / 0.3f) : base * 1.8f * std::pow(0.7f / 1.8f, (k - 0.3f) / 0.7f);
      f += std::sin(t * 11 * TAU) * base * 0.06f;
      ph += f / SYNTH_RATE;
      float in = wave(SAW, (float)ph);
      float F1 = 2 * std::sin(3.14159f * f1c / SYNTH_RATE), F2 = 2 * std::sin(3.14159f * f2c / SYNTH_RATE), Q = 1.f / 6;
      l1 += F1 * b1; float h1 = in - l1 - Q * b1; b1 += F1 * h1;
      l2 += F2 * b2; float h2 = in - l2 - Q * b2; b2 += F2 * h2;
      buf[s0 + i] += (b1 + b2) * 0.5f * env(t, 0.03f, vol, dur);
    }
  }
  PcmBuffer finish(float gain = 1, bool loop = false) {
    PcmBuffer p; p.loop = loop; p.samples.resize(buf.size());
    for (size_t i = 0; i < buf.size(); i++) { float v = std::tanh(buf[i] * gain * 1.2f) * 0.95f; p.samples[i] = (int16_t)(v * 32767); }
    return p;
  }
};
// make a seamless loop: render `len + fade` seconds, then cross-fade the tail into the head
PcmBuffer loopify(Synth& s, float len, float fade, float gain) {
  size_t L = (size_t)(len * SYNTH_RATE), F = (size_t)(fade * SYNTH_RATE);
  if (s.buf.size() < L + F) s.buf.resize(L + F, 0);
  for (size_t i = 0; i < F; i++) { float k = (float)i / F; s.buf[i] = s.buf[i] * k + s.buf[L + i] * (1 - k); }
  s.buf.resize(L);
  return s.finish(gain, true);
}
}  // namespace

PcmBuffer synthSound(SoundId id) {
  switch (id) {
    case SND_claw: { Synth s(0.3f); s.noise(0, 0.18f, 0.5f, HP, 1500, 5000, 2); s.osc(SQUARE, 900, 300, 0, 0.08f, 0.1f); return s.finish(); }
    case SND_tail: { Synth s(0.5f); s.noise(0, 0.4f, 0.6f, BP, 300, 1400, 1.5f); s.osc(SINE, 120, 60, 0, 0.3f, 0.3f); return s.finish(); }
    case SND_stomp: { Synth s(0.7f); s.osc(SINE, 110, 30, 0, 0.5f, 0.9f); s.noise(0, 0.35f, 0.6f, LP, 600, 80); return s.finish(); }
    case SND_bonk: { Synth s(0.35f); s.osc(SINE, 320, 110, 0, 0.18f, 0.8f, 0.002f); s.osc(SQUARE, 640, 0, 0, 0.04f, 0.1f); s.noise(0, 0.05f, 0.3f, BP, 1200, 0, 2); return s.finish(); }
    case SND_boing: { Synth s(0.7f); s.osc(SINE, 160, 520, 0, 0.55f, 0.6f, 0.005f, 9, 60); return s.finish(); }
    case SND_slap: { Synth s(0.45f); s.noise(0, 0.07f, 0.8f, HP, 2000, 1000, 1); s.noise(0.2f, 0.07f, 0.8f, HP, 2400, 1000, 1); return s.finish(); }
    case SND_spin: { Synth s(1.0f); for (int i = 0; i < 4; i++) s.noise(i * 0.2f, 0.22f, 0.45f, BP, 400, 2000, 2); return s.finish(); }
    case SND_slide: { Synth s(0.8f); s.noise(0, 0.65f, 0.5f, BP, 900, 250, 1.5f, 0.05f); s.osc(SINE, 90, 50, 0, 0.6f, 0.3f); return s.finish(); }
    case SND_breath: { Synth s(1.3f); s.noise(0, 1.1f, 0.4f, BP, 2500, 900, 3, 0.05f); s.osc(TRI, 880, 1320, 0, 1.0f, 0.06f); s.osc(TRI, 1100, 1650, 0, 1.0f, 0.05f); return s.finish(); }
    case SND_beam: { Synth s(1.7f); s.osc(SINE, 300, 1400, 0, 1.4f, 0.35f, 0.1f, 18, 40); s.osc(SINE, 600, 2000, 0, 1.4f, 0.1f, 0.1f); return s.finish(); }
    case SND_bubble: { Synth s(0.5f); for (int i = 0; i < 3; i++) s.osc(SINE, 300.f + i * 150, 1400.f + i * 300, i * 0.07f, 0.09f, 0.4f, 0.003f); return s.finish(); }
    case SND_sparkle: { Synth s(0.8f, 11); const float n[] = {1568, 2093, 2637, 3136, 2349, 2794}; for (int i = 0; i < 6; i++) s.osc(TRI, n[i], 0, i * 0.06f, 0.25f, 0.12f, 0.003f); return s.finish(); }
    case SND_rainbow: { Synth s(1.9f); const float n[] = {523, 659, 784, 988}; for (int i = 0; i < 4; i++) s.osc(SINE, n[i], n[i] * 1.5f, i * 0.05f, 1.5f, 0.12f, 0.1f, 6, 8); return s.finish(); }
    case SND_frost: { Synth s(1.4f); s.noise(0, 1.2f, 0.3f, HP, 4000, 7000, 1, 0.1f); for (int i = 0; i < 5; i++) s.osc(SINE, 2400.f + i * 330, 0, i * 0.18f, 0.3f, 0.05f); return s.finish(); }
    case SND_burp: { Synth s(0.9f); s.osc(SAW, 95, 60, 0, 0.7f, 0.5f, 0.02f, 14, 12); s.noise(0, 0.5f, 0.15f, LP, 400, 0); return s.finish(0.9f); }
    case SND_peepers: { Synth s(1.2f); s.osc(SQUARE, 1200, 900, 0, 1.0f, 0.08f, 0.02f, 20, 30); s.osc(SQUARE, 1212, 905, 0, 1.0f, 0.08f, 0.02f, 21, 30); return s.finish(); }
    case SND_zap: { Synth s(0.8f, 23); for (int i = 0; i < 8; i++) s.noise(i * 0.07f + s.rng.next() * 0.03f, 0.05f, 0.6f, BP, 3000.f + s.rng.next() * 3000, 0, 4, 0.001f); s.osc(SAW, 120, 60, 0, 0.6f, 0.15f); return s.finish(); }
    case SND_honk: { Synth s(0.6f); s.osc(SQUARE, 233, 220, 0, 0.45f, 0.35f, 0.02f); s.osc(SQUARE, 294, 277, 0, 0.45f, 0.25f, 0.02f); return s.finish(0.8f); }
    case SND_roar: { Synth s(1.6f); s.osc(SAW, 180, 70, 0, 1.2f, 0.5f, 0.05f, 28, 25); s.noise(0, 1.0f, 0.35f, LP, 800, 100); return s.finish(); }
    case SND_roarCharge: { Synth s(1.4f); s.osc(SAW, 60, 110, 0, 1.2f, 0.2f, 0.1f); return s.finish(); }
    case SND_quake: { Synth s(2.0f); s.noise(0, 1.7f, 0.8f, LP, 180, 60, 1, 0.1f); s.osc(SINE, 45, 30, 0, 1.6f, 0.6f, 0.1f, 5, 4); return s.finish(); }
    case SND_meteor: { Synth s(2.6f, 5); for (int i = 0; i < 5; i++) { float t0 = i * 0.45f; s.osc(SINE, 2200, 500, t0, 0.35f, 0.08f); s.noise(t0 + 0.35f, 0.25f, 0.35f, LP, 900, 100); } return s.finish(); }
    case SND_nova: { Synth s(1.8f); s.osc(SINE, 200, 1600, 0, 0.4f, 0.25f, 0.2f); s.noise(0.4f, 1.1f, 0.7f, LP, 3000, 100); s.osc(SINE, 90, 30, 0.4f, 1.0f, 0.6f); return s.finish(); }
    case SND_tornado: { Synth s(2.2f); for (int i = 0; i < 10; i++) s.noise(i * 0.2f, 0.3f, 0.35f, BP, 300.f + (i % 3) * 300, 1600, 2, 0.08f); return s.finish(); }
    case SND_crumble: { Synth s(0.6f, 3); for (int i = 0; i < 5; i++) s.noise(i * 0.06f + s.rng.next() * 0.05f, 0.15f + s.rng.next() * 0.2f, 0.3f, LP, 500.f + s.rng.next() * 600, 120); return s.finish(); }
    case SND_collapse: { Synth s(1.9f); s.noise(0, 1.6f, 0.7f, LP, 700, 60); s.osc(SINE, 70, 25, 0, 1.4f, 0.5f); return s.finish(); }
    case SND_coin: { Synth s(0.3f); s.osc(SQUARE, 988, 0, 0, 0.07f, 0.1f, 0.002f); s.osc(SQUARE, 1319, 0, 0.07f, 0.18f, 0.1f, 0.002f); return s.finish(); }
    case SND_pop: { Synth s(0.35f); s.osc(SINE, 700, 120, 0, 0.15f, 0.4f, 0.002f); const float n[] = {523, 659, 784}; for (int i = 0; i < 3; i++) s.osc(TRI, n[i], 0, 0.05f + i * 0.04f, 0.1f, 0.08f); return s.finish(); }
    case SND_scream1: case SND_scream2: case SND_scream3: case SND_scream4: case SND_scream5: case SND_scream6: {
      int v = id - SND_scream1; float base = 520.f + v * 90, dur = 0.5f + (v % 3) * 0.15f;
      Synth s(dur + 0.1f); s.squeak(0, base, dur, 0.35f, 900.f + (v % 2) * 200, 1500.f + v * 60); return s.finish(1.3f);
    }
    case SND_hurt: { Synth s(0.25f); s.osc(SQUARE, 220, 110, 0, 0.15f, 0.12f); return s.finish(); }
    case SND_shot: { Synth s(0.15f); s.noise(0, 0.08f, 0.35f, HP, 2000, 800); return s.finish(); }
    case SND_cannon: { Synth s(0.6f); s.osc(SINE, 160, 40, 0, 0.4f, 0.6f); s.noise(0, 0.3f, 0.5f, LP, 1500, 100); return s.finish(); }
    case SND_wreck: { Synth s(0.5f); s.noise(0, 0.4f, 0.5f, BP, 1200, 300, 2); s.osc(SQUARE, 300, 80, 0, 0.25f, 0.1f); return s.finish(); }
    case SND_ui: { Synth s(0.1f); s.osc(TRI, 660, 0, 0, 0.06f, 0.15f, 0.002f); return s.finish(); }
    case SND_back: { Synth s(0.12f); s.osc(TRI, 440, 330, 0, 0.08f, 0.15f, 0.002f); return s.finish(); }
    case SND_error: { Synth s(0.3f); s.osc(SQUARE, 180, 0, 0, 0.08f, 0.12f); s.osc(SQUARE, 150, 0, 0.12f, 0.1f, 0.12f); return s.finish(); }
    case SND_unlock: { Synth s(0.7f); const float n[] = {523, 659, 784, 1047}; for (int i = 0; i < 4; i++) s.osc(TRI, n[i], 0, i * 0.1f, 0.25f, 0.2f); return s.finish(); }
    case SND_boom: { Synth s(2.7f); s.osc(SINE, 80, 20, 0, 2.5f, 1.0f); s.noise(0, 2.5f, 0.8f, LP, 2000, 50); return s.finish(); }
    case SND_jet: { Synth s(1.8f); s.noise(0, 1.5f, 0.5f, BP, 400, 2500, 1.5f, 0.6f); s.osc(SAW, 900, 500, 0, 1.4f, 0.04f, 0.5f); return s.finish(); }
    case SND_bomb: { Synth s(0.9f); s.osc(SINE, 1500, 300, 0, 0.35f, 0.08f); s.osc(SINE, 120, 35, 0.35f, 0.45f, 0.7f); s.noise(0.35f, 0.4f, 0.5f, LP, 1200, 90); return s.finish(); }
    case SND_eggCrack: { Synth s(0.3f, 9); for (int i = 0; i < 3; i++) s.noise(i * 0.05f, 0.03f, 0.8f, HP, 3000, 0, 2, 0.001f); s.osc(SINE, 900, 500, 0, 0.06f, 0.15f); return s.finish(); }
    case SND_hatch: { Synth s(1.6f); for (int i = 0; i < 8; i++) s.osc(TRI, 523.f * std::pow(2.f, i / 4.f), 0, i * 0.12f, 0.3f, 0.12f); s.noise(0, 0.25f, 0.4f, HP, 2500, 0); return s.finish(); }
    case SND_fanfare: { Synth s(2.0f); const float n[] = {523, 659, 784, 1047, 784, 1047}; const float d[] = {0.15f, 0.15f, 0.15f, 0.4f, 0.15f, 0.7f};
      float t = 0; for (int i = 0; i < 6; i++) { s.osc(TRI, n[i], 0, t, d[i], 0.2f); s.osc(SQUARE, n[i] / 2, 0, t, d[i], 0.05f); t += d[i] * 0.9f; } return s.finish(); }
    case SND_splash: { Synth s(0.7f); s.noise(0, 0.5f, 0.5f, LP, 1800, 200); return s.finish(); }
    case SND_throwWhoosh: { Synth s(0.5f); s.noise(0, 0.4f, 0.5f, BP, 300, 1800, 2, 0.05f); return s.finish(); }
    case SND_gulp: { Synth s(0.45f); s.osc(SINE, 260, 90, 0, 0.18f, 0.6f, 0.005f); s.osc(SINE, 520, 180, 0.16f, 0.14f, 0.35f, 0.003f); const float n[] = {784, 1047}; for (int i = 0; i < 2; i++) s.osc(TRI, n[i], 0, 0.28f + i * 0.06f, 0.1f, 0.12f); return s.finish(); }
    case SND_blaster: { Synth s(0.3f); s.osc(SQUARE, 1800, 400, 0, 0.16f, 0.25f, 0.002f); s.osc(TRI, 2400, 900, 0.02f, 0.14f, 0.2f, 0.002f); return s.finish(); }
    case SND_stun: { Synth s(0.7f, 13); const float n[] = {2093, 2637, 3136, 2637, 2093}; for (int i = 0; i < 5; i++) s.osc(TRI, n[i], 0, i * 0.09f, 0.18f, 0.12f, 0.003f); s.osc(SINE, 90, 40, 0, 0.3f, 0.5f); return s.finish(); }
    case SND_tutorialDone: { Synth s(1.8f); const float n[] = {523, 659, 784, 1047, 1319, 1568}; for (int i = 0; i < 6; i++) { s.osc(TRI, n[i], 0, i * 0.11f, 0.5f, 0.18f); s.osc(SINE, n[i] * 2, 0, i * 0.11f + 0.05f, 0.3f, 0.06f); } return s.finish(); }
    default: break;
  }
  Synth s(0.1f); s.osc(SINE, 440, 0, 0, 0.05f, 0.1f); return s.finish();
}

PcmBuffer synthLoop(LoopId id) {
  switch (id) {
    case LOOP_SIREN: { Synth s(2.1f); double ph = 0; for (size_t i = 0; i < s.buf.size(); i++) { float t = (float)i / SYNTH_RATE; float f = 750 + 250 * std::sin(t * TAU * 1.0f); ph += f / SYNTH_RATE; s.buf[i] = Synth::wave(SQUARE, (float)ph) * 0.3f; }
      return loopify(s, 2.0f, 0.05f, 1); }
    case LOOP_HELI: { Synth s(1.1f, 4); s.noise(0, 1.2f, 0.9f, LP, 300, 0, 1, 0.001f);
      for (size_t i = 0; i < s.buf.size(); i++) { float t = (float)i / SYNTH_RATE; s.buf[i] *= 0.5f + 0.5f * (std::sin(t * TAU * 13) > 0 ? 1.f : 0.2f); }
      return loopify(s, 1.0f, 0.05f, 1.5f); }
    case LOOP_NUKE: { Synth s(2.1f); double ph = 0; for (size_t i = 0; i < s.buf.size(); i++) { float t = (float)i / SYNTH_RATE; float k = std::fmod(t * 0.5f, 1.f); float f = 500 + 300 * (2 * k - 1); ph += f / SYNTH_RATE; s.buf[i] = Synth::wave(SAW, (float)ph) * 0.3f; }
      return loopify(s, 2.0f, 0.05f, 1); }
    case LOOP_JET: { Synth s(2.1f, 8); s.noise(0, 2.2f, 0.6f, BP, 900, 0, 0.8f, 0.001f); s.noise(0, 2.2f, 0.4f, LP, 200, 0, 1, 0.001f); return loopify(s, 2.0f, 0.1f, 1.2f); }
    default: break;
  }
  Synth s(0.5f); return loopify(s, 0.4f, 0.05f, 1);
}
}  // namespace tz
