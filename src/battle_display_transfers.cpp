#include "ff6/battle_display_transfers.hpp"
#include <limits>
#include <stdexcept>

namespace ff6 {
void append_battle_menu_transfers(BattleDisplayTransfers& transfers,
                                 BattleMenuTransferRequests requests) {
    if (requests.command_window)
        transfers.push_back({BattleTransferOwner::command_window, 64});
    if (requests.menu_window)
        transfers.push_back({BattleTransferOwner::menu_window, 64});
    if (requests.menu_text)
        transfers.push_back({BattleTransferOwner::menu_text, 128});
    if (requests.character_readout) {
        // Current/maximum MP and cost, then one character gauge in each of
        // the three window layouts. A closed menu does not suppress this work.
        transfers.insert(transfers.end(), 2, {BattleTransferOwner::character_mp, 14});
        transfers.insert(transfers.end(), 3, {BattleTransferOwner::character_gauge, 12});
    }
}
BattleDisplayTransfers battle_publication_transfers(BattleDisplayTransfers actors,
    BattleMenuTransferRequests menus, bool foreground, bool background,
    bool damage_numerals, unsigned resource_bytes, bool ending_image) {
    append_battle_menu_transfers(actors, menus);
    if (foreground)
        append_battle_effect_transfer(actors, true);
    if (background)
        append_battle_effect_transfer(actors, false);
    if (damage_numerals)
        actors.push_back({BattleTransferOwner::damage_numerals, 128});
    if (resource_bytes)
        append_battle_resource_transfer(actors, resource_bytes);
    if (ending_image)
        actors.push_back({BattleTransferOwner::ending_image, 2048});
    return actors;
}
BattleDisplayTransfers battle_actor_transfers(bool hidden, std::uint8_t phase,
                                               std::optional<std::uint8_t> forced_character,
                                               bool pending_armor) {
    if (forced_character && *forced_character >= 4)
        throw std::invalid_argument("Invalid forced character transfer");
    // Shared publication precedes preparation. The palette extents include
    // unchanged banks; visibility suppresses character/status images only.
    BattleDisplayTransfers result{{BattleTransferOwner::sprite_geometry, 544},
                                  {BattleTransferOwner::effect_palettes, 160},
                                  {BattleTransferOwner::battlefield_palettes, 96},
                                  {BattleTransferOwner::sprite_palettes, 256}};
    if (!hidden) {
        result.push_back({BattleTransferOwner::status_images, 128});
        const auto character = [&] {
            result.push_back({BattleTransferOwner::character_image, 128});
            result.push_back({BattleTransferOwner::character_image, 128});
        };
        character();
        if (!pending_armor && forced_character && *forced_character != (phase & 3U))
            character();
    }
    result.push_back({BattleTransferOwner::column_offsets, 64});
    if (pending_armor)
        for (unsigned strip = 0; strip < 4; ++strip)
            result.push_back({BattleTransferOwner::armor_image, 128});
    return result;
}
void append_battle_effect_transfer(BattleDisplayTransfers& transfers, bool foreground) {
    const auto owner = foreground ? BattleTransferOwner::foreground_image
                                  : BattleTransferOwner::background_image;
    transfers.insert(transfers.end(), 16, BattleDisplayTransfer{owner, 32});
}
void append_battle_resource_transfer(BattleDisplayTransfers& transfers, unsigned bytes) {
    if (bytes == 0 || bytes > 1024)
        throw std::invalid_argument("Resource publication must be one nonempty chunk of at most 1024 bytes");
    transfers.push_back({BattleTransferOwner::resource, static_cast<std::uint16_t>(bytes)});
}
std::uint32_t battle_transfer_payload_clocks(const BattleDisplayTransfers& transfers) {
    std::uint32_t result = 0;
    for (const auto transfer : transfers) {
        if (transfer.bytes == 0 || static_cast<unsigned>(transfer.owner) >
                                       static_cast<unsigned>(BattleTransferOwner::resource))
            throw std::invalid_argument("Invalid battle transfer request");
        const auto clocks = std::uint32_t{transfer.bytes} * 8U;
        if (clocks > std::numeric_limits<std::uint32_t>::max() - result)
            throw std::invalid_argument("Battle transfer payload exceeds clock range");
        result += clocks;
    }
    return result;
}
} // namespace ff6
