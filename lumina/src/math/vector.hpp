#pragma once

#include "lumina_assert.hpp"
#include "lumina_types.hpp"

#include <cmath>

namespace lumina::math {

class VectorBase {
public:
  using ScalarType = f32;
};

/**
 * @brief 2D vector class
 *
 */
class Vec2 : public VectorBase {
  using VectorBase::VectorBase;

public:
  constexpr Vec2() noexcept : x{0}, y{0} {}             // NOLINT
  constexpr Vec2(ScalarType v) noexcept : x{v}, y{v} {} // NOLINT
  constexpr Vec2(ScalarType _x, ScalarType _y) noexcept // NOLINT
      : x{_x}, y{_y} {}

  union {
    struct {
      ScalarType x, y; // NOLINT
    };
    ScalarType e[2];
  };

  [[nodiscard]] auto xx() const -> Vec2 { return {x, x}; }
  [[nodiscard]] auto xy() const -> Vec2 { return {x, y}; }
  [[nodiscard]] auto yx() const -> Vec2 { return {y, x}; }
  [[nodiscard]] auto yy() const -> Vec2 { return {y, y}; }

  auto operator+=(Vec2 const &other) -> Vec2 & {
    x += other.x;
    y += other.y;
    return *this;
  }
  auto operator-=(Vec2 const &other) -> Vec2 & {
    x -= other.x;
    y -= other.y;
    return *this;
  }

  auto operator*=(ScalarType const &scalar) -> Vec2 & {
    x *= scalar;
    y *= scalar;
    return *this;
  }

  auto operator/=(ScalarType const &scalar) -> Vec2 & {
    x /= scalar;
    y /= scalar;
    return *this;
  }

  auto operator-() const -> Vec2 {
    Vec2 result;
    result.x = -x;
    result.y = -y;
    return result;
  }

  auto operator[](size_t idx) -> ScalarType & {
    ASSERT(idx < 2, "Vec2 index out of range");
    return e[idx];
  }

  [[nodiscard]] auto operator[](size_t idx) const -> const ScalarType & {
    ASSERT(idx < 2, "Vec2 index out of range");
    return e[idx];
  }

  [[nodiscard]] auto Length() const -> ScalarType {
    return std::sqrt((x * x) + (y * y));
  }
  [[nodiscard]] auto LengthSqr() const -> ScalarType {
    return (x * x) + (y * y);
  }

  auto Normalize() -> Vec2 & {
    auto len = Length();
    return *this /= len;
  }

  [[nodiscard]] auto DataPtr() -> ScalarType * { return &e[0]; }
  [[nodiscard]] auto DataPtr() const -> const ScalarType * { return &e[0]; }
};

/**
 * @brief 3D vector class
 *
 */
class Vec3 : public VectorBase {
  using VectorBase::VectorBase;

public:
  union {
    struct {
      ScalarType x, y, z; // NOLINT
    };
    struct {
      ScalarType pitch, yaw, roll;
    };
    ScalarType e[3];
  };

  constexpr Vec3() : x{0}, y{0}, z{0} {}                              // NOLINT
  constexpr Vec3(ScalarType v) : x{v}, y{v}, z{v} {}                  // NOLINT
  constexpr Vec3(ScalarType _x, ScalarType _y, ScalarType _z)         // NOLINT
      : x{_x}, y{_y}, z{_z} {}                                        // NOLINT
  constexpr Vec3(Vec2 v2, ScalarType _z) : x{v2.x}, y{v2.y}, z{_z} {} // NOLINT

  [[nodiscard]] auto xx() const -> Vec3 { return {x, x, x}; }
  [[nodiscard]] auto xy() const -> Vec3 { return {x, x, y}; }
  [[nodiscard]] auto xz() const -> Vec3 { return {x, x, z}; }
  [[nodiscard]] auto yx() const -> Vec3 { return {y, x, x}; }
  [[nodiscard]] auto yy() const -> Vec3 { return {y, y, y}; }
  [[nodiscard]] auto yz() const -> Vec3 { return {y, y, z}; }
  [[nodiscard]] auto zx() const -> Vec3 { return {z, x, x}; }
  [[nodiscard]] auto zy() const -> Vec3 { return {z, y, y}; }
  [[nodiscard]] auto zz() const -> Vec3 { return {z, z, z}; }

