#pragma once
// Files replaced whole or not at all, readable by their owner only (pp-browser AtomicFileWrite).

#include "common/Error.h"

#include <string>

namespace apps {

/** Write `data` to `path` atomically (temp file 0600, fsync, rename, fsync dir); parents made. */
pp::Roe<void> writeFileAtomic(const std::string &path, const std::string &data);
/** Like writeFileAtomic, but refuses an existing `path`. */
pp::Roe<void> writeNewFile(const std::string &path, const std::string &data);
/** A whole file's bytes. */
pp::Roe<std::string> readFile(const std::string &path);

} // namespace apps
