#pragma once

#include "lumina_assert.hpp"
#include "lumina_types.hpp"


#include <cmath>

namespace lumina::math {

#ifdef LUMINA_VECTOR_DEFAULT_DOUBLE_PRECISION
using DefaultVectorScalarType = f64;
#else
using DefaultVectorScalarType = f32;
#endif

class VectorBase {
public:
  using ScalarType = DefaultVectorScalarType;
};

/**
 * @brief 2D vector class
 *
 */
class Vec2 : public VectorBase {
  using VectorBase::VectorBase;
  template <int x, int y>
  struct Vec2SwizzleWrapper {
    // implicit conversion to Vec2
    operator Vec2() const { return Vec2(arr[x], arr[y]); }
    // support assignments from Vec2
    auto operator=(const Vec2 &vec) -> Vec2SwizzleWrapper & {
      arr[x] = vec.x;
      arr[y] = vec.y;
      return *this;
    }

  private:
    ScalarType arr[2];
  };

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
    Vec2SwizzleWrapper<0, 0> xx;
    Vec2SwizzleWrapper<0, 1> xy;
    Vec2SwizzleWrapper<1, 0> yx;
    Vec2SwizzleWrapper<1, 1> yy;
  };

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
  template <int x, int y>
  struct Vec2SwizzleWrapper {
    // implicit conversion to Vec2
    operator Vec2() const { return {arr[x], arr[y]}; }
    // support assignments from Vec2
    auto operator=(const Vec2 &vec) -> Vec2SwizzleWrapper & {
      arr[x] = vec.x;
      arr[y] = vec.y;
      return *this;
    }

  private:
    ScalarType arr[3];
  };

  template <int x, int y, int z>
  struct Vec3SwizzleWrapper {
    // implicit conversion to Vec3
    operator Vec3() const { return Vec3(arr[x], arr[y], arr[z]); }
    // support assignments from Vec3
    auto operator=(const Vec3 &vec) -> Vec3SwizzleWrapper & {
      arr[x] = vec.x;
      arr[y] = vec.y;
      arr[z] = vec.z;
      return *this;
    }

  private:
    ScalarType arr[3];
  };

public:
  union {
    struct {
      ScalarType x, y, z; // NOLINT
    };
    struct {
      ScalarType pitch, yaw, roll;
    };
    ScalarType e[3];
    Vec2SwizzleWrapper<0, 0> xx;
    Vec2SwizzleWrapper<0, 1> xy;
    Vec2SwizzleWrapper<0, 2> xz;
    Vec2SwizzleWrapper<1, 0> yx;
    Vec2SwizzleWrapper<1, 1> yy;
    Vec2SwizzleWrapper<1, 2> yz;
    Vec2SwizzleWrapper<2, 0> zx;
    Vec2SwizzleWrapper<2, 1> zy;
    Vec2SwizzleWrapper<2, 2> zz;

    Vec3SwizzleWrapper<0, 0, 0> xxx;
    Vec3SwizzleWrapper<0, 0, 1> xxy;
    Vec3SwizzleWrapper<0, 0, 2> xxz;
    Vec3SwizzleWrapper<0, 1, 0> xyx;
    Vec3SwizzleWrapper<0, 1, 1> xyy;
    Vec3SwizzleWrapper<0, 1, 2> xyz;
    Vec3SwizzleWrapper<0, 2, 0> xzx;
    Vec3SwizzleWrapper<0, 2, 1> xzy;
    Vec3SwizzleWrapper<0, 2, 2> xzz;
    Vec3SwizzleWrapper<1, 0, 0> yxx;
    Vec3SwizzleWrapper<1, 0, 1> yxy;
    Vec3SwizzleWrapper<1, 0, 2> yxz;
    Vec3SwizzleWrapper<1, 1, 0> yyx;
    Vec3SwizzleWrapper<1, 1, 1> yyy;
    Vec3SwizzleWrapper<1, 1, 2> yyz;
    Vec3SwizzleWrapper<1, 2, 0> yzx;
    Vec3SwizzleWrapper<1, 2, 1> yzy;
    Vec3SwizzleWrapper<1, 2, 2> yzz;
    Vec3SwizzleWrapper<2, 0, 0> zxx;
    Vec3SwizzleWrapper<2, 0, 1> zxy;
    Vec3SwizzleWrapper<2, 0, 2> zxz;
    Vec3SwizzleWrapper<2, 1, 0> zyx;
    Vec3SwizzleWrapper<2, 1, 1> zyy;
    Vec3SwizzleWrapper<2, 1, 2> zyz;
    Vec3SwizzleWrapper<2, 2, 0> zzx;
    Vec3SwizzleWrapper<2, 2, 1> zzy;
    Vec3SwizzleWrapper<2, 2, 2> zzz;
  };