  [[nodiscard]] auto xxx() const -> Vec3 { return {x, x, x}; }
  [[nodiscard]] auto xxy() const -> Vec3 { return {x, x, y}; }
  [[nodiscard]] auto xxz() const -> Vec3 { return {x, x, z}; }
  [[nodiscard]] auto xyx() const -> Vec3 { return {x, y, x}; }
  [[nodiscard]] auto xyy() const -> Vec3 { return {x, y, y}; }
  [[nodiscard]] auto xyz() const -> Vec3 { return {x, y, z}; }
  [[nodiscard]] auto xzx() const -> Vec3 { return {x, z, x}; }
  [[nodiscard]] auto xzy() const -> Vec3 { return {x, z, y}; }
  [[nodiscard]] auto xzz() const -> Vec3 { return {x, z, z}; }
  [[nodiscard]] auto yxx() const -> Vec3 { return {y, x, x}; }
  [[nodiscard]] auto yxy() const -> Vec3 { return {y, x, y}; }
  [[nodiscard]] auto yxz() const -> Vec3 { return {y, x, z}; }
  [[nodiscard]] auto yyx() const -> Vec3 { return {y, y, x}; }
  [[nodiscard]] auto yyy() const -> Vec3 { return {y, y, y}; }
  [[nodiscard]] auto yyz() const -> Vec3 { return {y, y, z}; }
  [[nodiscard]] auto yzx() const -> Vec3 { return {y, z, x}; }
  [[nodiscard]] auto yzy() const -> Vec3 { return {y, z, y}; }
  [[nodiscard]] auto yzz() const -> Vec3 { return {y, z, z}; }
  [[nodiscard]] auto zxx() const -> Vec3 { return {z, x, x}; }
  [[nodiscard]] auto zxy() const -> Vec3 { return {z, x, y}; }
  [[nodiscard]] auto zxz() const -> Vec3 { return {z, x, z}; }
  [[nodiscard]] auto zyx() const -> Vec3 { return {z, y, x}; }
  [[nodiscard]] auto zyy() const -> Vec3 { return {z, y, y}; }
  [[nodiscard]] auto zyz() const -> Vec3 { return {z, y, z}; }
  [[nodiscard]] auto zzx() const -> Vec3 { return {z, z, x}; }
  [[nodiscard]] auto zzy() const -> Vec3 { return {z, z, y}; }
  [[nodiscard]] auto zzz() const -> Vec3 { return {z, z, z}; }

  auto operator+=(Vec3 const &other) -> Vec3 & {
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
  }
  auto operator-=(Vec3 const &other) -> Vec3 & {
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
  }

  auto operator*=(ScalarType const &scalar) -> Vec3 & {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
  }

  auto operator/=(ScalarType const &scalar) -> Vec3 & {
    x /= scalar;
    y /= scalar;
    z /= scalar;
    return *this;
  }

  auto operator-() const -> Vec3 {
    Vec3 result;
    result.x = -x;
    result.y = -y;
    result.z = -z;
    return result;
  }

  auto operator[](size_t idx) -> ScalarType & {
    ASSERT(idx < 3, "Vec3 index out of range");
    return e[idx];
  }

  [[nodiscard]] auto operator[](size_t idx) const -> const ScalarType & {
    ASSERT(idx < 3, "Vec3 index out of range");
    return e[idx];
  }

  [[nodiscard]] auto Length() const -> ScalarType {
    return std::sqrt((x * x) + (y * y) + (z * z));
  }
  [[nodiscard]] auto LengthSqr() const -> ScalarType {
    return (x * x) + (y * y) + (z * z);
  }

  auto Normalize() -> Vec3 & {
    auto len = Length();
    return *this /= len;
  }

  [[nodiscard]] auto DataPtr() -> ScalarType * { return &e[0]; }
  [[nodiscard]] auto DataPtr() const -> const ScalarType * { return &e[0]; }
};

class Vec4 : public VectorBase {
  using VectorBase::VectorBase;

public:
  union {
    struct {
      ScalarType x, y, z, w; // NOLINT
    };
    ScalarType e[4];
  };

  constexpr Vec4() : x{0}, y{0}, z{0}, w{0} {}                // NOLINT
  constexpr Vec4(ScalarType v) : x{v}, y{v}, z{v}, w{v} {}    // NOLINT
  constexpr Vec4(ScalarType _x, ScalarType _y, ScalarType _z, // NOLINT
                 ScalarType _w)                               // NOLINT
      : x{_x}, y{_y}, z{_z}, w{_w} {}                         // NOLINT
  constexpr Vec4(Vec3 v3, ScalarType _w)                      // NOLINT
      : x{v3.x}, y{v3.y}, z{v3.z}, w{_w} {}                   // NOLINT

  [[nodiscard]] auto xx() const -> Vec2 { return {x, x}; }
  [[nodiscard]] auto xy() const -> Vec2 { return {x, y}; }
  [[nodiscard]] auto xz() const -> Vec2 { return {x, z}; }
  [[nodiscard]] auto xw() const -> Vec2 { return {x, w}; }
  [[nodiscard]] auto yx() const -> Vec2 { return {y, x}; }
  [[nodiscard]] auto yy() const -> Vec2 { return {y, y}; }
  [[nodiscard]] auto yz() const -> Vec2 { return {y, z}; }
  [[nodiscard]] auto yw() const -> Vec2 { return {y, w}; }
  [[nodiscard]] auto zx() const -> Vec2 { return {z, x}; }
  [[nodiscard]] auto zy() const -> Vec2 { return {z, y}; }
  [[nodiscard]] auto zz() const -> Vec2 { return {z, z}; }
  [[nodiscard]] auto zw() const -> Vec2 { return {z, w}; }
  [[nodiscard]] auto wx() const -> Vec2 { return {w, x}; }
  [[nodiscard]] auto wy() const -> Vec2 { return {w, y}; }
  [[nodiscard]] auto wz() const -> Vec2 { return {w, z}; }
  [[nodiscard]] auto ww() const -> Vec2 { return {w, w}; }

