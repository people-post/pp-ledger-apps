#include "core/AtomicFile.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <unistd.h>

namespace admin {

namespace {

std::filesystem::path tempPathFor(const std::filesystem::path &path) {
  std::random_device rd;
  std::filesystem::path tmp = path;
  tmp += ".tmp." + std::to_string(::getpid()) + "." + std::to_string(std::mt19937_64(rd())());
  return tmp;
}

pp::Roe<void> writeOwnerOnly(const std::filesystem::path &path, const std::string &data) {
  const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) {
    return pp::Error("Cannot create " + path.string() + ": " + std::strerror(errno));
  }
  size_t written = 0;
  while (written < data.size()) {
    const ssize_t n = ::write(fd, data.data() + written, data.size() - written);
    if (n < 0) {
      const int err = errno;
      ::close(fd);
      return pp::Error("Cannot write " + path.string() + ": " + std::strerror(err));
    }
    written += static_cast<size_t>(n);
  }
  if (::fsync(fd) != 0) {
    const int err = errno;
    ::close(fd);
    return pp::Error("Cannot sync " + path.string() + ": " + std::strerror(err));
  }
  if (::close(fd) != 0) {
    return pp::Error("Cannot close " + path.string() + ": " + std::strerror(errno));
  }
  return {};
}

void syncDirectory(const std::filesystem::path &dir) {
  const int fd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY);
  if (fd >= 0) {
    (void)::fsync(fd);
    ::close(fd);
  }
}

} // namespace

pp::Roe<void> writeFileAtomic(const std::string &path, const std::string &data) {
  const std::filesystem::path target(path);
  std::error_code ec;
  if (target.has_parent_path()) {
    std::filesystem::create_directories(target.parent_path(), ec);
  }
  const std::filesystem::path tmp = tempPathFor(target);
  if (auto written = writeOwnerOnly(tmp, data); !written) {
    std::filesystem::remove(tmp, ec);
    return written;
  }
  std::filesystem::rename(tmp, target, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    return pp::Error("Cannot replace " + path + ": " + ec.message());
  }
  syncDirectory(target.has_parent_path() ? target.parent_path() : std::filesystem::path("."));
  return {};
}

pp::Roe<void> writeNewFile(const std::string &path, const std::string &data) {
  std::error_code ec;
  if (std::filesystem::exists(path, ec)) {
    return pp::Error(path + " exists already");
  }
  return writeFileAtomic(path, data);
}

pp::Roe<std::string> readFile(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return pp::Error("Cannot open " + path);
  }
  std::ostringstream bytes;
  bytes << in.rdbuf();
  if (!in && !in.eof()) {
    return pp::Error("Cannot read " + path);
  }
  return bytes.str();
}

} // namespace admin
