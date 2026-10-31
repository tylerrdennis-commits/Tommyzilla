// Small vector / matrix helpers (row-vector convention: p' = p * M, translation in row 3).
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace tz {
constexpr float PI = 3.14159265358979f;
struct Vec3 {
  float x = 0, y = 0, z = 0;
  Vec3() = default;
  constexpr Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
  Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
  Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
};
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(const Vec3& a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(const Vec3& a) { float l = length(a); return l > 1e-6f ? a * (1.0f / l) : Vec3(0, 0, 0); }
inline float dist2D(float ax, float az, float bx, float bz) { return std::hypot(ax - bx, az - bz); }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float wrapAngle(float a) { return std::atan2(std::sin(a), std::cos(a)); }

struct Mat4 {
  float m[4][4];
  static Mat4 identity() { Mat4 r{}; for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) r.m[i][j] = i == j ? 1.f : 0.f; return r; }
  static Mat4 translation(float x, float y, float z) { Mat4 r = identity(); r.m[3][0] = x; r.m[3][1] = y; r.m[3][2] = z; return r; }
  static Mat4 scale(float x, float y, float z) { Mat4 r = identity(); r.m[0][0] = x; r.m[1][1] = y; r.m[2][2] = z; return r; }
  static Mat4 rotX(float a) { Mat4 r = identity(); float c = std::cos(a), s = std::sin(a); r.m[1][1] = c; r.m[1][2] = s; r.m[2][1] = -s; r.m[2][2] = c; return r; }
  static Mat4 rotY(float a) { Mat4 r = identity(); float c = std::cos(a), s = std::sin(a); r.m[0][0] = c; r.m[0][2] = -s; r.m[2][0] = s; r.m[2][2] = c; return r; }
  static Mat4 rotZ(float a) { Mat4 r = identity(); float c = std::cos(a), s = std::sin(a); r.m[0][0] = c; r.m[0][1] = s; r.m[1][0] = -s; r.m[1][1] = c; return r; }
  Mat4 operator*(const Mat4& o) const {
    Mat4 r{};
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) { float s = 0; for (int k = 0; k < 4; k++) s += m[i][k] * o.m[k][j]; r.m[i][j] = s; }
    return r;
  }
  Vec3 transformPoint(const Vec3& p) const { return {p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0], p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1], p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2]}; }
  // S * Rz * Rx * Ry * T  (scale, then roll, pitch, yaw, then translate)
  static Mat4 trs(const Vec3& p, float yaw, float pitch, float roll, const Vec3& s) { return scale(s.x, s.y, s.z) * rotZ(roll) * rotX(pitch) * rotY(yaw) * translation(p.x, p.y, p.z); }
  static Mat4 lookAtLH(const Vec3& eye, const Vec3& at, const Vec3& up) {
    Vec3 z = normalize(at - eye), x = normalize(cross(up, z)), y = cross(z, x);
    Mat4 r = identity();
    r.m[0][0] = x.x; r.m[1][0] = x.y; r.m[2][0] = x.z; r.m[3][0] = -dot(x, eye);
    r.m[0][1] = y.x; r.m[1][1] = y.y; r.m[2][1] = y.z; r.m[3][1] = -dot(y, eye);
    r.m[0][2] = z.x; r.m[1][2] = z.y; r.m[2][2] = z.z; r.m[3][2] = -dot(z, eye);
    return r;
  }
  static Mat4 perspectiveLH(float fovY, float aspect, float zn, float zf) {
    Mat4 r{}; float h = 1.f / std::tan(fovY * 0.5f), w = h / aspect;
    r.m[0][0] = w; r.m[1][1] = h; r.m[2][2] = zf / (zf - zn); r.m[2][3] = 1; r.m[3][2] = -zn * zf / (zf - zn);
    return r;
  }
  // right-handed view (matches the prototype's three.js world: x east, y up, z south), D3D clip z in [0,1]
  static Mat4 lookAtRH(const Vec3& eye, const Vec3& at, const Vec3& up) {
    Vec3 z = normalize(eye - at), x = normalize(cross(up, z)), y = cross(z, x);
    Mat4 r = identity();
    r.m[0][0] = x.x; r.m[1][0] = x.y; r.m[2][0] = x.z; r.m[3][0] = -dot(x, eye);
    r.m[0][1] = y.x; r.m[1][1] = y.y; r.m[2][1] = y.z; r.m[3][1] = -dot(y, eye);
    r.m[0][2] = z.x; r.m[1][2] = z.y; r.m[2][2] = z.z; r.m[3][2] = -dot(z, eye);
    return r;
  }
  static Mat4 perspectiveRH(float fovY, float aspect, float zn, float zf) {
    Mat4 r{}; float h = 1.f / std::tan(fovY * 0.5f), w = h / aspect;
    r.m[0][0] = w; r.m[1][1] = h; r.m[2][2] = zf / (zn - zf); r.m[2][3] = -1; r.m[3][2] = zn * zf / (zn - zf);
    return r;
  }
};
// view frustum (6 planes, from a row-vector view*proj matrix with D3D z range) for sphere culling
struct Frustum {
  float p[6][4];
  void fromMatrix(const Mat4& M) {
    auto col = [&](int c, float* o) { for (int i = 0; i < 4; i++) o[i] = M.m[i][c]; };
    float c0[4], c1[4], c2[4], c3[4]; col(0, c0); col(1, c1); col(2, c2); col(3, c3);
    for (int i = 0; i < 4; i++) { p[0][i] = c3[i] + c0[i]; p[1][i] = c3[i] - c0[i]; p[2][i] = c3[i] + c1[i]; p[3][i] = c3[i] - c1[i]; p[4][i] = c2[i]; p[5][i] = c3[i] - c2[i]; }
    for (auto& q : p) { float l = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]); if (l > 0) for (int i = 0; i < 4; i++) q[i] /= l; }
  }
  bool sphere(const Vec3& c, float r) const { for (auto& q : p) if (q[0] * c.x + q[1] * c.y + q[2] * c.z + q[3] < -r) return false; return true; }
};
// colour helpers
struct Color { float r = 1, g = 1, b = 1, a = 1; };
inline Color hexColor(uint32_t hex, float a = 1.f) { return {((hex >> 16) & 255) / 255.f, ((hex >> 8) & 255) / 255.f, (hex & 255) / 255.f, a}; }
inline Color scaleColor(Color c, float s) { return {std::min(1.f, c.r * s), std::min(1.f, c.g * s), std::min(1.f, c.b * s), c.a}; }
inline Color mixColor(Color a, Color b, float t) { return {lerpf(a.r, b.r, t), lerpf(a.g, b.g, t), lerpf(a.b, b.b, t), lerpf(a.a, b.a, t)}; }
}  // namespace tz