  [[nodiscard]] auto xxx() const -> Vec3 { return {x, x, x}; }
  [[nodiscard]] auto xxy() const -> Vec3 { return {x, x, y}; }
  [[nodiscard]] auto xxz() const -> Vec3 { return {x, x, z}; }
  [[nodiscard]] auto xxw() const -> Vec3 { return {x, x, w}; }
  [[nodiscard]] auto xyx() const -> Vec3 { return {x, y, x}; }
  [[nodiscard]] auto xyy() const -> Vec3 { return {x, y, y}; }
  [[nodiscard]] auto xyz() const -> Vec3 { return {x, y, z}; }
  [[nodiscard]] auto xyw() const -> Vec3 { return {x, y, w}; }
  [[nodiscard]] auto xzx() const -> Vec3 { return {x, z, x}; }
  [[nodiscard]] auto xzy() const -> Vec3 { return {x, z, y}; }
  [[nodiscard]] auto xzz() const -> Vec3 { return {x, z, z}; }
  [[nodiscard]] auto xzw() const -> Vec3 { return {x, z, w}; }
  [[nodiscard]] auto yxx() const -> Vec3 { return {y, x, x}; }
  [[nodiscard]] auto yxy() const -> Vec3 { return {y, x, y}; }
  [[nodiscard]] auto yxz() const -> Vec3 { return {y, x, z}; }
  [[nodiscard]] auto yxw() const -> Vec3 { return {y, x, w}; }
  [[nodiscard]] auto yyx() const -> Vec3 { return {y, y, x}; }
  [[nodiscard]] auto yyy() const -> Vec3 { return {y, y, y}; }
  [[nodiscard]] auto yyz() const -> Vec3 { return {y, y, z}; }
  [[nodiscard]] auto yyw() const -> Vec3 { return {y, y, w}; }
  [[nodiscard]] auto yzx() const -> Vec3 { return {y, z, x}; }
  [[nodiscard]] auto yzy() const -> Vec3 { return {y, z, y}; }
  [[nodiscard]] auto yzz() const -> Vec3 { return {y, z, z}; }
  [[nodiscard]] auto yzw() const -> Vec3 { return {y, z, w}; }
  [[nodiscard]] auto zxx() const -> Vec3 { return {z, x, x}; }
  [[nodiscard]] auto zxy() const -> Vec3 { return {z, x, y}; }
  [[nodiscard]] auto zxz() const -> Vec3 { return {z, x, z}; }
  [[nodiscard]] auto zxw() const -> Vec3 { return {z, x, w}; }
  [[nodiscard]] auto zyx() const -> Vec3 { return {z, y, x}; }
  [[nodiscard]] auto zyy() const -> Vec3 { return {z, y, y}; }
  [[nodiscard]] auto zyz() const -> Vec3 { return {z, y, z}; }
  [[nodiscard]] auto zyw() const -> Vec3 { return {z, y, w}; }
  [[nodiscard]] auto zzx() const -> Vec3 { return {z, z, x}; }
  [[nodiscard]] auto zzy() const -> Vec3 { return {z, z, y}; }
  [[nodiscard]] auto zzz() const -> Vec3 { return {z, z, z}; }
  [[nodiscard]] auto zzw() const -> Vec3 { return {z, z, w}; }
  [[nodiscard]] auto zwx() const -> Vec3 { return {z, w, x}; }
  [[nodiscard]] auto zwy() const -> Vec3 { return {z, w, y}; }
  [[nodiscard]] auto zwz() const -> Vec3 { return {z, w, z}; }
  [[nodiscard]] auto zww() const -> Vec3 { return {z, w, w}; }
  [[nodiscard]] auto wxx() const -> Vec3 { return {w, x, x}; }
  [[nodiscard]] auto wxy() const -> Vec3 { return {w, x, y}; }
  [[nodiscard]] auto wxz() const -> Vec3 { return {w, x, z}; }
  [[nodiscard]] auto wxw() const -> Vec3 { return {w, x, w}; }
  [[nodiscard]] auto wyx() const -> Vec3 { return {w, y, x}; }
  [[nodiscard]] auto wyy() const -> Vec3 { return {w, y, y}; }
  [[nodiscard]] auto wyz() const -> Vec3 { return {w, y, z}; }
  [[nodiscard]] auto wyw() const -> Vec3 { return {w, y, w}; }
  [[nodiscard]] auto wzx() const -> Vec3 { return {w, z, x}; }
  [[nodiscard]] auto wzy() const -> Vec3 { return {w, z, y}; }
  [[nodiscard]] auto wzz() const -> Vec3 { return {w, z, z}; }
  [[nodiscard]] auto wzw() const -> Vec3 { return {w, z, w}; }
  [[nodiscard]] auto wwx() const -> Vec3 { return {w, w, x}; }
  [[nodiscard]] auto wwy() const -> Vec3 { return {w, w, y}; }
  [[nodiscard]] auto wwz() const -> Vec3 { return {w, w, z}; }
  [[nodiscard]] auto www() const -> Vec3 { return {w, w, w}; }

