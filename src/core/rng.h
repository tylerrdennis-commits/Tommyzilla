// Deterministic RNG (xorshift-mul / splitmix based) so layouts and campaign shuffles are reproducible.
#pragma once
#include <cstdint>
#include <vector>
#include <utility>
namespace tz {
struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) : s(seed ? seed : 0x1234567ull) {}
  uint64_t nextU64() { uint64_t z = (s += 0x9E3779B97F4A7C15ull); z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); }
  float next() { return (float)((nextU64() >> 40) * (1.0 / 16777216.0)); }        // [0,1)
  float range(float a, float b) { return a + next() * (b - a); }
  int irange(int a, int b) { return a + (int)(nextU64() % (uint64_t)(b - a + 1)); } // inclusive
  template <class T> const T& pick(const std::vector<T>& v) { return v[(size_t)(nextU64() % v.size())]; }
  template <class T> void shuffle(std::vector<T>& v) { for (size_t i = v.size(); i > 1; i--) { size_t j = (size_t)(nextU64() % i); std::swap(v[i - 1], v[j]); } }
};
// global "cosmetic" random (debris spin etc.) - not used for anything that must be reproducible
float frand();
float frange(float a, float b);
}  // namespace tz
