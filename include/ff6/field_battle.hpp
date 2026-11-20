#pragma once
#include "ff6/field_state.hpp"
#include "ff6/colosseum.hpp"
#include "rom-materials/field_battle_mosaic_end.macros.inc"

namespace ff6 {
// Separate streams seeded together by load-menu return. Formation selection
// advances only its own pair; encounter eligibility and Veldt selection remain
// distinct consumers.
struct FieldBattleRandom {
    std::uint8_t encounter_index = 0, encounter_offset = 0;
    std::uint8_t formation_index = 0, formation_offset = 0, veldt = 0;
    bool operator==(const FieldBattleRandom&) const = default;
};
struct FieldBattleData {
    // Entry in the extracted event language, not original executable code.
    std::uint32_t random_event = 0x18;
    std::array<std::uint8_t, 256> random{};
    std::array<std::array<std::uint16_t, 2>, 256> event_groups{};
    std::array<std::array<std::uint16_t, 4>, 256> random_groups{};
    std::array<std::uint8_t, 512> map_groups{}, map_rates{};
    std::array<std::uint16_t, 16> rates{};
};
FieldBattleData decode_field_battle_data(ByteView random, ByteView groups);
FieldBattleData load_field_battle_data(const ContentPack& pack);
struct FieldBattleContext {
    FieldTilePosition scroll, party, contact;
    std::uint8_t map_background = 0, treasure_group = 0;
    bool continue_music = false;
};
struct FieldBattleReturn {
    FieldTilePosition scroll, party;
    bool run_startup = true, fade_in = false, keep_dimensions = false;
    bool operator==(const FieldBattleReturn&) const = default;
};
struct FieldBattleRequest {
    // Preserve bit 15: battle initialization later chooses among four variants.
    // This is the selected table word, not necessarily a final formation ID.
    std::uint16_t formation = 0;
    std::uint8_t background = 0;
    bool mosaic = true, sound = true, continue_music = false;
    bool on_veldt = false;
    FieldBattleReturn field_return;
    bool operator==(const FieldBattleRequest&) const = default;
};
FieldBattleRequest select_field_battle(const FieldBattleData& data, const EventStartBattle& command,
                                       const FieldBattleContext& context,
                                       FieldBattleRandom& random);
struct FieldLoopSeed;
// Called once per eligible completed step. Selection starts the shared encounter
// event; its party regrouping must finish before command 4E admits the battle.
bool check_field_random_battle(FieldLoopSeed& seed, const FieldBattleData& data,
                              std::uint8_t map_background);
// Complete only the event's scheduling operation. The battle and its return
// remain pending; no combat outcome or map reload is fabricated here.
bool queue_field_battle_request(FieldLoopSeed& seed, const FieldBattleData& data,
                                std::uint8_t map_background);
// The optional sound precedes these waits. Waiting advances field publication,
// not events/actions. The final wait blanks before another field image is shown.
class FieldBattleTransition {
  public:
    explicit FieldBattleTransition(bool mosaic) : enabled_(mosaic) {}
    bool complete() const {
        return !enabled_ || waits_ == FF6_ROM_VALUE_FIELD_BATTLE_MOSAIC_END_0;
    }
    std::uint8_t waits() const {
        return waits_;
    }
    // Returns a background sample block width, including the unseen final 1.
    std::uint8_t advance();

  private:
    bool enabled_;
    std::uint8_t waits_ = 0;
};
struct FieldBattleEntry {
    FieldBattleRequest request;
    std::uint8_t leader = 0, facing = 0, terrain_level = 0;
    bool operator==(const FieldBattleEntry&) const = default;
};
// Called after the transition, before native combat initialization. Preserves
// persistent party positions while publishing the field's current membership.
FieldBattleEntry prepare_field_battle_entry(FieldLoopSeed& seed);
} // namespace ff6
