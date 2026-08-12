# Configure-time checks for the standard library features the engine relies on.
#
# A missing library feature does not report itself as one. std::expected going
# absent surfaces as "no template named 'expected' in namespace 'std'" in every
# renderer header that returns one - headers that are themselves correct, since
# they do include <expected>. The library preprocessed its entire body away
# because the compiler did not advertise a feature macro it gates on, and
# nothing in that error names the compiler. These checks move the discovery to
# configure time and say which half of the toolchain to fix.
#
# try_compile propagates CMAKE_CXX_STANDARD under CMP0067 (NEW by way of the
# cmake_minimum_required at the top level), so the checks below see the same
# C++23 the real build does. Directory-level add_compile_options are not
# propagated, so the warning flags in CompilerOptions.cmake cannot skew them.

include(CheckCXXSourceCompiles)

check_cxx_source_compiles(
  "#include <print>
   int main() { std::println(\"{}\", 1); }" LUMINA_HAVE_STD_PRINT)

check_cxx_source_compiles(
  "#include <expected>
   int main() { std::expected<int, int> e{1}; return e.value() - 1; }"
  LUMINA_HAVE_STD_EXPECTED)

# Distinguishes "compiler too old" from "library too old" when <expected> is
# missing. libstdc++ defines __cpp_lib_expected only when __cpp_concepts is at
# least 202002L, the value that signals P0848R3 (conditionally trivial special
# member functions). A compiler can be perfectly C++23-capable in every other
# respect and still fail this one, which is what makes the resulting error so
# misleading.
check_cxx_source_compiles("static_assert(__cpp_concepts >= 202002L);
   int main() {}" LUMINA_HAVE_CONCEPTS_202002)

if(NOT LUMINA_HAVE_STD_PRINT)
  message(
    FATAL_ERROR
      "<print> / std::println is unavailable, but is used by DBG_PRINT in "
      "lumina/src/common/debug_print.hpp.\n"
      "This is a standard library gap, not a language-standard one: "
      "-std=c++23 is already in effect and cannot supply a header the library "
      "does not ship. Note that Clang on Linux uses libstdc++ by default, so "
      "the Clang version is irrelevant here.\n"
      "  libstdc++: GCC 14 or newer (Debian/Ubuntu: libstdc++-14-dev)\n"
      "  libc++:    LLVM 18 or newer (Debian/Ubuntu: libc++-20-dev)\n"
      "  MSVC:      Visual Studio 2022 17.4 or newer")
endif()

if(NOT LUMINA_HAVE_STD_EXPECTED)
  if(NOT LUMINA_HAVE_CONCEPTS_202002)
    message(
      FATAL_ERROR
        "std::expected is unavailable because ${CMAKE_CXX_COMPILER_ID} "
        "${CMAKE_CXX_COMPILER_VERSION} does not advertise __cpp_concepts >= "
        "202002L, which libstdc++ requires before defining "
        "__cpp_lib_expected. The standard library is not at fault here - the "
        "compiler is.\n"
        "  Clang: 20 or newer (Debian/Ubuntu: clang-20)\n"
        "  GCC:   12 or newer\n"
        "Configure a FRESH build directory afterwards. A compiler cannot be "
        "swapped in place, and precompiled headers written by the old compiler "
        "are rejected outright by the new one.")
  else()
    message(
      FATAL_ERROR
        "std::expected is unavailable even though ${CMAKE_CXX_COMPILER_ID} "
        "${CMAKE_CXX_COMPILER_VERSION} supports the required concepts level, "
        "so the standard library is the old half.\n"
        "  libstdc++: GCC 12 or newer (Debian/Ubuntu: libstdc++-14-dev)\n"
        "  libc++:    LLVM 16 or newer")
  endif()
endif()

message(
  STATUS
    "C++ toolchain: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}, "
    "C++${CMAKE_CXX_STANDARD} - std::print and std::expected available")
