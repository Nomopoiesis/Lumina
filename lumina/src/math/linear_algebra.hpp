#pragma once

#include "math/matrix.hpp"
#include "math/vector.hpp"

namespace lumina::math {

inline auto Dot(const Vec2 &a, const Vec2 &b) -> f32 {
  return (a.x * b.x) + (a.y * b.y);
}

inline auto Dot(const Vec3 &a, const Vec3 &b) -> f32 {
  return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

inline auto Dot(const Vec4 &a, const Vec4 &b) -> f32 {
  return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w);
}

inline auto Dot(const Vec2 &v, const Mat2 &m) -> Vec2 {
  Vec2 result;
  result.x = (v.x * m[0].x) + (v.y * m[1].x);
  result.y = (v.x * m[0].y) + (v.y * m[1].y);
  return result;
}

inline auto Dot(const Vec3 &v, const Mat3 &m) -> Vec3 {
  Vec3 result;
  result.x = (v.x * m[0].x) + (v.y * m[1].x) + (v.z * m[2].x);
  result.y = (v.x * m[0].y) + (v.y * m[1].y) + (v.z * m[2].y);
  result.z = (v.x * m[0].z) + (v.y * m[1].z) + (v.z * m[2].z);
  return result;
}

inline auto Dot(const Vec4 &v, const Mat4 &m) -> Vec4 {
  Vec4 result;
  result.x = (v.x * m[0].x) + (v.y * m[1].x) + (v.z * m[2].x) + (v.w * m[3].x);
  result.y = (v.x * m[0].y) + (v.y * m[1].y) + (v.z * m[2].y) + (v.w * m[3].y);
  result.z = (v.x * m[0].z) + (v.y * m[1].z) + (v.z * m[2].z) + (v.w * m[3].z);
  result.w = (v.x * m[0].w) + (v.y * m[1].w) + (v.z * m[2].w) + (v.w * m[3].w);
  return result;
}

inline auto Dot(const Mat2 &m, const Mat2 &n) -> Mat2 {
  Mat2 result;
  result[0] = Dot(m[0], Vec2(n[0].x, n[1].x));
  result[1] = Dot(m[1], Vec2(n[0].y, n[1].y));
  return result;
}

inline auto Dot(const Mat3 &m, const Mat3 &n) -> Mat3 {
  Mat3 result;
  result[0] = Dot(m[0], Vec3(n[0].x, n[1].x, n[2].x));
  result[1] = Dot(m[1], Vec3(n[0].y, n[1].y, n[2].y));
  result[2] = Dot(m[2], Vec3(n[0].z, n[1].z, n[2].z));
  return result;
}

inline auto Dot(const Mat4 &m, const Mat4 &n) -> Mat4 {
  Mat4 result;
  result[0].x = Dot(m[0], Vec4(n[0].x, n[1].x, n[2].x, n[3].x));
  result[0].y = Dot(m[0], Vec4(n[0].y, n[1].y, n[2].y, n[3].y));
  result[0].z = Dot(m[0], Vec4(n[0].z, n[1].z, n[2].z, n[3].z));
  result[0].w = Dot(m[0], Vec4(n[0].w, n[1].w, n[2].w, n[3].w));

  result[1].x = Dot(m[1], Vec4(n[0].x, n[1].x, n[2].x, n[3].x));
  result[1].y = Dot(m[1], Vec4(n[0].y, n[1].y, n[2].y, n[3].y));
  result[1].z = Dot(m[1], Vec4(n[0].z, n[1].z, n[2].z, n[3].z));
  result[1].w = Dot(m[1], Vec4(n[0].w, n[1].w, n[2].w, n[3].w));

  result[2].x = Dot(m[2], Vec4(n[0].x, n[1].x, n[2].x, n[3].x));
  result[2].y = Dot(m[2], Vec4(n[0].y, n[1].y, n[2].y, n[3].y));
  result[2].z = Dot(m[2], Vec4(n[0].z, n[1].z, n[2].z, n[3].z));
  result[2].w = Dot(m[2], Vec4(n[0].w, n[1].w, n[2].w, n[3].w));

  result[3].x = Dot(m[3], Vec4(n[0].x, n[1].x, n[2].x, n[3].x));
  result[3].y = Dot(m[3], Vec4(n[0].y, n[1].y, n[2].y, n[3].y));
  result[3].z = Dot(m[3], Vec4(n[0].z, n[1].z, n[2].z, n[3].z));
  result[3].w = Dot(m[3], Vec4(n[0].w, n[1].w, n[2].w, n[3].w));
  return result;
}

inline auto Cross(const Vec3 &a, const Vec3 &b) -> Vec3 {
  Vec3 Result((a.y * b.z) - (a.z * b.y), (a.z * b.x) - (a.x * b.z),
              (a.x * b.y) - (a.y * b.x));
  return Result;
}

} // namespace lumina::math
