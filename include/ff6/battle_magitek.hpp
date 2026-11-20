#pragma once
#include "rom-materials/battle_sketch_level_boost.macros.inc"
#include "ff6/battle_state.hpp"

namespace ff6 {
struct BattleMagicEffect {
    std::uint8_t targeting = 0, elements = 0, flags = 0, options = 0, behavior = 0;
    std::uint8_t power = 0, status_only = 0, hit_rate = 0, special = 255;
    std::uint32_t status = 0;
    std::uint8_t original_mp_cost = 0; // Runic uses this, not the queued/relic-adjusted cost.
};
struct BattleMagitekData {
    std::array<BattleMagicEffect, 8> effects;
    std::array<std::uint8_t, 256> random{};
};
BattleMagitekData load_battle_magitek(const ContentPack& pack);
struct BattleMagicData {
    std::array<std::array<std::uint8_t, 4>, 32> metamorph_items{};
    BattleInventoryData inventory;
    std::array<std::array<std::uint8_t, 4>, 384> control_attacks{};
    std::array<BattleMagicEffect, 256> effects;
    std::array<std::uint8_t, 256> random{};
};
BattleMagicData load_battle_magic(const ContentPack& pack);
// CheckSketchHit is shared by Sketch/Beret and Control/Coronet (C2/3792).
constexpr unsigned battle_sketch_threshold(std::uint8_t user_level, std::uint8_t target_level, bool boosted) {
    // MultAB keeps the high byte; division by zero yields the source FFFF.
    const unsigned denominator = boosted ? unsigned{target_level} * FF6_ROM_VALUE_BATTLE_SKETCH_LEVEL_BOOST_0 >> 8U : target_level;
    return denominator ? unsigned{user_level} * 256U / denominator : 65535U;
}
void refresh_battle_control_commands(BattleState&, const BattleMagicData&);

bool battle_magic_effect_supported(const BattleMagicEffect& effect, bool item = false);
struct BattleItemEffectData {
    std::array<BattleMagicEffect, 256> effects;
    std::array<std::uint8_t, 256> random{};
    std::array<std::optional<std::uint16_t>, 32> animations;
    std::array<BattleMagicEffect, 8> tools;
    std::array<BattleMagicEffect, 256> thrown;
    std::array<std::optional<std::uint16_t>, 256> throw_animations;
    std::array<std::uint16_t, 256> jump_animations{};
    std::array<bool, 256> throw_weapon_animation{};
    std::array<std::optional<std::uint8_t>, 256> throw_spells;
    std::array<std::optional<std::uint8_t>, 256> item_spells;
    std::bitset<256> reusable_items;
    std::array<std::optional<std::uint8_t>, 8> tool_spells;
};
BattleItemEffectData load_battle_item_effects(const ContentPack& pack);
struct BattleAttackTargetResult {
    std::uint8_t slot = 0;
    bool hit = false, healing = false;
    std::uint16_t amount = 0, hp_before = 0, hp_after = 0;
    std::uint32_t status_before = 0, status_after = 0;
    bool reflected = false, display_amount = false, display_miss = true, miss_notice = false;
};
struct BattleMagicReflection {
    std::uint8_t source = 0, target = 0;
    bool hit = false;
};
struct BattleMagicMessage {
    std::uint8_t identity = 0;
    std::array<std::uint32_t, 4> values{};
};
struct BattleAttackResult {
    std::uint8_t attacker = 0, attack = 0;
    std::vector<BattleAttackTargetResult> targets;
    bool suppress_retaliation = false;
    std::vector<BattleAttackTargetResult> drain_returns{};
    std::vector<BattleMagicReflection> reflections{};
    std::vector<BattleMagicMessage> messages{};
    std::bitset<6> rematerialize{};
    // A single volley may send several independently selected projectiles to
    // the same recipient. Order is the authored animation-thread order.
    std::array<std::optional<std::uint8_t>, 8> projectile_targets{};
    std::bitset<8> projectile_hits{};
    bool runic = false;
    std::bitset<10> departures{}, jumping{};
    std::optional<std::uint16_t> golem_hp{};
    std::vector<std::uint8_t> obtained_items{};
    // C2/3CCE,3CFD also change bit 7 of the caster's AI variable.
    std::bitset<10> seize_started{}, seize_released{};
    // Targetting publishes into the AI owner's one-use preferred target.
    std::array<std::optional<std::uint8_t>, 10> preferred_targets{};
    // Magicite schedules a free summon after its item picture, within the
    // same action. The caller retains item consumption and return ownership.
    std::optional<std::uint8_t> follow_up_summon{};

};
// Ordinary Magitek effects, through UpdateStatus. Selection is a converted
// queued command (attack 131..138), never a menu-local identity. No animation,
// AI, action arbitration or after-action pass is performed here. Unsupported
// linked-action contexts and malformed data fail without mutating the state.
BattleAttackResult resolve_battle_magitek(BattleState& state, std::uint8_t attacker,
                                          BattleQueuedCommand command,
                                          const BattleMagitekData& data);
// Shared magical HP/status consumer for native monster spells. Unsupported
// effects outside the supported families remain errors, never substituted attacks.
BattleAttackResult resolve_battle_monster_magic(BattleState& state, std::uint8_t attacker,
                                                BattleQueuedCommand command,
                                                const BattleMagicData& data);
// Casterless Raise, relic spells and Doom share free self-casts (C2/4F5F).
// Does not consume a normal turn or the recipient's queued command.
BattleAttackResult resolve_battle_immediate_spell(BattleState&, std::uint8_t target,
    std::uint8_t attack, const BattleMagicData&);
BattleAttackResult resolve_battle_doom(BattleState&, std::uint8_t target, const BattleMagicData&);
// Target selection belongs to the ordinary command owner; these effects own
// Control links/monster spell availability and the gil debit respectively.
BattleAttackResult resolve_battle_control(BattleState&, std::uint8_t, BattleQueuedCommand, const BattleMagicData&);
BattleAttackResult resolve_battle_gil_toss(BattleState&, std::uint8_t, BattleQueuedCommand, const BattleMagicData&);
BattleAttackResult resolve_battle_slot(BattleState&, std::uint8_t, BattleQueuedCommand, const BattleMagicData&);
BattleAttackResult resolve_battle_summon(BattleState&, std::uint8_t,
    BattleQueuedCommand, const BattleMagicData&);
BattleAttackResult resolve_battle_item_summon(BattleState&, std::uint8_t,
    BattleQueuedCommand, const BattleMagicData&);
// Player Magic uses the same effect rules and consumes the prepared MP cost.
// Blizzard Orb invokes Storm without learning, MP cost or a spell pre-cast.
BattleAttackResult resolve_battle_umaro_storm(BattleState&, std::uint8_t,
    BattleQueuedCommand, const BattleMagicData&);
BattleAttackResult resolve_battle_dance_magic(BattleState&, std::uint8_t,
    BattleQueuedCommand, const BattleMagicData&);
BattleAttackResult resolve_battle_rage_magic(BattleState&, std::uint8_t,
    BattleQueuedCommand, const BattleMagicData&);
BattleAttackResult resolve_battle_sketch_magic(BattleState&, std::uint8_t attacker,
    std::uint8_t source, BattleQueuedCommand, const BattleMagicData&);
BattleAttackResult resolve_battle_leap(BattleState&, std::uint8_t,
    BattleQueuedCommand, const BattleMagitekData&);
// Morph/Revert share the self-status attack owner (C2/1927–1949). Gauge
// ownership and command replacement remain in the ordinary status update pass.
BattleAttackResult resolve_battle_morph(BattleState&, std::uint8_t,
    BattleQueuedCommand, const BattleMagitekData&);
// Free weapon follow-up: original costs are zeroed before Runic admission.
BattleAttackResult resolve_battle_weapon_magic(BattleState&, std::uint8_t,
    BattleQueuedCommand, const BattleMagicData&, bool same_target);
// Paid Magic/Lore from character slots, including enemy-controlled characters.
// Only Magic can consume Terra's first-cast reaction flag.
BattleAttackResult resolve_battle_character_magic(BattleState& state, std::uint8_t attacker,
                                                  BattleQueuedCommand command,
                                                  const BattleMagicData& data);
// Each Empowerer pass shares drain/status arithmetic, but retains separate HP/MP outcomes.
BattleAttackResult resolve_battle_bushido_magic(BattleState&, std::uint8_t,
    BattleQueuedCommand, const BattleMagicData&, bool mp_pass = false, bool retort_stance = false);
BattleAttackResult resolve_battle_blitz_magic(BattleState& state, std::uint8_t attacker,
                                              BattleQueuedCommand command,
                                              const BattleMagicData& data);
// Health and Shock select their built-in effects without a learned spell,
// MP charge or Terra's first-Magic reaction (C2/171A..1725).
BattleAttackResult resolve_battle_innate_magic(BattleState& state, std::uint8_t attacker,
                                         BattleQueuedCommand command, const BattleMagicData& data);
BattleAttackResult resolve_battle_tool_magic(BattleState& state, std::uint8_t attacker,
                                             BattleQueuedCommand command,
                                             const BattleMagicData& data, std::uint8_t spell);
// Throw and Tools share the original spell-item branch; inventory ownership
// remains at confirmation and neither command pays the spell's MP cost.
BattleAttackResult resolve_battle_throw_magic(BattleState& state, std::uint8_t attacker,
                                              BattleQueuedCommand command,
                                              const BattleMagicData& data, std::uint8_t spell);
// Inventory consumption belongs to target confirmation, never this effect pass.
BattleAttackResult resolve_battle_item_magic(BattleState& state, std::uint8_t attacker,
                                             BattleQueuedCommand command,
                                             const BattleMagicData& data, std::uint8_t spell,
                                             bool reusable = false);
BattleAttackResult resolve_battle_item(BattleState& state, std::uint8_t attacker,
                                       BattleQueuedCommand command,
                                       const BattleItemEffectData& data);
// Queued poison/regen/sap self-effects use zero-level magic damage and no cast.
BattleAttackResult resolve_battle_periodic(BattleState& state, std::uint8_t target,
                                          BattleQueuedCommand command,
                                          const BattleMagicData& data);
BattleAttackResult resolve_battle_seize_drain(BattleState&, std::uint8_t attacker,
                                             BattleQueuedCommand, const BattleMagicData&);
// Word-sized base before relic/target modifiers; intermediates are widened.
std::uint16_t battle_magic_base(std::uint8_t power, std::uint8_t magic, std::uint8_t level);
} // namespace ff6
