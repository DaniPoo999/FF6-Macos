#pragma once
#include "ff6/battle_refresh.hpp"
#include <variant>

namespace ff6 {
struct BattleControlLink {
    std::uint8_t target = 0;
    bool remove_pending = false;
    bool operator==(const BattleControlLink&) const = default;
};
struct BattleTargetLinks {
    std::optional<BattleControlLink> controls, controlled_by;
    std::optional<std::uint8_t> love_target, love_attacker, charm_target, charm_attacker;
    std::optional<std::uint8_t> seize_target, seize_attacker;
    bool control_menu = false;
    bool operator==(const BattleTargetLinks&) const = default;
};
struct BattleZingerLink {
    std::uint8_t attacker = 0, target = 0;
    bool operator==(const BattleZingerLink&) const = default;
};
struct BattleQuickState {
    std::uint8_t target = 0, remaining_actions = 0;
    bool operator==(const BattleQuickState&) const = default;
};
struct BattleRaiseAction {
    std::uint8_t target = 0;
    bool operator==(const BattleRaiseAction&) const = default;
};
enum class BattleMonsterVisibility { hide_keep_battle, revive_current_hp };
enum class BattleMonsterAnimation { instant, fade_down };
struct BattleMonsterVisibilityAction {
    std::bitset<6> monsters;
    BattleMonsterVisibility effect = BattleMonsterVisibility::hide_keep_battle;
    BattleMonsterAnimation animation = BattleMonsterAnimation::instant;
    bool operator==(const BattleMonsterVisibilityAction&) const = default;
};
enum class BattleRelicSpell : std::uint8_t { shell, safe, reflect };
struct BattleRelicSpellAction {
    std::uint8_t target = 0;
    BattleRelicSpell spell = BattleRelicSpell::shell;
    bool operator==(const BattleRelicSpellAction&) const = default;
};
struct BattleRestorePoseAction {
    std::uint8_t target = 0;
    bool operator==(const BattleRestorePoseAction&) const = default;
};
enum class BattleAnnouncement : std::uint8_t { back, pincer, side, preemptive };
struct BattleAnnouncementAction {
    BattleAnnouncement message = BattleAnnouncement::back;
    bool operator==(const BattleAnnouncementAction&) const = default;
};
struct BattleTimerExpiredAction {
    std::uint8_t target = 0;
    // Stop, reflect, freeze, sleep; condition removal belongs to action execution.
    std::bitset<4> expired;
    bool operator==(const BattleTimerExpiredAction&) const = default;
};
struct BattleDoomAction {
    std::uint8_t target = 0;
    bool operator==(const BattleDoomAction&) const = default;
};
struct BattleEscapeAction {
    bool operator==(const BattleEscapeAction&) const = default;
};
struct BattleScriptedEventAction {
    std::uint8_t identity = 0;
    bool operator==(const BattleScriptedEventAction&) const = default;
};
using BattleImmediateAction =
    std::variant<BattleRaiseAction, BattleMonsterVisibilityAction, BattleRelicSpellAction,
                 BattleRestorePoseAction, BattleAnnouncementAction, BattleTimerExpiredAction,
                 BattleDoomAction, BattleEscapeAction, BattleScriptedEventAction>;
// Game-command identities and their command-specific operands, not CPU code.
// The scheduler preserves these requests; execution needs native command handlers.
struct BattleQueuedCommand {
    std::uint8_t command = 0, attack = 0;
    std::uint16_t parameters = 0;
    std::uint8_t mp_cost = 0;
    bool operator==(const BattleQueuedCommand&) const = default;
};
struct BattleParticipantSchedule {
    std::array<std::optional<std::uint8_t>, 256> entries;
    std::uint8_t read = 0, write = 0;
    bool operator==(const BattleParticipantSchedule&) const = default;
};
struct BattleTurnState {
    std::uint16_t atb = 65535, advance_wait = 0;
    std::uint8_t advance_duration = 255, condemned = 0, morph_display = 0;
    BattleSpeedRates rates;
    bool player_input = false, weapon_locked = false, atb_stopped = false;
    bool status_actions_blocked = false, action_ready = false, actions_blocked = false;
    bool bypass_advance_wait = false, defending = false, reschedule_pending = false;
    bool menu_requests_allowed = true, near_fatal_available = true;
    std::vector<BattleQueuedCommand> normal, retaliation;
};
struct BattleObservedAttackValue {
    std::uint8_t attacker = 0, value = 0;
};
struct BattleAttackMemory {
    std::optional<std::uint8_t> retaliation_attacker;
    bool retaliation_pending = false;
    std::optional<BattleObservedAttackValue> command, attack, item, elements;
};
struct BattleMorphState {
    std::optional<std::uint8_t> owner;
    std::uint8_t saved_gauge = 0;
    std::uint16_t counter = 0, rate = 0;
    bool permanent = false, double_duration = false;
};
struct BattleMenuRequest {
    std::uint8_t character = 0;
    bool open = false, control = false;
    bool operator==(const BattleMenuRequest&) const = default;
};
struct BattleCharacterReadout {
    CombatVitals hp, mp;
    std::uint32_t status = 0;
    bool operator==(const BattleCharacterReadout&) const = default;
};
enum class BattleApproach : std::uint8_t { normal, back, pincer, side };
struct BattleParticipation {
    // Visibility commands can temporarily remove targets from this active set.
    std::bitset<10> active{1023}, targetable;
    std::bitset<4> enemy_characters, living_allies, living_enemy_characters;
    std::bitset<6> living_monsters;
    std::uint8_t ally_count = 0, enemy_count = 0;
};
struct BattleEntryState {
    BattleApproach approach = BattleApproach::normal;
    std::bitset<4> back_rows, displayed_back_rows;
    std::bitset<6> monster_back_rows;
    bool preemptive = false, back_or_pincer = false;
    bool gau_recruitment = false, final_scrolling = false;
};
struct BattleMonsterInformation {
    std::optional<std::uint16_t> name_identity;
    bool harder_to_escape = false, cannot_escape = false;
    bool cannot_scan = false;
    bool cannot_suplex = false, cannot_sketch = false;
    std::uint16_t identity = 0; // Original monster identity, distinct from name grouping.
    bool cannot_control = false;
    // Enemy GP Rain spends this battle-owned stock; victory awards the remainder.
    std::uint16_t remaining_gil = 0;
};
struct BattleMonsterNameRow {
    std::optional<std::uint16_t> identity;
    std::uint16_t count = 0;
    bool operator==(const BattleMonsterNameRow&) const = default;
};
struct BattleCounterReadout {
    std::uint8_t atb = 255, morph = 255, condemned = 255;
    bool operator==(const BattleCounterReadout&) const = default;
};
struct BattleDisplayState {
    std::array<BattleMonsterNameRow, 4> names;
    std::array<BattleCounterReadout, 4> counters;
    std::uint8_t distinct_names = 0, escape_difficulty = 0;
    bool cannot_run = false, escape_tools_blocked = false;
    bool operator==(const BattleDisplayState&) const = default;
};
struct BattleTargetTime {
    std::uint8_t status_fraction = 0, periodic_phase = 0;
    std::uint8_t poison_multiplier = 0; // Retained through curing/reapplying poison in this battle.
    std::uint16_t elapsed = 0;
    std::uint8_t escape_counter = 0, escape_rate = 0;
    bool poison_trigger = false, regen_trigger = false, periodic_pending = false;
};
struct BattleTimeState {
    std::uint8_t previous_clock = 0, status_phase = 0;
    std::uint16_t turns = 0, elapsed = 0;
    // Escape admission stays latched after consuming its request, until the
    // immediate/counter queue boundary admits ordinary work (C2/007D).
    bool wait_mode = false, escape_action_pending = false;
    std::bitset<4> escaped, cannot_escape;
    std::optional<std::uint8_t> roulette_target;
    std::array<BattleTargetTime, 10> targets;
};
struct BattlePlayerAttack {
    std::uint8_t attack = 0;
    // Encoded game target mask; not a host pointer or CPU address.
    std::uint16_t targets = 0;
    bool operator==(const BattlePlayerAttack&) const = default;
};
struct BattlePlayerCommand {
    std::uint8_t character = 0, command = 0;
    BattlePlayerAttack first;
    std::optional<BattlePlayerAttack> second;
    bool operator==(const BattlePlayerCommand&) const = default;
};
struct BattlePlayerQueue {
    std::array<std::optional<BattlePlayerCommand>, 4> pending;
    std::uint8_t read = 0, write = 0;
    bool operator==(const BattlePlayerQueue&) const = default;
};
struct BattlePlayerSelections {
    std::array<std::uint8_t, 4> items{255, 255, 255, 255}, dances{255, 255, 255, 255};
    std::array<std::uint16_t, 4> rages{65535, 65535, 65535, 65535};
    std::uint8_t last_dance = 0;
    bool operator==(const BattlePlayerSelections&) const = default;
};
struct BattleState {
    struct StealStock {
        std::array<std::uint8_t, 2> items{255, 255};
    };
    std::array<StealStock, 10> steal_stock;
    std::array<BattleAttackMemory, 10> attack_memory;
    // ForceField protects the battlefield, independently of each actor's gear.
    std::uint8_t nullified_elements = 0;
    BattleParticipation participation;
    // Authored target permission is independent of visibility and enemy turns.
    // Character scripts initialize it; AI TARGET_ON/OFF change it in battle.
    std::bitset<6> monster_targets{63};
    // AI protection suppresses damage at application, not targeting or ailments.
    // It belongs to the battle and applies equally to HP and MP (C2/13A7).
    std::bitset<10> invincible;
    BattleEntryState entry;
    std::array<BattleMonsterInformation, 6> monster_information;
    // Initial battle setup includes every monster in both sets. The scene's
    // position owner replaces these when it has resolved actual left/right groups.
    std::bitset<6> monsters_on_left{63}, monsters_on_right{63};
    BattleDisplayState display;
    std::array<CombatStats, 10> stats;
    BattleStatusState status;
    BattleAvailability availability;
    std::array<BattleTurnState, 10> turns;
    std::array<BattleAiState, 10> ai;
    // Control menus hold attack identities, including monster-specific attacks.
    std::array<std::array<std::optional<BattleSpell>, 4>, 4> control_commands;
    BattleParticipantSchedule advance_schedule, ready_schedule, retaliation_schedule;
    BattleTimeState time;
    BattlePlayerQueue player_commands;
    BattlePlayerSelections selections;
    BattleInventory inventory;
    BattleSkills skills;
    // LearnLore records observation during combat; ordinary victory commits
    // the new entries. Escapes and scripted returns do not award them.
    std::bitset<24> observed_lores;
    // Current logical terrain, including Dance changes even on preserved scenes.
    std::optional<std::uint8_t> terrain_background;
    BattleMorphState morph;
    BattleRandom random;
    std::uint8_t speed_constant = 255;
    bool colosseum = false, retaliation_action = false, menus_disabled = false;
    std::bitset<10> untargetable;
    // Successful departures survive later party deaths (CheckBattleEnd).
    // Distinct from the timer's candidates and from temporary untargetability.
    std::bitset<4> departed_characters;
    std::bitset<6> departed_monsters;
    // Engulf must cover the original roster, including initially incapacitated
    // members. Later deaths/departures must not shrink this entry mask.
    std::bitset<4> potential_engulf_targets, engulfed_characters;
    // BabaBreath also clears field-party membership on return (C2/4936).
    // Ordinary running/Sneeze only depart this battle and must not do that.
    std::bitset<4> removed_from_party;
    // Availability differs from an actor allocated for a scripted encounter.
    // Shadow's mercenary departure also returns his Esper, unlike BabaBreath.
    std::uint16_t available_characters = 0;
    std::optional<std::uint8_t> shadow_departure;
    // C2/15ED: consumed by a successful desperation lottery, even for an
    // ineligible character identity. This belongs to the battle, not the save.
    std::bitset<4> desperation_used;
    bool departed(unsigned slot) const {
        return slot < 4 ? departed_characters[slot] : departed_monsters[slot - 4];
    }
    std::vector<BattleMenuRequest> menu_requests;
    std::array<BattleTargetLinks, 10> links;
    std::array<std::optional<std::uint8_t>, 4> actors;
    // Relic identities are needed by Umaro; their generic effect bits are insufficient.
    std::array<std::array<std::uint8_t, 2>, 4> relics{};
    // Original character attributes/equipment, separate from mutable combat
    // stats. Recalculation must not use already boosted or damaged statistics.
    std::array<std::optional<Character>, 4> equipment_characters;
    std::bitset<4> equipment_changed, genji_gloves;
    std::array<std::array<std::uint8_t, 2>, 4> block_graphics{};
    // Incoming participants skip the first pass. Jump/Seize postpone death
    // until their return; deferred_death also represents Air Anchor's effect.
    std::bitset<10> arrival_override, away_from_battle, deferred_death;
    std::optional<BattleZingerLink> zinger;
    std::optional<BattleQuickState> quick;
    // The final free action still cannot renew Quick; the main-loop boundary
    // releases this guard after that action finishes.
    bool quick_recast_blocked = false;
    // AI recruitment is committed to the persistent roster on battle return.
    std::bitset<4> recruited_characters;
    // Only the properties replaced by Rage are restored when that status ends.
    std::array<std::optional<CombatStats>, 4> rage_baseline;
    std::optional<std::uint8_t> last_rage_user;
    std::array<std::uint8_t, 4> rage_special_animations{};
    bool leap_allowed = true;
    std::uint32_t field_steps = 0;
    std::uint8_t end_event = 0;
    bool event_timer_expired = false;
    // Monster entry/exit operation 4 holds the empty battlefield until its
    // authored replacement appears or the action queue drains.
    bool ending_inhibited = false;
    bool waive_next_action_mp = false, terra_magic_pending = false;
    // FIFO intentions; this owner neither executes them nor restores HP.
    std::vector<BattleImmediateAction> immediate_actions;
    std::array<BattleCharacterReadout, 4> character_display;
};
BattleState initialize_battle_state(const BattlePreparation& preparation, const BattleParty& party,
                                    const BattleStatusState& status);
// Complete AfterAction1, including the second control cleanup pass and display
// publication. Actions remain queued; malformed input leaves state unchanged.
void resolve_battle_status_consequences(BattleState& state);
std::bitset<4> refresh_battle_equipment(BattleState&, const PartyData&, const EquipmentData&,
                                       const BattleCommandData&);
// Shared C2/07C8 cleanup for death and departure; the caller owns its transaction.
void release_battle_target_links(BattleState& state, std::uint8_t participant);
void validate_battle_state(const BattleState& state);
// All pending action classes share the original bounded capacity. Overflow is
// explicit until source pool-exhaustion behavior has its own supported contract.
void append_battle_immediate_action(BattleState& state, BattleImmediateAction action);
// Uses the prepared character spell menu, including relic-adjusted costs.
std::uint8_t battle_character_command_mp_cost(const BattleState& state, unsigned slot,
                                              BattleQueuedCommand command);
void append_battle_normal_action(BattleState& state, std::uint8_t participant,
                                 BattleQueuedCommand action);
void append_battle_retaliation_action(BattleState& state, std::uint8_t participant,
                                      BattleQueuedCommand action);
void schedule_battle_turn(BattleState& state, std::uint8_t participant, bool ready);
} // namespace ff6