  constexpr Vec3() : x{0}, y{0}, z{0} {}                              // NOLINT
  constexpr Vec3(ScalarType v) : x{v}, y{v}, z{v} {}                  // NOLINT
  constexpr Vec3(ScalarType _x, ScalarType _y, ScalarType _z)         // NOLINT
      : x{_x}, y{_y}, z{_z} {}                                        // NOLINT
  constexpr Vec3(Vec2 v2, ScalarType _z) : x{v2.x}, y{v2.y}, z{_z} {} // NOLINT

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
private:
  template <int x, int y>
  struct Vec2SwizzleWrapper {
    // implicit conversion to Vec2
    operator Vec2() const { return Vec2(arr[x], arr[y]); }
    // support assignments from Vec2
    Vec2SwizzleWrapper &operator=(const Vec2 &vec) {
      arr[x] = vec.x;
      arr[y] = vec.y;
      return *this;
    }

  private:
    ScalarType arr[4];
  };

  template <int x, int y, int z>
  struct Vec3SwizzleWrapper {
    // implicit conversion to Vec3
    operator Vec3() const { return Vec3(arr[x], arr[y], arr[z]); }
    // support assignments from Vec3
    Vec3SwizzleWrapper &operator=(const Vec3 &vec) {
      arr[x] = vec.x;
      arr[y] = vec.y;
      arr[z] = vec.z;
      return *this;
    }

  private:
    ScalarType arr[4];
  };

  template <int x, int y, int z, int w>
  struct Vec4SwizzleWrapper {
    // implicit conversion to Vec4
    operator Vec4() const { return Vec4(arr[x], arr[y], arr[z], arr[w]); }
    // support assignments from Vec4
    Vec4SwizzleWrapper &operator=(const Vec4 &vec) {
      arr[x] = vec.x;
      arr[y] = vec.y;
      arr[z] = vec.z;
      arr[w] = vec.w;
      return *this;
    }

  private:
    ScalarType arr[4];
  };

public:
  union {
    struct {
      ScalarType x, y, z, w; // NOLINT
    };
    ScalarType e[4];

    Vec2SwizzleWrapper<0, 0> xx;
    Vec2SwizzleWrapper<0, 1> xy;
    Vec2SwizzleWrapper<0, 2> xz;
    Vec2SwizzleWrapper<0, 3> xw;
    Vec2SwizzleWrapper<1, 0> yx;
    Vec2SwizzleWrapper<1, 1> yy;
    Vec2SwizzleWrapper<1, 2> yz;
    Vec2SwizzleWrapper<1, 3> yw;
    Vec2SwizzleWrapper<2, 0> zx;
    Vec2SwizzleWrapper<2, 1> zy;
    Vec2SwizzleWrapper<2, 2> zz;
    Vec2SwizzleWrapper<2, 3> zw;
    Vec2SwizzleWrapper<3, 0> wx;
    Vec2SwizzleWrapper<3, 1> wy;
    Vec2SwizzleWrapper<3, 2> wz;
    Vec2SwizzleWrapper<3, 3> ww;