  [[nodiscard]] auto xxxx() const -> Vec4 { return {x, x, x, x}; }
  [[nodiscard]] auto xxxy() const -> Vec4 { return {x, x, x, y}; }
  [[nodiscard]] auto xxxz() const -> Vec4 { return {x, x, x, z}; }
  [[nodiscard]] auto xxxw() const -> Vec4 { return {x, x, x, w}; }
  [[nodiscard]] auto xxyx() const -> Vec4 { return {x, x, y, x}; }
  [[nodiscard]] auto xxyy() const -> Vec4 { return {x, x, y, y}; }
  [[nodiscard]] auto xxyz() const -> Vec4 { return {x, x, y, z}; }
  [[nodiscard]] auto xxyw() const -> Vec4 { return {x, x, y, w}; }
  [[nodiscard]] auto xxzx() const -> Vec4 { return {x, x, z, x}; }
  [[nodiscard]] auto xxzy() const -> Vec4 { return {x, x, z, y}; }
  [[nodiscard]] auto xxzz() const -> Vec4 { return {x, x, z, z}; }
  [[nodiscard]] auto xxzw() const -> Vec4 { return {x, x, z, w}; }
  [[nodiscard]] auto xyxx() const -> Vec4 { return {x, y, x, x}; }
  [[nodiscard]] auto xyxy() const -> Vec4 { return {x, y, x, y}; }
  [[nodiscard]] auto xyxz() const -> Vec4 { return {x, y, x, z}; }
  [[nodiscard]] auto xyxw() const -> Vec4 { return {x, y, x, w}; }
  [[nodiscard]] auto xyyx() const -> Vec4 { return {x, y, y, x}; }
  [[nodiscard]] auto xyyy() const -> Vec4 { return {x, y, y, y}; }
  [[nodiscard]] auto xyyz() const -> Vec4 { return {x, y, y, z}; }
  [[nodiscard]] auto xyyw() const -> Vec4 { return {x, y, y, w}; }
  [[nodiscard]] auto xyzx() const -> Vec4 { return {x, y, z, x}; }
  [[nodiscard]] auto xyzy() const -> Vec4 { return {x, y, z, y}; }
  [[nodiscard]] auto xyzz() const -> Vec4 { return {x, y, z, z}; }
  [[nodiscard]] auto xyzw() const -> Vec4 { return {x, y, z, w}; }
  [[nodiscard]] auto xzxx() const -> Vec4 { return {x, z, x, x}; }
  [[nodiscard]] auto xzxy() const -> Vec4 { return {x, z, x, y}; }
  [[nodiscard]] auto xzxz() const -> Vec4 { return {x, z, x, z}; }
  [[nodiscard]] auto xzxw() const -> Vec4 { return {x, z, x, w}; }
  [[nodiscard]] auto xzyx() const -> Vec4 { return {x, z, y, x}; }
  [[nodiscard]] auto xzyy() const -> Vec4 { return {x, z, y, y}; }
  [[nodiscard]] auto xzyz() const -> Vec4 { return {x, z, y, z}; }
  [[nodiscard]] auto xzyw() const -> Vec4 { return {x, z, y, w}; }
  [[nodiscard]] auto xzzx() const -> Vec4 { return {x, z, z, x}; }
  [[nodiscard]] auto xzzy() const -> Vec4 { return {x, z, z, y}; }
  [[nodiscard]] auto xzzz() const -> Vec4 { return {x, z, z, z}; }
  [[nodiscard]] auto xzzw() const -> Vec4 { return {x, z, z, w}; }
  [[nodiscard]] auto xzwx() const -> Vec4 { return {x, z, w, x}; }
  [[nodiscard]] auto xzwy() const -> Vec4 { return {x, z, w, y}; }
  [[nodiscard]] auto xzwz() const -> Vec4 { return {x, z, w, z}; }
  [[nodiscard]] auto xzww() const -> Vec4 { return {x, z, w, w}; }
  [[nodiscard]] auto xwxx() const -> Vec4 { return {x, w, x, x}; }
  [[nodiscard]] auto xwxy() const -> Vec4 { return {x, w, x, y}; }
  [[nodiscard]] auto xwxz() const -> Vec4 { return {x, w, x, z}; }
  [[nodiscard]] auto xwxw() const -> Vec4 { return {x, w, x, w}; }
  [[nodiscard]] auto xwyx() const -> Vec4 { return {x, w, y, x}; }
  [[nodiscard]] auto xwyy() const -> Vec4 { return {x, w, y, y}; }
  [[nodiscard]] auto xwyz() const -> Vec4 { return {x, w, y, z}; }
  [[nodiscard]] auto xwyw() const -> Vec4 { return {x, w, y, w}; }
  [[nodiscard]] auto xwzx() const -> Vec4 { return {x, w, z, x}; }
  [[nodiscard]] auto xwzy() const -> Vec4 { return {x, w, z, y}; }
  [[nodiscard]] auto xwzz() const -> Vec4 { return {x, w, z, z}; }
  [[nodiscard]] auto xwzw() const -> Vec4 { return {x, w, z, w}; }
  [[nodiscard]] auto xwwx() const -> Vec4 { return {x, w, w, x}; }
  [[nodiscard]] auto xwwy() const -> Vec4 { return {x, w, w, y}; }
  [[nodiscard]] auto xwwz() const -> Vec4 { return {x, w, w, z}; }
  [[nodiscard]] auto xwww() const -> Vec4 { return {x, w, w, w}; }

