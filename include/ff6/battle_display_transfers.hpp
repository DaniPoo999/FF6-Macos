#pragma once
#include <cstdint>
#include <optional>
#include <vector>

namespace ff6 {
enum class BattleTransferOwner {
    sprite_geometry, effect_palettes, battlefield_palettes, sprite_palettes,
    status_images, character_image, column_offsets, armor_image,
    foreground_image, background_image, command_window, menu_window, menu_text,
    character_mp, character_gauge, damage_numerals, ending_image, resource
};
struct BattleDisplayTransfer {
    BattleTransferOwner owner;
    std::uint16_t bytes;
    bool operator==(const BattleDisplayTransfer&) const = default;
};
using BattleDisplayTransfers = std::vector<BattleDisplayTransfer>;
struct BattleMenuTransferRequests {
    bool command_window = false, menu_window = false, menu_text = false;
    bool character_readout = true;
};
// Shared display-pass order. Direct timer/HP/SwdTech stores are CPU work,
// not transfer bursts, and must be accounted for separately by the caller.
void append_battle_menu_transfers(BattleDisplayTransfers& transfers,
                                 BattleMenuTransferRequests requests);
BattleDisplayTransfers battle_publication_transfers(BattleDisplayTransfers actors,
    BattleMenuTransferRequests menus, bool foreground, bool background,
    bool damage_numerals, unsigned resource_bytes, bool ending_image);
// Ordered publication requests, not device registers or memory addresses.
// Keep burst boundaries: sixteen 32-byte requests do not have the same overhead
// as one 512-byte request. These are original packed bytes, not native tile size.
BattleDisplayTransfers battle_actor_transfers(bool hidden, std::uint8_t phase,
                                               std::optional<std::uint8_t> forced_character,
                                               bool pending_armor);
void append_battle_effect_transfer(BattleDisplayTransfers& transfers, bool foreground);
void append_battle_resource_transfer(BattleDisplayTransfers& transfers, unsigned bytes);
// Payload time only, in SNES master-clock units. Setup, synchronization, refresh
// and other contention are separate work; this must not be used as a complete
// transfer stall or converted directly into a scanline deadline.
std::uint32_t battle_transfer_payload_clocks(const BattleDisplayTransfers& transfers);
} // namespace ff6