    Vec3SwizzleWrapper<0, 0, 0> xxx;
    Vec3SwizzleWrapper<0, 0, 1> xxy;
    Vec3SwizzleWrapper<0, 0, 2> xxz;
    Vec3SwizzleWrapper<0, 0, 3> xxw;
    Vec3SwizzleWrapper<0, 1, 0> xyx;
    Vec3SwizzleWrapper<0, 1, 1> xyy;
    Vec3SwizzleWrapper<0, 1, 2> xyz;
    Vec3SwizzleWrapper<0, 1, 3> xyw;
    Vec3SwizzleWrapper<0, 2, 0> xzx;
    Vec3SwizzleWrapper<0, 2, 1> xzy;
    Vec3SwizzleWrapper<0, 2, 2> xzz;
    Vec3SwizzleWrapper<0, 2, 3> xzw;
    Vec3SwizzleWrapper<0, 3, 0> xwx;
    Vec3SwizzleWrapper<0, 3, 1> xwy;
    Vec3SwizzleWrapper<0, 3, 2> xwz;
    Vec3SwizzleWrapper<0, 3, 3> xww;
    Vec3SwizzleWrapper<1, 0, 0> yxx;
    Vec3SwizzleWrapper<1, 0, 1> yxy;
    Vec3SwizzleWrapper<1, 0, 2> yxz;
    Vec3SwizzleWrapper<1, 0, 3> yxw;
    Vec3SwizzleWrapper<1, 1, 0> yyx;
    Vec3SwizzleWrapper<1, 1, 1> yyy;
    Vec3SwizzleWrapper<1, 1, 2> yyz;
    Vec3SwizzleWrapper<1, 1, 3> yyw;
    Vec3SwizzleWrapper<1, 2, 0> yzx;
    Vec3SwizzleWrapper<1, 2, 1> yzy;
    Vec3SwizzleWrapper<1, 2, 2> yzz;
    Vec3SwizzleWrapper<1, 2, 3> yzw;
    Vec3SwizzleWrapper<1, 3, 0> ywx;
    Vec3SwizzleWrapper<1, 3, 1> ywy;
    Vec3SwizzleWrapper<1, 3, 2> ywz;
    Vec3SwizzleWrapper<1, 3, 3> yww;
    Vec3SwizzleWrapper<2, 0, 0> zxx;
    Vec3SwizzleWrapper<2, 0, 1> zxy;
    Vec3SwizzleWrapper<2, 0, 2> zxz;
    Vec3SwizzleWrapper<2, 0, 3> zxw;
    Vec3SwizzleWrapper<2, 1, 0> zyx;
    Vec3SwizzleWrapper<2, 1, 1> zyy;
    Vec3SwizzleWrapper<2, 1, 2> zyz;
    Vec3SwizzleWrapper<2, 1, 3> zyw;
    Vec3SwizzleWrapper<2, 2, 0> zzx;
    Vec3SwizzleWrapper<2, 2, 1> zzy;
    Vec3SwizzleWrapper<2, 2, 2> zzz;
    Vec3SwizzleWrapper<2, 2, 3> zzw;
    Vec3SwizzleWrapper<2, 3, 0> zwx;
    Vec3SwizzleWrapper<2, 3, 1> zwy;
    Vec3SwizzleWrapper<2, 3, 2> zwz;
    Vec3SwizzleWrapper<2, 3, 3> zww;
    Vec3SwizzleWrapper<3, 0, 0> wxx;
    Vec3SwizzleWrapper<3, 0, 1> wxy;
    Vec3SwizzleWrapper<3, 0, 2> wxz;
    Vec3SwizzleWrapper<3, 0, 3> wxw;
    Vec3SwizzleWrapper<3, 1, 0> wyx;
    Vec3SwizzleWrapper<3, 1, 1> wyy;
    Vec3SwizzleWrapper<3, 1, 2> wyz;
    Vec3SwizzleWrapper<3, 1, 3> wyw;
    Vec3SwizzleWrapper<3, 2, 0> wzx;
    Vec3SwizzleWrapper<3, 2, 1> wzy;
    Vec3SwizzleWrapper<3, 2, 2> wzz;
    Vec3SwizzleWrapper<3, 2, 3> wzw;
    Vec3SwizzleWrapper<3, 3, 0> wwx;
    Vec3SwizzleWrapper<3, 3, 1> wwy;
    Vec3SwizzleWrapper<3, 3, 2> wwz;
    Vec3SwizzleWrapper<3, 3, 3> www;
  };

  constexpr Vec4() : x{0}, y{0}, z{0}, w{0} {}                // NOLINT
  constexpr Vec4(ScalarType v) : x{v}, y{v}, z{v}, w{v} {}    // NOLINT
  constexpr Vec4(ScalarType _x, ScalarType _y, ScalarType _z, // NOLINT
                 ScalarType _w)                               // NOLINT
      : x{_x}, y{_y}, z{_z}, w{_w} {}                         // NOLINT
  constexpr Vec4(Vec3 v3, ScalarType _w)                      // NOLINT
      : x{v3.x}, y{v3.y}, z{v3.z}, w{_w} {}                   // NOLINT

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
concept VectorType = requires(T vec) { std::is_base_of_v<VectorBase, T>; };

template <VectorType T>
auto Normalize(const T &&vec) -> T {
  T result = vec;
  return result.Normalize();
}

template <VectorType T>
auto Length(const T &&vec) -> typename T::ScalarType {
  return vec.Length();
}

template <VectorType T>
auto LengthSqr(const T &&vec) -> typename T::ScalarType {
  return vec.LengthSqr();
}

template <VectorType T>
auto operator*(T vec, const typename T::ScalarType scalar) -> T {
  return vec *= scalar;
}

template <VectorType T>
auto operator*(const typename T::ScalarType scalar, T vec) -> T {
  return vec *= scalar;
}

} // namespace lumina::math
