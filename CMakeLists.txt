cmake_minimum_required(VERSION 3.15)
project(pissbot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PISSBOT_BUILD_TESTS "Build the unit tests (run them with ctest)" ON)
option(PISSBOT_STATIC
       "Produce a fully static executable with no runtime dependencies (build on Alpine/musl, see scripts/build-static.sh)"
       OFF)

if(PISSBOT_STATIC)
    # D++ as a static library, without voice (drops libopus; the bot only uses
    # voice-state events, not voice connections), and prefer .a files for
    # SQLite, zlib and OpenSSL so everything is linked into the executable.
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_VOICE_SUPPORT OFF CACHE BOOL "" FORCE)
    set(DPP_INSTALL OFF CACHE BOOL "" FORCE)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# D++ is pinned to an exact commit so builds are reproducible. Upgrading D++ is
# a deliberate change: bump this hash, rebuild and run the tests. (Following the
# master branch previously broke modal dialogs without any change on our side.)
include(FetchContent)
FetchContent_Declare(
    dpp
    GIT_REPOSITORY https://github.com/brainboxdotcc/DPP.git
    GIT_TAG        786ceb2e24f286d4e7f4b6b4a5dd934effdee6a4
)

# Disable building DPP tests/demos to speed up compile time
set(DPP_BUILD_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(dpp)

find_package(SQLite3 REQUIRED)

# Pure logic with no Discord dependency (fast to build and test).
add_library(pissbot_util STATIC
    src/template.cpp
    src/database.cpp
    src/answers.cpp
)
target_include_directories(pissbot_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
# Use the variables rather than an imported target: CMake's own module names
# the target SQLite::SQLite3, and the variables work with every CMake version.
target_include_directories(pissbot_util PUBLIC ${SQLite3_INCLUDE_DIRS})
target_link_libraries(pissbot_util PUBLIC ${SQLite3_LIBRARIES})

# Module framework and modules. New files in src/modules/ are picked up
# automatically; re-run cmake after adding one.
file(GLOB MODULE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/*.cpp
)
add_library(pissbot_modules STATIC
    src/common.cpp
    src/module.cpp
    ${MODULE_SOURCES}
)
target_link_libraries(pissbot_modules PUBLIC pissbot_util dpp)

add_executable(pissbot src/main.cpp)
target_link_libraries(pissbot PRIVATE pissbot_modules)
if(PISSBOT_STATIC)
    target_link_options(pissbot PRIVATE -static)
endif()

if(PISSBOT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
