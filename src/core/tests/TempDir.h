#pragma once

#include <filesystem>
#include <random>
#include <string>

namespace admin::test {

/** A fresh directory, removed with its contents at the end of scope. */
struct TempDir {
  std::filesystem::path path;
  TempDir() {
    std::random_device rd;
    path = std::filesystem::temp_directory_path() / ("pp-ledger-admin-test-" + std::to_string(rd()) + std::to_string(rd()));
    std::filesystem::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
  std::string operator/(const std::string &name) const { return (path / name).string(); }
};

} // namespace admin::test
