#pragma once
#include "ff6/battle_character_view.hpp"

namespace ff6 {
struct BattleCharacterPaletteData {
    std::array<Rgb555, 11> petrify{}, frozen{};
    std::array<std::array<Rgb555, 2>, 3> skin{};
    std::array<Rgb555, 8> outline{};
    std::array<std::uint8_t, 4> outline_delay{};
};
BattleCharacterPaletteData load_battle_character_palette_data(const ContentPack& pack);
struct BattleCharacterPresentationControl {
    std::uint32_t active_status = 0, shown_status = 0;
    std::uint8_t base_graphics = 255, character = 255, ready_action = 0, status_sprite = 0;
    std::uint8_t vanish_counter = 0;
    bool frame_frozen = false, outlined = false;
    std::optional<std::array<Tile, 2>> condemned_overlay;
};
struct BattleCharacterPresentation {
    BattleCharacterLayout layout;
    std::array<BattleCharacterPresentationControl, 4> control;
    std::array<BattleCharacterFrame, 4> frames{};
    // One original transition can own this shared prior sheet at a time.
    BattleCharacterSheet previous_sheet{};
    std::uint8_t status_cursor = 0;
    bool transition_busy = false;
};
struct BattleCharacterAppearanceRules {
    bool disable_status_graphics = false, green_soldier_disguise = false;
};
struct BattleCharacterImageWork {
    // Work of the original image owner, from entry to its return marker. These
    // are source CPU cycles, not elapsed display clocks or a scanline deadline.
    // Bus stalls, interrupts and the surrounding display pass remain separate.
    std::uint32_t source_cycles = 0;
    std::uint16_t source_bytes = 0; // Copied bytes in the original planar format.
    std::uint8_t refreshed_slots = 0;
    bool operator==(const BattleCharacterImageWork&) const = default;
};
// Visits (palette_phase+1)&3 without advancing the caller-owned phase or clock.
void update_battle_character_palette(const BattleCharacterPaletteData& data, std::uint8_t counter,
                                     BattleCharacterPresentation& state);
// Visits status_cursor&3 and advances that byte even when a transition blocks
// changes. Graphics, shown conditions and pose selection publish together.
void update_battle_character_status(const BattleCharacterGraphicsData& data,
                                    BattleCharacterAppearanceRules rules,
                                    BattleCharacterPresentation& state);
// Complete ordinary/mirrored frame scheduling, transition wipe and prepared
// condemned-overlay composition. Unresolved out-of-sheet frame identities fail
// explicitly. The external scene owns publication of these prepared tiles.
// Returns source work for the actual decisions taken, including unchanged and
// frozen slots. No state or work is published if a frame is rejected.
BattleCharacterImageWork publish_battle_character_frames(bool magitek,
                                                         BattleCharacterPresentation& state);
// InitCharGfx caller: layout, x projection, then four palette/status visits.
// Initial buffer clearing and visibility belong to the scene initialization.
void initialize_battle_character_presentation(const BattleCharacterViewData& view,
                                              const BattleCharacterGraphicsData& graphics,
                                              const BattleCharacterPaletteData& palettes,
                                              const BattleCharacterLayoutRequest& request,
                                              BattleCharacterAppearanceRules rules,
                                              std::uint8_t& counter,
                                              BattleCharacterPresentation& state);
} // namespace ff6
