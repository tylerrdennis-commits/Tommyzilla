#include "animator.h"
#include <algorithm>
#include <cmath>
namespace tz {
namespace mdl {

void sampleClip(const AnimationClip& clip, float t, std::vector<Transform>& locals) {
  for (const Channel& ch : clip.channels) {
    if (ch.node < 0 || ch.node >= (int)locals.size() || ch.times.empty()) continue;
    const int comps = ch.path == Path::Rotation ? 4 : 3;
    const auto& T = ch.times;
    size_t k1 = (size_t)(std::upper_bound(T.begin(), T.end(), t) - T.begin());
    size_t k0 = k1 == 0 ? 0 : k1 - 1; if (k1 >= T.size()) k1 = T.size() - 1;
    float f = 0;
    if (k1 != k0 && !ch.step) f = clampf((t - T[k0]) / std::max(1e-6f, T[k1] - T[k0]), 0, 1);
    const float* a = &ch.values[k0 * (size_t)comps];
    const float* b = &ch.values[k1 * (size_t)comps];
    Transform& x = locals[(size_t)ch.node];
    if (ch.path == Path::Rotation) x.r = slerpQ({a[0], a[1], a[2], a[3]}, {b[0], b[1], b[2], b[3]}, f);
    else {
      Vec3 v(lerpf(a[0], b[0], f), lerpf(a[1], b[1], f), lerpf(a[2], b[2], f));
      if (ch.path == Path::Translation) x.t = v; else x.s = v;
    }
  }
}

void Animator::bind(const Model* m) {
  m_ = m; cur_ = Layer(); prev_ = Layer(); fadeT_ = fadeDur_ = 0;
  rest_ = m ? m->restLocals() : std::vector<Transform>();
  pose_ = Pose();
  if (m) computePose(*m, rest_, pose_);
}
bool Animator::play(int clip, bool loop, float fade, float speed, bool restart) {
  if (!m_ || clip < 0 || clip >= (int)m_->clips.size()) return false;
  if (clip == cur_.clip && !restart) { cur_.loop = loop; cur_.speed = speed; return true; }
  prev_ = cur_;
  cur_.clip = clip; cur_.t = 0; cur_.loop = loop; cur_.speed = speed;
  fadeDur_ = prev_.clip >= 0 || fade > 0 ? std::max(0.f, fade) : 0; fadeT_ = 0;
  return true;
}
bool Animator::play(const std::string& name, bool loop, float fade, float speed) { return m_ && play(m_->findClip(name), loop, fade, speed); }
void Animator::stop(float fade) { prev_ = cur_; cur_ = Layer(); fadeDur_ = fade; fadeT_ = 0; }
float Animator::phase() const {
  if (!m_ || cur_.clip < 0) return 0;
  float d = m_->clips[(size_t)cur_.clip].duration;
  return d > 0 ? std::fmod(cur_.t, d) / d : 0;
}
void Animator::setPhase(float p) {
  if (!m_ || cur_.clip < 0) return;
  cur_.t = clampf(p, 0, 1) * m_->clips[(size_t)cur_.clip].duration;
}
bool Animator::finished() const {
  if (!m_ || cur_.clip < 0 || cur_.loop) return false;
  return cur_.t >= m_->clips[(size_t)cur_.clip].duration;
}
void Animator::advance(Layer& l, float dt) {
  if (l.clip < 0) return;
  float d = m_->clips[(size_t)l.clip].duration;
  l.t += dt * l.speed;
  if (d <= 0) { l.t = 0; return; }
  if (l.loop) { l.t = std::fmod(l.t, d); if (l.t < 0) l.t += d; }
  else l.t = clampf(l.t, 0, d);
}
void Animator::update(float dt) {
  if (!m_) return;
  advance(cur_, dt); advance(prev_, dt);
  fadeT_ += dt;
  a_ = rest_;
  if (cur_.clip >= 0) sampleClip(m_->clips[(size_t)cur_.clip], cur_.t, a_);
  float w = fadeWeight();
  if (w < 1.f) {
    b_ = rest_;
    if (prev_.clip >= 0) sampleClip(m_->clips[(size_t)prev_.clip], prev_.t, b_);
    for (size_t i = 0; i < a_.size(); i++) a_[i] = lerpTransform(b_[i], a_[i], w);
  } else prev_.clip = -1;
  computePose(*m_, a_, pose_);
}

}  // namespace mdl
}  // namespace tz
