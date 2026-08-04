#pragma once

#include "core/input/input_state.hpp"

#include <array>

namespace lumina::platform::llinux {

// Sentinel value for XCB keycodes that have no KeyCode mapping
inline constexpr auto UNMAPPED_KEY = core::KeyCode::COUNT_;

// XCB keycodes are evdev scancodes + 8, consistent across standard PC keyboards.
// clang-format off
[[nodiscard]] constexpr auto BuildXcbToKeyCode()
    -> std::array<core::KeyCode, 256> {
  std::array<core::KeyCode, 256> table{};
  table.fill(UNMAPPED_KEY);

  // Letters (QWERTY physical layout)
  table[38] = core::KeyCode::A;
  table[56] = core::KeyCode::B;
  table[54] = core::KeyCode::C;
  table[40] = core::KeyCode::D;
  table[26] = core::KeyCode::E;
  table[41] = core::KeyCode::F;
  table[42] = core::KeyCode::G;
  table[43] = core::KeyCode::H;
  table[31] = core::KeyCode::I;
  table[44] = core::KeyCode::J;
  table[45] = core::KeyCode::K;
  table[46] = core::KeyCode::L;
  table[58] = core::KeyCode::M;
  table[57] = core::KeyCode::N;
  table[32] = core::KeyCode::O;
  table[33] = core::KeyCode::P;
  table[24] = core::KeyCode::Q;
  table[27] = core::KeyCode::R;
  table[39] = core::KeyCode::S;
  table[28] = core::KeyCode::T;
  table[30] = core::KeyCode::U;
  table[55] = core::KeyCode::V;
  table[25] = core::KeyCode::W;
  table[53] = core::KeyCode::X;
  table[29] = core::KeyCode::Y;
  table[52] = core::KeyCode::Z;

  // Numbers (top row)
  table[19] = core::KeyCode::Num0;
  table[10] = core::KeyCode::Num1;
  table[11] = core::KeyCode::Num2;
  table[12] = core::KeyCode::Num3;
  table[13] = core::KeyCode::Num4;
  table[14] = core::KeyCode::Num5;
  table[15] = core::KeyCode::Num6;
  table[16] = core::KeyCode::Num7;
  table[17] = core::KeyCode::Num8;
  table[18] = core::KeyCode::Num9;

  // Function keys
  table[67]  = core::KeyCode::F1;
  table[68]  = core::KeyCode::F2;
  table[69]  = core::KeyCode::F3;
  table[70]  = core::KeyCode::F4;
  table[71]  = core::KeyCode::F5;
  table[72]  = core::KeyCode::F6;
  table[73]  = core::KeyCode::F7;
  table[74]  = core::KeyCode::F8;
  table[75]  = core::KeyCode::F9;
  table[76]  = core::KeyCode::F10;
  table[95]  = core::KeyCode::F11;
  table[96]  = core::KeyCode::F12;

  // Modifiers
  table[50]  = core::KeyCode::LeftShift;
  table[62]  = core::KeyCode::RightShift;
  table[37]  = core::KeyCode::LeftCtrl;
  table[105] = core::KeyCode::RightCtrl;
  table[64]  = core::KeyCode::LeftAlt;
  table[108] = core::KeyCode::RightAlt;

  // Special keys
  table[65]  = core::KeyCode::Space;
  table[36]  = core::KeyCode::Enter;
  table[9]   = core::KeyCode::Escape;
  table[23]  = core::KeyCode::Tab;
  table[22]  = core::KeyCode::Backspace;
  table[119] = core::KeyCode::Delete;
  table[118] = core::KeyCode::Insert;
  table[110] = core::KeyCode::Home;
  table[115] = core::KeyCode::End;
  table[112] = core::KeyCode::PageUp;
  table[117] = core::KeyCode::PageDown;

  // Arrow keys
  table[113] = core::KeyCode::Left;
  table[114] = core::KeyCode::Right;
  table[111] = core::KeyCode::Up;
  table[116] = core::KeyCode::Down;

  return table;
}
// clang-format on

inline constexpr auto XCB_TO_KEY_CODE = BuildXcbToKeyCode();

} // namespace lumina::platform::llinux
