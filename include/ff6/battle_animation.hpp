#pragma once
#include "ff6/battle_character_motion.hpp"
#include <memory>
#include "ff6/battle_stats.hpp"

namespace ff6 {
using BattleActorPaletteOverrides = std::array<std::optional<std::array<Rgb555, 16>>, 10>;
struct BattleCharacterAnimationContent {
    Bytes programs;
    std::array<std::size_t, 24> entries{};
    std::shared_ptr<const BattleCharacterMotionData> motion;
};
BattleCharacterAnimationContent load_battle_character_animation_content(const ContentPack& pack, bool victory = false);
struct BattleAnimationParticipant {
    std::uint8_t graphics = 255;
    bool present = false, ai = false;
    std::uint32_t status = 0;
};
std::optional<std::uint8_t> select_battle_character_entry(BattleAnimationParticipant character,
                                                          bool magitek, bool victory);
struct BattleAnimationTask {
    std::uint8_t attacker = 0, target = 0, rate = 1, counter = 1;
    bool active = true, flipped = false;
    // Offsets into the owned game program resource, never executable addresses.
    std::size_t resume = 0, instruction = 0;
    std::optional<std::size_t> loop_body, return_to;
    std::uint8_t loop_count = 0;
    BattleAnimationPoint position, offset, attacker_position, target_position;
    std::optional<std::uint8_t> effect_frame;
    std::optional<BattleCharacterLine> line;
    bool operator==(const BattleAnimationTask&) const = default;
};
struct BattleAnimationControl {
    std::array<BattleAnimationPoint, 4> saved_positions{};
    std::array<bool, 4> visible{};
    std::array<std::uint8_t, 4> armor_actions{};
    // Separate from presence: entering monsters remain hidden until their command.
    std::array<bool, 6> monster_entry_visible{true, true, true, true, true, true};
    // Effect show/hide commands own a different gate from entry scripts.
    std::array<bool, 6> monster_visible{true, true, true, true, true, true};
    // Rippler draws a second vertically reflected image of the selected actors.
    // Separate kind-specific masks preserve the script's last-writer behavior.
    std::uint8_t reflected_characters = 0, reflected_monsters = 0;
    // Paired entrance/exit effects retain an incoming actor offscreen until
    // the outgoing image has been copied for the crossfade.
    std::optional<std::uint8_t> staged_monster;
    std::uint16_t staged_monster_y = 0;
    // An authored transition can replace the usual dimension-table overlap.
    std::optional<std::array<std::uint16_t, 6>> monster_overlap;
    bool fixed_draw_order = false, run_animation_blocked = false;
    std::optional<std::uint8_t> force_character_frame;
    std::optional<std::uint8_t> echo_character;
    std::uint8_t echo_delay_mask = 0;
    // Disable clears history even if a later task re-enables it this frame.
    std::uint32_t echo_reset_serial = 0;
    bool operator==(const BattleAnimationControl&) const = default;
};
enum class BattleAnimationBoundary { work_limit, frame_wait, complete };
template <std::size_t Slots> struct BattleAnimationSequence {
    // One primary task per participant, visited in ascending slot order.
    std::array<std::optional<BattleAnimationTask>, Slots> tasks;
    BattleAnimationControl control;
    std::shared_ptr<const BattleCharacterMotionData> motion;
    bool magitek = false;
    std::uint8_t next_task = 0, active_visits = 0;
    bool task_started = false;
    BattleAnimationBoundary boundary = BattleAnimationBoundary::work_limit;
    bool operator==(const BattleAnimationSequence&) const = default;
};
using BattleCharacterAnimation = BattleAnimationSequence<4>;
using BattleMonsterAnimationTasks = BattleAnimationSequence<6>;

BattleCharacterAnimation
make_battle_character_entry_animation(const BattleCharacterAnimationContent& content,
                                      const std::array<BattleAnimationParticipant, 4>& participants,
                                      const std::array<BattleCharacterPose, 4>& poses,
                                      const BattleSceneAnchors& anchors, bool magitek,
                                      bool victory = false, BattleAnimationControl retained = {});
// Advances commands, never time. A budget continuation stays within this pass.
// Invalid commands fail transactionally at this call's boundary.
BattleAnimationBoundary
advance_battle_character_animation(ByteView programs, BattleCharacterAnimation& state,
                                   std::array<BattleCharacterPose, 4>& poses,
                                   std::size_t command_budget = 1024);
// The caller must service the requested logical frame before acknowledging it.
// Final cleanup releases fixed ordering only after the final empty pass's wait.
void resume_battle_character_animation(BattleCharacterAnimation& state);
// Resource selection is separate from execution and retains caller-owned effects.
// Background/extra tasks require their own renderer and are rejected by this loader.
struct BattleMonsterSpriteDefinition {
    Bytes programs;
    std::size_t entry = 0;
    std::uint8_t width = 1, height = 1, target_delay = 0;
    std::uint8_t initialization = 0, sound = 0, sprite_palette = 0;
};
BattleMonsterSpriteDefinition load_battle_monster_sprite_definition(const ContentPack& pack,
                                                                    std::uint16_t property);
// Attacker is a monster slot (0..5). Initialization/effects remain caller-owned.
BattleMonsterAnimationTasks make_battle_monster_animation(
    const BattleMonsterSpriteDefinition& definition, std::bitset<6> targets, std::uint8_t attacker,
    const BattleSceneAnchors& anchors, const std::array<BattleMonsterPosition, 6>& monsters,
    const std::array<std::uint8_t, 6>& retained_facing, BattleAnimationControl retained = {});
BattleAnimationBoundary
advance_battle_monster_animation(ByteView programs, BattleMonsterAnimationTasks& state,
                                 std::array<BattleMonsterPosition, 6>& monsters,
                                 const std::array<std::uint8_t, 6>& retained_facing,
                                 std::size_t command_budget = 1024);
void resume_battle_monster_animation(BattleMonsterAnimationTasks& state);
} // namespace ff6
