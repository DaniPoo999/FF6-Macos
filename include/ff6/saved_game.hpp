#pragma once
#include "ff6/field_loop.hpp"
#include <filesystem>
namespace ff6 {
// Save points retain semantic game state, not suspended routines or an emulated
// memory image. Loading reconstructs field presentation and runs map startup.
struct SavedFieldCharacter {
    FieldCharacterLocation location;
    std::uint8_t palette = 0, facing = 0;
    FieldRide ride = FieldRide::none;
    bool rider_visible = false;
    bool operator==(const SavedFieldCharacter&) const = default;
};
struct SavedGame {
    std::optional<WorldState> world;
    std::optional<FieldTilePosition> parked_airship;
    std::optional<WorldFlightResume> airship_resume;
    bool world_minimap = true, terra_magic_pending = false;
    Roster roster;
    Inventory inventory;
    FieldFlags flags;
    std::array<std::uint16_t, 8> event_variables{};
    std::array<FieldEventTimer, 4> timers{};
    std::array<std::uint8_t, 20> battle_variables{};
    std::bitset<512> encountered_formations;
    GameMenuState menu;
    InputSettings input;
    WindowPalettes colors{};
    std::uint8_t skin = 0;
    Rgb555 font_color = 32767;
    std::uint32_t steps = 0;
    std::array<SavedFieldCharacter, 16> characters{};
    FieldDestination destination, parent;
    FieldTilePosition scroll;
    std::uint8_t leader = 0, terrain_level = 1, map_song = 0, field_random = 0;
    FieldEntranceMemory travel;
    FieldBattleRandom battle_random;
    std::uint16_t encounter_counter = 0, doom_gaze_hp = 0;
    std::bitset<16> battle_conditions;
    BattleCommandRules battle_rules;
    // Reserved compatibility tail for the future SRAM adapter. Native captures
    // leave it empty; gameplay never reads it as state or executes its bytes.
    Bytes original_slot;
    std::array<std::uint8_t, 4> party_levels{1, 1, 1, 1};
};
void validate_saved_game(const SavedGame& game);
Bytes encode_saved_game(const SavedGame& game);
SavedGame decode_saved_game(ByteView bytes);
SavedGame capture_saved_game(const FieldLoopSeed& seed);
FieldLoop restore_saved_game(const ContentPack& pack, const SavedGame& game);
// RestartGame retains only level/experience for matching character identities.
// Inventory, story progress, learned magic and equipment come from the save.
SavedGame retain_game_over_progress(SavedGame saved, const Roster& defeated,
                                    const PartyData& growth);
struct SaveSlot {
    std::optional<SavedGame> game;
    bool recovered = false;
    std::string error;
};
// Each slot is independently published. A validated previous version survives
// interrupted writes, and corruption is reported separately from an empty slot.
class SaveStore {
  public:
    explicit SaveStore(std::filesystem::path folder);
    std::array<SaveSlot, 3> read() const;
    void write(unsigned slot, const SavedGame& game) const;
    const std::filesystem::path& folder() const {
        return folder_;
    }

  private:
    std::filesystem::path folder_;
};
} // namespace ff6
