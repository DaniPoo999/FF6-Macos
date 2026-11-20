#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace ff6 {
using Bytes = std::vector<std::uint8_t>;
using ByteView = std::span<const std::uint8_t>;

inline ByteView slice(ByteView bytes, std::size_t offset, std::size_t count) {
    if (offset > bytes.size() || count > bytes.size() - offset) {
        throw std::runtime_error("Byte range outside resource");
    }
    return bytes.subspan(offset, count);
}
inline std::uint16_t read16(ByteView bytes, std::size_t offset) {
    const auto s = slice(bytes, offset, 2);
    return static_cast<std::uint16_t>(std::uint32_t{s[0]} | (std::uint32_t{s[1]} << 8U));
}
inline std::uint32_t read24(ByteView bytes, std::size_t offset) {
    const auto s = slice(bytes, offset, 3);
    return std::uint32_t{s[0]} | (std::uint32_t{s[1]} << 8U) | (std::uint32_t{s[2]} << 16U);
}
inline std::uint32_t read32(ByteView bytes, std::size_t offset) {
    const auto s = slice(bytes, offset, 4);
    return std::uint32_t{read16(s, 0)} | (std::uint32_t{read16(s, 2)} << 16U);
}
inline void append16(Bytes& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
}
inline void append32(Bytes& out, std::uint32_t value) {
    append16(out, static_cast<std::uint16_t>(value & 0xffffU));
    append16(out, static_cast<std::uint16_t>(value >> 16U));
}
inline void append(Bytes& out, ByteView bytes) {
    out.insert(out.end(), bytes.begin(), bytes.end());
}

// These functions state original truncation/sign boundaries without CPU state.
constexpr std::uint8_t wrap8(std::uint32_t value) {
    return static_cast<std::uint8_t>(value & 0xffU);
}
constexpr std::uint16_t wrap16(std::uint32_t value) {
    return static_cast<std::uint16_t>(value & 0xffffU);
}
constexpr std::int32_t signed8(std::uint8_t value) {
    return value < 128 ? value : std::int32_t{value} - 256;
}
constexpr std::int32_t signed16(std::uint16_t value) {
    return value < 32768 ? value : std::int32_t{value} - 65536;
}
struct ByteSum {
    std::uint8_t value;
    bool carry;
};
constexpr ByteSum add8(std::uint8_t a, std::uint8_t b, bool carry) {
    const auto wide = std::uint32_t{a} + b + (carry ? 1U : 0U);
    return {wrap8(wide), wide > 255U};
}
struct ByteDifference {
    std::uint8_t value;
    bool no_borrow;
};
constexpr ByteDifference subtract8(std::uint8_t a, std::uint8_t b, bool no_borrow) {
    const auto rhs = std::uint32_t{b} + (no_borrow ? 0U : 1U);
    return {wrap8(256U + a - rhs), std::uint32_t{a} >= rhs};
}
} // namespace ff6
