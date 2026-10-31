// Clip playback for one model instance: play / loop / crossfade ("blend-lite": the previous clip keeps playing
// while its weight fades out over the fade time). Produces a Pose (node globals + skin palettes).
#pragma once
#include <vector>
#include "model.h"
namespace tz {
namespace mdl {
// Samples clip at time t (seconds, clamped) into locals (which should start as the rest pose).
void sampleClip(const AnimationClip& clip, float t, std::vector<Transform>& locals);

class Animator {
 public:
  void bind(const Model* m);           // resets to the rest pose
  const Model* model() const { return m_; }
  // Start a clip (index into Model::clips). Same clip already playing -> no restart unless restart = true.
  bool play(int clip, bool loop = true, float fade = 0.25f, float speed = 1.0f, bool restart = false);
  bool play(const std::string& clipName, bool loop = true, float fade = 0.25f, float speed = 1.0f);
  void stop(float fade = 0.25f);       // fade back to the rest pose
  void update(float dt);               // advance time and rebuild the pose
  const Pose& pose() const { return pose_; }
  int current() const { return cur_.clip; }
  float time() const { return cur_.t; }
  float fadeWeight() const { return fadeDur_ > 0 ? clampf(fadeT_ / fadeDur_, 0, 1) : 1.f; }  // weight of the current clip
  bool finished() const;               // non-looping clip reached its end
 private:
  struct Layer { int clip = -1; float t = 0, speed = 1; bool loop = true; };
  void advance(Layer& l, float dt);
  const Model* m_ = nullptr;
  Layer cur_, prev_;
  float fadeT_ = 0, fadeDur_ = 0;
  std::vector<Transform> rest_, a_, b_;
  Pose pose_;
};
}  // namespace mdl
}  // namespace tz