  [[nodiscard]] auto yxxx() const -> Vec4 { return {y, x, x, x}; }
  [[nodiscard]] auto yxxy() const -> Vec4 { return {y, x, x, y}; }
  [[nodiscard]] auto yxxz() const -> Vec4 { return {y, x, x, z}; }
  [[nodiscard]] auto yxxw() const -> Vec4 { return {y, x, x, w}; }
  [[nodiscard]] auto yxyx() const -> Vec4 { return {y, x, y, x}; }
  [[nodiscard]] auto yxyy() const -> Vec4 { return {y, x, y, y}; }
  [[nodiscard]] auto yxyz() const -> Vec4 { return {y, x, y, z}; }
  [[nodiscard]] auto yxyw() const -> Vec4 { return {y, x, y, w}; }
  [[nodiscard]] auto yxzx() const -> Vec4 { return {y, x, z, x}; }
  [[nodiscard]] auto yxzy() const -> Vec4 { return {y, x, z, y}; }
  [[nodiscard]] auto yxzz() const -> Vec4 { return {y, x, z, z}; }
  [[nodiscard]] auto yxzw() const -> Vec4 { return {y, x, z, w}; }
  [[nodiscard]] auto yyxx() const -> Vec4 { return {y, y, x, x}; }
  [[nodiscard]] auto yyxy() const -> Vec4 { return {y, y, x, y}; }
  [[nodiscard]] auto yyxz() const -> Vec4 { return {y, y, x, z}; }
  [[nodiscard]] auto yyxw() const -> Vec4 { return {y, y, x, w}; }
  [[nodiscard]] auto yyyx() const -> Vec4 { return {y, y, y, x}; }
  [[nodiscard]] auto yyyy() const -> Vec4 { return {y, y, y, y}; }
  [[nodiscard]] auto yyyz() const -> Vec4 { return {y, y, y, z}; }
  [[nodiscard]] auto yyyw() const -> Vec4 { return {y, y, y, w}; }
  [[nodiscard]] auto yyzx() const -> Vec4 { return {y, y, z, x}; }
  [[nodiscard]] auto yyzy() const -> Vec4 { return {y, y, z, y}; }
  [[nodiscard]] auto yyzz() const -> Vec4 { return {y, y, z, z}; }
  [[nodiscard]] auto yyzw() const -> Vec4 { return {y, y, z, w}; }
  [[nodiscard]] auto yzxx() const -> Vec4 { return {y, z, x, x}; }
  [[nodiscard]] auto yzxy() const -> Vec4 { return {y, z, x, y}; }
  [[nodiscard]] auto yzxz() const -> Vec4 { return {y, z, x, z}; }
  [[nodiscard]] auto yzxw() const -> Vec4 { return {y, z, x, w}; }
  [[nodiscard]] auto yzyx() const -> Vec4 { return {y, z, y, x}; }
  [[nodiscard]] auto yzyy() const -> Vec4 { return {y, z, y, y}; }
  [[nodiscard]] auto yzyz() const -> Vec4 { return {y, z, y, z}; }
  [[nodiscard]] auto yzyw() const -> Vec4 { return {y, z, y, w}; }
  [[nodiscard]] auto yzzx() const -> Vec4 { return {y, z, z, x}; }
  [[nodiscard]] auto yzzy() const -> Vec4 { return {y, z, z, y}; }
  [[nodiscard]] auto yzzz() const -> Vec4 { return {y, z, z, z}; }
  [[nodiscard]] auto yzzw() const -> Vec4 { return {y, z, z, w}; }
  [[nodiscard]] auto yzwx() const -> Vec4 { return {y, z, w, x}; }
  [[nodiscard]] auto yzwy() const -> Vec4 { return {y, z, w, y}; }
  [[nodiscard]] auto yzwz() const -> Vec4 { return {y, z, w, z}; }
  [[nodiscard]] auto yzww() const -> Vec4 { return {y, z, w, w}; }
  [[nodiscard]] auto ywxx() const -> Vec4 { return {y, w, x, x}; }
  [[nodiscard]] auto ywxy() const -> Vec4 { return {y, w, x, y}; }
  [[nodiscard]] auto ywxz() const -> Vec4 { return {y, w, x, z}; }
  [[nodiscard]] auto ywxw() const -> Vec4 { return {y, w, x, w}; }
  [[nodiscard]] auto ywyx() const -> Vec4 { return {y, w, y, x}; }
  [[nodiscard]] auto ywyy() const -> Vec4 { return {y, w, y, y}; }
  [[nodiscard]] auto ywyz() const -> Vec4 { return {y, w, y, z}; }
  [[nodiscard]] auto ywyw() const -> Vec4 { return {y, w, y, w}; }
  [[nodiscard]] auto ywzx() const -> Vec4 { return {y, w, z, x}; }
  [[nodiscard]] auto ywzy() const -> Vec4 { return {y, w, z, y}; }
  [[nodiscard]] auto ywzz() const -> Vec4 { return {y, w, z, z}; }
  [[nodiscard]] auto ywzw() const -> Vec4 { return {y, w, z, w}; }
  [[nodiscard]] auto ywwx() const -> Vec4 { return {y, w, w, x}; }
  [[nodiscard]] auto ywwy() const -> Vec4 { return {y, w, w, y}; }
  [[nodiscard]] auto ywwz() const -> Vec4 { return {y, w, w, z}; }
  [[nodiscard]] auto ywww() const -> Vec4 { return {y, w, w, w}; }

