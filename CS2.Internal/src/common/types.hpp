#pragma once

#include <cmath>

namespace vectra {
struct Vec2 { float x{}, y{}; };
struct Vec3 { float x{}, y{}, z{}; };
struct Angles { float pitch{}, yaw{}; };

inline bool Finite(const Vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline bool Finite(const Angles& v) { return std::isfinite(v.pitch) && std::isfinite(v.yaw); }
inline float Length(const Vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
}
