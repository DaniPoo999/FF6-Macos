#pragma once
#include "ff6/battle_preparation.hpp"

namespace ff6 {
enum class BattleStatus : std::uint8_t {
    blind,
    zombie,
    poison,
    magitek,
    vanish,
    imp,
    petrify,
    dead,
    condemned,
    near_fatal,
    image,
    silence,
    berserk,
    confuse,
    sap,
    sleep,
    dance,
    regen,
    slow,
    haste,
    stop,
    shell,
    safe,
    reflect,
    rage,
    frozen,
    reraise,
    morph,
    control,
    hide,
    interceptor,
    floating
};
constexpr std::uint32_t status_mask(BattleStatus status) {
    return std::uint32_t{1} << static_cast<unsigned>(status);
}
enum class BattleStatusUpdate : std::uint8_t {
    quetzal,
    morph,
    speed,
    commands,
    stop_condemned,
    start_condemned,
    remove_advance_actions,
    spells
};
struct BattleStatusChange {
    std::uint32_t apply = 0, remove = 0;
    bool operator==(const BattleStatusChange&) const = default;
};
struct BattleStatusTarget {
    std::uint32_t active = 0, retaliation = 0;
    BattleStatusChange pending;
    std::bitset<8> updates;
    std::uint8_t sleep_counter = 0, stop_counter = 0, reflect_counter = 0, freeze_counter = 0;
    std::uint8_t equipment_refresh_count = 0;
    bool present = false, capture_retaliation = false;
    bool menu_allowed = false, wake_action_pending = false;
    bool overcast = false, phantasm = false;
    bool appearance_changed = false, reversed_facing = false, invisible = false;
    bool died = false, monster_alive = false, monster_dead_or_escaped = false;
    bool operator==(const BattleStatusTarget&) const = default;
};
struct BattleStatusState {
    std::array<BattleStatusTarget, 10> targets;
    // The last character's word-sized equipment increment can carry here.
    // Battle menu rendering owns subsequent interpretation of this index.
    std::uint8_t menu_window_graphics_index = 0;
};
struct BattleStatusContext {
    CombatVitals hp;
    std::uint32_t blocked_status = 0;
};
// Apply the complete descending pass. Absent targets retain their requests.
// Equipment refresh carry links adjacent character counters, including absent
// slots, so the pass must not be replaced by independent per-target calls.
void update_battle_status(BattleStatusState& state,
                          const std::array<BattleStatusContext, 10>& context);
// Fresh entry only. Preparation remains the immutable entry record; this
// returned owner holds live conditions, pending requests and refresh intentions.
BattleStatusState initialize_battle_status(const BattlePreparation& preparation,
                                           const BattleParty& party);
} // namespace ff6
