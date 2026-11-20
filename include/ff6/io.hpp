#pragma once
#include "ff6/bytes.hpp"
#include <filesystem>

namespace ff6 {
Bytes read_file(const std::filesystem::path& path, std::size_t maximum = 16U * 1024U * 1024U);
// Publish a complete file atomically; an existing destination is never replaced.
void write_new_file(const std::filesystem::path& path, ByteView bytes);
} // namespace ff6
