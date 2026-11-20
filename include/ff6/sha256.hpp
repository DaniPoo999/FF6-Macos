#pragma once
#include "ff6/bytes.hpp"
#include <array>
#include <string_view>

namespace ff6 {
using Digest = std::array<std::uint8_t, 32>;
Digest sha256(ByteView input);
std::string hex(const Digest& digest);
Digest parse_digest(std::string_view value);
} // namespace ff6
