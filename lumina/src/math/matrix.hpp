#pragma once

#include "vector.hpp"

#include <utility>

namespace lumina::math {

class MatrixBase : public VectorBase {};

class Mat2 : public MatrixBase {
public:
  using RowType = Vec2;
  RowType rows[2]{};

  constexpr Mat2() noexcept : rows{{1, 0}, {0, 1}} {}
  constexpr Mat2(ScalarType _00, ScalarType _01, ScalarType _10,
                 ScalarType _11) noexcept
      : rows{{_00, _01}, {_10, _11}} {}
  constexpr Mat2(const RowType &_0, const RowType &_1) noexcept
      : rows{{_0.x, _0.y}, {_1.x, _1.y}} {}

  constexpr auto operator[](size_t idx) -> RowType & { return rows[idx]; }
  constexpr auto operator[](size_t idx) const -> const RowType & {
    return rows[idx];
  }

  static constexpr auto Identity() noexcept -> Mat2 { return {1, 0, 0, 1}; }
  static constexpr auto Zero() noexcept -> Mat2 { return {0, 0, 0, 0}; }
  static constexpr auto One() noexcept -> Mat2 { return {1, 1, 1, 1}; }

  auto T() -> Mat2 & {
    std::swap(rows[0][1], rows[1][0]);
    return *this;
  }

  [[nodiscard]] auto Det() const -> ScalarType {
    return (rows[0].x * rows[1].y) - (rows[0].y * rows[1].x);
  }

  [[nodiscard]] auto DataPtr() -> ScalarType * { return rows[0].DataPtr(); }
  [[nodiscard]] auto DataPtr() const -> const ScalarType * {
    return rows[0].DataPtr();
  }
};

class Mat3 : public MatrixBase {
public:
  using RowType = Vec3;
  RowType rows[3]{};

  constexpr Mat3() noexcept : rows{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}} {}
  constexpr Mat3(ScalarType _00, ScalarType _01, ScalarType _02, ScalarType _10,
                 ScalarType _11, ScalarType _12, ScalarType _20, ScalarType _21,
                 ScalarType _22) noexcept
      : rows{{_00, _01, _02}, {_10, _11, _12}, {_20, _21, _22}} {}
  constexpr Mat3(const RowType &_0, const RowType &_1,
                 const RowType &_2) noexcept
      : rows{{_0.x, _0.y, _0.z}, {_1.x, _1.y, _1.z}, {_2.x, _2.y, _2.z}} {}

  constexpr auto operator[](size_t idx) -> RowType & { return rows[idx]; }
  constexpr auto operator[](size_t idx) const -> const RowType & {
    return rows[idx];
  }

  static constexpr auto Identity() noexcept -> Mat3 {
    return {1, 0, 0, 0, 1, 0, 0, 0, 1};
  }
  static constexpr auto Zero() noexcept -> Mat3 {
    return {0, 0, 0, 0, 0, 0, 0, 0, 0};
  }
  static constexpr auto One() noexcept -> Mat3 {
    return {1, 1, 1, 1, 1, 1, 1, 1, 1};
  }

  auto T() -> Mat3 & {
    std::swap(rows[0][1], rows[1][0]);
    std::swap(rows[0][2], rows[2][0]);
    std::swap(rows[1][2], rows[2][1]);
    return *this;
  }

  [[nodiscard]] auto Det() const -> ScalarType {
    return (rows[0].x * (rows[1].y * rows[2].z - rows[1].z * rows[2].y)) -
           (rows[0].y * (rows[1].x * rows[2].z - rows[1].z * rows[2].x)) +
           (rows[0].z * (rows[1].x * rows[2].y - rows[1].y * rows[2].x));
  }

  [[nodiscard]] auto DataPtr() -> ScalarType * { return rows[0].DataPtr(); }
  [[nodiscard]] auto DataPtr() const -> const ScalarType * {
    return rows[0].DataPtr();
  }
};

class Mat4 : public MatrixBase {
public:
  using RowType = Vec4;
  RowType rows[4]{};

  constexpr Mat4() noexcept
      : rows{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}} {}
  constexpr Mat4(ScalarType _00, ScalarType _01, ScalarType _02, ScalarType _03,
                 ScalarType _10, ScalarType _11, ScalarType _12, ScalarType _13,
                 ScalarType _20, ScalarType _21, ScalarType _22, ScalarType _23,
                 ScalarType _30, ScalarType _31, ScalarType _32,
                 ScalarType _33) noexcept
      : rows{{_00, _01, _02, _03},
             {_10, _11, _12, _13},
             {_20, _21, _22, _23},
             {_30, _31, _32, _33}} {}
  constexpr Mat4(const RowType &_0, const RowType &_1, const RowType &_2,
                 const RowType &_3) noexcept
      : rows{{_0.x, _0.y, _0.z, _0.w},
             {_1.x, _1.y, _1.z, _1.w},
             {_2.x, _2.y, _2.z, _2.w},
             {_3.x, _3.y, _3.z, _3.w}} {}

  constexpr auto operator[](size_t idx) -> RowType & { return rows[idx]; }
  constexpr auto operator[](size_t idx) const -> const RowType & {
    return rows[idx];
  }

  static constexpr auto Identity() noexcept -> Mat4 {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  }
  static constexpr auto Zero() noexcept -> Mat4 {
    return {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  }
  static constexpr auto One() noexcept -> Mat4 {
    return {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  }

  auto T() -> Mat4 & {
    std::swap(rows[0][1], rows[1][0]);
    std::swap(rows[0][2], rows[2][0]);
    std::swap(rows[1][2], rows[2][1]);
    std::swap(rows[0][3], rows[3][0]);
    std::swap(rows[1][3], rows[3][1]);
    std::swap(rows[2][3], rows[3][2]);
    return *this;
  }

  [[nodiscard]] auto Det() const -> ScalarType {
    Mat3 m11{rows[1].yzw(), rows[2].yzw(), rows[3].yzw()};
    Mat3 m12{rows[1].xzw(), rows[2].xzw(), rows[3].xzw()};
    Mat3 m13{rows[1].xyw(), rows[2].xyw(), rows[3].xyw()};
    Mat3 m14{rows[1].xyz(), rows[2].xyz(), rows[3].xyz()};

    return (rows[0].x * m11.Det()) - (rows[0].y * m12.Det()) +
           (rows[0].z * m13.Det()) - (rows[0].w * m14.Det());
  }

  [[nodiscard]] auto DataPtr() -> ScalarType * { return rows[0].DataPtr(); }
  [[nodiscard]] auto DataPtr() const -> const ScalarType * {
    return rows[0].DataPtr();
  }
};

} // namespace lumina::math