  [[nodiscard]] auto zxxx() const -> Vec4 { return {z, x, x, x}; }
  [[nodiscard]] auto zxxy() const -> Vec4 { return {z, x, x, y}; }
  [[nodiscard]] auto zxxz() const -> Vec4 { return {z, x, x, z}; }
  [[nodiscard]] auto zxxw() const -> Vec4 { return {z, x, x, w}; }
  [[nodiscard]] auto zxyx() const -> Vec4 { return {z, x, y, x}; }
  [[nodiscard]] auto zxyy() const -> Vec4 { return {z, x, y, y}; }
  [[nodiscard]] auto zxyz() const -> Vec4 { return {z, x, y, z}; }
  [[nodiscard]] auto zxyw() const -> Vec4 { return {z, x, y, w}; }
  [[nodiscard]] auto zxzx() const -> Vec4 { return {z, x, z, x}; }
  [[nodiscard]] auto zxzy() const -> Vec4 { return {z, x, z, y}; }
  [[nodiscard]] auto zxzz() const -> Vec4 { return {z, x, z, z}; }
  [[nodiscard]] auto zxzw() const -> Vec4 { return {z, x, z, w}; }
  [[nodiscard]] auto zyxx() const -> Vec4 { return {z, y, x, x}; }
  [[nodiscard]] auto zyxy() const -> Vec4 { return {z, y, x, y}; }
  [[nodiscard]] auto zyxz() const -> Vec4 { return {z, y, x, z}; }
  [[nodiscard]] auto zyxw() const -> Vec4 { return {z, y, x, w}; }
  [[nodiscard]] auto zyyx() const -> Vec4 { return {z, y, y, x}; }
  [[nodiscard]] auto zyyy() const -> Vec4 { return {z, y, y, y}; }
  [[nodiscard]] auto zyyz() const -> Vec4 { return {z, y, y, z}; }
  [[nodiscard]] auto zyyw() const -> Vec4 { return {z, y, y, w}; }
  [[nodiscard]] auto zyzx() const -> Vec4 { return {z, y, z, x}; }
  [[nodiscard]] auto zyzy() const -> Vec4 { return {z, y, z, y}; }
  [[nodiscard]] auto zyzz() const -> Vec4 { return {z, y, z, z}; }
  [[nodiscard]] auto zyzw() const -> Vec4 { return {z, y, z, w}; }
  [[nodiscard]] auto zzxx() const -> Vec4 { return {z, z, x, x}; }
  [[nodiscard]] auto zzxy() const -> Vec4 { return {z, z, x, y}; }
  [[nodiscard]] auto zzxz() const -> Vec4 { return {z, z, x, z}; }
  [[nodiscard]] auto zzxw() const -> Vec4 { return {z, z, x, w}; }
  [[nodiscard]] auto zzyx() const -> Vec4 { return {z, z, y, x}; }
  [[nodiscard]] auto zzyy() const -> Vec4 { return {z, z, y, y}; }
  [[nodiscard]] auto zzyz() const -> Vec4 { return {z, z, y, z}; }
  [[nodiscard]] auto zzyw() const -> Vec4 { return {z, z, y, w}; }
  [[nodiscard]] auto zzzx() const -> Vec4 { return {z, z, z, x}; }
  [[nodiscard]] auto zzzy() const -> Vec4 { return {z, z, z, y}; }
  [[nodiscard]] auto zzzz() const -> Vec4 { return {z, z, z, z}; }
  [[nodiscard]] auto zzzw() const -> Vec4 { return {z, z, z, w}; }
  [[nodiscard]] auto zzwx() const -> Vec4 { return {z, z, w, x}; }
  [[nodiscard]] auto zzwy() const -> Vec4 { return {z, z, w, y}; }
  [[nodiscard]] auto zzwz() const -> Vec4 { return {z, z, w, z}; }
  [[nodiscard]] auto zzww() const -> Vec4 { return {z, z, w, w}; }
  [[nodiscard]] auto zwxx() const -> Vec4 { return {z, w, x, x}; }
  [[nodiscard]] auto zwxy() const -> Vec4 { return {z, w, x, y}; }
  [[nodiscard]] auto zwxz() const -> Vec4 { return {z, w, x, z}; }
  [[nodiscard]] auto zwxw() const -> Vec4 { return {z, w, x, w}; }
  [[nodiscard]] auto zwyx() const -> Vec4 { return {z, w, y, x}; }
  [[nodiscard]] auto zwyy() const -> Vec4 { return {z, w, y, y}; }
  [[nodiscard]] auto zwyz() const -> Vec4 { return {z, w, y, z}; }
  [[nodiscard]] auto zwyw() const -> Vec4 { return {z, w, y, w}; }
  [[nodiscard]] auto zwzx() const -> Vec4 { return {z, w, z, x}; }
  [[nodiscard]] auto zwzy() const -> Vec4 { return {z, w, z, y}; }
  [[nodiscard]] auto zwzz() const -> Vec4 { return {z, w, z, z}; }
  [[nodiscard]] auto zwzw() const -> Vec4 { return {z, w, z, w}; }
  [[nodiscard]] auto zwwx() const -> Vec4 { return {z, w, w, x}; }
  [[nodiscard]] auto zwwy() const -> Vec4 { return {z, w, w, y}; }
  [[nodiscard]] auto zwwz() const -> Vec4 { return {z, w, w, z}; }
  [[nodiscard]] auto zwww() const -> Vec4 { return {z, w, w, w}; }

