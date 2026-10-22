#pragma once
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

// Unlike assert(), CHECK still works in Release builds.
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond    \
                << std::endl;                                                  \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

// A throwaway database path in the temp directory.
inline std::string temp_db_path(const std::string &name) {
  auto p = std::filesystem::temp_directory_path() /
           (name + "_" + std::to_string(getpid()) + ".db");
  std::filesystem::remove(p);
  return p.string();
}
