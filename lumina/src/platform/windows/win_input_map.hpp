#pragma once

#include "core/input/input_state.hpp"

#include <Windows.h>

#include <array>

namespace lumina::platform::windows {

// Sentinel value for VK codes that have no KeyCode mapping
inline constexpr auto UNMAPPED_KEY = core::KeyCode::COUNT_;

// clang-format off
[[nodiscard]] constexpr auto BuildVkToKeyCode()
    -> std::array<core::KeyCode, 256> {
  std::array<core::KeyCode, 256> table{};
  table.fill(UNMAPPED_KEY);

  // Letters (VK_A–VK_Z are 0x41–0x5A, matching ASCII)
  table[0x41] = core::KeyCode::A;
  table[0x42] = core::KeyCode::B;
  table[0x43] = core::KeyCode::C;
  table[0x44] = core::KeyCode::D;
  table[0x45] = core::KeyCode::E;
  table[0x46] = core::KeyCode::F;
  table[0x47] = core::KeyCode::G;
  table[0x48] = core::KeyCode::H;
  table[0x49] = core::KeyCode::I;
  table[0x4A] = core::KeyCode::J;
  table[0x4B] = core::KeyCode::K;
  table[0x4C] = core::KeyCode::L;
  table[0x4D] = core::KeyCode::M;
  table[0x4E] = core::KeyCode::N;
  table[0x4F] = core::KeyCode::O;
  table[0x50] = core::KeyCode::P;
  table[0x51] = core::KeyCode::Q;
  table[0x52] = core::KeyCode::R;
  table[0x53] = core::KeyCode::S;
  table[0x54] = core::KeyCode::T;
  table[0x55] = core::KeyCode::U;
  table[0x56] = core::KeyCode::V;
  table[0x57] = core::KeyCode::W;
  table[0x58] = core::KeyCode::X;
  table[0x59] = core::KeyCode::Y;
  table[0x5A] = core::KeyCode::Z;

  // Numbers (top row, VK_0–VK_9 are 0x30–0x39)
  table[0x30] = core::KeyCode::Num0;
  table[0x31] = core::KeyCode::Num1;
  table[0x32] = core::KeyCode::Num2;
  table[0x33] = core::KeyCode::Num3;
  table[0x34] = core::KeyCode::Num4;
  table[0x35] = core::KeyCode::Num5;
  table[0x36] = core::KeyCode::Num6;
  table[0x37] = core::KeyCode::Num7;
  table[0x38] = core::KeyCode::Num8;
  table[0x39] = core::KeyCode::Num9;

  // Function keys
  table[VK_F1]  = core::KeyCode::F1;
  table[VK_F2]  = core::KeyCode::F2;
  table[VK_F3]  = core::KeyCode::F3;
  table[VK_F4]  = core::KeyCode::F4;
  table[VK_F5]  = core::KeyCode::F5;
  table[VK_F6]  = core::KeyCode::F6;
  table[VK_F7]  = core::KeyCode::F7;
  table[VK_F8]  = core::KeyCode::F8;
  table[VK_F9]  = core::KeyCode::F9;
  table[VK_F10] = core::KeyCode::F10;
  table[VK_F11] = core::KeyCode::F11;
  table[VK_F12] = core::KeyCode::F12;

  // Modifiers — use extended-key flag to distinguish L/R sides
  table[VK_LSHIFT]   = core::KeyCode::LeftShift;
  table[VK_RSHIFT]   = core::KeyCode::RightShift;
  table[VK_LCONTROL] = core::KeyCode::LeftCtrl;
  table[VK_RCONTROL] = core::KeyCode::RightCtrl;
  table[VK_LMENU]    = core::KeyCode::LeftAlt;
  table[VK_RMENU]    = core::KeyCode::RightAlt;

  // Special keys
  table[VK_SPACE]  = core::KeyCode::Space;
  table[VK_RETURN] = core::KeyCode::Enter;
  table[VK_ESCAPE] = core::KeyCode::Escape;
  table[VK_TAB]    = core::KeyCode::Tab;
  table[VK_BACK]   = core::KeyCode::Backspace;
  table[VK_DELETE] = core::KeyCode::Delete;
  table[VK_INSERT] = core::KeyCode::Insert;
  table[VK_HOME]   = core::KeyCode::Home;
  table[VK_END]    = core::KeyCode::End;
  table[VK_PRIOR]  = core::KeyCode::PageUp;
  table[VK_NEXT]   = core::KeyCode::PageDown;

  // Arrow keys
  table[VK_LEFT]  = core::KeyCode::Left;
  table[VK_RIGHT] = core::KeyCode::Right;
  table[VK_UP]    = core::KeyCode::Up;
  table[VK_DOWN]  = core::KeyCode::Down;

  return table;
}
// clang-format on

inline constexpr auto VK_TO_KEY_CODE = BuildVkToKeyCode();

} // namespace lumina::platform::windows
