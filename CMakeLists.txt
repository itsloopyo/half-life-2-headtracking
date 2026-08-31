cmake_minimum_required(VERSION 3.20)
project(HalfLife2HeadTracking VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Half-Life 2 is 32-bit. Configure with -A Win32.")
endif()

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# MinHook (vendored in cameraunlock-core/vendor/minhook when present).
set(MINHOOK_DIR "${CMAKE_SOURCE_DIR}/cameraunlock-core/vendor/minhook")
if(EXISTS "${MINHOOK_DIR}/CMakeLists.txt")
    add_subdirectory(${MINHOOK_DIR} minhook)
else()
    include(FetchContent)
    # Pinned to the commit SHA, not the tag: MinHook is statically linked into
    # the shipped .asi and this is a network fetch on every clean build,
    # including the release job. A git tag is mutable, so a retagged upstream
    # would change what we ship with nothing in the diff to show it. The SHA is
    # v1.3.4.
    FetchContent_Declare(
        minhook
        GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
        GIT_TAG c3fcafdc10146beb5919319d0683e44e3c30d537  # v1.3.4
    )
    FetchContent_MakeAvailable(minhook)
endif()

set(CAMERAUNLOCK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_HOOKS ON CACHE BOOL "" FORCE)
add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

add_library(HalfLife2HeadTracking SHARED
    src/pch.cpp
    src/dllmain.cpp
    src/debug_log.cpp
    src/config.cpp
    src/plugin.cpp
    src/tracker_feed.cpp
    src/hotkey_handler.cpp
    src/detour.cpp
    src/camera_hook.cpp
    src/fov_override.cpp
    src/aim_state.cpp
    src/aim_point.cpp
    src/crosshair_hook.cpp
    src/game_state.cpp
    src/source_math.cpp
    src/builds/build_registry.cpp
    src/builds/steam_offsets.cpp
)

target_precompile_headers(HalfLife2HeadTracking PRIVATE src/pch.h)

# The same bar the test suite is held to. Everything in src/ writes into a
# running game's memory through offsets no header declares, so a truncation or
# an unreferenced local is worth stopping the build for.
if(MSVC)
    target_compile_options(HalfLife2HeadTracking PRIVATE /W4 /WX)
else()
    target_compile_options(HalfLife2HeadTracking PRIVATE -Wall -Wextra -Werror)
endif()

set_target_properties(HalfLife2HeadTracking PROPERTIES
    OUTPUT_NAME "HalfLife2HeadTracking"
    SUFFIX ".asi"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

target_include_directories(HalfLife2HeadTracking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(HalfLife2HeadTracking PRIVATE
    cameraunlock
    cameraunlock_hooks
    minhook
    ws2_32
)

enable_testing()
add_subdirectory(tests)
