# Compiler-specific options and warnings

# CPU zone profiling, on for every configuration that exists today. Performance
# is measured in Release, so a Debug-only profiler — the ASSERT pattern — would
# instrument the build nobody profiles. Written as a per-config expression so a
# future shipping configuration switches it off by name rather than by editing
# call sites.
add_compile_definitions(
  "LUMINA_PROFILING_ENABLED=$<IF:$<CONFIG:Shipping>,0,1>")

# Emit a PDB for every configuration, Release included. Optimizations and NDEBUG
# stay exactly as they were, so release-only bugs still reproduce; the symbols
# just make the resulting crash readable in a debugger.
if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase")
  add_link_options("$<$<NOT:$<CONFIG:Debug>>:/DEBUG>"
                   "$<$<NOT:$<CONFIG:Debug>>:/OPT:REF>"
                   "$<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  message(STATUS "Using MSVC compiler")
  # MSVC compiler options
  add_compile_options(
    /W4 # Warning level 4
    /WX- # Don't treat warnings as errors
    /external:W0 # Warning level 0 for headers reached through a SYSTEM include
                 # directory; without it /W4 still applies to them
    /permissive- # Conformance mode
    /Zc:__cplusplus # Enable __cplusplus macro
    /Zc:preprocessor # Enable conforming preprocessor
  )

  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  message(STATUS "Using GCC/Clang compiler")
  # GCC/Clang compiler options
  add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -Wno-c++98-compat
    -Wno-c++98-compat-pedantic
    -Wno-extra-semi-stmt
    -Wno-switch-default
    # -Wswitch-enum demands every enumerator be listed even when a `default:`
    # is present, which is unworkable for the third-party enums we switch over
    # (Clay commands, Vulkan flag bits with their MAX_ENUM sentinel, 871 SPIR-V
    # opcodes) - those need a default precisely because upstream may add values.
    # Plain -Wswitch, still enabled, is the check worth keeping: it flags a
    # missing enumerator in a switch that has no default, which is how the
    # switches over our own enums are written.
    -Wno-switch-enum
    # Every target here is a STATIC or INTERFACE library linked into a single
    # executable; there is no shared library for a mutable inline object to be
    # duplicated across. Revisit if Lumina ever ships a DLL or .so, because the
    # warning is correct in that world.
    -Wno-unique-object-duplication
    # Lumina deliberately uses Meyers singletons (Logger, PlatformServices) and
    # a handful of namespace-scope globals with explicit init/shutdown calls.
    # Silencing these two is the point of that design, not a gap in it: a
    # function-local static is already the fix for the initialization-order
    # problem the warnings exist to prevent, and the alternative - never
    # destroying the objects - trades a benign exit-time destructor for a
    # deliberate leak and loses RAII shutdown.
    -Wno-global-constructors
    -Wno-exit-time-destructors
    # Fires on partially designated-initialized Vulkan and Clay structs, where
    # leaving pNext/flags/resolveMode out is the idiom: C++ initializes an
    # omitted member as if by `= {}`, which is precisely the nullptr/0 those
    # fields require. Spelling them out would trade a guarantee the language
    # already makes for several lines of noise per struct literal, and would
    # bury the fields that carry actual information.
    -Wno-missing-designated-field-initializers
    # Reports layout facts, not defects, and fires on things that cannot be
    # reordered anyway (lambda closure types) or must not be (the cache-line
    # padding in lock_free_concurrent_queue.hpp, which is the point of that
    # type). Worth switching back on deliberately when hunting struct bloat.
    -Wno-padded
    -Wno-gnu-anonymous-struct
    -Wno-unsafe-buffer-usage-in-libc-call
    -Wno-nested-anon-types
    -Wno-c++17-extensions
    -Wno-c++20-extensions
    -Wno-c++20-compat
    -Wno-unsafe-buffer-usage)

endif()
