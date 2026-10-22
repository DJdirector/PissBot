cmake_minimum_required(VERSION 3.15)
project(pissbot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PISSBOT_BUILD_TESTS "Build the unit tests (run them with ctest)" ON)

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
target_link_libraries(pissbot_util PUBLIC SQLite3::SQLite3)

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

if(PISSBOT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