  [[nodiscard]] auto wxxx() const -> Vec4 { return {w, x, x, x}; }
  [[nodiscard]] auto wxxy() const -> Vec4 { return {w, x, x, y}; }
  [[nodiscard]] auto wxxz() const -> Vec4 { return {w, x, x, z}; }
  [[nodiscard]] auto wxxw() const -> Vec4 { return {w, x, x, w}; }
  [[nodiscard]] auto wxyx() const -> Vec4 { return {w, x, y, x}; }
  [[nodiscard]] auto wxyy() const -> Vec4 { return {w, x, y, y}; }
  [[nodiscard]] auto wxyz() const -> Vec4 { return {w, x, y, z}; }
  [[nodiscard]] auto wxyw() const -> Vec4 { return {w, x, y, w}; }
  [[nodiscard]] auto wxzx() const -> Vec4 { return {w, x, z, x}; }
  [[nodiscard]] auto wxzy() const -> Vec4 { return {w, x, z, y}; }
  [[nodiscard]] auto wxzz() const -> Vec4 { return {w, x, z, z}; }
  [[nodiscard]] auto wxzw() const -> Vec4 { return {w, x, z, w}; }
  [[nodiscard]] auto wyxx() const -> Vec4 { return {w, y, x, x}; }
  [[nodiscard]] auto wyxy() const -> Vec4 { return {w, y, x, y}; }
  [[nodiscard]] auto wyxz() const -> Vec4 { return {w, y, x, z}; }
  [[nodiscard]] auto wyxw() const -> Vec4 { return {w, y, x, w}; }
  [[nodiscard]] auto wyyx() const -> Vec4 { return {w, y, y, x}; }
  [[nodiscard]] auto wyyy() const -> Vec4 { return {w, y, y, y}; }
  [[nodiscard]] auto wyyz() const -> Vec4 { return {w, y, y, z}; }
  [[nodiscard]] auto wyyw() const -> Vec4 { return {w, y, y, w}; }
  [[nodiscard]] auto wyzx() const -> Vec4 { return {w, y, z, x}; }
  [[nodiscard]] auto wyzy() const -> Vec4 { return {w, y, z, y}; }
  [[nodiscard]] auto wyzz() const -> Vec4 { return {w, y, z, z}; }
  [[nodiscard]] auto wyzw() const -> Vec4 { return {w, y, z, w}; }
  [[nodiscard]] auto wzxx() const -> Vec4 { return {w, z, x, x}; }
  [[nodiscard]] auto wzxy() const -> Vec4 { return {w, z, x, y}; }
  [[nodiscard]] auto wzxz() const -> Vec4 { return {w, z, x, z}; }
  [[nodiscard]] auto wzxw() const -> Vec4 { return {w, z, x, w}; }
  [[nodiscard]] auto wzyx() const -> Vec4 { return {w, z, y, x}; }
  [[nodiscard]] auto wzyy() const -> Vec4 { return {w, z, y, y}; }
  [[nodiscard]] auto wzyz() const -> Vec4 { return {w, z, y, z}; }
  [[nodiscard]] auto wzyw() const -> Vec4 { return {w, z, y, w}; }
  [[nodiscard]] auto wzzx() const -> Vec4 { return {w, z, z, x}; }
  [[nodiscard]] auto wzzy() const -> Vec4 { return {w, z, z, y}; }
  [[nodiscard]] auto wzzz() const -> Vec4 { return {w, z, z, z}; }
  [[nodiscard]] auto wzzw() const -> Vec4 { return {w, z, z, w}; }
  [[nodiscard]] auto wzwx() const -> Vec4 { return {w, z, w, x}; }
  [[nodiscard]] auto wzwy() const -> Vec4 { return {w, z, w, y}; }
  [[nodiscard]] auto wzwz() const -> Vec4 { return {w, z, w, z}; }
  [[nodiscard]] auto wzww() const -> Vec4 { return {w, z, w, w}; }
  [[nodiscard]] auto wwxx() const -> Vec4 { return {w, w, x, x}; }
  [[nodiscard]] auto wwxy() const -> Vec4 { return {w, w, x, y}; }
  [[nodiscard]] auto wwxz() const -> Vec4 { return {w, w, x, z}; }
  [[nodiscard]] auto wwxw() const -> Vec4 { return {w, w, x, w}; }
  [[nodiscard]] auto wwyx() const -> Vec4 { return {w, w, y, x}; }
  [[nodiscard]] auto wwyy() const -> Vec4 { return {w, w, y, y}; }
  [[nodiscard]] auto wwyz() const -> Vec4 { return {w, w, y, z}; }
  [[nodiscard]] auto wwyw() const -> Vec4 { return {w, w, y, w}; }
  [[nodiscard]] auto wwzx() const -> Vec4 { return {w, w, z, x}; }
  [[nodiscard]] auto wwzy() const -> Vec4 { return {w, w, z, y}; }
  [[nodiscard]] auto wwzz() const -> Vec4 { return {w, w, z, z}; }
  [[nodiscard]] auto wwzw() const -> Vec4 { return {w, w, z, w}; }
  [[nodiscard]] auto wwwx() const -> Vec4 { return {w, w, w, x}; }
  [[nodiscard]] auto wwwy() const -> Vec4 { return {w, w, w, y}; }
  [[nodiscard]] auto wwwz() const -> Vec4 { return {w, w, w, z}; }
  [[nodiscard]] auto wwww() const -> Vec4 { return {w, w, w, w}; }

