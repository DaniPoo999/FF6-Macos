#include "ff6/original_sram.hpp"
#include <algorithm>

namespace ff6 {
namespace {
constexpr std::size_t selection_offset = 0x1ff0, seed_offset = 0x1ff1;
constexpr std::array<std::size_t, 4> header_offsets{0x1ff8, 0x1ffa, 0x1ffc, 0x1ffe};
constexpr std::uint16_t header_marker = 0xe41b;
} // namespace
std::uint16_t original_save_checksum(ByteView slot) {
    if (slot.size() != original_save_slot_size)
        throw std::invalid_argument("Original save slot must contain exactly 2560 bytes");
    // Widen before summing: the full byte sum fits uint32_t. Only the final
    // sixteen-bit value is retained; the stored checksum bytes are excluded.
    std::uint32_t total = 0;
    for (auto byte : slot.first(original_save_slot_size - 2))
        total += byte;
    return wrap16(total);
}
OriginalSram::OriginalSram(ByteView encoded) {
    if (encoded.size() != original_sram_size)
        throw std::invalid_argument("Original SRAM must contain exactly 8192 bytes");
    std::ranges::copy(encoded, bytes_.begin());
}
bool OriginalSram::valid_header() const {
    return std::ranges::any_of(
        header_offsets, [&](auto offset) { return read16(bytes_, offset) == header_marker; });
}
ByteView OriginalSram::slot(std::size_t index) const {
    if (index >= original_save_slot_count)
        throw std::invalid_argument("Original save slot index is outside the three slots");
    return ByteView(bytes_).subspan(index * original_save_slot_size, original_save_slot_size);
}
OriginalSaveSlotCheck OriginalSram::check_slot(std::size_t index) const {
    const auto bytes = slot(index);
    return {original_save_checksum(bytes), read16(bytes, original_save_slot_size - 2)};
}
std::uint8_t OriginalSram::random_seed() const {
    return bytes_[seed_offset];
}
std::uint8_t OriginalSram::initial_selection() const {
    if (!valid_header())
        throw std::logic_error("Invalid SRAM has no load-menu selection");
    bool available = false;
    for (std::size_t i = 0; i < original_save_slot_count; ++i)
        available = available || check_slot(i).loadable();
    if (!available)
        return 0;
    const auto remembered = bytes_[selection_offset];
    if (remembered > original_save_slot_count)
        throw std::invalid_argument("Original SRAM remembers an invalid menu selection");
    return remembered;
}
bool OriginalSram::begin_load() {
    bytes_[seed_offset] = wrap8(std::uint32_t{random_seed()} + 1);
    if (valid_header())
        return true;
    bytes_.fill(0);
    return false;
}
std::uint8_t OriginalSram::finish_load() {
    bytes_[seed_offset] = wrap8(std::uint32_t{random_seed()} + 13);
    return random_seed();
}
} // namespace ff6
