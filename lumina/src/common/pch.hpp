#pragma once

// Precompiled header for Lumina.
//
// Only stable, external headers belong here. Project headers are deliberately
// excluded: anything listed below invalidates the PCH when it changes, which
// forces a full rebuild of every target that uses it (lumina_core alone is 22
// translation units). The standard library headers here are the ones Build
// Insights attributed the most wall time to — the <format>/<print> stack in
// particular is pulled in by debug_print.hpp and logger.hpp and so lands in
// nearly every translation unit.

#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <mutex>
#include <print>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
