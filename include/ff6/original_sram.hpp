#pragma once
#include "ff6/bytes.hpp"
#include <array>

namespace ff6 {
inline constexpr std::size_t original_sram_size = 8192;
inline constexpr std::size_t original_save_slot_size = 2560;
inline constexpr std::size_t original_save_slot_count = 3;
struct OriginalSaveSlotCheck {
    std::uint16_t calculated = 0, stored = 0;
    // CheckSaveSlotChecksum returns zero both on mismatch and on a matching
    // zero checksum. Its callers treat both cases as an unavailable slot.
    bool loadable() const {
        return calculated != 0 && calculated == stored;
    }
    bool operator==(const OriginalSaveSlotCheck&) const = default;
};
std::uint16_t original_save_checksum(ByteView slot);

// Compatibility-file storage, never an authoritative gameplay RAM image.
// Construction copies the input, preserving every unknown serialized byte.
// There is no file I/O here; clearing invalid storage affects this owned copy.
class OriginalSram {
  public:
    OriginalSram() = default;
    explicit OriginalSram(ByteView encoded);
    ByteView encoded() const {
        return bytes_;
    }
    bool valid_header() const;
    ByteView slot(std::size_t index) const;
    OriginalSaveSlotCheck check_slot(std::size_t index) const;
    std::uint8_t random_seed() const;
    // 0 selects New Game, 1..3 select slots. With no available slot, the source
    // ignores the remembered byte. Otherwise malformed out-of-range values
    // reject rather than indexing outside the native menu's four choices.
    std::uint8_t initial_selection() const;
    // GameLoadMenu increments the seed BEFORE CheckSRAM. Returns whether the
    // selection UI is needed; rejection clears even the incremented seed.
    bool begin_load();
    // Field AB's separate post-menu increment, modulo 256. The owning menu
    // lifecycle must call this exactly once after its actual result is ready.
    std::uint8_t finish_load();

  private:
    std::array<std::uint8_t, original_sram_size> bytes_{};
};
} // namespace ff6
