#pragma once
#include "ff6/battle_action_preparation.hpp"
#include "ff6/battle_magitek.hpp"

namespace ff6 {
struct BattlePhysicalData {
    BattleActionPreparationData targeting;
    BattleInventoryData inventory;
    std::array<std::array<std::uint16_t, 4>, 4> umaro_weights{};
    std::array<std::array<std::uint8_t, 2>, 384> sketch_attacks{};
};
BattlePhysicalData load_battle_physical(const ContentPack& pack);
// Fight admission only, once before weapon repetitions (C2/15C8). A returned
// spell replaces Fight; the ordinary magic owner executes its authored effect.
std::optional<std::uint8_t> choose_battle_desperation(BattleState&, const BattlePhysicalData&,
    std::uint8_t attacker, BattleQueuedCommand);
enum class BattleUmaroAttackKind { fight, tackle, storm, ally_throw };
struct BattleUmaroAttack {
    BattleUmaroAttackKind kind = BattleUmaroAttackKind::fight;
    std::uint8_t user = 0;
    std::optional<std::uint8_t> ally;
};
// Fight admission chooses once, before weapon repetitions. Hidden allies allow
// the Ring lottery, but cannot be thrown; an empty final pool falls back to Tackle.
BattleUmaroAttack choose_battle_umaro_attack(BattleState&, const BattlePhysicalData&, std::uint8_t);

struct BattlePhysicalEnvironment {
    BattleTargetEnvironment targets;
    // Owned by equipment, formation/facing and active summon effects. These
    // are game inputs, not renderer or machine-register state.
    std::array<std::uint8_t, 4> block_graphics{};
    std::bitset<10> hit_from_behind;
    std::uint16_t golem_hp = 0;
    std::uint8_t nullified_elements = 0;
    bool random_retarget = false;
    // The action owner retains Jump's chosen hand across Dragon Horn landings.
    std::optional<std::uint8_t> weapon_hand;
    // Sketch loads offensive properties from a monster; the character still
    // owns level, damage modifiers, target selection, MP and action history.
    std::optional<std::uint8_t> property_source;
    unsigned remaining_hits = 0;
    std::uint8_t clock = 0; // Logical game frame byte, used by Fixed Dice.
    bool counterattack = false, prevent_weapon_break = false;
};
struct BattlePhysicalImpact {
    std::uint8_t slot = 0;
    bool hit = false;
    // Separate amounts preserve drains onto the attacker and simultaneous
    // healing/damage. No value means no numeral; zero is a displayed zero.
    std::optional<std::uint16_t> damage, healing;
};
struct BattlePhysicalResult {
    std::uint8_t attacker = 0;
    bool special = false, critical = false, affects_mp = false, capture = false;
    // C2/3865 records the selected mask before Cover redirects the impact.
    // Mimic reuses that selection, not the protector or a new random target.
    std::uint16_t selected_targets = 0;
    // Attack targets stay separate from additional drain-recipient numerals.
    std::bitset<10> targets, hits;
    std::optional<std::uint8_t> covered_target, golem_target, interceptor_target;
    std::array<std::uint8_t, 4> block_animations{};
    std::vector<BattlePhysicalImpact> impacts;
    std::vector<BattleMagicMessage> messages;
    bool suppress_retaliation = false, chainsaw_death = false;
    std::uint8_t weapon_hand = 0, weapon_animation = 0;
    std::optional<std::uint8_t> sketch_source, sketch_attack;
    std::optional<std::uint8_t> followup_spell, death_animation;
    bool followup_same_target = false, thrown_weapon = false;
    std::bitset<6> rematerialize;
    std::array<std::uint8_t, 3> dice{}; // Face values; zero disables a die.
};
// Facing is sampled at attack admission. Pincer monster attacks and side
// character attacks can strike opponents facing the same direction in the back.
std::bitset<10> battle_rear_attack_targets(BattleApproach approach, std::uint8_t attacker,
                                          std::bitset<10> facing_right);
BattlePhysicalResult resolve_battle_umaro_physical(BattleState&, BattlePhysicalEnvironment&,
    const BattlePhysicalData&, BattleUmaroAttack, BattleQueuedCommand);
// Standard monster Fight/Special through status application. The caller owns
// admission, facing, graphics, sound and after-action consequences. Weapon
// follow-ups and repeated hands are scheduled by the action owner.
// Failures leave both the battle and environment unchanged.
BattlePhysicalResult resolve_battle_monster_physical(BattleState& battle,
                                                     BattlePhysicalEnvironment& environment,
                                                     const BattlePhysicalData& data,
                                                     std::uint8_t attacker,
                                                     BattleQueuedCommand command);
// One weapon strike. The result describes any free spell follow-up; the action
// owner schedules it and supplies the retained hand/repetition state.
BattlePhysicalResult resolve_battle_character_physical(BattleState& battle,
                                                       BattlePhysicalEnvironment& environment,
                                                       const BattlePhysicalData& data,
                                                       std::uint8_t attacker,
                                                       BattleQueuedCommand command);
BattlePhysicalResult
resolve_battle_physical_technique(BattleState& battle, BattlePhysicalEnvironment& environment,
                                  const BattlePhysicalData& data, std::uint8_t attacker,
                                  BattleQueuedCommand command, const BattleMagicEffect& technique);
BattlePhysicalResult resolve_battle_rage_physical(BattleState&, BattlePhysicalEnvironment&,
    const BattlePhysicalData&, std::uint8_t, BattleQueuedCommand);
BattlePhysicalResult resolve_battle_steal(BattleState&, BattlePhysicalEnvironment&,
                                          const BattlePhysicalData&, std::uint8_t attacker,
                                          BattleQueuedCommand);
BattlePhysicalResult resolve_battle_possess(BattleState&, BattlePhysicalEnvironment&,
    const BattlePhysicalData&, std::uint8_t attacker, BattleQueuedCommand);
BattlePhysicalResult resolve_battle_tool_physical(BattleState&, BattlePhysicalEnvironment&,
                                                  const BattlePhysicalData&, std::uint8_t attacker,
                                                  BattleQueuedCommand, const BattleMagicEffect&);
std::uint16_t battle_character_physical_base(std::uint8_t power, std::uint8_t strength,
                                            std::uint8_t level, bool two_handed = false);
std::uint16_t battle_monster_physical_base(std::uint8_t power, std::uint8_t strength,
                                          std::uint8_t level);
} // namespace ff6