  auto operator+=(Vec4 const &other) -> Vec4 & {
    x += other.x;
    y += other.y;
    z += other.z;
    w += other.w;
    return *this;
  }

  auto operator-=(Vec4 const &other) -> Vec4 & {
    x -= other.x;
    y -= other.y;
    z -= other.z;
    w -= other.w;
    return *this;
  }

  auto operator*=(ScalarType const &scalar) -> Vec4 & {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    w *= scalar;
    return *this;
  }

  auto operator/=(ScalarType const &scalar) -> Vec4 & {
    x /= scalar;
    y /= scalar;
    z /= scalar;
    w /= scalar;
    return *this;
  }

  auto operator-() const -> Vec4 {
    Vec4 result;
    result.x = -x;
    result.y = -y;
    result.z = -z;
    result.w = -w;
    return result;
  }

  auto operator[](size_t idx) -> ScalarType & {
    ASSERT(idx < 4, "Vec4 index out of range");
    return e[idx];
  }

  [[nodiscard]] auto operator[](size_t idx) const -> const ScalarType & {
    ASSERT(idx < 4, "Vec4 index out of range");
    return e[idx];
  }

  [[nodiscard]] auto Length() const -> ScalarType {
    return sqrtf((x * x) + (y * y) + (z * z) + (w * w));
  }
  [[nodiscard]] auto LengthSqr() const -> ScalarType {
    return (x * x) + (y * y) + (z * z) + (w * w);
  }

  auto Normalize() -> Vec4 & {
    auto len = Length();
    return *this /= len;
  }

  [[nodiscard]] auto DataPtr() -> ScalarType * { return &e[0]; }
  [[nodiscard]] auto DataPtr() const -> const ScalarType * { return &e[0]; }
};

// Vector concept, check that T has VectorBase as a base class
template <typename T>
concept LuminaVectorType =
    requires(T vec) { std::is_base_of_v<VectorBase, T>; };

template <LuminaVectorType T>
auto Normalize(const T &&vec) -> T {
  T result = vec;
  return result.Normalize();
}

template <LuminaVectorType T>
auto Length(const T &&vec) -> typename T::ScalarType {
  return vec.Length();
}

template <LuminaVectorType T>
auto LengthSqr(const T &&vec) -> typename T::ScalarType {
  return vec.LengthSqr();
}

template <LuminaVectorType T>
auto operator*(T vec, const typename T::ScalarType scalar) -> T {
  return vec *= scalar;
}

template <LuminaVectorType T>
auto operator*(const typename T::ScalarType scalar, T vec) -> T {
  return vec *= scalar;
}

} // namespace lumina::math
