#include "rom-materials/battle_row_movement_parameters.macros.inc"
#include "rom-materials/effect_particle_start_last.macros.inc"
#include "rom-materials/effect_particle_start_middle.macros.inc"
#include "rom-materials/effect_particle_start_first.macros.inc"
#include "rom-materials/effect_cross_copy_groups.macros.inc"
#include "rom-materials/effect_cross_copy_interval.macros.inc"
#include "rom-materials/effect_curved_row_split.macros.inc"
#include "rom-materials/effect_party_run_step.macros.inc"
#include "rom-materials/effect_boarding_inset.macros.inc"
#include "rom-materials/effect_low_row_difference.macros.inc"
#include "rom-materials/effect_lifeshaver_low_rows.macros.inc"
#include "rom-materials/effect_specter_copy_height.macros.inc"
#include "rom-materials/effect_large_orbit_radius.macros.inc"
#include "rom-materials/effect_small_orbit_radius.macros.inc"
#include "rom-materials/effect_affine_shrink.macros.inc"
#include "rom-materials/effect_bounce_first_threshold.macros.inc"
#include "rom-materials/effect_bounce_second_threshold.macros.inc"
#include "rom-materials/effect_bounce_first_frame.macros.inc"
#include "rom-materials/effect_bounce_second_frame.macros.inc"
#include "rom-materials/effect_wide_wave_radius.macros.inc"
#include "rom-materials/effect_narrow_wave_radius.macros.inc"
#include "rom-materials/effect_additive_wave_radius.macros.inc"
#include "rom-materials/effect_disintegration_delay.macros.inc"
#include "rom-materials/effect_disintegration_step.macros.inc"
#include "rom-materials/effect_monster_arc_height.macros.inc"
#include "rom-materials/effect_line_destination_offset.macros.inc"
#include "rom-materials/effect_horizontal_phase_step.macros.inc"
#include "rom-materials/effect_vertical_phase_step.macros.inc"
#include "rom-materials/effect_supplemental_summon.macros.inc"
#include "rom-materials/effect_dice_frame_base.macros.inc"
#include "rom-materials/effect_dice_blank_frame.macros.inc"
#include "rom-materials/effect_armored_step_distance.macros.inc"
#include "rom-materials/effect_ordinary_step_distance.macros.inc"
#include "rom-materials/effect_reflection_script.macros.inc"
#include "rom-materials/effect_reflection_extent.macros.inc"
#include "rom-materials/effect_target_frame_first.macros.inc"
#include "rom-materials/effect_target_frame_second.macros.inc"
#include "rom-materials/effect_target_frame_pair.macros.inc"
#include "rom-materials/effect_extra_launch_delay.macros.inc"
#include "rom-materials/effect_thread_loop_increment.macros.inc"
#include "rom-materials/effect_ellipse_radius.macros.inc"
#include "rom-materials/effect_precast_turn.macros.inc"
#include "rom-materials/effect_ellipse_phase.macros.inc"
#include "rom-materials/effect_orbit_growth.macros.inc"
#include "rom-materials/effect_orbit_cap.macros.inc"
#include "rom-materials/effect_cone_left.macros.inc"
#include "rom-materials/effect_cone_right.macros.inc"
#include "rom-materials/effect_cone_spread.macros.inc"
#include "rom-materials/effect_boomerang_edge.macros.inc"
#include "rom-materials/effect_boomerang_rate.macros.inc"
#include "rom-materials/effect_sketch_height.macros.inc"
#include "rom-materials/effect_sketch_offset.macros.inc"
#include "rom-materials/effect_sketch_full_offset.macros.inc"
#include "rom-materials/effect_transition_overlap.macros.inc"
#include "rom-materials/effect_transition_displacement.macros.inc"
#include "rom-materials/effect_transition_chance.macros.inc"
#include "rom-materials/effect_seize_height.macros.inc"
#include "rom-materials/effect_seize_rows.macros.inc"
#include "rom-materials/effect_rainbow_cadence.macros.inc"
#include "rom-materials/effect_monster_palette_shift.macros.inc"
#include "rom-materials/effect_system_cue.macros.inc"
#include "rom-materials/effect_monster_lunge_script.macros.inc"
#include "rom-materials/effect_whiff_sound.macros.inc"
#include "rom-materials/effect_critical_script.macros.inc"
#include "rom-materials/effect_scatter_edge.macros.inc"
#include "rom-materials/effect_orbit_extent.macros.inc"
#include "rom-materials/effect_orbit_turn.macros.inc"
#include "rom-materials/effect_spiral_forward.macros.inc"
#include "rom-materials/effect_spiral_reverse.macros.inc"
#include "rom-materials/effect_spiral_y.macros.inc"
#include "rom-materials/effect_spiral_angle.macros.inc"
#include "rom-materials/effect_bottom_alignment.macros.inc"
#include "rom-materials/effect_top_alignment.macros.inc"
#include "rom-materials/effect_monster_near.macros.inc"
#include "rom-materials/effect_monster_far.macros.inc"
#include "rom-materials/effect_character_front.macros.inc"
#include "rom-materials/effect_alignment_right.macros.inc"
#include "rom-materials/effect_spiral_initial.macros.inc"
#include "rom-materials/effect_steering_limit.macros.inc"
#include "rom-materials/effect_homing_cadence.macros.inc"
#include "rom-materials/effect_spiral_cadence.macros.inc"
#include "rom-materials/effect_arrival_padding.macros.inc"
#include "rom-materials/effect_spiral_bounds.macros.inc"
#include "ff6/battle_effects.hpp"
#include "rom-materials/battle_animation_capture_origin.macros.inc"
#include "rom-materials/battle_animation_magitek_offset.macros.inc"

#include "ff6/battle_task_clock.hpp"
#include "rom-materials/battle_particle_copy_counts.macros.inc"
#include "rom-materials/battle_particle_copy_delays.macros.inc"
#include "rom-materials/battle_animation_thread_limits.macros.inc"
#include "rom-materials/battle_precast_delays.macros.inc"
#include "rom-materials/battle_precast_phases.macros.inc"
#include "rom-materials/battle_summon_image_choices.macros.inc"
#include "rom-materials/battle_animation_blend_presets.macros.inc"
#include "rom-materials/battle_animation_mask_shapes.macros.inc"
#include "rom-materials/battle_animation_mask_extents.macros.inc"
#include "rom-materials/battle_animation_mask_span_pattern.macros.inc"
#include "rom-materials/battle_animation_affine_sources.macros.inc"
#include "rom-materials/battle_animation_palette_rectangle_prefix.source.macros.inc"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
std::uint16_t add(std::uint16_t value, int delta) {
    return wrap16(static_cast<std::uint32_t>(static_cast<std::int32_t>(value) + delta));
}
std::size_t index(BattleEffectLayer layer) {
    return static_cast<std::size_t>(layer);
}
bool flipped(const BattleEffectContext& context, unsigned actor) {
    require(actor < 10, "Invalid animation participant");
    return actor < 4
               ? (context.characters[actor].facing & 1U) != 0
               : ((context.monsters[actor - 4].flip ^ context.monster_facing[actor - 4] ^ 1U) &
                  1U) != 0;
}
unsigned selected_thread_owner(const BattleEffectContext& context) {
    // REFLECT_ANIM chooses the first populated reflector mapping in slot order.
    for (unsigned slot = 0; slot < 10; ++slot)
        if (context.reflection_targets[slot])
            return slot;
    // Shared selection prefers the first monster, then the first character.
    for (unsigned slot : {4U, 5U, 6U, 7U, 8U, 9U, 0U, 1U, 2U, 3U})
        if (context.targets[slot])
            return slot;
    throw std::invalid_argument("Animation requires a selected target");
}
unsigned selected_target(const BattleEffectContext& context) {
    const auto owner = selected_thread_owner(context);
    return context.reflection_targets[owner].value_or(static_cast<std::uint8_t>(owner));
}
BattleAnimationPoint live_character_origin(const BattleEffectContext& context, unsigned slot) {
    require(slot < 4, "Live character origin requires a character attacker");
    const auto& actor = context.characters[slot];
    return {wrap16(unsigned{actor.x} + actor.offset_x + actor.animation_x + 8U),
            wrap16(unsigned{actor.y} + actor.offset_y)};
}
std::array<Rgb555, 16> specter_colors(const BattleEffectContext& context) {
    const unsigned bank = context.attacker & 7U;
    if (bank < 4)
        return context.character_colors[bank];

    // C2/EFBA shifts the caster into an eight-bit palette offset. Enemy slots
    // 4..7 therefore interpret the following scene fields as colors; 8/9 wrap
    // to character banks 0/1. Preserve this visible game quirk explicitly.
    // InitAnimType's completed clear fixes the first six colors. Subsequent
    // colors combine the scroll selections and semantic actor coordinates in
    // the source's byte order (C1/0659). The prefix is functional completed-
    // transfer metadata: idle flag, advanced source/destination addresses,
    // chunk size, zero remaining size, completion flag and cleared padding
    // (C1/B0F9, C1/198D). It is not an authored palette. No memory image or
    // transfer device is exposed to the renderer.
    Bytes color_bytes{0x00, 0x7f, 0xad, 0x7e, 0x00, 0x56,
                      0x00, 0x04, 0x00, 0x00, 0x01, 0x00};
    for (const auto program : context.scroll_programs)
        color_bytes.push_back(static_cast<std::uint8_t>(program | 0x80U));
    const auto word = [&](std::uint16_t value) {
        color_bytes.push_back(wrap8(value));
        color_bytes.push_back(wrap8(unsigned{value} >> 8U));
    };
    for (const auto member : {&BattleActorAnchors::center_x, &BattleActorAnchors::center_y,
                              &BattleActorAnchors::bottom_y})
        for (unsigned slot = 4; slot < 10; ++slot) word(context.anchors[slot].*member);
    for (const auto member : {&BattleActorAnchors::center_x, &BattleActorAnchors::center_y,
                              &BattleActorAnchors::bottom_y})
        for (unsigned slot = 0; slot < 4; ++slot) word(context.anchors[slot].*member);
    for (unsigned slot = 4; slot < 10; ++slot) word(context.anchors[slot].draw_y);
    for (unsigned slot = 4; slot < 10; ++slot)
        word(wrap16(unsigned{context.anchors[slot].draw_y} - context.anchors[slot].bottom_y));
    for (unsigned slot = 4; slot < 10; ++slot) word(context.anchors[slot].cursor_x);
    for (unsigned slot = 4; slot < 10; ++slot) word(context.anchors[slot].center_y);
    for (unsigned slot = 4; slot < 10; ++slot)
        word(wrap16(unsigned{context.anchors[slot].cursor_x} - context.monsters[slot - 4].x + 8U));
    std::array<Rgb555, 16> result{};
    const unsigned first = (bank - 4U) * 32U;
    for (unsigned color = 0; color < result.size(); ++color)
        result[color] = static_cast<Rgb555>((unsigned{color_bytes[first + color * 2U]} |
            (unsigned{color_bytes[first + color * 2U + 1U]} << 8U)) & 0x7fffU);
    return result;
}
BattleAnimationPoint point(const BattleEffectContext& context, unsigned slot,
                           BattleEffectGraphics graphics, std::uint8_t alignment, bool target,
                           std::optional<BattleAnimationPoint> overlay) {
    if (alignment == 0xa0 && (target || slot >= 4)) {
        // CalcAttackerPos takes the same screen-alignment branch for a monster
        // as CalcTargetPos. Only a character attacker uses its bottom anchor.
        require(overlay.has_value(), "Screen-aligned effect needs a retained overlay origin");
        return *overlay;
    }
    const auto& a = context.anchors.at(slot);
    constexpr auto raw_armor = FF6_ROM_VALUE_BATTLE_ANIMATION_MAGITEK_OFFSET_0;
    constexpr int armor_offset = raw_armor < 32768 ? raw_armor : raw_armor - 65536;
    const int armor = slot < 4 && context.magitek ? armor_offset : 0;
    int x = a.center_x, y = a.center_y + armor;
    if (alignment == 0)
        y = a.bottom_y + FF6_ROM_VALUE_EFFECT_BOTTOM_ALIGNMENT_0 - int(wrap8(unsigned{graphics.height} * 8)) + armor;
    else if (alignment == 0x40)
        y = slot < 4 ? a.bottom_y - FF6_ROM_VALUE_EFFECT_TOP_ALIGNMENT_0 + armor : context.monsters[slot - 4].y;
    else if (alignment == 0x80 || alignment == 0xc0) {
        y = a.bottom_y + armor;
        if (slot >= 4)
            x += (int(context.monster_widths[slot - 4]) * 4 + (alignment == 0x80 ? FF6_ROM_VALUE_EFFECT_MONSTER_NEAR_0 : FF6_ROM_VALUE_EFFECT_MONSTER_FAR_0)) *
                 ((context.characters[context.attacker & 3U].facing & 1U) ? -1 : 1);
        else if (target)
            x += (context.characters[context.attacker & 3U].facing & 1U) ? -FF6_ROM_VALUE_EFFECT_CHARACTER_FRONT_0 : FF6_ROM_VALUE_EFFECT_CHARACTER_FRONT_0;
    } else if (alignment == 0xe0 && slot < 4) {
        x -= FF6_ROM_VALUE_BATTLE_ANIMATION_CAPTURE_ORIGIN_0;
        y -= FF6_ROM_VALUE_BATTLE_ANIMATION_CAPTURE_ORIGIN_0;
    } else if (alignment == 0x60 && slot < 4)
        y = a.bottom_y + armor;
    else if (alignment == 0xa0) {
        // Screen alignment is initialized from the selected overlay image,
        // not a guessed screen center. The factory supplies this separately.
        y = a.bottom_y + armor;
    }
    if (slot < 4 &&
        (alignment == 0x60 || alignment == 0x80 || alignment == 0xa0 || alignment == 0xc0)) {
        // Front placement uses the attacker's character-facing entry, including
        // its low-two-bit alias for monster attackers. The original clamps a
        // wrapped signed word, not an unbounded host-coordinate calculation.
        const auto wrapped = wrap16(static_cast<std::uint32_t>(x));
        x = wrapped >= 32768 ? 0 : std::min(int{wrapped}, FF6_ROM_VALUE_EFFECT_ALIGNMENT_RIGHT_0);
    }
    return {wrap16(static_cast<std::uint32_t>(x)), wrap16(static_cast<std::uint32_t>(y))};
}
std::pair<int, int> displacement(std::uint8_t value, bool flip) {
    constexpr std::array<int, 8> dx{-1, 0, 1, -1, 1, -1, 0, 1};
    constexpr std::array<int, 8> dy{1, 1, 1, 0, 0, -1, -1, -1};
    const unsigned direction = value >> 5U;
    const int distance = (value & 31U) + 1;
    return {dx[direction] * distance * (flip ? -1 : 1), dy[direction] * distance};
}
BattleEffectDefinition ordinary_definition(const std::shared_ptr<const BattleEffectLibrary>& library,
                                           std::uint16_t identity) {
    require(library && identity < library->definitions.size(), "Unowned attack animation");
    return library->definitions[identity];
}
std::uint8_t physical_block(const BattleEffectContext& context) {
    const auto target = selected_target(context);
    const auto block = target < 4 ? context.block_animations[target] : 0;
    require(block <= 7, "Invalid physical block presentation");
    return static_cast<std::uint8_t>(block);
}
BattleEffectDefinition physical_definition(const std::shared_ptr<const BattleEffectLibrary>& library,
                                          BattlePhysicalAttackPresentation request,
                                          const BattleEffectContext& context) {
    require(library && context.attacker < 10,
            "Invalid physical animation participant");
    const unsigned index = request.property + (request.special ? 93U : 0U);
    require(request.property < (request.special ? 35U : 93U) &&
                index < library->weapon_properties.size(), "Unowned physical animation property");
    const auto& row = library->weapon_properties[index];
    require((row[5] & 127U) <= 2, "Unknown weapon presentation initializer");
    BattleEffectDefinition definition;
    definition.layers[0] = BattleEffectSelection{
        row[(context.characters[context.attacker & 3U].facing ^ unsigned(context.weapon_left_hand)) & 1U], false};
    definition.layers[1] = BattleEffectSelection{
        static_cast<std::uint16_t>(row[3] + (request.special && row[3] >= 0x60 ? 0x200 : 0)),
        context.hits.any()};
    // FightCmdAnim selects a character's weapon script, while a monster uses
    // the shared lunge script with the same weapon and hit-image resources.
    definition.layers[3] = BattleEffectSelection{
        context.attacker < 4 ? definition.layers[0]->script : std::uint16_t{FF6_ROM_VALUE_EFFECT_MONSTER_LUNGE_SCRIPT_0}, true};
    // The critical flash is a standalone task. It never loads overlay pixels.
    definition.palettes = {row[2], row[4], 0};
    // FightCmdAnim substitutes the shared whiff cue when every target missed.
    // Authored nonzero cue operands still override this default.
    const auto block = physical_block(context);
    require(!block || library->blocks.has_value(), "Missing shared block presentation resources");
    definition.sound = block ? library->blocks->selections[block - 1].sound
                            : context.hits.any() ? row[6] : FF6_ROM_VALUE_EFFECT_WHIFF_SOUND_0;
    return definition;
}
} // namespace
BattleEffectImageState initial_battle_effect_images() {
    BattleEffectImageState state;
    state.origins.fill(BattleAnimationPoint{});
    state.overlay_projection.emplace();
    return state;
}
void BattleAttackAnimation::apply_actor_state(BattleActorScene& actor,
                                              BattleActorSceneRules& rules) const {
    apply_battle_effect_actor_state(context_, actor, rules);
}
void apply_battle_effect_actor_state(const BattleEffectContext& context, BattleActorScene& actor,
                                     BattleActorSceneRules& rules) {
    set_battle_character_colors(actor, context.character_colors);
    actor.monster_colors = context.monster_colors;
    rules.freeze_palettes = context.freeze_character_palettes;
    rules.hide_characters = context.hide_characters;
    rules.invisible_monsters = ~context.visible_monsters;
    for (unsigned i = 0; i < 4; ++i)
        if (context.revealed_characters[i]) rules.shown_characters[i] = true;
    for (unsigned i = 0; i < 4; ++i) {
        auto& to = actor.characters.layout.poses[i];
        const auto& from = context.characters[i];
        to.x = from.x;
        to.y = from.y;
        to.offset_x = from.offset_x;
        to.offset_y = from.offset_y;
        to.animation_x = from.animation_x;
        to.desired_angle = from.desired_angle;
        to.jump_y = from.jump_y;
        to.action = from.action;
        to.frame_override = from.frame_override;
        // Anim command CA owns the same speed flag read by DrawCharSprite.
        // Keeping it only in the script context leaves the visible walk/jump
        // poses at ordinary cadence during accelerated attack choreography.
        to.double_speed = from.double_speed;
        to.appearance.priority = from.appearance.priority;
        to.appearance.flip_x = from.appearance.flip_x;
        to.disable_float = from.disable_float;
        to.seize_origin = from.seize_origin;
        to.facing = from.facing;
        if (context.character_hand_swaps)
            actor.characters.layout.swap_hands[i] = (*context.character_hand_swaps)[i];
        if (context.character_back_rows)
            actor.characters.layout.back_row[i] = (*context.character_back_rows)[i];
    }
    actor.monsters.positions = context.monsters;
    actor.monster_styles = context.monster_styles;
    actor.animation = context.control;
}
BattleAttackAnimation::BattleAttackAnimation(std::shared_ptr<const BattleEffectLibrary> library,
    std::shared_ptr<const Bytes> programs, BattleEffectContext context)
    : library_(std::move(library)), event_programs_(std::move(programs)) {
    require(library_ && event_programs_, "Missing battle event animation data");
    mask_.rows.fill({0, 248});
    begin_event_batch(std::move(context));
}
void BattleAttackAnimation::begin_event_batch(BattleEffectContext context) {
    require(event_programs_ && !pass_started_, "Cannot reset an interrupted event animation");
    context_ = std::move(context);
    base_character_colors_ = context_.character_colors;
    base_monster_colors_ = context_.monster_colors;
    base_battlefield_colors_ = context_.battlefield_colors;
    // CopyPal snapshots the current colors at each batch. Terrain offsets stay
    // applied to the immutable battlefield image until a later command changes it.
    const auto terrain = color_offsets_[3];
    color_offsets_ = {};
    if (context_.battlefield_colors.empty()) color_offsets_[3] = terrain;
    tasks_ = {};
    event_task_sources_.reset();
    complete_ = false;
    next_task_ = 0;
}
void BattleAttackAnimation::add_event_task(unsigned slot, BattleAnimationTask script) {
    require(event_programs_ && slot < 10 && script.instruction < event_programs_->size(),
            "Invalid standalone event animation task");
    BattleEffectTask task;
    task.script = std::move(script);
    task.frame_height = 1;
    // Event slots have the same eight-thread stride as ordinary actor groups.
    tasks_[slot * 8] = std::move(task);
    event_task_sources_.set(slot * 8);
}
void BattleAttackAnimation::inherit_event_tasks(const BattleAttackAnimation& source) {
    require(source.event_programs_ != nullptr, "Missing event program resource");
    event_programs_ = source.event_programs_;
    for (unsigned i = 0; i < tasks_.size(); ++i)
        if (source.event_task_sources_[i]) {
            tasks_[i] = source.tasks_[i];
            event_task_sources_.set(i);
        }
}
std::vector<MusicPlayerCommand> BattleAttackAnimation::take_event_audio() {
    auto out = std::move(event_audio_);
    event_audio_.clear();
    return out;
}
BattleAttackAnimation::BattleAttackAnimation(std::shared_ptr<const BattleEffectLibrary> library,
                                             std::uint16_t definition, BattleEffectContext context,
                                             BattleEffectImageState retained)
    : BattleAttackAnimation(library, ordinary_definition(library, definition), std::move(context),
                            std::move(retained), false) {}
BattleAttackAnimation::BattleAttackAnimation(std::shared_ptr<const BattleEffectLibrary> library,
                                             BattlePhysicalAttackPresentation request,
                                             BattleEffectContext context,
                                             BattleEffectImageState retained)
    : BattleAttackAnimation(library, physical_definition(library, request, context), context,
                            std::move(retained), true) {
    // Weapon initializer 2 shares two actor tasks with the normal type, but
    // leaves color addition disabled. Slime-like opaque hits use this path.
    const auto kind = library_->weapon_properties[request.property + (request.special ? 93U : 0U)][5];
    if ((kind & 127U) != 0) set_blend(0, 0, 0);
    if ((kind & 127U) == 1) {
        // InitWeaponType::THROWN duplicates the weapon task with two/four
        // frame delays. Copies animate independently; only task zero moves
        // the actual caster during the shared forward hop.
        const unsigned first = context_.attacker * 8U;
        auto& source = *tasks_[first];
        const auto target = source.script.target;
        if (context_.attacker < 4 && target < 4 &&
            (context_.battle_type != 3 || (context_.attacker < 2) == (target < 2)))
            source.script.flipped = !source.script.flipped;
        for (unsigned copy = 1; copy <= 2; ++copy) {
            auto task = source;
            task.script.counter = wrap8(unsigned{source.script.counter} + copy * 2U);
            tasks_[first + copy] = task;
        }
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_21, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_21};
    }
    // Bit 7 chooses the target's second sprite task for the hit. It is
    // independent of the low-seven-bit initializer (C1/C05C, C1/C269).
    // That task addresses the retained weapon/shared sprite sheet and palette,
    // while ordinary hits use their separately loaded foreground image.
    if ((kind & 128U) && !physical_block(context_) && tasks_[80]) {
        auto hit = *tasks_[80];
        hit.layer = BattleEffectLayer::sprite;
        tasks_[selected_target(context_) * 8U + 1U] = hit;
        tasks_[80].reset();
    }
    if (request.critical) {
        constexpr unsigned script = FF6_ROM_VALUE_EFFECT_CRITICAL_SCRIPT_0;
        require(script < library_->entries.size(), "Missing critical flash script");
        const auto entry = library_->entries[script];
        require(std::size_t{entry} + 2 <= library_->programs.size(), "Truncated critical script");
        BattleEffectTask task;
        task.layer = BattleEffectLayer::overlay;
        task.script.rate = static_cast<std::uint8_t>((library_->programs[entry] >> 4U) + 1U);
        task.script.counter = 1;
        task.script.resume = task.script.instruction = entry + 2U;
        tasks_[81] = task;
    }
}
BattleAttackAnimation::BattleAttackAnimation(std::shared_ptr<const BattleEffectLibrary> library,
                                             BattleEffectDefinition definition,
                                             BattleEffectContext context,
                                             BattleEffectImageState retained, bool physical)
    : library_(std::move(library)), context_(std::move(context)),
      base_character_colors_(context_.character_colors),
      base_monster_colors_(context_.monster_colors),
      base_battlefield_colors_(context_.battlefield_colors) {
    images_ = std::move(retained.images);
    background_anchors_ = retained.origins;
    background_flipped_ = retained.flipped;
    gradient_.phase = retained.gradient_phase;
    gradient_.intensity = retained.gradient_intensity;
    if (retained.overlay_projection) {
        scrolling_[2].projection = *retained.overlay_projection;
        // InitAnimType clears only the first row; the remaining rows retain
        // their earlier owner until a wave or explicit reset replaces them.
        scrolling_[2].projection[0] = {};
        scrolling_[2].projection_owned = true;
    }
    mask_.rows.fill({0, 248});
    scrolling_[1].pattern = BattleScrollPattern::wave32;
    require(library_ != nullptr, "Missing animation library");
    require(context_.attacker < 10 && context_.targets.any(),
            "Attack animation needs participants");
    require((context_.hits & ~context_.targets).none(), "Hit participant is not an attack target");
    require(context_.display.brightness <= 15 && context_.display.mosaic_size >= 1 &&
                context_.display.mosaic_size <= 16,
            "Invalid attack display state");
    definition_ = definition;
    // LoadAnimProp transfers converted image rows even when the corresponding
    // task is disabled. FFFF retains the image and performs no transfer.
    // Each original transfer publishes at most 1024 bytes per explicit wait.
    constexpr std::array<unsigned, 3> image_waits{4, 5, 2};
    for (unsigned i = 0; i < 3; ++i)
        if (definition_.layers[i] && !context_.repeated_hit)
            loading_waits_ += image_waits[i];
    preparation_waits_ = physical ? 0 : battle_effect_preparation_waits(*library_, definition_,
        context_.repeated_hit != 0, context_.message_window_uses != 0);
    // Shared foreground/overlay reset; the actor-background prefix also clears
    // a full target plane and waits around its actor-copy publication.
    loading_waits_ += (definition_.initialization & 128U) ? 9U : 3U;
    // anim_window_rset (C1/9914) keeps its use count after clearing: every
    // later LoadAnimProp, including repeated hits, republishes blank text,
    // blank glyphs and the foreground before continuing. Fight's separate
    // loader replaces its wait count below and does not call this routine.
    if (context_.message_window_uses)
        loading_waits_ += 5;
    for (unsigned i = 0; i < 3; ++i)
        if (const auto selection = definition_.layers[i]) {
            images_[i] = load_battle_effect_image(*library_, selection->script,
                                                  static_cast<BattleEffectLayer>(i),
                                                  definition_.palettes[i]);
            if (i) {
                const auto g = images_[i]->geometry;
                background_anchors_[i - 1] = BattleAnimationPoint{
                    static_cast<std::uint16_t>(
                        flipped(context_, context_.attacker) ? 256U - g.width * 8U : g.width * 8U),
                    static_cast<std::uint16_t>(g.height * 8U)};
            }
        }
    // Palette loading is independent of the FFFF image sentinel. An absent
    // image selection retains pixels/origin, but still replaces its colors.
    for (unsigned layer = 0; layer < 3; ++layer)
        if (!definition_.layers[layer]) {
            const auto palette = definition_.palettes[layer];
            const auto& colors = battle_effect_palette(*library_, palette);
            auto& base = images_[layer] ? images_[layer]->base_colors : imageless_base_colors_[layer];
            auto& working = images_[layer] ? images_[layer]->colors : imageless_colors_[layer];
            for (unsigned color = 0; color < (layer == 2 ? 4U : 16U); ++color)
                working[color] = base[color] = colors[color & 7U];
        }
    // LoadBG1AnimPal fills both sixteen-color banks from the same eight colors.
    secondary_colors_ = secondary_base_colors_ = effect_colors(BattleEffectLayer::foreground);
    if (definition_.layers[0] && images_[0] && library_->shared_frames) {
        auto& image = *images_[0];
        // ClearSpriteAnimFrameBuf makes unused slots explicitly blank.
        image.frames.resize(32);
        image.frame_geometry.resize(32, image.geometry);
        // The ordinary attack loader installs only entries zero and six. The
        // intervening block/weapon frames belong to a different caller.
        for (unsigned index = 0; index < 7; ++index) {
            if (physical ? index == 0 : (index != 0 && index != 6))
                continue;
            const auto shared = library_->shared_frames->at(index);
            const auto resource =
                load_battle_effect_image(*library_, shared.script, BattleEffectLayer::sprite, 0);
            require(shared.frame + resource.frames.size() <= image.frames.size(),
                    "Shared animation frames exceed their destination");
            for (unsigned frame = 0; frame < resource.frames.size(); ++frame) {
                image.frames[shared.frame + frame] = resource.frames[frame];
                image.frame_geometry[shared.frame + frame] = resource.geometry;
            }
        }
    }
    if (context_.reflecting_targets.any() && library_->blocks) {
        require(images_[0].has_value() && images_[0]->tiles.size() <= 160 &&
                    library_->blocks->status_tiles.size() == 32,
                "Reflection requires the retained status sprite sheet");
        auto& image = *images_[0];
        image.owned_tiles.resize(image.tiles.size(), true);
        image.owned_tiles.resize(160, false);
        image.tiles.resize(160);
        image.tiles.insert(image.tiles.end(), library_->blocks->status_tiles.begin(),
                           library_->blocks->status_tiles.end());
        image.owned_tiles.resize(192, true);
    }
    const auto create = [&](unsigned task_slot, unsigned target, unsigned layer,
                            std::uint8_t delay) {
        const bool reflection_block = layer == 0 && context_.reflecting_targets[target];
        const auto selection = reflection_block ? std::optional{BattleEffectSelection{FF6_ROM_VALUE_EFFECT_REFLECTION_SCRIPT_0, true}}
                                                : definition_.layers[layer];
        if (!selection || (layer < 3 && !selection->runs))
            return;
        require(selection->script < library_->entries.size(), "Unowned animation script identity");
        const auto entry = library_->entries[selection->script];
        require(std::size_t{entry} + 2 <= library_->programs.size(), "Truncated animation header");
        BattleEffectTask task;
        task.layer = layer == 3 ? BattleEffectLayer::sprite : static_cast<BattleEffectLayer>(layer);
        auto& s = task.script;
        // C2/34E4 stores reflector -> recipient: BitToTargetID returns Y,
        // preserving X. C1/A83E uses those roles for the rebound task.
        // The original caster still owns facing and the eventual return.
        const auto recipient = context_.reflection_targets[target];
        s.attacker = recipient && !context_.projectile_volley
                         ? static_cast<std::uint8_t>(target) : context_.attacker;
        s.target = recipient.value_or(static_cast<std::uint8_t>(target));
        s.flipped = flipped(context_, context_.attacker);
        s.rate = static_cast<std::uint8_t>((library_->programs[entry] >> 4U) + 1U);
        s.counter = delay;
        s.resume = s.instruction = entry + 2U;
        const auto alignment = static_cast<std::uint8_t>(library_->programs[entry + 1] & 0xe0U);
        require(selection->script < library_->graphics.size(), "Unowned task geometry");
        auto geometry = library_->graphics[selection->script];
        if (reflection_block)
            geometry.width = geometry.height = FF6_ROM_VALUE_EFFECT_REFLECTION_EXTENT_0; // InitAnimThreads' shared frame 29.
        task.frame_height = geometry.height;
        task.target_height = s.target < 4 ? 3 : context_.monster_heights[s.target - 4];
        s.attacker_position = point(context_, s.attacker, geometry, alignment, false, background_anchors_[1]);
        s.target_position = point(context_, s.target, geometry, alignment, true, background_anchors_[1]);
        s.position = s.target_position;
        tasks_[task_slot] = task;
        if (layer == 0)
            thread_launch_spacing_ = wrap8(unsigned{thread_launch_spacing_} + FF6_ROM_VALUE_EFFECT_THREAD_LOOP_INCREMENT_0);
    };
    std::uint8_t delay = 1;
    for (unsigned i = 0; i < 10; ++i)
        if (context_.targets[i]) {
            const auto init = definition_.initialization & 127U;
            const unsigned count = init == 2 || init == 5 ? 3 : 1;
            for (unsigned copy = 0; copy < count; ++copy) {
                create(i * 8 + (copy == 0 ? 0 : copy + 3), i, 0, delay);
                delay = wrap8(unsigned{delay} + definition_.target_delay);
            }
        }
    // GetTargetNum prefers the lowest monster bit, then the lowest character.
    // It does not choose by visual depth, even when several targets are hit.
    const auto preferred_target = selected_target(context_);
    const auto preferred_owner = selected_thread_owner(context_);
    // Background frame geometry is oriented at image loading, independently
    // of later task-direction commands. These four frame families use target
    // orientation; all other ordinary background effects use the attacker.
    for (unsigned layer = 1; layer < 3; ++layer) {
        if (const auto selection = definition_.layers[layer]) {
            const auto script = selection->script;
            const bool target = layer == 1 && (script == FF6_ROM_VALUE_EFFECT_TARGET_FRAME_FIRST_0 || script == FF6_ROM_VALUE_EFFECT_TARGET_FRAME_PAIR_0 + 1 ||
                                               script == FF6_ROM_VALUE_EFFECT_TARGET_FRAME_SECOND_0 || script == FF6_ROM_VALUE_EFFECT_TARGET_FRAME_PAIR_0);
            background_flipped_[layer - 1] =
                flipped(context_, target ? preferred_target : context_.attacker);
            const auto g = images_[layer]->geometry;
            background_anchors_[layer - 1] = BattleAnimationPoint{
                static_cast<std::uint16_t>(background_flipped_[layer - 1] ? 256U - g.width * 8U
                                                                          : g.width * 8U),
                static_cast<std::uint16_t>(g.height * 8U)};
        }
    }
    create(80, preferred_owner, 1, 1);
    create(81, preferred_owner, 2, 1);
    if (const auto extra = definition_.layers[3]) {
        require(images_[0].has_value(), "Extra animation needs a retained sprite frame owner");
        if (!extra->runs)
            create(82, preferred_owner, 3, 1);
        else {
            const auto init = definition_.initialization & 127U;
            const unsigned copies = init == 0x11 ? FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_21
                : init == 0x12 ? FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_22 : 1;
            const unsigned first =
                (context_.reflection_targets[preferred_owner] ? preferred_owner
                                                              : context_.attacker) *
                    8U +
                (definition_.initialization == 0x18 ? 6U : 1U);
            for (unsigned i = 0; i < copies; ++i) {
                require(first + i < 80 && !tasks_[first + i], "Extra task allocation collision");
                create(first + i, preferred_owner, 3, static_cast<std::uint8_t>(1 + i * FF6_ROM_VALUE_EFFECT_EXTRA_LAUNCH_DELAY_0));
            }
        }
    }
    if (std::ranges::none_of(context_.draw_order,
                             [](const auto& item) { return item.participant.has_value(); })) {
        BattleDrawOrder order;
        stage_battle_draw_order(order, context_.anchors);
        context_.draw_order = order.staged;
    }
    update_draw_order(context_.draw_order);
    prioritize_from(static_cast<std::uint8_t>(preferred_target), false);
    if (physical) {
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_19, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_19};
        // Fight owns one attacker task and one shared hit plane, not one
        // spell sprite per target. Its movement starts at the attacker.
        const unsigned extra = context_.attacker * 8U + 1U;
        auto task = *tasks_[extra];
        tasks_[extra].reset();
        const auto geometry = images_[0]->geometry;
        const auto entry = library_->entries[definition_.layers[3]->script];
        const auto alignment = static_cast<std::uint8_t>(library_->programs[entry + 1] & 0xe0U);
        task.frame_height = geometry.height;
        task.script.attacker_position = point(context_, context_.attacker, geometry, alignment, false, background_anchors_[1]);
        task.script.target_position = point(context_, preferred_target, geometry, alignment, true, background_anchors_[1]);
        task.script.position = task.script.attacker_position;
        tasks_[context_.attacker * 8U] = task;
        if (const auto block = physical_block(context_)) {
            const auto& data = *library_->blocks;
            const auto selection = data.selections[block - 1];
            require(selection.palette < data.palettes.size() && data.tiles.size() == 32 &&
                        data.status_tiles.size() == 32 &&
                        selection.script < library_->entries.size(), "Invalid block resource selection");
            // LoadBlockPal replaces only the working upper eight colors. Weapon
            // colors and the retained base palette still belong to InitWeaponAnim.
            std::copy(data.palettes[selection.palette].begin(), data.palettes[selection.palette].end(),
                      images_[0]->colors.begin() + 8);
            supplemental_image_ = images_[0];
            auto& block_image = *supplemental_image_;
            require(block_image.tiles.size() >= 96, "Missing weapon image following block tiles");
            block_image.tiles.resize(96);
            block_image.tiles.insert(block_image.tiles.begin(), data.tiles.begin(), data.tiles.end());
            // Shield frames reference the status sheet loaded at battle entry,
            // beyond the weapon area. Preserve the intervening unowned gap.
            block_image.tiles.resize(160);
            block_image.tiles.insert(block_image.tiles.end(), data.status_tiles.begin(), data.status_tiles.end());
            block_image.owned_tiles.assign(192, true);
            std::fill(block_image.owned_tiles.begin() + 128, block_image.owned_tiles.begin() + 160, false);
            // A block is the defender's second sprite task. It replaces the hit
            // plane and suppresses a monster's lunge, while a character keeps
            // its ordinary weapon/hop task. Shared frame layouts stay intact.
            tasks_[80].reset();
            if (context_.attacker >= 4) tasks_[context_.attacker * 8U].reset();
            const auto block_entry = library_->entries[selection.script];
            require(std::size_t{block_entry} + 2 <= library_->programs.size(), "Truncated block script");
            BattleEffectTask defender;
            defender.supplemental_image = true;
            defender.frame_height = images_[1]->geometry.height;
            defender.target_height = 3;
            auto& script = defender.script;
            script.attacker = context_.attacker;
            script.target = static_cast<std::uint8_t>(preferred_target);
            script.flipped = flipped(context_, context_.attacker);
            script.rate = static_cast<std::uint8_t>((library_->programs[block_entry] >> 4U) + 1U);
            script.counter = 1;
            script.resume = script.instruction = block_entry + 2U;
            const auto align = static_cast<std::uint8_t>(library_->programs[block_entry + 1] & 0xe0U);
            script.attacker_position = point(context_, context_.attacker, images_[1]->geometry, align, false, background_anchors_[1]);
            script.target_position = point(context_, preferred_target, images_[1]->geometry, align, true, background_anchors_[1]);
            script.position = script.target_position;
            tasks_[preferred_target * 8U + 1U] = defender;
        }
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_33, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_34, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_35);
        // Blocking upload, weapon/hit images, and the published hit palette.
        loading_waits_ = 11;
        // Fight has a separate 96-tile weapon/block loader. Do not charge the
        // ordinary 128-tile spell-image path used by its preflight definition.
        preparation_waits_ = 0;
    } else
        initialize(definition_.initialization);
    // Block admission follows initialization. Keep one authored reflection
    // sprite, suppress its duplicate/extra tasks, and hide backgrounds when
    // every target reflected (C1/AE2F). The block script unpauses the layers.
    for (unsigned slot = 0; slot < 10; ++slot)
        if (context_.reflecting_targets[slot])
            for (unsigned copy = 1; copy < 8; ++copy)
                if (tasks_[slot * 8 + copy])
                    tasks_[slot * 8 + copy]->script.active = false;
    if (context_.reflecting_targets.any() && context_.hits.none()) {
        for (unsigned slot = 80; slot < tasks_.size(); ++slot)
            if (tasks_[slot])
                tasks_[slot]->script.active = false;
        paused_.fill(false);
    }
    initializing_ = false;
    mask_publication_rows_ = mask_.rows;
    for (auto& edge : mask_publication_rows_)
        if (edge == BattleMaskSpan{})
            edge = {255, 1}; // Original closed-window edges, converted to half-open.
    raster_picture_.reset(gradient_.colors, mask_publication_rows_);
    mask_pending_ = false;
}
BattleEffectImageState BattleAttackAnimation::retained_images() const {
    auto images = images_;
    if (foreground_frames_cleared_ && images[1])
        for (auto& frame : images[1]->frames)
            frame.clear();
    return {std::move(images),
            background_anchors_,
            background_flipped_,
            gradient_.phase,
            gradient_.intensity,
            scrolling_[2].projection_owned ? std::optional{scrolling_[2].projection}
                                           : std::nullopt};
}
void BattleAttackAnimation::copy_target_background(TargetBackgroundCopy kind) {
    const auto target = kind == TargetBackgroundCopy::attacker ? context_.attacker : selected_target(context_);
    const bool monster_only = kind == TargetBackgroundCopy::monster_only ||
                              kind == TargetBackgroundCopy::attacker;
    const bool death = kind == TargetBackgroundCopy::death;
    require(!monster_only || target >= 4,
            "Monster-only background initializer needs a monster target");
    // The monster-only family waits around one two-boundary target copy. The
    // general family additionally clears the overlay; a character instead
    // clears the foreground and retains its separately published image.
    if (monster_only)
        loading_waits_ += 4;
    else if (target >= 4)
        loading_waits_ += 6;
    else
        // Death retains the character image without the general foreground clear.
        loading_waits_ += death ? 3U : 4U;
    for (auto& pose : context_.characters)
        pose.appearance.priority = 3;
    for (auto& style : context_.monster_styles)
        style.priority = 3;
    if (target >= 4) {
        require(context_.monster_images != nullptr, "Monster background needs native actor images");
        BattleMonsterBackgroundRequest request;
        request.selected[target - 4] = true;
        request.shown = context_.shown_monsters;
        for (unsigned slot = 0; slot < 6; ++slot)
            request.displayed[slot] = context_.control.monster_visible[slot];
        request.visible = context_.visible_monsters;
        request.positions = context_.monsters;
        request.widths = context_.monster_widths;
        request.heights = context_.monster_heights;
        request.facing = context_.monster_facing;
        request.colors = context_.monster_colors;
        for (unsigned slot = 0; slot < 6; ++slot)
            request.palette_slots[slot] = context_.monster_styles[slot].palette;
        monster_background_ = copy_battle_monsters_to_background(*context_.monster_images, request);
        // The actor-background prefix clears the full target plane before
        // this initializer replaces its first nineteen rows.
        if (definition_.initialization & 128U)
            for (auto& cell : monster_background_->image.cells)
                cell.available = true;
        hidden_background_monsters_[target - 4] = !death;
        // The foreground palette commands own bank three after CopyPal. Keep
        // this retained snapshot distinct from later actor palette updates.
        // C1/1E63 clears the plane even when visibility filters every monster.
        // C1/1E89 (Rev 1) skips both palette and tile copies. Keep the foreground
        // colors already owned by LoadAnimProp, rather than rejecting an
        // invisible target (for example a vanished monster using Escape).
        copied_background_base_ = monster_background_->copied.any()
            ? monster_background_->image.palettes[1]
            : effect_colors(BattleEffectLayer::foreground);
        prioritize_from(static_cast<std::uint8_t>(target));
    } else {
        require(context_.published_character_frames.has_value(),
                "Character background needs published actor frames");
        copied_background_base_ = context_.character_colors[target];
    }
    copied_background_colors_ = copied_background_base_;
    if (images_[1] && copied_background_colors_)
        images_[1]->colors = images_[1]->base_colors = *copied_background_colors_;
    if (monster_only && context_.attacker < 4)
        context_.characters[context_.attacker].appearance.priority = 3;
    if (death) {
        if (target < 4)
            prioritize_from(static_cast<std::uint8_t>(target));
        if (target < 4)
            context_.characters[target].appearance.priority = 3;
        else
            context_.monster_styles[target - 4].priority = 3;
    }
}
void BattleAttackAnimation::initialize(std::uint8_t identity) {
    unsigned copies = 0, copy_delay = 0;
    const auto masked = [&](std::uint8_t shape, bool foreground, bool overlay) {
        mask_.shape = shape;
        mask_.rows.fill({});
        mask_.published = true;
        masked_backgrounds_ = {foreground, overlay};
    };
    const auto multipart_summon = [&] {
        require(tasks_[82].has_value(), "Three-part esper needs its first task");
        tasks_[82]->copy = 0;
        for (unsigned copy = 1; copy < 3; ++copy) {
            tasks_[82 + copy] = tasks_[82];
            tasks_[82 + copy]->copy = static_cast<std::uint8_t>(copy);
        }
    };
    if (identity == 0xf1 || (identity == 0xf3 && context_.hits.none())) {
        // InitAnimType retains only formation-designated background monsters.
        // This prefix survives Remedy and Soul Out's all-missed initializer;
        // copying every enemy here would incorrectly hide ordinary sprites or
        // recolor their palettes.
        require(context_.monster_images || context_.background_monsters.none(),
                "Formation background needs native actor images");
        BattleMonsterBackgroundRequest request;
        request.selected = context_.background_monsters;
        request.shown = context_.shown_monsters;
        request.visible = context_.visible_monsters;
        request.positions = context_.monsters;
        request.widths = context_.monster_widths;
        request.heights = context_.monster_heights;
        request.facing = context_.monster_facing;
        request.colors = context_.monster_colors;
        for (unsigned slot = 0; slot < 6; ++slot) {
            request.displayed[slot] = context_.control.monster_visible[slot] &&
                                      context_.control.monster_entry_visible[slot];
            request.palette_slots[slot] = context_.monster_styles[slot].palette;
        }
        monster_background_ = copy_battle_monsters_to_background(
            context_.monster_images ? *context_.monster_images : BattleMonsterBackgroundSource{}, request);
        for (auto& cell : monster_background_->image.cells) cell.available = true;
        hidden_background_monsters_ = request.selected;
        for (auto& pose : context_.characters) pose.appearance.priority = 3;
        for (auto& style : context_.monster_styles) style.priority = 3;
        identity &= 127U;
    }
    // These initializers replace the prefix's copied monsters, clear its hide
    // policy and select their own target. Other high-bit families still need
    // the separate formation-background owner and must not lose that flag.
    if (identity == 0x86 || identity == 0x9d || identity == 0xa4 || identity == 0xef || identity == 0xf0 ||
        identity == 0xa7 || identity == 0x88 || identity == 0xe2 || identity == 0xf7 || identity == 0xe6 ||
        identity == 0xf3)
        identity &= 127U;
    // Blend preset operands come from the ROM; set_blend owns only native
    // format conversion. Nonzero shape choices also come from ROM operands;
    // zero initialization is native state. Other setup/choreography retains
    // separate provenance, including window routing and reset control flow.
    // Each case owns the complete setup for this family, including pause and
    // duplicate timing. Families needing unowned actor/window/raster setup
    // remain rejected; sharing an eventual script does not make setup optional.
    switch (identity) {
    case 0:
    case 2:
        if (identity == 2)
            sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_14, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_14};
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_45, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_46, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_47);
        break;
    case 1:
    case 7:
        sprite_task_counts_[0] = FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_11;
        paused_[1] = true;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_45, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_46, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_47);
        break;
    case 5:
        // C2/F812 sets the sprite DRAW alternation flag ($60aa), not the
        // script pause flag. C1/2608,2669 draw two threads on odd frames;
        // every script, including the caster's forward step, keeps running.
        alternate_sprite_count_ = true;
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_12, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_13};
        break;
    case 4:
        // C2/F436: Debilitator opens the foreground circle and orders the
        // effect around its caster, then raises that caster above the mask.
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_42, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_43, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_44);
        masked(0, true, false);
        prioritize_from(context_.attacker);
        require(context_.attacker < 4, "Debilitator needs a character caster");
        context_.characters[context_.attacker].appearance.priority = 3;
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_2, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_2};
        break;
    case 8:
        // C2/F2BF: Slash first copies the target, then subtracts a masked
        // overlay from actors and terrain. Its sprite releases that overlay.
        copy_target_background(TargetBackgroundCopy::general);
        // This family calls ClearBG3Tiles_far again after the general target
        // copy. Its blank overlay publication waits once before script entry,
        // including repeated hits and character targets (C1/B07B, B0F6).
        ++loading_waits_;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_63, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_64, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_65);
        masked(0, false, true);
        overlay_high_priority_ = true;
        scrolling_[2].projection.fill({});
        scrolling_[2].projection_owned = true;
        background_views_[1] = {};
        paused_[2] = true;
        break;
    case 9:
        // C2/F49C: physical spell family reserves all actor threads while
        // its sprite program owns the foreground release.
        paused_[1] = true;
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_15, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_15};
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_36, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_37, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_38);
        break;
    case 0x0a:
        // Takedown/Stunner/Spin Edge share two tasks per actor and let the
        // sprite program release the foreground impact (AnimType_0a).
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_42, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_43, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_44);
        paused_[1] = true;
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_4, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_4};
        break;
    case 0x2f:
        // Rev 1 clears the failed image; successful Sketch expands the raw
        // monster stencil with its own palette, independently of actor visibility.
        visible_layers_ &= ~1U;
        if (context_.sketch_monster) {
            require(*context_.sketch_monster < library_->sketch_monsters.size(), "Invalid Sketch image");
            load_monster_image(library_->sketch_monsters[*context_.sketch_monster], BattleEffectLayer::foreground);
        } else if (images_[1]) {
            images_[1]->tiles.assign(256, Tile{});
        }
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x6e:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_2 - 0x180), BattleEffectLayer::sprite);
        require(library_->tigerbreak_palette.has_value(), "Tiger Break requires its original character palette");
        images_[0]->colors = images_[0]->base_colors = *library_->tigerbreak_palette;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_3, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_4, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_5);
        [[fallthrough]];
    case 0x0b:
    case 0x10:
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x0c:
        // AuraBolt's caster script releases both backgrounds after its charge.
        // C2/F46B masks BG3/color math with the ordinary expanding circle.
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_42, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_43, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_44);
        masked(0, false, true);
        paused_[1] = paused_[2] = true;
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_3, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_3};
        break;
    case 0x0e:
        // Mantra (C2/F7C7): the caster is ordered above the foreground blob;
        // only objects receive its additive light until the script changes it.
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_39, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_40, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_41);
        masked(FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_12, true, false);
        prioritize_from(context_.attacker);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_9, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_9};
        break;
    case 0x0d:
        break;
    case 0x13:
        // C2/F7A1: Bushido, Rage/Lore and Storm share a foreground circle.
        // Reset depths, then place the caster and actors behind it below BG1.
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        masked(0, true, false);
        prioritize_from(context_.attacker);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_8, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_8};
        break;
    case 0x11: {
        // White/effect casting uses three staggered pairs around the caster.
        // Each pair shares a frame; opposite members reverse the vertical
        // phase to trace crossing diagonal ellipses (AnimType_11 / 80/0A).
        const unsigned first = context_.attacker * 8U + 1;
        for (unsigned i = 0; i < FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_21; ++i) {
            require(first + i < 80 && tasks_[first + i].has_value(),
                    "White/effect pre-cast task is unowned");
            auto& task = *tasks_[first + i];
            task.script.counter = static_cast<std::uint8_t>(
                FF6_ROM_VALUE_BATTLE_PRECAST_DELAYS_0 + (i / 2) *
                (FF6_ROM_VALUE_BATTLE_PRECAST_DELAYS_1 - FF6_ROM_VALUE_BATTLE_PRECAST_DELAYS_0));
            task.frame_offset = static_cast<std::uint8_t>(i / 2);
            task.motion_phase = (i & 1U) ? FF6_ROM_VALUE_BATTLE_PRECAST_PHASES_0 : 0;
            task.radius = (i & 1U) ? FF6_ROM_VALUE_BATTLE_PRECAST_PHASES_1 : 0;
        }
        [[fallthrough]];
    }
    case 0x0f:
        // Both casting families prioritize relative to the caster, not the
        // target. The overlay stays behind actors, with shared BG1 color math.
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        prioritize_from(context_.attacker);
        sprite_task_counts_ = {static_cast<std::uint8_t>(identity == 0x11 ? FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_17 : FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_5), FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_5};
        overlay_high_priority_ = false;
        break;
    case 0x45:
    case 0x6b: {
        // Original image IDs use the shared monster-image directory; the
        // extracted summon slice has its own zero-based native index.
        const unsigned summon = identity == 0x45 ? (FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_5 - 0x180) : (FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_6 - 0x180);
        initialize_affine(summon, false);
        load_summon(summon, BattleEffectLayer::sprite);
        esper_behind_actors_ = true;
        break;
    }
    case 0x75:
        // H-bomb shares the authored affine loader with other cinematic
        // attacks. ROM addresses select graphics within the decompression
        // buffer at AE3F; only that resource origin is a native format rule.
        initialize_authored_affine(FF6_ROM_VALUE_BATTLE_ANIMATION_AFFINE_SOURCES_0 - 0xae3f,
                                   FF6_ROM_VALUE_BATTLE_ANIMATION_AFFINE_SOURCES_1);
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_3; copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_9;
        break;
    case 0x74:
        initialize_affine((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_0 - 0x180), true);
        // The initializer also loads the first figure into the sprite image
        // before replacing it with the second. Invisible work still waits.
        loading_waits_ += 5;
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_1 - 0x180), BattleEffectLayer::sprite);
        multipart_summon();
        mask_.rows.fill({});
        mask_.published = true;
        masked_backgrounds_[1] = true;
        break;
    case 0x44:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_11 - 0x180), BattleEffectLayer::foreground);
        [[fallthrough]];
    case 0x59:
        // Sonic Dive's primary party tasks each move their own target. Extra
        // copies and the background keep the original summoner as attacker.
        // Run Away shares this ownership setup without a summon image.
        for (unsigned slot = 0; slot < 4; ++slot)
            if (auto& task = tasks_[slot * 8]) {
                task->script.attacker = task->script.target;
                task->script.attacker_position = task->script.target_position;
            }
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x6d:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_3 - 0x180), BattleEffectLayer::foreground);
        [[fallthrough]];
    case 3:
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_42, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_43, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_44);
        mask_.shape = FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_13;
        mask_.rows.fill({});
        mask_.published = true;
        masked_backgrounds_[1] = true;
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_10, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_10};
        break;
    case 0x52:
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        mask_.shape = FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_3;
        mask_.rows.fill({});
        mask_.published = true;
        masked_backgrounds_[0] = true;
        break;
    case 0x77:
        // Possess uses its authored ghost frames with the user's live colors.
        // C2/EACB clears the other overlay and adds BG1 to BG3 (C2/FA0F).
        require(context_.attacker < 4 && images_[1], "Possess requires a character and ghost image");
        images_[1]->colors = images_[1]->base_colors = context_.character_colors[context_.attacker];
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_27, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_28, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_29);
        break;
    case 0x62:
        // C2/ED6E shares the monster copier with initializer 06, but chooses
        // the attacker. Mind Blast/Seize/Zinger distort their caster's image.
        copy_target_background(TargetBackgroundCopy::attacker);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_1, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_1};
        break;
    case 6:
        copy_target_background(TargetBackgroundCopy::monster_only);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_1, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_1};
        break;
    case 0x70:
    case 0x66:
    case 0x1d:
    case 0x24:
    case 0x6f:
    case 0x7c:
    case 0x7b:
    case 0x58:
        copy_target_background(TargetBackgroundCopy::general);
        // Monster fade/entry copies share the ordinary target image, with
        // the overlay in front of actors (ANIM_TYPE_7c / ANIM_TYPE_1d).
        overlay_high_priority_ = identity == 0x7c || identity == 0x7b || identity == 0x58;
        // Mirager shares the target-copy owner, then waits for its character
        // script to release BG1 (C2/EC4B). Do not run that thread early.
        if (identity == 0x66) paused_[1] = true;
        if (identity == 0x7b) visible_layers_ &= ~1U;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_27, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_28, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_29);
        // Clear reuses the ordinary target copy, then raises its overlay and
        // selects FA33's additive actor/terrain/foreground composition.
        if (identity == 0x58) set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_42, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_43, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_44);
        if (identity == 0x24) {
            copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_13;
            copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_16;
        } else if (identity == 0x6f) {
            copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_6;
            copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_11;
        } else if (identity == 0x70) {
            // Revivify/Eyedrop combine six sprite tasks with the ordinary
            // actor image copy. The background tasks stay inactive for a
            // displayed dead character; newly resolved status is too early.
            copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_12;
            paused_[1] = false;
            const auto target = selected_target(context_);
            if (target < 4 && (context_.shown_character_status[target] & 128U)) {
                tasks_[80].reset();
                tasks_[81].reset();
            }
        }
        break;
    case 0x12: {
        require(context_.attacker < 4, "Summon pre-cast needs a character attacker");
        const unsigned first = context_.attacker * 8U + 1;
        for (unsigned i = 0; i < FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_22; ++i) {
            require(tasks_[first + i].has_value(), "Summon pre-cast task is unowned");
            tasks_[first + i]->script.counter = FF6_ROM_VALUE_BATTLE_PRECAST_DELAYS_3;
            tasks_[first + i]->motion_phase = static_cast<std::uint8_t>(i * FF6_ROM_VALUE_BATTLE_PRECAST_PHASES_2);
        }
        sprite_task_counts_[0] = FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_16;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_48, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_49, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_50);
        break;
    }
    case 0x33:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_28 - 0x180), BattleEffectLayer::sprite);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_39, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_40, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_41);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x34:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_27 - 0x180), BattleEffectLayer::sprite);
        esper_behind_actors_ = true;
        break;
    case 0x35:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_26 - 0x180), BattleEffectLayer::sprite);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_9, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_10, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_11);
        mask_.shape = FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_6;
        mask_.rows.fill({});
        mask_.published = true;
        masked_backgrounds_[0] = true;
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x36:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_25 - 0x180), BattleEffectLayer::sprite);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        mask_.rows.fill({});
        mask_.published = true;
        masked_backgrounds_[1] = true;
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x37:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_24 - 0x180), BattleEffectLayer::foreground);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_21, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_22, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_23);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x3a:
    case 0x42: {
        require(library_->target_copies.has_value(), "Target-dependent task table is unowned");
        copies = (*library_->target_copies)[std::min<std::size_t>(6, context_.targets.count())];
        copy_delay = identity == 0x3a ? FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_23 : FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_24;
        mask_.shape = identity == 0x3a ? FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_10 : 0;
        load_summon(identity == 0x3a ? (FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_21 - 0x180) : (FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_13 - 0x180), BattleEffectLayer::foreground);
        if (identity == 0x3a) {
            gradient_.phase = gradient_.intensity = 0;
            set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_24, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_25, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_26);
        } else {
            mask_.rows.fill({});
            mask_.published = true;
            masked_backgrounds_[1] = true;
        }
        break;
    }
    case 0x3d:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_18 - 0x180), BattleEffectLayer::sprite);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_39, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_40, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_41);
        mask_.shape = FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_5;
        mask_.rows.fill({});
        mask_.published = true;
        masked_backgrounds_[0] = true;
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x3b:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_20 - 0x180), BattleEffectLayer::foreground);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_27, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_28, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_29);
        // Justice's initializer resets actor depth, then lowers participants
        // whose bottom anchor is above the ROM-selected cutoff. Rectangle
        // extents/endpoints are separate ROM operands; four bytes represent
        // one source row record, independently of depth and blend operands.
        for (unsigned slot = 0; slot < 10; ++slot) {
            const std::uint8_t priority = context_.anchors[slot].bottom_y < FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_EXTENTS_1 ? 2 : 3;
            if (slot < 4)
                context_.characters[slot].appearance.priority = priority;
            else
                context_.monster_styles[slot - 4].priority = priority;
        }
        mask_.rows.fill({});
        std::fill_n(mask_.rows.begin(), FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_EXTENTS_3 / 4, BattleMaskSpan{FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SPAN_PATTERN_0 & 0xffffU, FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SPAN_PATTERN_0 >> 16U});
        mask_.published = true;
        masked_backgrounds_[0] = true;
        multipart_summon();
        overlay_high_priority_ = false;
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x68:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_9 - 0x180), BattleEffectLayer::foreground);
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_18;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        break;
    case 0x69:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_8 - 0x180), BattleEffectLayer::foreground);
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_11;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_15;
        break;
    case 0x6a:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_7 - 0x180), BattleEffectLayer::foreground);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x40:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_15 - 0x180), BattleEffectLayer::foreground);
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_18;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_42, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_43, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_44);
        break;
    case 0x38:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_23 - 0x180), BattleEffectLayer::sprite);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_21, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_22, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_23);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x39:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_22 - 0x180), BattleEffectLayer::sprite);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_15, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_16, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_17);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x3c:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_19 - 0x180), BattleEffectLayer::sprite);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x3e:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_17 - 0x180), BattleEffectLayer::sprite);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_6, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_7, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_8);
        mask_.rows.fill({});
        mask_.published = true;
        masked_backgrounds_[0] = true;
        overlay_high_priority_ = false;
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x43:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_12 - 0x180), BattleEffectLayer::sprite);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        mask_.rows.fill({});
        mask_.published = true;
        masked_backgrounds_[1] = true;
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x3f:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_16 - 0x180), BattleEffectLayer::sprite);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_9, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_10, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_11);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x41:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_14 - 0x180), BattleEffectLayer::sprite);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x46:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_10 - 0x180), BattleEffectLayer::foreground);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x6c:
        load_summon((FF6_ROM_VALUE_BATTLE_SUMMON_IMAGE_CHOICES_4 - 0x180), BattleEffectLayer::sprite);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_15, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_16, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_17);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x14:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_13;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_16;
        break;
    case 0x15:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_11;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_15;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_42, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_43, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_44);
        break;
    case 0x17:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_10;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_8;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        break;
    case 0x16:
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        masked(FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_11, true, false);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_6, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_6};
        break;
    case 0x18:
        // Absorption particles converge on the target before travelling back
        // to the caster. Their leading task releases the paused foreground.
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_39, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_40, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_41);
        masked(0, true, false);
        paused_[1] = true;
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_14;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_17;
        break;
    case 0x19:
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_60, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_61, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_62);
        masked(0, true, false);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_7, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_7};
        break;
    case 0x1a:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_15;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_18;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_42, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_43, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_44);
        masked(0, false, true);
        break;
    case 0x1b:
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        break;
    case 0x1f:
    case 0x29: {
        paused_[1] = identity == 0x1f;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_21, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_22, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_23);
        masked(identity == 0x1f ? 0 : FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_8, true, false);
        const auto target = selected_target(context_);
        if (target < 4)
            context_.characters[target].appearance.priority = 3;
        else
            context_.monster_styles[target - 4].priority = 3;
        break;
    }
    case 0x1c:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_18;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        break;
    case 0x22:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_12;
        paused_[1] = true;
        break;
    case 0x23:
    case 0x2d:
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_42, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_43, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_44);
        break;
    case 0x25:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_4;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_10;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_45, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_46, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_47);
        break;
    case 0x26:
        overlay_high_priority_ = false;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_27, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_28, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_29);
        break;
    case 0x20:
        // C2/F3B5: TekBarrier has six particles eight updates apart. Its
        // script changes the initially closed circle into a moving triangle.
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_16;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_19;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_24, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_25, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_26);
        masked(0, true, true);
        break;
    case 0x27:
    case 0x73:
        if (context_.hits.any()) {
            copy_target_background(TargetBackgroundCopy::death);
            overlay_high_priority_ = false;
            set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        }
        if (identity == 0x73) {
            // C2/EB87: Soul Out retains the death-copy hit policy, pauses
            // the foreground until a particle releases it, and branches four
            // simultaneous particles. Its final blend comes from C2/F011.
            paused_[1] = true;
            copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_6;
            set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_21, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_22, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_23);
        }
        break;
    case 0x28:
        require(library_->target_copies.has_value(), "Target-dependent task table is unowned");
        copies = (*library_->target_copies)[std::min<std::size_t>(6, context_.targets.count())];
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_23;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_24, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_25, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_26);
        masked(FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_10, true, true);
        break;
    case 0x2b:
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_57, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_58, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_59);
        break;
    case 0x7e: {
        // C2/E9D7: BG1 carries the thrown ally to the enemy. Only the sprite
        // tasks become Umaro approaching that ally. Each script saves/restores
        // its own actor, so no final-position patch is needed.
        paused_[1] = true;
        const auto found = std::ranges::find(context_.character_identities, std::optional<std::uint8_t>{13});
        require(found != context_.character_identities.end(), "Ally throw needs Umaro's character image");
        const auto user = static_cast<std::uint8_t>(found - context_.character_identities.begin());
        for (unsigned i = 0; i < 80; ++i) if (auto& task = tasks_[i]) {
            auto& script = task->script;
            script.target = script.attacker;
            script.target_position = script.attacker_position;
            script.attacker = user;
            script.attacker_position = {
                wrap16(static_cast<std::uint32_t>(context_.anchors[user].center_x - 8)),
                wrap16(static_cast<std::uint32_t>(context_.anchors[user].center_y - 8))};
        }
        break;
    }
    case 0x2a:
    case 0x7a:
        // C2/F3E9: X-Zone owns the large shrinking overlay mask. C2/EA88
        // calls this setup before branching Magicite transformation particles;
        // neither caller uses the unrelated Umaro ally-throw actor remapping.
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_18, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_19, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_20);
        masked(FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_9, false, true);
        if (identity == 0x7a) {
            // C2/EA88: Magicite transformation shares X-Zone's mask,
            // with eight staggered sprite tasks releasing the paused overlay.
            copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_7;
            copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_2;
            paused_[2] = true;
        }
        break;
    case 0x30:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_19;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_39, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_40, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_41);
        masked(0, true, false);
        break;
    case 0x31:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_20;
        overlay_high_priority_ = false;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_12, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_13, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_14);
        paused_[1] = true;
        break;
    case 0x32:
        // This shape keeps every row above its center and compresses the
        // lower half. Both image layers use the same published outline.
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_54, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_55, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_56);
        masked(FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_7, true, true);
        overlay_high_priority_ = false;
        break;
    case 0x72:
        // C2/EB2C combines the six simultaneous particles of $22 with $32's
        // lower-half window, but explicitly leaves foreground tasks running.
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_12;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_54, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_55, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_56);
        masked(FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_7, true, true);
        overlay_high_priority_ = false;
        break;
    case 0x47:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_6;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_11;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_21, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_22, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_23);
        break;
    case 0x48:
        // Harvester/Scar Beam (C2/EFFE): target-count-dependent particles,
        // sixteen pictures apart, with actors above y=96 behind the effect.
        require(library_->target_copies.has_value(), "Target-dependent task table is unowned");
        copies = (*library_->target_copies)[std::min<std::size_t>(6, context_.targets.count())];
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_25;
        mask_.shape = FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_10;
        for (unsigned slot = 0; slot < 10; ++slot) {
            const std::uint8_t priority = context_.anchors[slot].bottom_y < FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_EXTENTS_0 ? 2 : 3;
            if (slot < 4)
                context_.characters[slot].appearance.priority = priority;
            else
                context_.monster_styles[slot - 4].priority = priority;
        }
        break;
    case 0x4d:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_2;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_5;
        break;
    case 0x49:
    case 0x50:
        // Lifeshaver/Snare lower only the first frame's top tile rows.
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_3, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_4, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_5);
        foreground_low_rows_ = identity == 0x50 ? (FF6_ROM_VALUE_EFFECT_LIFESHAVER_LOW_ROWS_0 + FF6_ROM_VALUE_EFFECT_LOW_ROW_DIFFERENCE_0) : FF6_ROM_VALUE_EFFECT_LIFESHAVER_LOW_ROWS_0;
        for (auto& pose : context_.characters) pose.appearance.priority = 3;
        for (auto& style : context_.monster_styles) style.priority = 3;
        if (identity == 0x50) masked(FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_4, true, false);
        break;
    case 0x4b: {
        // Specter C2/EF7B shares the staggered flame tasks, but its two
        // foreground frames sample the caster's published character sheet.
        // Only the first two rows of 16x16 cells are rewritten; frame one's
        // own flip flags survive while its selectors come from frame zero.
        require(images_[1] && images_[1]->frames.size() >= 2,
                "Specter requires its foreground frame set");
        auto& image = *images_[1];
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_8; copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_3;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_57, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_58, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_59);
        require(context_.published_character_frames.has_value(),
                "Specter requires a published caster image");
        image.character_frames = true;
        image.adjacent_status_frames = context_.attacker >= 4;
        require(!image.adjacent_status_frames || context_.published_status_tiles.has_value(),
                "Enemy Specter requires the adjacent published status images");
        for (auto& piece : image.frames[0]) if (piece.y < FF6_ROM_VALUE_EFFECT_SPECTER_COPY_HEIGHT_0) {
            piece.tile = wrap8(unsigned{piece.tile} + context_.attacker * 4U);
            auto& second = image.frames[1];
            const auto found = std::ranges::find_if(second, [&](const auto& other) {
                return other.x == piece.x && other.y == piece.y;
            });
            if (found != second.end()) found->tile = piece.tile;
            else second.push_back({piece.x, piece.y, piece.tile, false, false});
        }
        image.colors = image.base_colors = specter_colors(context_);
        break;
    }
    case 0x4a:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_8;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_3;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_21, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_22, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_23);
        break;
    case 0x4c:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_9;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_4;
        masked(FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_11, true, false);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_39, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_40, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_41);
        break;
    case 0x4e:
        paused_[0] = paused_[2] = true;
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_2;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_6;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_15, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_16, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_17);
        break;
    case 0x4f:
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_21, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_22, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_23);
        break;
    case 0x51:
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_15, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_16, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_17);
        break;
    case 0x53:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_17;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_20;
        paused_[1] = true;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_57, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_58, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_59);
        break;
    case 0x54:
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_51, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_52, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_53);
        mask_.shape = 0;
        mask_.rows.fill({});
        mask_.published = true;
        masked_backgrounds_[1] = true;
        break;
    case 0x55:
        copy_target_background(TargetBackgroundCopy::general);
        overlay_high_priority_ = true;
        mask_.shape = FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_2;
        mask_.rows.fill({});
        mask_.published = true;
        masked_backgrounds_[1] = true;
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_17;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_21;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_42, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_43, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_44);
        break;
    case 0x56:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_1;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_7;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        break;
    case 0x57:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_6;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_12;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_21, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_22, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_23);
        break;
    case 0x5a:
        masked(FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_1, true, false);
        prioritize_from(context_.attacker);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_9, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_9};
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        break;
    case 0x61:
        // Flare Star windows both image layers around its expanding circle.
        masked(0, true, true);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_1, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_2);
        break;
    case 0x5b:
        // Delta Hit/Evil Toot (C2/EE76) duplicate the dedicated image task,
        // not each target's sprite task. All three start together and use
        // their copy index to select their own motion through the circle.
        if (tasks_[82])
            multipart_summon();
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_18, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_18};
        masked(0, true, false);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        break;
    case 0x5c:
        // Shared missile/launcher setup opens the ROM-selected foreground
        // rectangle. Dedicated-task start counters come from the same script owner.
        mask_.rows.fill({});
        std::fill_n(mask_.rows.begin(), FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_EXTENTS_2 / 4, BattleMaskSpan{FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SPAN_PATTERN_0 & 0xffffU, FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SPAN_PATTERN_0 >> 16U});
        mask_.published = true;
        masked_backgrounds_[0] = true;
        // Some callers have no dedicated task. Copying the original inactive
        // slots does not create a visible task or invent a resource owner.
        if (tasks_[82]) {
            multipart_summon();
            for (unsigned i = 0; i < 3; ++i)
                tasks_[82 + i]->script.counter = static_cast<std::uint8_t>(
                    std::array{FF6_ROM_VALUE_EFFECT_PARTICLE_START_FIRST_0,
                               FF6_ROM_VALUE_EFFECT_PARTICLE_START_MIDDLE_0,
                               FF6_ROM_VALUE_EFFECT_PARTICLE_START_LAST_0}[i]);
        }
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        break;
    case 0x5d:
        // GrandTrain/Train (C2/EDFD) start a closed circle over BG3, then
        // share the BG1/BG3 additive setup. The scripts own its later shape.
        masked(0, false, true);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_15, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_16, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_17);
        break;
    case 0x5e:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_10;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_14;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        break;
    case 0x5f: {
        // C2/EDAC edits two rectangles in frame zero's 16x16-cell layout,
        // rather than separate animation frames. Extraction decodes their extents.
        require(images_[1].has_value(), "Unfolding effect needs a foreground image");
        auto& cells = images_[1]->secondary_palette_cells;
        cells.resize(1);
        // Authored extents and attribute addresses come from private ROM
        // inputs. Preserve the native loop shape; the build strips its folded
        // membership words using the independently derived rectangle pattern.
        constexpr unsigned width = FF6_ROM_VALUE_BATTLE_ANIMATION_PALETTE_RECTANGLE_PREFIX_SOURCE_0;
        constexpr unsigned height = (FF6_ROM_VALUE_BATTLE_ANIMATION_PALETTE_RECTANGLE_PREFIX_SOURCE_7 | (FF6_ROM_VALUE_BATTLE_ANIMATION_PALETTE_RECTANGLE_PREFIX_SOURCE_8 << 8U)) / 32U;
        constexpr unsigned first_cell = ((FF6_ROM_VALUE_BATTLE_ANIMATION_PALETTE_RECTANGLE_PREFIX_SOURCE_1 | (FF6_ROM_VALUE_BATTLE_ANIMATION_PALETTE_RECTANGLE_PREFIX_SOURCE_2 << 8U)) - 0xc401U) / 2U;
        constexpr unsigned second_cell = ((FF6_ROM_VALUE_BATTLE_ANIMATION_PALETTE_RECTANGLE_PREFIX_SOURCE_3 | (FF6_ROM_VALUE_BATTLE_ANIMATION_PALETTE_RECTANGLE_PREFIX_SOURCE_4 << 8U)) - 0xc401U) / 2U;
        static_assert(width * 2U + (FF6_ROM_VALUE_BATTLE_ANIMATION_PALETTE_RECTANGLE_PREFIX_SOURCE_5 | (FF6_ROM_VALUE_BATTLE_ANIMATION_PALETTE_RECTANGLE_PREFIX_SOURCE_6 << 8U)) == 32U);
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x) {
                cells[0].set(y * 16 + x + first_cell);
                cells[0].set((y + second_cell / 16) * 16 + x + second_cell % 16);
            }
        if (tasks_[82]) {
            multipart_summon();
            for (unsigned i = 0; i < 3; ++i)
                tasks_[82 + i]->script.counter = static_cast<std::uint8_t>(
                    std::array{FF6_ROM_VALUE_EFFECT_PARTICLE_START_FIRST_0,
                               FF6_ROM_VALUE_EFFECT_PARTICLE_START_MIDDLE_0,
                               FF6_ROM_VALUE_EFFECT_PARTICLE_START_LAST_0}[i]);
        }
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_15, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_16, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_17);
        break;
    }
    case 0x60:
        // Goner prepares the outline but does not enable either image mask
        // until its script does so. Premature masking hides its opening flash.
        masked(0, false, false);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        break;
    case 0x63:
        // C2/ECF6 copies the dedicated S.Cross particle into six pairs of
        // ordinary task groups, retaining its actor references. These groups
        // are drawing slots, not a requirement for six present combatants.
        initialize_authored_affine(FF6_ROM_VALUE_BATTLE_ANIMATION_AFFINE_SOURCES_2 - 0xae3f,
                                   FF6_ROM_VALUE_BATTLE_ANIMATION_AFFINE_SOURCES_3);
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0};
        require(tasks_[82].has_value(), "Cross effect needs its dedicated particle task");
        // Extraction authenticates both selected extents and both interval
        // operands. The offline builder also restores the complete unrolled
        // native copy, so its chosen repeat count cannot survive in a template.
        for (unsigned group = 0; group < FF6_ROM_VALUE_EFFECT_CROSS_COPY_GROUPS_0; ++group)
            for (unsigned particle = 0; particle < 2; ++particle) {
                auto& task = tasks_[group * 8 + particle];
                task = tasks_[82];
                task->copy = static_cast<std::uint8_t>(particle == 0 ? 1 : 0);
                task->script.counter = static_cast<std::uint8_t>(
                    (group * 2 + particle + 1) * FF6_ROM_VALUE_EFFECT_CROSS_COPY_INTERVAL_0);
            }
        break;
    case 0x64:
        initialize_authored_affine(FF6_ROM_VALUE_BATTLE_ANIMATION_AFFINE_SOURCES_4 - 0xae3f,
                                   FF6_ROM_VALUE_BATTLE_ANIMATION_AFFINE_SOURCES_5);
        affine_radius_ = 255;
        sprite_task_counts_ = {FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0, FF6_ROM_VALUE_BATTLE_ANIMATION_THREAD_LIMITS_0}; // Shared init 0B.
        break;
    case 0x65:
        // C2/EC52: ForceField starts a closed horizontal oval on the overlay.
        // Its script expands that mask and owns the actor/terrain white fade;
        // the initializer does not change the shared color-math setup.
        masked(FF6_ROM_VALUE_BATTLE_ANIMATION_MASK_SHAPES_0, false, true);
        break;
    case 0x76:
        // C2/EB1D: WaveCannon opens a circular foreground window in its script.
        masked(0, true, false);
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_30, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_31, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_32);
        break;
    case 0x67:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_6;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_13;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_57, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_58, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_59);
        break;
    case 0x79:
        // Event Espers use the default-sound byte as an image selector.
        // C2/EAA7 loads that figure into the foreground with its full palette.
        require(definition_.sound < library_->event_esper_images.size(),
                "Invalid event Esper image selector");
        load_summon(library_->event_esper_images[definition_.sound], BattleEffectLayer::foreground);
        break;
    case 0x78:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_5;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_1;
        break;
    case 0x71:
        // Remedy shares the six-task copier. SetColorMathHDMA preserves A,
        // so its following copier receives delay 16 (not Grav Bomb's two).
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_17;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_22;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_21, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_22, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_23);
        break;
    case 0x7d:
        copies = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_COUNTS_0;
        copy_delay = FF6_ROM_VALUE_BATTLE_PARTICLE_COPY_DELAYS_0;
        set_blend(FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_21, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_22, FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_23);
        break;
    default:
        throw std::invalid_argument("Attack animation initializer is not implemented: " +
                                    std::to_string(identity));
    }
    if (copies)
        sprite_task_counts_.fill(static_cast<std::uint8_t>(copies + 1));
    for (unsigned i = 0; i < 10; ++i)
        if (tasks_[i * 8])
            for (unsigned copy = 1; copy <= copies; ++copy) {
                auto task = *tasks_[i * 8];
                task.copy = static_cast<std::uint8_t>(copy);
                task.script.counter = wrap8(unsigned{task.script.counter} + copy * copy_delay);
                tasks_[i * 8 + copy] = task;
            }
}
void BattleAttackAnimation::update_anchors(const BattleSceneAnchors& anchors) {
    context_.anchors = anchors;
}
void BattleAttackAnimation::update_draw_order(const std::array<BattleDrawTarget, 10>& order) {
    std::bitset<10> seen;
    for (const auto& item : order)
        if (item.participant) {
            require(*item.participant < 10 && !seen[*item.participant],
                    "Invalid effect actor ordering");
            seen[*item.participant] = true;
        }
    context_.draw_order = order;
}
void BattleAttackAnimation::prioritize_from(std::uint8_t participant, bool reset) {
    if (reset) {
        for (auto& pose : context_.characters)
            pose.appearance.priority = 3;
        for (auto& style : context_.monster_styles)
            style.priority = 3;
    }
    bool lower = false;
    for (const auto& item : context_.draw_order) {
        if (!item.participant)
            continue;
        const auto slot = *item.participant;
        lower = lower || slot == participant;
        if (lower) {
            if (slot < 4)
                context_.characters[slot].appearance.priority = 2;
            else
                context_.monster_styles[slot - 4].priority = 2;
        }
    }
}
void BattleAttackAnimation::set_blend(std::uint8_t operation, std::uint8_t affected,
                                      std::uint8_t operand) {
    // The initializer helpers put affected layers in the high byte of $10;
    // A selects the operand layers. Some disassembly comments reverse those
    // roles. Follow SetColorMathHDMA's instructions, as for script command D4.
    if (operation == 0 && affected == 0 && operand == 0) {
        operand_layers_ = blended_layers_ = 0;
        blend_ = ColorBlend::none;
        fixed_color_ = false;
        return;
    }
    require((operation == 0 || operation == 2 || operation == 0x20) && (affected & 0x08U) == 0 &&
                (operand & 0xe8U) == 0,
            "Animation color math needs unsupported window coverage");
    fixed_color_ = (operation & 2U) == 0;
    operand_layers_ = fixed_color_ ? 0 : operand;
    blended_layers_ = affected & 0x37U;
    blend_ = (affected & 128U)
                 ? ((affected & 64U) ? ColorBlend::half_difference : ColorBlend::subtract)
                 : ((affected & 64U) ? ColorBlend::average : ColorBlend::add);
}
std::uint8_t BattleAttackAnimation::random() {
    const auto value = library_->random[context_.random_index];
    context_.random_index = wrap8(unsigned{context_.random_index} + 1U);
    return value;
}
bool BattleAttackAnimation::command(std::size_t id, BattleEffectLayer execution_layer) {
    auto& task = *tasks_[id];
    auto& s = task.script;
    const bool event_script = event_task_sources_[id];
    const ByteView programs =
        event_script ? ByteView{*event_programs_} : ByteView{library_->programs};
    const auto read = [&]() {
        require(s.instruction < programs.size(), "Truncated attack animation command");
        return programs[s.instruction++];
    };
    const auto word = [&]() {
        const auto low = read(), high = read();
        return std::size_t{low} | (std::size_t{high} << 8U);
    };
    const auto jump = [&](std::size_t to) {
        require(to < programs.size(), "Attack animation branch leaves its resource");
        s.instruction = to;
    };
    const auto absolute = [&](std::size_t pointer) {
        const unsigned base = event_script ? 0x9800U : 0U;
        require(pointer >= base && pointer - base < programs.size(), "Animation pointer leaves its resource");
        return pointer - base;
    };
    const auto sound = [&](std::uint8_t identity, std::uint16_t pan) {
        pending_sound_ = GameSoundEffect{identity ? identity : definition_.sound, wrap8(pan)};
        sound_requests_.push_back(*pending_sound_);
    };
    const auto op = read();
    if (op >= 128 && op != 255) {
        const auto work = battle_task_command_entry_work(op == 128);
        accrue_update_work(work.cycles, (work.fast_rom_clocks - 6U * work.cycles) / 2U);
    }
    if (s.attacker >= 10 || s.target >= 10)
        require(event_script && (op == 31 || op == 255 || op == 0x89 || op == 0x8a || op == 0xbf ||
                                 op == 0xc0 || op == 0xc7),
                "Event task without a participant used a participant-dependent command");
    if (op < 128) {
        const auto layer = index(execution_layer);
        // Sprite/BG1 frame 31 is exempt from animated-loop offsets. BG3
        // has its own blank frame (9), processed like any other frame there.
        s.effect_frame = static_cast<std::uint8_t>(
            op == 31 && layer != 2 ? op : (op + task.frame_offset) & 127U);
        s.resume = s.instruction;
        task.sprite_visible = layer == 0;
        if (task.sprite_visible)
            task.selected_sprite = {{add(s.position.x, s.offset.x), add(s.position.y, s.offset.y)},
                                    task.priority,
                                    task.palette,
                                    s.flipped,
                                    task.supplemental_image, task.character_image};
        if (layer == 1 || layer == 2) {
            pending_backgrounds_[layer - 1] = s.effect_frame;
            background_positions_[layer - 1] = {add(s.position.x, s.offset.x),
                                                add(s.position.y, s.offset.y)};
        }
        return true;
    }
    if (op == 255) {
        s.active = false;
        task.sprite_visible = false;
        s.effect_frame.reset();
        if (index(execution_layer) == 1 || index(execution_layer) == 2) {
            s.resume = s.instruction;
            background_views_[index(execution_layer) - 1].x = 256;
            if (execution_layer == BattleEffectLayer::overlay)
                scrolling_[2].projection[0].x = 256;
        }
        return true;
    }
    switch (op) {
    case 0x80: {
        const auto extension = read();
        if (extension == 0x00 || extension == 0x01) {
            require(s.attacker < 4, "Character return requires a character attacker");
            auto& actor = context_.characters[s.attacker];
            // C1/D9A9,D9BE retain the step-forward displacement separately
            // from the traveling offsets used by the four-strike animation.
            if (extension == 0x00)
                s.target_position = {add(actor.x, actor.animation_x), actor.y};
            else {
                actor.offset_x = actor.offset_y = 0;
                s.offset = {};
            }
        } else if (extension == 0x48) {
            // C1/CF8A (Rev 1): clear only the selected monster's image bit.
            // Character targets are unchanged. This is neither actor removal
            // nor the independent script hide/show gate used by E1/F1.
            if (s.target >= 4) context_.visible_monsters.reset(s.target - 4U);
        } else if (extension == 0x83) {
            // C2/AF63: keep the new form hidden while its predecessor owns
            // the foreground copy. The following exit effect restores it.
            context_.control.monster_overlap.emplace();
            if (s.target >= 4) {
                const auto slot = s.target - 4U;
                (*context_.control.monster_overlap)[slot] = FF6_ROM_VALUE_EFFECT_TRANSITION_OVERLAP_0; // Original word-wrapped displacement.
                context_.control.staged_monster = static_cast<std::uint8_t>(slot);
                context_.control.staged_monster_y = context_.monsters[slot].y;
                context_.monsters[slot].y = add(context_.monsters[slot].y, -FF6_ROM_VALUE_EFFECT_TRANSITION_DISPLACEMENT_0);
            }
        } else if (extension == 0x84) {
            // C1/CA29 restores the incoming form, not this outgoing target.
            require(context_.control.staged_monster && *context_.control.staged_monster < 6,
                    "Monster crossfade has no staged entrance");
            context_.monsters[*context_.control.staged_monster].y = context_.control.staged_monster_y;
            context_.control.staged_monster.reset();
        } else if (extension == 0x85) {
            const auto mode = read();
            require(mode <= 2 && library_->transition_lighting && context_.battlefield_colors.size() == 3,
                    "Monster transition needs three lighting palettes and a valid mode");
            // C2/AF21: the random mode changes to dark only one time in eight;
            // unsuccessful rolls leave the current colors alone.
            if (mode != 2 || (random() & FF6_ROM_VALUE_EFFECT_TRANSITION_CHANCE_0) == 0) {
                const auto& colors = (*library_->transition_lighting)[mode ? 1 : 0];
                std::ranges::copy(colors, context_.battlefield_colors.begin());
            }
        } else if (extension == 0x79 || extension == 0x7a || extension == 0x7b) {
            // C2/B2B7–B332: the authored loop moves the whole party, including
            // fallen/petrified actors. Only their running pose is suppressed.
            // The final flip persists into the following reinforcement battle.
            for (unsigned i = 0; i < 4; ++i) {
                auto& pose = context_.characters[i];
                if (extension == 0x7b) {
                    pose.action = 0;
                    pose.facing ^= 1U;
                    if (context_.character_hand_swaps)
                        (*context_.character_hand_swaps)[i] = !(*context_.character_hand_swaps)[i];
                } else {
                    pose.desired_angle = add(pose.desired_angle, extension == 0x79 ? -FF6_ROM_VALUE_EFFECT_PARTY_RUN_STEP_0 : FF6_ROM_VALUE_EFFECT_PARTY_RUN_STEP_0);
                    if (!(context_.shown_character_status[i] & 0xc0U))
                        pose.action = extension == 0x79 ? 4 : 5;
                }
            }
        } else if (extension == 0x76) {
            // C2/B38D: 64 rows climb from the caster's baseline. Accumulate
            // the fractional slope before each integer displacement; the
            // return steps reuse the same word rather than restarting it.
            const auto increment = read();
            task.seize_stretch = add(task.seize_stretch, increment < 128 ? int{increment} : int{increment} - 256);
            auto& scroll = scrolling_[0];
            unsigned fraction = 0;
            std::uint16_t displacement = 0;
            for (int i = 0; i < FF6_ROM_VALUE_EFFECT_SEIZE_ROWS_0; ++i) {
                const auto sum = wrap16(fraction + task.seize_stretch);
                fraction = sum & 255U;
                displacement = add(displacement, s.flipped ? -int{sum >> 8U} : int{sum >> 8U});
                const int row = int{wrap8(s.attacker_position.y)} - i;
                if (row >= 0 && static_cast<unsigned>(row) < scroll.projection.size())
                    scroll.projection[static_cast<unsigned>(row)].x = displacement;
            }
            scroll.projection_owned = true;
        } else if (extension == 0x77 || extension == 0x78) {
            require(s.target < 4, "Seize placement requires a character target");
            auto& pose = context_.characters[s.target];
            if (extension == 0x77) {
                pose.seize_origin = std::pair{pose.x, pose.y};
                pose.x = s.attacker_position.x;
                pose.y = add(s.attacker_position.y, -FF6_ROM_VALUE_EFFECT_SEIZE_HEIGHT_0);
            } else {
                require(pose.seize_origin.has_value(), "Discard needs the retained Seize placement");
                pose.x = pose.seize_origin->first;
                pose.y = pose.seize_origin->second;
                pose.offset_x = pose.animation_x = pose.offset_y = pose.jump_y = 0;
                pose.seize_origin.reset();
            }
        } else if (extension == 0x19) {
            require(context_.sketch_monster && *context_.sketch_monster < library_->sketch_monsters.size(),
                    "Sketch position needs its monster image");
            const auto height = library_->sketch_monsters[*context_.sketch_monster].dimensions.height;
            s.position.y = add(s.position.y, (FF6_ROM_VALUE_EFFECT_SKETCH_HEIGHT_0 - int{height}) * 8 - (height == FF6_ROM_VALUE_EFFECT_SKETCH_HEIGHT_0 ? FF6_ROM_VALUE_EFFECT_SKETCH_FULL_OFFSET_0 : FF6_ROM_VALUE_EFFECT_SKETCH_OFFSET_0));
            require(s.attacker < 4, "Sketch priority needs a character");
            context_.characters[s.attacker].appearance.priority = 3;
        } else if (extension == 0x18) {
            require(context_.sketch_monster && images_[1], "Sketch palette needs its image");
            images_[1]->colors = images_[1]->base_colors =
                library_->sketch_monsters.at(*context_.sketch_monster).colors;
        } else if (extension == 0x3d) {
            require(s.attacker < 4, "Spiral movement requires a character attacker");
            const auto& anchor = context_.anchors[s.target];
            const unsigned x = wrap8(anchor.center_x);
            const unsigned shifted = x + (s.flipped ? unsigned{FF6_ROM_VALUE_EFFECT_SPIRAL_REVERSE_0} : unsigned{FF6_ROM_VALUE_EFFECT_SPIRAL_FORWARD_0});
            s.target_position = {static_cast<std::uint16_t>(s.target < 4 && shifted > 255 ? x
                                                               : wrap8(shifted)),
                // 80/3D uses the published bottom anchor for either kind of
                // target (magic_init_61), not the monster's top-left origin.
                wrap8(unsigned{anchor.bottom_y} - FF6_ROM_VALUE_EFFECT_SPIRAL_Y_0)};
            const auto& actor = context_.characters[s.attacker];
            const auto clamp = [](std::uint16_t value) { return value & 256U ? std::uint8_t{255} : wrap8(value); };
            const auto vector = measure_battle_effect_vector(*library_,
                clamp(add(add(actor.x, actor.offset_x), actor.animation_x)),
                clamp(add(actor.y, actor.offset_y)), wrap8(s.target_position.x), wrap8(s.target_position.y));
            task.vector_angle = wrap8(unsigned{vector.angle} + FF6_ROM_VALUE_EFFECT_SPIRAL_ANGLE_0);
            steering_limit_ = FF6_ROM_VALUE_EFFECT_SPIRAL_INITIAL_0; steering_ticks_ = 0;
        } else if (extension == 0x02) {
            require(s.attacker < 4, "Saving facing requires a character attacker");
            const auto& actor = context_.characters[s.attacker];
            context_.saved_attacker_direction = std::pair{actor.facing, actor.appearance.flip_x};
        } else if (extension == 0x03) {
            require(s.attacker < 4 && context_.saved_attacker_direction.has_value(),
                    "Restoring facing requires its retained action owner");
            auto& actor = context_.characters[s.attacker];
            actor.facing = context_.saved_attacker_direction->first;
            actor.appearance.flip_x = context_.saved_attacker_direction->second;
        } else if (extension == 0x86 || extension == 0x8c)
            sound(read(), extension == 0x8c ? 128 : s.attacker_position.x);
        else if (extension == 0x8b)
            pending_system_sound_ = true; // UpdateSfx's separate command 2C latch.
        else if (extension == 0x87) {
            // C1/C9F7 admits these cues only for callers retaining the
            // Dance/Sketch gate; ordinary attacks still consume the operand.
            const auto identity = read();
            if (context_.vertical_sound_pan) sound(identity, s.attacker_position.y);
        } else if (extension == 0x88) {
            require(s.target < 4, "Block position requires a character target");
            const auto& pose = context_.characters[s.target];
            s.position = {add(pose.x, int{pose.offset_x} + pose.animation_x + 8),
                          add(pose.y, pose.offset_y)};
        } else if (extension == 0x1c) {
            if (context_.display.brightness)
                --context_.display.brightness;
        } else if (extension == 0x3a)
            gradient_.intensity = read();
        else if (extension == 0x37) {
            begin_gradient(extension, 0);
        } else if (extension == 0x38)
            overlay_high_priority_ = true;
        else if (extension == 0x33 || extension == 0x39 || extension == 0x4b || extension == 0x5f) {
            begin_gradient(extension, read());
        } else if (extension == 0x34) {
            for (unsigned i = 0; i < 3; ++i)
                context_.character_colors[i + 1] = context_.monster_colors[i];
        } else if (extension == 0x71) {
            for (unsigned i = 1; i < 4; ++i)
                context_.character_colors[i] = base_character_colors_[i];
        } else if (extension == 0x35 || extension == 0x36) {
            monster_palette_shift_ =
                wrap8(unsigned{monster_palette_shift_} + (extension == 0x35 ? unsigned{FF6_ROM_VALUE_EFFECT_MONSTER_PALETTE_SHIFT_0} / 2U : 256U - FF6_ROM_VALUE_EFFECT_MONSTER_PALETTE_SHIFT_0 / 2U));
        } else if ((extension >= 0x2a && extension <= 0x2c) || extension == 0x49) {
            // RandSpriteAnimPal reloads working and base colors on every call.
            // It shares LoadSpriteAnimPal with the explicit palette command.
            require(extension != 0x49 || library_->random_sprite_palettes.has_value(),
                    "Random sprite palette change requires its selection table");
            const auto palette =
                extension == 0x49 ? (*library_->random_sprite_palettes)[random() & 7U] : read();
            const unsigned layer = extension == 0x49 ? 0 : extension - 0x2a;
            require(images_[layer].has_value(), "Palette replacement needs an owned effect image");
            const auto& colors = battle_effect_palette(*library_, palette);
            auto& image = *images_[layer];
            // Load*AnimPal changes both working and retained base colors, but
            // keeps the relative fade/cycle counters. BG3 loads four colors.
            for (unsigned i = 0; i < (layer == 2 ? 4U : 16U); ++i)
                image.colors[i] = image.base_colors[i] = colors[i & 7U];
            if (layer == 1) secondary_colors_ = secondary_base_colors_ = image.colors;
        } else if (extension == 0x45 || extension == 0x57) {
            const auto value = read();
            const bool foreground = extension == 0x45;
            require(value == (foreground ? 0x33 : 0xc3) || value == (foreground ? 0x3c : 0xcc),
                    "Unsupported animation mask selection");
            masked_backgrounds_[foreground ? 0 : 1] = (value & 15U) == 12;
        } else if (extension == 0x46 || extension == 0x47) {
            const auto selection = read();
            const bool layout = extension == 0x46;
            require((layout && selection == 0x28) || selection == (layout ? 0x0c : 0x10) || selection == (layout ? 0x61 : 0x11),
                    "Unowned foreground image selection");
            // These two encoded commands select the original battlefield
            // layout/images or the ordinary effect surface. They do not give
            // production code a video-memory address space.
            if (layout) sketch_layout_ = selection == 0x28;
            (layout ? battlefield_layout_ : battlefield_pixels_) =
                selection == (layout ? 0x61 : 0x11);
        } else if (extension == 0x58)
            select_mask_shape(read());
        else if (extension == 0x59)
            open_mask(read());
        else if (extension == 0x3e) {
            const auto layers = read();
            require((layers & ~0x1fU) == 0, "Unowned animation visible layer");
            // BG4 is inert in the game's ordinary three-layer battle mode.
            visible_layers_ = layers & 0x17U;
        } else if (extension == 0x40) {
            const auto mode = read();
            require(affine_.has_value() && (mode == 1 || mode == 7),
                    "Animation projection needs an owned affine image and supported mode");
            affine_->enabled = mode == 7;
            raster_event(BattleEffectRasterOperation::affine);
        } else if (extension == 0x41) {
            require(affine_.has_value(), "Affine movement needs an owned summon image");
            const auto dx = read(), dy = read(), cx = read(), cy = read();
            const auto delta = [](std::uint16_t value, std::uint8_t step) {
                return wrap16(unsigned{value} +
                              (step < 128 ? unsigned{step} : unsigned{step} + 65280U));
            };
            affine_->scale_x = delta(affine_->scale_x, dx);
            affine_->scale_y = delta(affine_->scale_y, dy);
            affine_->center_x = delta(affine_->center_x, cx);
            affine_->center_y = delta(affine_->center_y, cy);
            raster_event(BattleEffectRasterOperation::affine);
        } else if (extension == 0x5e) {
            require(affine_ && affine_radius_, "Rotating effect needs its authored image");
            const auto phase = wrap8(0U - unsigned{context_.presentation_tick} * 2U);
            const auto diagonal = scale_battle_effect_sine(*library_, phase, *affine_radius_);
            const auto cross = scale_battle_effect_sine(*library_, wrap8(unsigned{phase} + 64U),
                                                       *affine_radius_);
            affine_->scale_x = affine_->scale_y = add(0, diagonal);
            affine_->cross_x = add(0, cross);
            affine_->cross_y = add(0, -cross);
            *affine_radius_ = wrap8(unsigned{*affine_radius_} - FF6_ROM_VALUE_EFFECT_AFFINE_SHRINK_0);
            raster_event(BattleEffectRasterOperation::affine);
        } else if (extension == 0x55) {
            update_unfolding_scroll(read());
        } else if (extension == 0x42) {
            const auto flags = read();
            require(affine_.has_value() && flags < 4, "Unowned affine flip or overflow policy");
            affine_->flip_x = (flags & 1U) != 0;
            affine_->flip_y = (flags & 2U) != 0;
            raster_event(BattleEffectRasterOperation::affine);
        } else if (extension == 0x6a || extension == 0x6b) {
            // Bottom/center alignment moves by half the two retained heights.
            // Use the task's original geometry, including for blank frames.
            const int delta = int{task.target_height} * 4 - int{task.frame_height} * 8;
            s.position.y = add(s.position.y, extension == 0x6a ? delta : -delta);
        } else if (extension == 0x82) {
            const auto value = read();
            const int delta = value < 128 ? int{value} : int{value} - 256;
            for (auto& monster : context_.monsters) monster.x = add(monster.x, delta);
        } else if (extension == 0x80) {
            const auto parameter = read(), count = read(), offset = read();
            require(count == 16 && offset == 0,
                    "Partial boss palette range needs a separate color owner");
            // Relative per-channel subtraction shares the retained monster
            // fade offsets and the animation-start palette snapshot.
            palette_command(0xbc, parameter);
        } else if (extension == 0x7c) {
            // C2/B281: Engulf transfers this task's movement ownership to
            // the swallowed character. Other image tasks, hit flags and
            // facing retain their original owners.
            std::swap(s.attacker, s.target);
            std::swap(s.attacker_position, s.target_position);
        } else if (extension == 0x7d) {
            // C1/B03D branches over the ordinary return for another landing.
            // The dispatcher advances past the operand after adding it. A
            // one-byte-short branch lands on END instead of the next JUMP,
            // skipping Dragon Horn's airborne return and retaining travel.
            const auto distance = read();
            if (context_.jump_again) jump(s.instruction + distance);
        } else if (extension == 0x7f) {
            context_.control.monster_visible.fill(false);
        } else if (extension == 0x6f) {
            // Re-select the current window bounds after the entry caller's
            // fade. Preserve the retained edges, rather than rebuilding them.
            mask_publication_rows_ = mask_.rows;
            mask_pending_ = true;
            mask_.published = true;
        } else if (extension == 0x53 || extension == 0x54) {
            // C1/CDDF and C1/CDC4 act only on character targets. The script
            // owns six movement steps, followed by the visible row toggle.
            if (s.target < 4) {
                require(context_.character_back_rows.has_value(), "Character row presentation is unowned");
                auto& back = (*context_.character_back_rows)[s.target];
                auto& pose = context_.characters[s.target];
                if (extension == 0x54) {
                    back = !back;
                    pose.action = 0;
                } else {
                    const int distance = context_.character_row_distances[s.target];
                    require(distance % static_cast<int>(FF6_ROM_VALUE_BATTLE_ROW_MOVEMENT_PARAMETERS_0) == 0, "Invalid character row distance");
                    pose.desired_angle = add(pose.desired_angle, (back ? -distance : distance) / static_cast<int>(FF6_ROM_VALUE_BATTLE_ROW_MOVEMENT_PARAMETERS_0));
                    pose.action = 4;
                }
            }
        } else if (extension == 0x51) {
            // C1/CE62: disabling clears both kinds. Enabling writes attacker
            // then target; two actors of one kind deliberately retain only the
            // latter, while a character/monster pair retains both.
            auto& control = context_.control;
            if (read()) {
                control.reflected_characters = control.reflected_monsters = 0;
            } else {
                for (const auto actor : {s.attacker, s.target}) {
                    require(actor < 10, "Vertical image needs a valid actor");
                    if (actor < 4) control.reflected_characters = static_cast<std::uint8_t>(1U << actor);
                    else control.reflected_monsters = static_cast<std::uint8_t>(1U << (actor - 4U));
                }
            }
        } else if (extension == 0x1b) {
            // C1/CB5D: this effect-wide flag survives until the next attack.
            detached_character_vector_ = true;
        } else if (extension == 0x1d || extension == 0x52) {
            // C1/CB61 and CE29 share CE31's vector offset. Magicite uses
            // radius 32 at one quarter of the smaller orbit's angular speed.
            // Phase follows the presentation counter, not each copy's age.
            const std::uint8_t radius = extension == 0x1d ? FF6_ROM_VALUE_EFFECT_LARGE_ORBIT_RADIUS_0 : FF6_ROM_VALUE_EFFECT_SMALL_ORBIT_RADIUS_0;
            const unsigned speed = extension == 0x1d ? 8U : 32U;
            const auto phase = wrap8(unsigned{context_.presentation_tick} * speed);
            s.offset.x = add(s.offset.x, scale_battle_effect_sine8(*library_, phase, radius));
            s.offset.y = add(s.offset.y, scale_battle_effect_sine8(
                *library_, wrap8(unsigned{phase} + 64U), radius));
        } else if (extension == 0x89) {
            // C1/C9C1 shares the terrain-shake owner with event C7/14. The
            // operand also retains horizontal-only ($80) versus both axes.
            context_.display.screen_shake = read();
        } else if (extension == 0x50 || extension == 0x56) {
            if (extension == 0x50)
                read(); // C1/CE97 reads the command byte instead of this direction operand.
            // Preserve the visible authored motion: overlapping RNG-table words
            // select two scroll speeds. This consumes no battle random draw.
            const unsigned tick = context_.presentation_tick;
            // The foreground variant (C1/CD28) correctly negates both speeds
            // when facing left. The overlay's original EOR-with-zero typo
            // instead adds one. Share the sampling, preserving that quirk.
            auto& scroll = scrolling_[extension == 0x50 ? 2 : 0];
            for (unsigned row = 0; row < scroll.projection.size(); ++row) {
                const unsigned sample =
                    unsigned{library_->random[row]} | (unsigned{library_->random[row + 1]} << 8U);
                const unsigned speed = tick * ((sample & 0x8000U) ? 8U : 2U);
                const unsigned displacement = s.flipped ? speed :
                    extension == 0x50 ? speed + 1U : 0U - speed;
                scroll.projection[row].x = wrap16(sample - displacement);
            }
            scroll.projection_owned = true;
        } else if (extension == 0x70) {
            // The horizontal wave retains its samples while the vertical
            // projection copies the last frame-selected overlay position.
            // Moving the task does not update that owner until a frame command;
            // sampling it here would advance entry/exit scrolling by one step.
            require(task.layer == BattleEffectLayer::overlay,
                    "Overlay alignment needs an overlay task");
            const auto origin = background_origin(2);
            for (unsigned row = 0; row < 32; ++row)
                scrolling_[2].projection[row].y = origin.y;
            scrolling_[2].projection_owned = true;
        } else if (extension == 0x1a) {
            require(s.target >= 4 && s.target < 10,
                    "Monster entry visibility needs a monster target");
            context_.control.monster_entry_visible[s.target - 4] = (read() & 1U) != 0;
        } else if (extension == 0x6c) {
            // Clear the staged frame set, not already-published quadrants.
            // Later frame publications select their destination independently.
            foreground_frames_cleared_ = true;
        } else if (extension == 0x64) {
            // Clearing the sprite tile offset retains its frame layout.
            // Crusader supplies an extra image in this region; ordinary
            // character echoes use the live published character tiles.
            require(supplemental_image_ || context_.published_character_frames,
                    "Small sprite needs published character tiles or a supplemental image");
            task.supplemental_image = supplemental_image_.has_value();
            task.character_image = !task.supplemental_image;
        } else if (extension == 0x67) {
            require(loaded_summon_.has_value() &&
                        library_->summon_following_colors[*loaded_summon_].has_value(),
                    "Extra summon palette is unowned");
            context_.character_colors[3] = base_character_colors_[3] =
                *library_->summon_following_colors[*loaded_summon_];
        } else if (extension == 0x68) {
            if (presentation_timing_) {
                resource_waits_ = 2; // 64 tiles, 2048 bytes, two publication waits.
                supplemental_pending_ = true;
            } else
                finish_supplemental_load();
        } else if (extension == 0x60) {
            // C1/CC3F: Morph/Revert switch the displayed sheet at the authored
            // midpoint. Combat already owns the logical status; this event
            // changes presentation only and uses the usual status-image loader.
            std::uint32_t mask = 0;
            for (unsigned byte = 0; byte < 4; ++byte)
                mask |= std::uint32_t{read()} << (byte * 8U);
            if (s.attacker < 4)
                appearance_changes_.push_back(
                    {BattleActorAppearanceOperation::toggle_status, s.attacker, 255, mask});
        } else if (extension == 0x13 || extension == 0x14) {
            require(s.target < 10, "Invalid appearance-change target");
            // Vanish's animation changes only characters. Monster visibility
            // is handled by combat outcomes, not this data-language command.
            if (extension == 0x13 || s.target < 4)
                appearance_changes_.push_back(
                    {extension == 0x13 ? BattleActorAppearanceOperation::toggle_imp
                                       : BattleActorAppearanceOperation::vanish,
                     s.target});
            if (extension == 0x13 && s.target >= 4 && presentation_timing_)
                resource_waits_ = 8;
        } else if (extension == 0x7e) {
            // Suplex toggles the target character's vertical orientation;
            // C1/CAA1 deliberately leaves monster targets unchanged.
            require(s.target < 10, "Invalid vertical-flip target");
            if (s.target < 4) {
                auto& style = context_.characters[s.target].appearance;
                style.flip_y = !style.flip_y;
            }
        } else if (extension == 0x15) {
            mask_.x = wrap8(unsigned{s.position.x} + s.offset.x);
            mask_.y = wrap8(unsigned{s.position.y} + s.offset.y);
        } else if (extension == 0x4c) {
            triangle_.x = wrap8(unsigned{s.position.x} + s.offset.x);
            triangle_.y = wrap8(unsigned{s.position.y} + s.offset.y);
        } else if (extension == 0x4d) {
            // Linked C1/CF19 seeds the projectile vector from the triangle's
            // byte coordinates. Unlike $95, the target truncates, not clamps.
            s.attacker_position = {triangle_.x, triangle_.y};
            set_vector(task, triangle_.x, triangle_.y,
                       wrap8(s.target_position.x), wrap8(s.target_position.y));
        } else if (extension == 0x8a) {
            if (s.target >= 4)
                context_.monster_styles[s.target - 4].priority = 0;
        } else if (extension == 0x26)
            context_.freeze_character_palettes = read() != 0;
        else if (extension == 0x27)
            context_.hide_characters = read() != 0;
        else if (extension == 0x28) {
            const auto priority = static_cast<std::uint8_t>((read() >> 4U) & 3U);
            for (auto& character : context_.characters)
                character.appearance.priority = priority;
        } else if (extension == 0x29)
            context_.hide_character_indicator = read() != 0;
        else if (extension == 0x10)
            s.position = s.target_position;
        else if (extension == 0x4a)
            context_.needs_step_back.fill(false);
        else if (extension == 0x3f) {
            require(tasks_[80].has_value(), "Boarding wait needs a foreground task");
            const auto x = tasks_[80]->script.position.x;
            const unsigned inset = (context_.battle_type & 3U) == 2 ? 0 : FF6_ROM_VALUE_EFFECT_BOARDING_INSET_0;
            const auto bucket = wrap8(unsigned{x} - inset) >> 3U;
            const auto origin = wrap8(s.attacker_position.x);
            if ((x & 256U) || (bucket != (origin >> 3U) && bucket != (wrap8(0U - origin) >> 3U))) {
                // Revisit the preceding authored frame, yielding one task
                // tick. Do not spin in the command interpreter while waiting.
                require(s.instruction >= 3, "Boarding wait branch underflow");
                jump(s.instruction - 3);
            }
        } else if (extension == 0x44) {
            require(id > 0 && tasks_[id - 1].has_value(), "Previous animation task is unavailable");
            const auto& previous = tasks_[id - 1]->script;
            s.attacker_position = previous.attacker_position;
            s.target_position = previous.target_position;
            s.flipped = previous.flipped;
            s.position = previous.position;
            s.offset = previous.offset;
        } else if (extension == 0x5b) {
            // C2/BC99 indexes every fourth byte of the fixed random table;
            // neither the animation random cursor nor combat RNG advances.
            for (unsigned row = 0; row < 32; ++row) {
                disintegration_delays_[row] = library_->random[row * 4] & FF6_ROM_VALUE_EFFECT_DISINTEGRATION_DELAY_0;
                disintegration_displacements_[row] = 0;
            }
        } else if (extension == 0x5a) {
            // C2/BCB7: AE has freshly written the wave. Each expired row adds
            // its growing negative displacement to that wave, not last frame.
            for (unsigned row = 0; row < 32; ++row) {
                if (disintegration_delays_[row])
                    --disintegration_delays_[row];
                else {
                    auto& displacement = disintegration_displacements_[row];
                    displacement = wrap16(unsigned{displacement} - FF6_ROM_VALUE_EFFECT_DISINTEGRATION_STEP_0);
                    auto& offset = scrolling_[0].axes[0].offsets[row];
                    offset = wrap16(unsigned{offset} + displacement);
                }
            }
        } else if (extension == 0x5c) {
            disintegration_delays_.fill(0);
            disintegration_displacements_.fill(0);
            for (auto& axis : scrolling_[0].axes)
                std::fill_n(axis.offsets.begin(), 32, 0);
        } else if (extension == 0x65) {
            if ((context_.presentation_tick & FF6_ROM_VALUE_EFFECT_RAINBOW_CADENCE_0) == 0) {
                require(library_->rainbow_palettes.has_value(),
                        "Rainbow palette change requires its selection table");
                const auto identity = (*library_->rainbow_palettes)[random() & 7U];
                const auto& colors = battle_effect_palette(*library_, identity);
                auto& working = images_[0] ? images_[0]->colors : imageless_colors_[0];
                // Every eligible command consumes its own RNG value, even if
                // another particle ran it this tick. Fade bases stay unchanged.
                for (unsigned color = 0; color < working.size(); ++color)
                    working[color] = colors[color & 7U];
            }
        } else if (extension == 0x66)
            s.offset = {};
        else if (extension == 0x4f)
            // C1/CEF0 owns a live word-sized origin, not a cached task anchor.
            s.position = live_character_origin(context_, s.attacker);
        else if (extension == 0x4e)
            task.frame_offset = 0;
        else if (extension == 0x11) {
            require(!task.character_line, "Character-line to polar motion remains unowned");
            task.motion_phase = random();
            task.radius &= 0xff00U;
        } else if (extension == 0x12) {
            require(!task.character_line, "Character-line to polar motion remains unowned");
            task.motion_phase = 0;
            task.radius &= 0xff00U;
        } else if (extension == 0x05) {
            require(s.attacker < 4 && s.target < 10, "Character orbit needs valid participants");
            const auto& actor = context_.characters[s.attacker];
            s.offset = {actor.offset_x, actor.offset_y};
            const auto width = s.target < 4 ? 2U : unsigned{context_.monster_widths[s.target - 4]};
            // C1/D938 packs the orbit into temporary coordinates. Keep its
            // phase/radius semantic and preserve the preceding jump's offsets.
            task.character_orbit = BattleCharacterOrbit{
                static_cast<std::uint8_t>((actor.facing & 1U) ? 0 : 128),
                FF6_ROM_VALUE_BATTLE_CHARACTER_ORBIT_RADIUS_0,
                wrap8(width * 4U + FF6_ROM_VALUE_BATTLE_CHARACTER_ORBIT_PADDING_0)};
        } else if (extension == 0x04) {
            // C1/D96E draws two independent byte phases. The second write
            // preserves the high byte shared with word-sized vector motion.
            task.motion_phase = random();
            task.radius = static_cast<std::uint16_t>((task.radius & 0xff00U) | random());
        } else if (extension == 0x06)
            reset_tornado();
        else if (extension == 0x07) {
            require(tornado_.initialized, "Tornado origin requires initialization");
            tornado_.origin = s.offset;
        } else if (extension == 0x09)
            prioritize_tornado(task);
        else if (extension == 0x08)
            move_vector(task);
        else if (extension == 0x0a) {
            require(!task.character_line, "Pre-cast ellipse needs polar task state");
            // CalcVecSine uses the eight-bit sine table and a fixed radius 48.
            // The low radius byte here is the initializer's vertical phase mask,
            // not a growing radius as in the summon orbit command.
            const auto phase = task.motion_phase;
            s.offset.y =
                add(0, scale_battle_effect_sine8(
                           *library_, static_cast<std::uint8_t>(phase ^ wrap8(task.radius)), FF6_ROM_VALUE_EFFECT_ELLIPSE_RADIUS_0));
            s.offset.x =
                add(0, scale_battle_effect_sine8(*library_, wrap8(unsigned{phase} + FF6_ROM_VALUE_EFFECT_ELLIPSE_PHASE_0), FF6_ROM_VALUE_EFFECT_ELLIPSE_RADIUS_0));
            task.motion_phase = wrap8(unsigned{phase} - FF6_ROM_VALUE_EFFECT_PRECAST_TURN_0);
        } else if (extension == 0x0b) {
            require(!task.character_line, "Pre-cast orbit needs polar task state");
            const auto radius = wrap8(task.radius);
            // Like the casting ellipse, this command calls CalcVecSine.
            // Its eight-bit coefficients have a different scale from the
            // general projectile helper; substituting that helper doubles the orbit.
            s.offset.y = add(0, scale_battle_effect_sine8(*library_, task.motion_phase, radius));
            s.offset.x = add(0, scale_battle_effect_sine8(
                                    *library_, wrap8(unsigned{task.motion_phase} + 64U), radius));
            task.motion_phase = wrap8(unsigned{task.motion_phase} - FF6_ROM_VALUE_EFFECT_PRECAST_TURN_0);
            const auto next = wrap8(unsigned{radius} + FF6_ROM_VALUE_EFFECT_ORBIT_GROWTH_0);
            if (next < FF6_ROM_VALUE_EFFECT_ORBIT_CAP_0)
                task.radius = static_cast<std::uint16_t>((task.radius & 0xff00U) | next);
        } else if (extension == 0x1f || extension == 0x20)
            initialize_curved_overlay(extension == 0x1f);
        else if (extension == 0x1e)
            advance_curved_overlay();
        else if (extension == 0x21) {
            require(!task.character_line, "Character-line phase reuse remains unowned");
            task.behind_actor = (wrap8(unsigned{task.motion_phase} + 64U) & 128U) != 0;
        } else if (extension == 0x0e) {
            // C1/D79D retains the monster's top-left independently of the
            // bottom-center vector, and clears the accumulated displacement.
            if (s.attacker >= 4) {
                const auto& monster = context_.monsters[s.attacker - 4];
                task.monster_line_origin = BattleAnimationPoint{monster.x, monster.y};
                task.monster_line_displacement = {};
            }
        } else if (extension == 0x0f) {
            // C1/D779: eight adjacent directions around left/right, without
            // changing the vector's radius or endpoint.
            task.vector_angle = wrap8((s.flipped ? unsigned{FF6_ROM_VALUE_EFFECT_CONE_RIGHT_0} : unsigned{FF6_ROM_VALUE_EFFECT_CONE_LEFT_0}) + (random() & FF6_ROM_VALUE_EFFECT_CONE_SPREAD_0));
            steering_limit_ = FF6_ROM_VALUE_EFFECT_STEERING_INITIAL_0;
            steering_ticks_ = 0;
        } else if (extension == 0x23) {
            // C1/D4BE decrements all 151 visible foreground displacement rows.
            for (auto& row : scrolling_[0].projection) row.y = wrap16(unsigned{row.y} - 1U);
            scrolling_[0].projection_owned = true;
        } else if (extension == 0x22) {
            require(library_->curved_scroll_repeats.has_value(), "Curved scroll table is missing");
            auto& scroll = scrolling_[0];
            int displacement = background_views_[0].y;
            unsigned row = 0, group = 0;
            // C1/D4F2 uses the first half's final repeat length throughout the
            // second half; X is deliberately not decremented there.
            for (unsigned half = 0; half < 2; ++half)
                while (row < (half == 0 ? FF6_ROM_VALUE_EFFECT_CURVED_ROW_SPLIT_0 : 151U)) {
                    require(group < library_->curved_scroll_repeats->size(),
                            "Curved foreground exhausted its repeat table");
                    unsigned count = (*library_->curved_scroll_repeats)[group];
                    require(count != 0, "Empty curved scroll group");
                    while (count-- && row < (half == 0 ? FF6_ROM_VALUE_EFFECT_CURVED_ROW_SPLIT_0 : 151U))
                        scroll.projection[row++].y = wrap16(static_cast<std::uint32_t>(displacement--));
                    --displacement;
                    if (half == 0 && row < FF6_ROM_VALUE_EFFECT_CURVED_ROW_SPLIT_0) ++group;
                }
            scroll.projection_owned = true;
        } else if (extension == 0x0c) {
            const auto& first = tasks_[id / 8 * 8];
            require(first.has_value(), "First animation task is unavailable");
            s.position = first->script.position;
            s.offset = first->script.offset;
        } else if (extension == 0x2d) {
            const std::array<std::size_t, 3> destinations{word(), word(), word()};
            const unsigned approach = context_.battle_type & 3U;
            const unsigned branch =
                approach == 3 ? (s.attacker < 4 && s.attacker >= 2 ? 1 : 2) : approach;
            jump(destinations[branch]);
        } else if (extension == 0x32) {
            const std::array<std::size_t, 2> destinations{word(), word()};
            jump(destinations[s.flipped ? 1 : 0]);
        } else if (extension == 0x43)
            update_entrance_scroll(read());
        else if (extension == 0x24 || extension == 0x25) {
            auto& scroll = scrolling_[extension == 0x25 ? 0 : 2];
            scroll.projection = {};
            scroll.projection_owned = true;
        } else if (extension == 0x2e) {
            const auto x = read(), y = read();
            s.position = {x, y};
        } else if (extension == 0x16)
            task.bounce_phase = task.wave_phase = 0;
        else if (extension == 0x61 || extension == 0x62 || extension == 0x75) {
            const auto radius = read(), speed = read(), mask = read();
            const auto offset = scale_battle_effect_sine8(
                *library_, static_cast<std::uint8_t>(task.bounce_phase | mask), radius);
            if (extension == 0x75) {
                // Selection precedes the additive bounce. The source comments
                // reverse the first two ranges; the comparisons define them.
                const auto phase = task.bounce_phase | 128U;
                task.frame_offset = phase < FF6_ROM_VALUE_EFFECT_BOUNCE_FIRST_THRESHOLD_0 ? FF6_ROM_VALUE_EFFECT_BOUNCE_FIRST_FRAME_0 : phase < FF6_ROM_VALUE_EFFECT_BOUNCE_SECOND_THRESHOLD_0 ? FF6_ROM_VALUE_EFFECT_BOUNCE_SECOND_FRAME_0 : 0;
                s.offset.y = add(s.offset.y, offset);
            } else
                (extension == 0x61 ? s.offset.y : s.offset.x) = add(0, offset);
            task.bounce_phase = wrap8(unsigned{task.bounce_phase} + speed);
        } else if (extension == 0x31 || extension == 0x63) {
            const auto speed = read();
            s.offset.y = add(0, scale_battle_effect_sine8(*library_, task.wave_phase,
                                                          extension == 0x31 ? FF6_ROM_VALUE_EFFECT_WIDE_WAVE_RADIUS_0 : FF6_ROM_VALUE_EFFECT_NARROW_WAVE_RADIUS_0));
            task.wave_phase = wrap8(unsigned{task.wave_phase} + speed);
        } else if (extension == 0x5d) {
            // The instructions ADD the wave, despite the original comment
            // describing assignment. This command follows vector projection.
            s.offset.y = add(
                s.offset.y,
                scale_battle_effect_sine8(*library_, wrap8(unsigned{task.bounce_phase} * 4U), FF6_ROM_VALUE_EFFECT_ADDITIVE_WAVE_RADIUS_0));
            task.bounce_phase = wrap8(unsigned{task.bounce_phase} + 1U);
        } else if (extension == 0x73) {
            // C1/CB1D: the authored die chooses its settled face, not another
            // random number. Graphics order is third, first, second die.
            const auto die = read();
            require(die < 3, "Invalid dice animation index");
            const auto face = context_.dice[die == 0 ? 2 : die - 1];
            require(face <= 6, "Invalid settled die face");
            task.frame_offset = face ? face + (FF6_ROM_VALUE_EFFECT_DICE_FRAME_BASE_0 - 1U) : FF6_ROM_VALUE_EFFECT_DICE_BLANK_FRAME_0;
        } else if (extension == 0x72) {
            const auto forward = read();
            // Shared background/summon tasks miss only when no target was hit;
            // sprite tasks retain the outcome of their individual participant.
            if (id < 80 ? context_.hits[context_.reflection_targets[id / 8] ? id / 8 : s.target]
                        : context_.hits.any())
                jump(s.instruction + forward);
        } else if (extension == 0x17 || extension == 0x69)
            prioritize_from(extension == 0x17 ? s.target : s.attacker);
        else if (extension == 0x3b || extension == 0x3c) {
            require(images_[0].has_value(), "Target flash needs an owned effect palette");
            recolored_targets_[s.target] = extension == 0x3b;
        } else if (extension == 0x6d)
            context_.menus_disabled = true;
        else
            throw std::invalid_argument("Unimplemented attack animation extension " +
                                        std::to_string(extension));
        break;
    }
    case 0x81:
    case 0x82: {
        const auto left = read(), right = read();
        const auto slot = op == 0x81 ? s.attacker : s.target;
        // These commands explicitly return for monsters after consuming both operands.
        if (slot < 4) {
            context_.characters[slot].frame_override = flipped(context_, slot) ? right : left;
            context_.control.force_character_frame = slot;
        }
        break;
    }
    case 0x83:
    case 0x86:
    case 0x87: {
        const auto [dx, dy] = displacement(read(), s.flipped);
        if (op == 0x83) {
            s.position.x = add(s.position.x, dx);
            s.position.y = add(s.position.y, dy);
        } else {
            const auto slot = op == 0x86 ? s.attacker : s.target;
            if (slot < 4) {
                auto& character = context_.characters[slot];
                character.animation_x = add(character.animation_x, dx);
                character.offset_y = add(character.offset_y, dy);
            } else {
                auto& monster = context_.monsters[slot - 4];
                monster.x = add(monster.x, dx);
                monster.y = add(monster.y, dy);
            }
            auto& p = op == 0x86 ? s.attacker_position : s.target_position;
            p.x = add(p.x, dx);
            p.y = add(p.y, dx > 0 && dy < 0 ? -dy : dy);
        }
        break;
    }
    case 0x88: {
        const auto frame = read();
        if (s.attacker >= 4 || !context_.needs_step_back[s.attacker] || context_.suppress_fight_step)
            break;
        require(context_.magitek || (library_->fight_step_y && frame < 8),
                "Fight step needs its motion table and valid frame");
        const int dx = (s.flipped ? 1 : -1) * (context_.magitek ? FF6_ROM_VALUE_EFFECT_ARMORED_STEP_DISTANCE_0 : FF6_ROM_VALUE_EFFECT_ORDINARY_STEP_DISTANCE_0);
        const int dy = context_.magitek ? 0 : (*library_->fight_step_y)[frame];
        // The main task moves both the actor and weapon; companion weapon
        // threads move only themselves. The jump returns to the same baseline.
        if (id % 8 == 0) {
            auto& pose = context_.characters[s.attacker];
            pose.animation_x = add(pose.animation_x, dx);
            pose.offset_y = add(pose.offset_y, dy);
            s.attacker_position.x = add(s.attacker_position.x, dx);
            s.attacker_position.y = add(s.attacker_position.y, dy);
        }
        s.position.x = add(s.position.x, dx);
        s.position.y = add(s.position.y, dy);
        break;
    }
    case 0x8d:
    case 0x8f: {
        const auto direction = read();
        // C1/F263 compares character facing with the selected hand's direction.
        if (s.attacker < 4 && flipped(context_, s.attacker) == context_.weapon_left_hand) {
            const auto [dx, dy] = displacement(direction, s.flipped);
            s.position.x = add(s.position.x, dx);
            s.position.y = add(s.position.y, dy);
        }
        break;
    }
    case 0x8e: {
        const auto flags = read();
        if (s.attacker < 4)
            task.behind_actor = (flags & 128U) ? true : (flags & 64U) ? false
                : (flipped(context_, s.attacker) != context_.weapon_left_hand) != ((flags & 1U) != 0);
        break;
    }
    case 0x84:
        s.rate = read();
        break;
    case 0x85:
        s.position = s.attacker_position;
        break;
    case 0x91: {
        const auto& weapon = tasks_[s.attacker * 8U];
        require(weapon.has_value(), "Weapon hit has no attacker task");
        // The projectile starts from the weapon's adjusted launch point,
        // including the caster's hop, not the hit task's original anchor.
        s.position = s.attacker_position = weapon->script.attacker_position;
        break;
    }
    case 0x9f: {
        read(); // Reserved operand, consumed like the ordinary loop count.
        // InitAnimThreads / C1/B277,F7E2: InitThread contributes eight per sprite launch,
        // including repeated copies. Completion does not decrease the count.
        s.loop_count = thread_launch_spacing_;
        s.loop_body = s.instruction;
        task.frame_offset = 0;
        break;
    }
    case 0x89:
    case 0x8b:
        s.loop_count = read();
        s.loop_body = s.instruction;
        if (op == 0x8b)
            task.frame_offset = 0;
        break;
    case 0x8a:
    case 0x8c:
        if (s.loop_count) {
            require(s.loop_body.has_value(), "Animation loop has no start");
            --s.loop_count;
            jump(*s.loop_body);
            if (op == 0x8c)
                task.frame_offset = wrap8(unsigned{task.frame_offset} + 1);
        } else if (op == 0x8c)
            task.frame_offset = 0;
        break;
    case 0x90:
        task.priority = static_cast<std::uint8_t>((read() >> 4U) & 3U);
        break;
    case 0xee: {
        const auto priority = static_cast<std::uint8_t>((read() >> 4U) & 3U);
        if (s.target < 4)
            context_.characters[s.target].appearance.priority = priority;
        else
            context_.monster_styles[s.target - 4].priority = priority;
        break;
    }
    case 0xf0:
    case 0xf3: {
        std::array<std::size_t, 5> destinations;
        for (auto& destination : destinations)
            destination = word();
        const unsigned slot = op == 0xf0 ? s.target : s.attacker;
        jump(absolute(destinations[std::min(slot, 4U)]));
        break;
    }
    case 0x95: {
        // InitAttackerVec tests bit eight, not the whole high byte. These
        // coordinates can have wrapped during earlier character movement.
        const auto coordinate = [](std::uint16_t value) {
            return (value & 256U) ? std::uint8_t{255} : wrap8(value);
        };
        set_vector(task, coordinate(s.attacker_position.x), coordinate(s.attacker_position.y),
                   coordinate(s.target_position.x), coordinate(s.target_position.y));
        break;
    }
    case 0xf2:
        set_vector(task, wrap8(s.target_position.x), wrap8(s.target_position.y),
                   wrap8(s.attacker_position.x), wrap8(s.attacker_position.y));
        break;
    case 0x94: {
        const auto width = s.target < 4 ? 2 : context_.monster_widths[s.target - 4];
        const auto height = s.target < 4 ? 3 : context_.monster_heights[s.target - 4];
        const auto scatter = [&](std::uint16_t center, unsigned dimension) {
            const auto half = wrap8((dimension / 2U) * 4U);
            const auto offset =
                wrap8(((unsigned{random()} * wrap8(unsigned{half} * 2U)) >> 8U) - half);
            const int displacement = offset < 128 ? int{offset} : int{offset} - 256;
            const int point = int{wrap8(center)} + displacement;
            // Preserve the original unsigned -8..247 window and its asymmetric
            // underflow clamp, rather than clamping coordinates to 0..255.
            return static_cast<std::uint8_t>(
                point < FF6_ROM_VALUE_EFFECT_SCATTER_EDGE_0 - 256 || point >= FF6_ROM_VALUE_EFFECT_SCATTER_EDGE_0 ? FF6_ROM_VALUE_EFFECT_SCATTER_EDGE_0 : wrap8(static_cast<std::uint32_t>(point)));
        };
        const auto x = scatter(s.target_position.x, static_cast<unsigned>(width));
        const auto y = scatter(s.target_position.y, static_cast<unsigned>(height));
        const auto retained_dx = task.vector_dx; // This initializer leaves dx untouched.
        set_vector(task, wrap8(s.attacker_position.x), wrap8(s.attacker_position.y), x, y);
        task.vector_dx = retained_dx;
        break;
    }
    case 0x93:
        task.radius = read();
        move_vector(task);
        break;
    case 0xed: {
        const auto speed = read(), backward = read();
        const auto clamp = [](std::uint16_t coordinate) {
            return coordinate & 256U ? std::uint8_t{255} : wrap8(coordinate);
        };
        const auto vector = measure_battle_effect_vector(*library_, clamp(s.position.x),
            clamp(s.position.y), wrap8(s.target_position.x), wrap8(s.target_position.y));
        const auto difference = wrap8(unsigned{vector.angle} - task.vector_angle);
        const bool negative = difference >= 128;
        const auto magnitude = negative ? wrap8(0U - difference) : difference;
        const auto turn = std::min(magnitude, steering_limit_);
        task.vector_angle = wrap8(unsigned{task.vector_angle} +
                                 (negative ? 256U - turn : turn));
        if (steering_limit_ < FF6_ROM_VALUE_EFFECT_STEERING_LIMIT_0 && !(++steering_ticks_ & FF6_ROM_VALUE_EFFECT_HOMING_CADENCE_0)) ++steering_limit_;
        if (vector.length < wrap8(unsigned{speed} + FF6_ROM_VALUE_EFFECT_ARRIVAL_PADDING_0)) break;
        s.position.x = add(s.position.x, scale_battle_effect_sine(*library_,
            wrap8(unsigned{task.vector_angle} + 64U), speed));
        s.position.y = add(s.position.y, scale_battle_effect_sine(*library_, task.vector_angle, speed));
        require(backward <= s.instruction - 1, "Homing vector branch underflow");
        jump(s.instruction - 1 - backward);
        break;
    }
    case 0x92:
    case 0xc1: {
        task.motion_phase = read();
        const auto backward = read();
        move_vector(task);
        task.radius = wrap16(unsigned{task.radius} + task.motion_phase);
        bool repeat = task.radius < task.vector_length;
        if (op == 0xc1 && task.radius >= FF6_ROM_VALUE_EFFECT_ORBIT_EXTENT_0)
            repeat = false;
        else if (op == 0xc1 && !repeat) {
            task.vector_length = FF6_ROM_VALUE_EFFECT_ORBIT_EXTENT_0;
            task.radius = 0;
            s.position = {add(s.position.x, s.offset.x), add(s.position.y, s.offset.y)};
            s.offset = {};
            task.vector_angle = wrap8(unsigned{task.vector_angle} + FF6_ROM_VALUE_EFFECT_ORBIT_TURN_0);
        }
        if (repeat && (op == 0xc1 || backward != 0)) {
            require(backward <= s.instruction - 1, "Animation vector branch underflow");
            jump(s.instruction - 1 - backward);
        }
        break;
    }
    case 0x97: {
        const auto width = s.target < 4 ? 2 : context_.monster_widths[s.target - 4];
        const auto extent = wrap8(static_cast<unsigned>(width) * 4U);
        const auto x0 = wrap8(s.attacker_position.x);
        unsigned x1 = wrap8(s.target_position.x);
        if (x1 >= x0) {
            x1 += extent;
            if (x1 > 255)
                x1 = FF6_ROM_VALUE_EFFECT_BOOMERANG_EDGE_0;
        } else
            x1 = wrap8(x1 - extent);
        set_vector(task, x0, wrap8(s.attacker_position.y), wrap8(x1), wrap8(s.target_position.y));
        task.motion_phase = read();
        move_boomerang(task);
        break;
    }
    case 0x96: {
        read(); // Original consumes this byte but derives speed from horizontal distance.
        const auto backward = read();
        move_boomerang(task);
        task.motion_phase = wrap8(unsigned{task.motion_phase} + FF6_ROM_VALUE_EFFECT_BOOMERANG_RATE_0 - (task.vector_dx >> 7U));
        if (task.motion_phase < 128) {
            if (task.motion_phase >= 64)
                task.behind_actor = true;
            require(backward <= s.instruction - 1, "Animation boomerang branch underflow");
            jump(s.instruction - 1 - backward);
        }
        break;
    }
    case 0x98: {
        const auto rate = read(), range = read();
        task.frame_counter = wrap8(unsigned{task.frame_counter} - 1U);
        if (!task.frame_counter) {
            task.frame_counter = rate;
            task.frame_offset = wrap8(unsigned{task.frame_offset} + 1U);
            if (task.frame_offset == (range & 15U))
                task.frame_offset = range >> 4U;
        }
        break;
    }
    case 0x99: {
        const auto selection = read();
        require((selection & 0xf1U) == 0 && selection >= 6,
                "Effect sprite palette has no supported color owner");
        task.palette = selection >> 1U;
        break;
    }
    case 0x9c: {
        const auto direction = read();
        if (s.target < 4 && !flipped(context_, s.target)) {
            const auto [dx, dy] = displacement(direction, s.flipped);
            s.position.x = add(s.position.x, dx);
            s.position.y = add(s.position.y, dy);
        }
        break;
    }
    case 0x9e: {
        const auto left = read(), right = read();
        if (s.target < 4)
            s.flipped = s.flipped != (((flipped(context_, s.target) ? right : left) & 64U) != 0);
        break;
    }
    case 0x9b: {
        const auto flags = read();
        if (s.target < 4)
            task.behind_actor = (flags & 128U) ? true
                                : (flags & 64U)
                                    ? false
                                    : flipped(context_, s.target) != ((flags & 1U) != 0);
        break;
    }
    case 0xf4:
        task.behind_actor = (read() & 1U) != 0;
        break;
    case 0x9a:
        s.flipped = flipped(context_, s.target);
        break;
    case 0xa2:
        s.flipped = s.flipped != ((read() & 64U) != 0);
        break;
    case 0xa0:
    case 0xa1: {
        const auto speed = read(), backwards = read();
        const bool finished = move_character_jump(task, speed, op == 0xa1);
        if (!finished) {
            require(backwards <= s.instruction - 1, "Character jump branch underflow");
            jump(s.instruction - 1 - backwards);
        }
        break;
    }
    case 0xa3:
    case 0xa4: {
        const auto range = read(), selection = read();
        const unsigned palette = selection >> 4U;
        const bool monster = palette >= 8 && palette <= 10;
        const unsigned layer = palette == 11 ? 0 : palette == 3 ? 1 : palette == 1 ? 2 : 3;
        require(monster || palette == 4 || layer < 3,
                "Animation palette cycle needs an unimplemented palette owner");
        const unsigned start = range >> 4U, count = range & 15U;
        require(count != 0 && start + count <= 16, "Invalid animation palette cycle range");
        auto& counter = palette_counters_[monster ? palette - 8 + 3 : palette == 4 ? 6 : layer];
        if (counter == 0) {
            // USA C1/F165..F269 rotates both working and base colors, indexed by
            // palette bank rather than target or visible image. Monster banks
            // supply e.g. the accelerating gears during Atma's charge.
            auto& colors = palette == 4 ? secondary_colors_ : monster ? context_.monster_colors[palette - 8]
                : images_[layer] ? images_[layer]->colors : imageless_colors_[layer];
            auto& base = palette == 4 ? secondary_base_colors_ : monster ? base_monster_colors_[palette - 8]
                : images_[layer] ? images_[layer]->base_colors : imageless_base_colors_[layer];
            const auto begin = colors.begin() + start, end = begin + count;
            std::rotate(begin, op == 0xa3 ? end - 1 : begin + 1, end);
            std::rotate(base.begin() + start,
                        op == 0xa3 ? base.begin() + start + count - 1 : base.begin() + start + 1,
                        base.begin() + start + count);
            counter = selection & 15U;
        }
        counter = wrap8(unsigned{counter} - 1U);
        break;
    }
    case 0xa5:
        mask_.configured = true;
        mask_.x = read();
        mask_.y = read();
        mask_.radius = read();
        mask_.final_x = read();
        mask_.final_y = read();
        mask_.final_radius = read();
        mask_.speed = read();
        mask_.phase = flipped(context_, s.attacker) ? 192 : 64;
        break;
    case 0xa6:
    case 0xa9: {
        auto x = read();
        const auto y = read();
        const auto radius = op == 0xa6 ? read() : std::uint8_t{0};
        if (op == 0xa9 && flipped(context_, s.attacker))
            x = wrap8(0U - x);
        const auto old_radius = mask_.radius;
        move_mask(x, y, radius);
        if (op == 0xa9)
            mask_.radius = old_radius;
        break;
    }
    case 0xa7:
        if (mask_crosses_picture())
            mask_build_pending_ = true;
        else
            update_mask();
        break;
    case 0xa8:
    case 0xc3: {
        const auto p = op == 0xa8 ? s.attacker_position : s.target_position;
        mask_.x = wrap8(p.x);
        mask_.y = wrap8(p.y);
        break;
    }
    case 0xac: {
        const auto selection = read(), axes = read();
        init_scroll_wave(selection, axes);
        break;
    }
    case 0xad:
        select_scroll(read());
        break;
    case 0xda: {
        const auto horizontal = read(), vertical = read();
        move_tornado(task, horizontal, vertical);
        break;
    }
    case 0xae: {
        const auto selection = read();
        unsigned axes = 0, overlay_axes = 0;
        for (unsigned layer = 0; layer < 3; ++layer)
            if (selection & (4U >> layer))
                for (unsigned axis = 0; axis < 2; ++axis)
                    if (selection & (64U << axis)) {
                        ++axes;
                        overlay_axes += layer == 2;
                    }
        // C1/EF65 copies 32 curve samples per axis; EF9B additionally adds
        // the overlay origin. Parameter setup and final loop exits are omitted.
        if (update_crosses_picture(axes * 31U * 41U + overlay_axes * 31U * 6U,
                                   axes * 31U * 6U))
            wave_build_pending_ = selection;
        else
            update_scroll_wave(selection);
        break;
    }
    case 0xaa:
    case 0xab:
    case 0xaf:
    case 0xb0:
    case 0xb1:
    case 0xb2:
    case 0xb3:
    case 0xb4:
    case 0xb5:
    case 0xb6:
    case 0xb7:
    case 0xb8:
    case 0xb9:
    case 0xba:
    case 0xbb:
    case 0xbc:
    case 0xcc:
    case 0xcd:
    case 0xce:
    case 0xcf:
    case 0xfb:
    case 0xfc:
    case 0xfd:
    case 0xfe:
        palette_command(op, read());
        break;
    case 0xbd: {
        const auto flags = read();
        if (flags & 128U)
            hold_image_[1] = (flags & 32U) != 0;
        if (flags & 64U)
            hold_image_[2] = (flags & 16U) != 0;
        break;
    }
    case 0xbe: {
        const auto effect = read();
        context_.display.mosaic_size = static_cast<std::uint8_t>((effect >> 4U) + 1U);
        context_.display.mosaic_layers = effect & 7U;
        break;
    }
    case 0xbf: {
        const auto to = word();
        // Return storage is one slot, not a call stack.
        task.return_to = s.instruction;
        jump(absolute(to));
        break;
    }
    case 0xc0:
        require(task.return_to.has_value(), "Animation return has no call");
        jump(*task.return_to);
        break;
    case 0xc4: {
        const auto flags = read();
        for (unsigned layer = 0; layer < 2; ++layer)
            if (flags & (128U >> layer)) {
                background_positions_[layer] = {add(s.position.x, s.offset.x),
                                                add(s.position.y, s.offset.y)};
                background_anchors_[layer] = background_positions_[layer];
            }
        break;
    }
    case 0xc2: {
        const auto flags = read();
        if (flags & 128U)
            paused_[1] = false;
        if (flags & 64U)
            paused_[2] = false;
        if (flags & 32U)
            paused_[0] = false;
        break;
    }
    case 0xc5: {
        // C1/E8FB selects a pointer by the attack caller's retained hit index.
        require(context_.repeated_hit < 4, "Repeated-hit animation index outside branch table");
        const auto at = s.instruction + unsigned{context_.repeated_hit} * 2U;
        require(at + 1 < programs.size(), "Truncated repeated-hit branch table");
        jump(absolute(read16(programs, at)));
        ++context_.repeated_hit;
        break;
    }
    case 0xc6: {
        require(s.attacker < 4, "Spiral step requires a character attacker");
        const auto speed = read(), backward = read();
        auto& actor = context_.characters[s.attacker];
        const auto clamp = [](std::uint16_t value) { return value & 256U ? std::uint8_t{255} : wrap8(value); };
        const auto vector = measure_battle_effect_vector(*library_,
            clamp(add(add(actor.x, actor.offset_x), actor.animation_x)),
            clamp(add(actor.y, actor.offset_y)), wrap8(s.target_position.x), wrap8(s.target_position.y));
        const auto difference = wrap8(unsigned{vector.angle} - task.vector_angle);
        const bool negative = difference >= 128;
        const auto turn = std::min(negative ? wrap8(0U - difference) : difference, steering_limit_);
        task.vector_angle = wrap8(unsigned{task.vector_angle} + (negative ? 256U - turn : turn));
        if (steering_limit_ < FF6_ROM_VALUE_EFFECT_STEERING_LIMIT_0 && !(++steering_ticks_ & FF6_ROM_VALUE_EFFECT_SPIRAL_CADENCE_0)) ++steering_limit_;
        if (vector.length < wrap8(unsigned{speed} + FF6_ROM_VALUE_EFFECT_ARRIVAL_PADDING_0)) break;
        actor.offset_x = add(actor.offset_x, scale_battle_effect_sine(*library_,
            wrap8(unsigned{task.vector_angle} + 64U), speed));
        actor.offset_y = add(actor.offset_y, scale_battle_effect_sine(*library_, task.vector_angle, speed));
        const auto x = (unsigned{actor.x} + actor.offset_x + actor.animation_x) & 511U;
        const auto y = (unsigned{actor.y} + actor.offset_y) & 511U;
        if (x < FF6_ROM_VALUE_EFFECT_SPIRAL_BOUNDS_1 || x >= FF6_ROM_VALUE_EFFECT_SPIRAL_BOUNDS_0 ||
            y < FF6_ROM_VALUE_EFFECT_SPIRAL_BOUNDS_1 || y >= FF6_ROM_VALUE_EFFECT_SPIRAL_BOUNDS_0) steering_limit_ = 255;
        require(backward <= s.instruction - 1, "Spiral branch underflow");
        jump(s.instruction - 1 - backward);
        break;
    }
    case 0xca:
        require(s.attacker < 4, "Action speed requires a character attacker");
        context_.characters[s.attacker].double_speed = read() != 0;
        break;
    case 0xc8:
        require(s.attacker < 4, "Attacker pose command requires a character");
        context_.characters[s.attacker].action = read();
        break;
    case 0xc7: {
        const auto event = read();
        if (event == 14) {
            context_.display.screen_shake = read();
            break;
        }
        if (event == 17) {
            context_.control.run_animation_blocked = true;
            break;
        }
        if (event == 8) {
            // C2/C1D6 uses signed offsets from the current vector target,
            // despite the older comment describing an attacker-relative origin.
            const auto x = read(), y = read();
            const int dx = x < 128 ? int{x} : int{x} - 256;
            const int dy = y < 128 ? int{y} : int{y} - 256;
            s.target_position.x = add(s.target_position.x, s.flipped ? -dx : dx);
            s.target_position.y = add(s.target_position.y, dy);
            break;
        }
        if (event == 16) {
            const auto identity = read();
            const auto found = std::ranges::find(context_.character_identities, std::optional{identity});
            // C2/C0D2 resolves the named character, falling back to slot zero.
            // Drawing admission precedes the later event's add-target/menu work.
            const auto slot = found == context_.character_identities.end() ? 0U
                : static_cast<unsigned>(found - context_.character_identities.begin());
            context_.revealed_characters.set(slot);
            break;
        }
        if (event == 12) {
            const auto identity = read(), graphics = read();
            const auto found = std::ranges::find(context_.character_identities,
                                                 std::optional{identity});
            // C1/C136 resolves the supplied character identity, independently
            // of the thread's attacker. An absent character is a defined no-op.
            if (found != context_.character_identities.end())
                appearance_changes_.push_back({BattleActorAppearanceOperation::replace_graphics,
                    static_cast<std::uint8_t>(found - context_.character_identities.begin()), graphics});
            break;
        }
        if (event == 11) {
            const auto command = read(), identity = read(), parameter = read();
            if (command == 0x10) event_audio_.push_back(GameSong{identity, parameter, false});
            else if (command == 0x18) event_audio_.push_back(GameSoundEffect{identity, parameter});
            else if (command >= 0x80 && command <= 0x82)
                event_audio_.push_back(PlaybackVolumeRamp{command == 0x80 ? PlaybackGroup::music_and_effects
                    : command == 0x81 ? PlaybackGroup::music : PlaybackGroup::effects, identity, parameter});
            else throw std::invalid_argument("Unsupported battle event sound command " + std::to_string(command));
            break;
        }
        if (event == 13) {
            const auto palette = read();
            require(palette < library_->event_palettes.size(), "Invalid event monster palette");
            context_.monster_colors[0] = library_->event_palettes[palette];
            break;
        }
        require(s.attacker < 4, "Event position command needs a character");
        auto& pose = context_.characters[s.attacker];
        auto& saved = context_.saved_character_positions[s.attacker];
        if (event == 0) {
            const bool right = read() != 0;
            if ((pose.facing != 0) != right) {
                pose.facing = right ? 1 : 0;
                s.flipped = right;
                pose.appearance.flip_x = !pose.appearance.flip_x;
            }
        } else if (event == 1) {
            pose.x = add(add(pose.x, pose.offset_x), pose.animation_x);
            pose.y = add(add(pose.y, pose.offset_y), pose.jump_y);
            s.attacker_position = {pose.x, pose.y};
            pose.offset_x = pose.offset_y = pose.animation_x = pose.jump_y = 0;
        } else if (event == 2)
            saved = BattleAnimationPoint{pose.x, pose.y};
        else if (event == 3 || event == 4) {
            require(saved.has_value(), "Character position has no saved owner");
            if (event == 4)
                s.target_position = *saved;
            else {
                pose.x = saved->x;
                pose.y = saved->y;
                pose.offset_x = pose.offset_y = pose.animation_x = pose.jump_y = 0;
            }
        } else if (event == 6) {
            pose.x = read(); pose.y = read();
            pose.offset_x = pose.offset_y = pose.animation_x = pose.jump_y = 0;
        } else if (event == 7 || event == 9) {
            const auto angle = battle_vector_angle(library_->angles, s.attacker_position, s.target_position);
            const auto& actions = event == 7 ? library_->character_walking : library_->character_jumping;
            pose.action = actions[(angle >> 5U) + (s.flipped ? 8U : 0U)];
        } else
            throw std::invalid_argument("Unimplemented attack event command " +
                                        std::to_string(event));
        break;
    }
    case 0xc9:
        sound(read(), s.target_position.x);
        break;
    case 0xcb: {
        const auto parameter = read();
        // C1/E779: monster scripts consume this command without changing the
        // character trail. The low bits are a frame-counter mask, not a delay.
        if (s.attacker < 4) {
            context_.control.echo_delay_mask = parameter & 127U;
            if (parameter & 128U)
                context_.control.echo_character = s.attacker;
            else {
                context_.control.echo_character.reset();
                ++context_.control.echo_reset_serial;
            }
        }
        break;
    }
    case 0xd0: {
        const auto priority = static_cast<std::uint8_t>((read() >> 4U) & 3U);
        for (auto& pose : context_.characters)
            pose.appearance.priority = priority;
        for (auto& style : context_.monster_styles)
            style.priority = priority;
        break;
    }
    case 0xd1:
        context_.control.fixed_draw_order = read() != 0;
        break;
    case 0xd2: {
        const auto x = read(), y = read();
        s.target_position = {static_cast<std::uint16_t>(s.flipped ? 255U - x : x), y};
        break;
    }
    case 0xd3: {
        // C1/F044 samples the moving actor, unlike A8's cached task anchor.
        // Jump height is deliberately absent from this authored origin.
        const auto origin = live_character_origin(context_, s.attacker);
        mask_.x = wrap8(origin.x);
        mask_.y = wrap8(origin.y);
        break;
    }
    case 0xd4: {
        const auto operation = read(), affected = read(), operand = read();
        set_blend(operation, affected, operand);
        break;
    }
    case 0xd5: {
        const auto axes = read();
        require(axes < 4, "Invalid monster flip axes");
        if (s.target >= 4) {
            const auto slot = s.target - 4U;
            context_.monsters[slot].flip ^=
                static_cast<std::uint8_t>(axes ^ context_.monster_facing[slot]);
        }
        break;
    }
    case 0xdb: {
        const auto forward = read();
        if (s.attacker >= 4 || context_.needs_step_back[s.attacker])
            jump(s.instruction + forward);
        break;
    }
    case 0xd9:
        require(s.attacker < 4, "Attacker priority needs a character");
        context_.characters[s.attacker].appearance.priority = (read() >> 4U) & 3U;
        break;
    case 0xd8: {
        require(task.character_orbit.has_value() && s.attacker < 4,
                "Character orbit update requires initialization");
        auto& orbit = *task.character_orbit;
        auto& actor = context_.characters[s.attacker];
        // C1/E5F9 samples this ellipse before changing radius or phase. Its
        // vertical radius is truncated first; handedness changes phase travel.
        actor.offset_x = add(s.offset.x, scale_battle_effect_sine(
            *library_, wrap8(unsigned{orbit.phase} + 64U), orbit.radius));
        actor.offset_y = add(s.offset.y, scale_battle_effect_sine(
            *library_, orbit.phase, static_cast<std::uint16_t>(orbit.radius / 2U)));
        const auto radial = read(), angular = read(), forward = read();
        auto radius = wrap8(unsigned{orbit.radius} + radial);
        if (radius & 128U) radius = 0;
        if (radius >= orbit.limit) {
            radius = orbit.limit;
            jump(s.instruction + forward);
        }
        orbit.radius = radius;
        orbit.phase = wrap8(unsigned{orbit.phase} +
            ((actor.facing & 1U) ? unsigned{angular} : 256U - angular));
        break;
    }
    case 0xd7: {
        // Fire Dance's two oscillators advance independently (C1/E68D).
        // Their offsets are not mirrored; forward travel is a separate command.
        const auto magnitude = read();
        s.offset = {add(0, scale_battle_effect_sine8(*library_, task.motion_phase, magnitude)),
                    add(0, scale_battle_effect_sine8(*library_, wrap8(task.radius), magnitude))};
        task.motion_phase = wrap8(unsigned{task.motion_phase} + FF6_ROM_VALUE_EFFECT_HORIZONTAL_PHASE_STEP_0);
        task.radius = static_cast<std::uint16_t>((task.radius & 0xff00U) |
                                                wrap8(unsigned{wrap8(task.radius)} + FF6_ROM_VALUE_EFFECT_VERTICAL_PHASE_STEP_0));
        break;
    }
    case 0xdc:
    case 0xf6:
        update_triangle(op == 0xf6);
        break;
    case 0xdd:
        triangle_.configured = true;
        triangle_.x = read();
        triangle_.y = read();
        triangle_.size = read();
        triangle_.phase = read();
        break;
    case 0xde:
    case 0xdf: {
        const auto p = op == 0xde ? s.attacker_position : s.target_position;
        triangle_.x = wrap8(p.x);
        triangle_.y = wrap8(p.y);
        break;
    }
    case 0xe0: {
        auto x = read();
        const auto y = read(), size = read(), phase = read();
        if (s.flipped)
            x = wrap8(0U - x);
        triangle_.x = wrap8(unsigned{triangle_.x} + x);
        triangle_.y = wrap8(unsigned{triangle_.y} + y);
        triangle_.size = wrap8(unsigned{triangle_.size} + size);
        triangle_.phase = wrap8(unsigned{triangle_.phase} + phase);
        break;
    }
    case 0xe9: {
        const auto x_mask = read(), y_mask = read();
        const auto x = random() & x_mask, y = random() & y_mask;
        s.offset = {add(0, s.flipped ? -int(x) : int(x)), static_cast<std::uint16_t>(y)};
        break;
    }
    case 0xea: {
        const auto selection = read();
        const auto destination = static_cast<std::uint8_t>(selection & 15U);
        require(destination < 4 || (destination == 4 && selection == 0x84),
                "Background destination needs an unowned alternate image owner");
        for (unsigned layer = 0; layer < 2; ++layer)
            if (selection & (128U >> layer))
                background_destinations_[layer] = destination;
        break;
    }
    case 0xe8:
    case 0xef: {
        require(!task.character_line, "Character-line to polar motion remains unowned");
        const auto radius = read(), angle = read();
        const auto polar_radius = wrap8(task.radius);
        auto x = scale_battle_effect_sine8(*library_, task.motion_phase, polar_radius);
        if (s.flipped)
            x = ~x; // The original mirrors with NOT, not two's-complement negation.
        s.offset = {add(0, x), add(0, scale_battle_effect_sine8(
                                          *library_, wrap8(unsigned{task.motion_phase} + 64U),
                                          static_cast<std::uint8_t>(op == 0xef ? polar_radius / 2U
                                                                               : polar_radius)))};
        task.radius = static_cast<std::uint16_t>((task.radius & 0xff00U) |
                                                 wrap8(unsigned{polar_radius} + radius));
        task.motion_phase = wrap8(unsigned{task.motion_phase} + angle);
        break;
    }
    case 0xe4: {
        // C1/E1FC: live bottom centers, plus four pixels at the destination;
        // script alignment and Magitek's ordinary vector adjustment do not apply.
        const auto& from = context_.anchors[s.attacker];
        const auto& to = context_.anchors[s.target];
        task.character_line = true;
        task.line = make_battle_character_line({from.center_x, from.bottom_y},
            {to.center_x, add(to.bottom_y, FF6_ROM_VALUE_EFFECT_LINE_DESTINATION_OFFSET_0)}, false);
        break;
    }
    case 0xe2:
    case 0xe3: {
        const auto speed = read(), backwards = read();
        require(s.attacker >= 4 && task.character_line && task.monster_line_origin.has_value(),
                "Monster vector needs an initialized origin and line");
        const auto step = advance_battle_line(task.line, task.monster_line_displacement,
                                              library_->sine16, speed, FF6_ROM_VALUE_EFFECT_MONSTER_ARC_HEIGHT_0, op == 0xe3);
        auto& monster = context_.monsters[s.attacker - 4];
        monster.x = add(task.monster_line_origin->x, signed16(task.monster_line_displacement.x));
        monster.y = add(task.monster_line_origin->y,
                        signed16(task.monster_line_displacement.y) + signed16(step.arc));
        if (step.complete && op == 0xe3) {
            monster.x = task.monster_line_origin->x;
            monster.y = task.monster_line_origin->y;
        }
        if (!step.complete) {
            require(backwards <= s.instruction - 1, "Monster vector branch underflow");
            jump(s.instruction - 1 - backwards);
        }
        break;
    }
    case 0xe7: {
        task.character_line = true;
        task.line = make_battle_character_line(s.attacker_position, s.target_position, context_.magitek);
        break;
    }
    case 0xe5:
    case 0xe6: {
        const auto speed = read(), backwards = read(), height = read();
        require(task.character_line && s.attacker < 4, "Character vector needs an initialized character line");
        auto offset = s.offset;
        const bool complete = advance_battle_character_line(
            task.line, context_.characters[s.attacker], offset,
            library_->sine16, speed, height, op == 0xe6);
        // C1/E0F1..E154 separates actor displacement from the effect sprite.
        // The reverse command still clears sprite Y when its return finishes.
        if (!detached_character_vector_) s.offset = offset;
        else if (complete && op == 0xe6) s.offset.y = 0;
        if (!complete) {
            require(backwards <= s.instruction - 2, "Animation relative branch underflow");
            jump(s.instruction - 2 - backwards);
        }
        break;
    }
    case 0xd6: {
        const auto x = read(), y = read();
        auto& axes = scrolling_[1].axes;
        // C1/E6E0 writes only low bytes. Its incidental high bytes can come
        // from menu text pointers, not displacement. Composition verifies a
        // 256-pixel source period before using this equivalent residue.
        axes[0].offsets.fill(s.flipped ? wrap8(0U - x) : x);
        axes[1].offsets.fill(y);
        axes[0].byte_displacement = axes[1].byte_displacement = true;
        break;
    }
    case 0xec: {
        const auto layer = read();
        require(layer < 3 && images_[layer].has_value(),
                "Layer change needs an owned effect image");
        task.layer = static_cast<BattleEffectLayer>(layer);
        task.frame_offset = 0;
        break;
    }
    case 0xeb: {
        const auto at = s.instruction + unsigned{task.copy} * 2U;
        require(at + 2 <= programs.size(), "Truncated animation task branch table");
        jump(absolute(read16(programs, at)));
        break;
    }
    case 0xe1:
    case 0xf1: {
        const auto flags = read();
        const auto slot = op == 0xe1 ? s.attacker : s.target;
        if (!(flags & 128U) && slot < 4)
            context_.control.visible[slot] = (flags & 1U) != 0;
        else if ((flags & 128U) && slot >= 4)
            context_.control.monster_visible[slot - 4] = (flags & 1U) != 0;
        break;
    }
    case 0xf7: {
        const auto scanline = read();
        // C1/DB4D compares the supplied unsigned byte directly. There is no
        // whitelist of lines; El Nino, for example, requests line 200.
        if (presentation_timing_ && !picture_scrolling_)
            picture_scrolling_ = scrolling_;
        // Rev 1 first leaves vblank and crosses an hblank. The requested line
        // is a lower bound, not a delay or proof of the actual write deadline.
        // Keep ordered barriers even when a later request has a smaller value.
        raster_floor_ = std::max(raster_floor_, std::uint16_t{scanline});
        raster_event(BattleEffectRasterOperation::barrier);
        break;
    }
    case 0xf8: {
        const auto ordinary = word(), armor = word();
        jump(absolute(context_.magitek ? armor : ordinary));
        break;
    }
    case 0xf9: {
        const auto action = read(), target = read(), override_slot = read();
        const auto slot = override_slot & 128U ? override_slot & 3U
                          : target             ? unsigned{s.target}
                                               : unsigned{s.attacker};
        if (slot < 4)
            context_.control.armor_actions[slot] = action;
        break;
    }
    case 0xfa:
        jump(absolute(word()));
        break;
    default:
        throw std::invalid_argument("Unimplemented attack animation command " + std::to_string(op));
    }
    return false;
}
void BattleAttackAnimation::finish_supplemental_load() {
    load_supplemental_summon(FF6_ROM_VALUE_EFFECT_SUPPLEMENTAL_SUMMON_0);
    require(images_[1].has_value(), "Small summon needs its foreground palette");
    context_.character_colors[2] = images_[1]->colors;
    esper_behind_actors_ = true;
}
void BattleAttackAnimation::raster_event(BattleEffectRasterOperation operation) {
    if (!initializing_)
        raster_events_.push_back({operation, raster_floor_});
}
void BattleAttackAnimation::publish_backgrounds() {
    effect_image_transfers_.clear();
    publish_mask();
    // Publish at a presentation wait; a later task can hold an earlier
    // request. A held upload still updates positioning, and does not consume
    // the selected destination (TfrAnimBGTiles).
    for (unsigned layer = 0; layer < 2; ++layer)
        if (const auto frame = pending_backgrounds_[layer]) {
            // Standalone palette tasks (including critical flashes) hide their
            // image uploads and use blank frames as waits. They need no pixel
            // owner. Retained images still receive positioning while held.
            if (hold_image_[layer + 1] && !images_[layer + 1])
                continue;
            background_views_[layer] = background_origin(layer == 0 ? 0 : 2);
            if (!hold_image_[layer + 1]) {
                append_battle_effect_transfer(effect_image_transfers_, layer == 0);
                // An ordinary frame publication replaces the foreground copy;
                // held uploads only move it (Doom and related dissolves).
                if (layer == 0 && monster_background_) {
                    // An all-missed death-copy initializer can retain only the
                    // cleared formation plane. Its first ordinary publication
                    // has no actor pixels/palettes to preserve (Soul Out).
                    // Real copied monsters still need an explicit replacement
                    // layout; do not silently discard their image ownership.
                    require(monster_background_->copied.none(),
                            "Monster background replacement needs an eight-pixel frame owner");
                    monster_background_.reset();
                }
                const auto published =
                    layer == 0 && foreground_frames_cleared_ ? std::uint8_t{15} : *frame;
                if (layer == 0 && background_destinations_[layer] == 4)
                    sketch_frame_ = published;
                else
                    background_images_[layer][background_destinations_[layer]] = published;
                background_frames_[layer] = published;
                background_destinations_[layer] = 0;
            }
            if (layer == 1 && scrolling_[2].pattern == BattleScrollPattern::fixed) {
                scrolling_[2].projection[0] = background_origin(2);
                scrolling_[2].projection_owned = true;
            }
        }
    pending_backgrounds_.fill(std::nullopt);
}
BattleEffectFrame BattleAttackAnimation::step(std::size_t budget, bool presentation) {
    if (complete_)
        return {{}, true, {}};
    require(presentation || !pass_started_,
            "Cannot switch to isolated passes during a resource wait");
    if (presentation && !presentation_timing_ && !picture_work_)
        raster_picture_.reset(gradient_.colors, mask_publication_rows_);
    presentation_timing_ = presentation;
    pending_sound_.reset();
    sound_requests_.clear();
    appearance_changes_.clear();
    raster_events_.clear();
    raster_floor_ = 0; // A presentation wait enters a new active picture.
    update_work_floor_ = 0;
    if (presentation && !picture_work_)
        raster_picture_.begin_picture();
    picture_scrolling_.reset();
    if (!pass_started_) {
        pass_started_ = true;
        pass_active_ = false;
        next_task_ = 0;
    }
    if (resume_gradient()) {
        publish_backgrounds();
        return {pending_sound_, false, sound_requests_, false, true};
    }
    if (resource_waits_) {
        --resource_waits_;
        publish_backgrounds();
        return {pending_sound_, false, sound_requests_, true};
    }
    if (mask_build_pending_) {
        update_mask();
        mask_build_pending_ = false;
    }
    if (wave_build_pending_) {
        update_scroll_wave(*wave_build_pending_);
        wave_build_pending_.reset();
    }
    if (gradient_build_pending_) {
        const auto command = *gradient_build_pending_;
        gradient_build_pending_.reset();
        begin_gradient(command[0], command[1], true);
    }
    if (supplemental_pending_) {
        finish_supplemental_load();
        supplemental_pending_ = false;
    }
    for (; next_task_ < tasks_.size(); ++next_task_) {
        auto& task = tasks_[next_task_];
        if (!resumed_layer_) {
            if (next_task_ != 0) {
                const auto traversal = battle_task_traversal_work(
                    static_cast<unsigned>(next_task_ - 1));
                accrue_update_work(traversal.cycles,
                    (traversal.fast_rom_clocks - 6U * traversal.cycles) / 2U);
            }
            const auto work = battle_task_gate_work({static_cast<unsigned>(next_task_),
                task && task->script.active, paused_[0], task ? task->script.counter : std::uint8_t{1},
                task ? task->script.rate : std::uint8_t{1}}).work;
            // The traversal follows this visit; using only the gate here
            // keeps a command's bound independent of later tasks.
            accrue_update_work(work.cycles, (work.fast_rom_clocks - 6U * work.cycles) / 2U);
        }
        if (!task || !task->script.active || (next_task_ < 80 && paused_[0]))
            continue;
        pass_active_ = true;
        auto& s = task->script;
        auto execution_layer = task->layer;
        if (resumed_layer_) {
            execution_layer = *resumed_layer_;
            resumed_layer_.reset();
        } else {
            s.counter = wrap8(unsigned{s.counter} - 1U);
            if (s.counter)
                continue;
            s.counter = s.rate;
            s.instruction = s.resume;
        }
        bool yielded = false;
        while (!yielded) {
            if (execution_layer != BattleEffectLayer::sprite) {
                task->sprite_visible = false;
                if (paused_[index(execution_layer)])
                    break;
            }
            require(budget != 0, "Attack animation exceeded its per-frame work limit");
            --budget;
            yielded = command(next_task_, execution_layer);
            if (mask_build_pending_ || gradient_build_pending_ || wave_build_pending_) {
                resumed_layer_ = execution_layer;
                publish_backgrounds();
                return {pending_sound_, false, sound_requests_, false, true,
                        appearance_changes_};
            }
            if (resource_waits_) {
                resumed_layer_ = execution_layer;
                --resource_waits_; // This call publishes the first transfer wait.
                publish_backgrounds();
                return {pending_sound_, false, sound_requests_, true, false,
                        appearance_changes_};
            }
            // BG1 can return to the sprite dispatcher. BG3 keeps its current
            // dispatcher until publication, even when the task changes type.
            if (execution_layer == BattleEffectLayer::sprite)
                execution_layer = task->layer;
            else if (execution_layer == BattleEffectLayer::foreground &&
                     task->layer == BattleEffectLayer::sprite)
                execution_layer = BattleEffectLayer::sprite;
            if (resume_gradient()) {
                require(!yielded, "A suspended gradient cannot also yield its task");
                resumed_layer_ = execution_layer;
                publish_backgrounds();
                return {pending_sound_, false, sound_requests_, false, true,
                        appearance_changes_};
            }
        }
    }
    publish_backgrounds();
    pass_started_ = false;
    ++frames_;
    complete_ = !pass_active_;
    if (complete_) {
        context_.control.fixed_draw_order = false;
        for (auto& pose : context_.characters)
            pose.appearance.priority = 3;
        for (auto& style : context_.monster_styles)
            style.priority = 3;
    }
    return {pending_sound_, complete_, sound_requests_, false, false, appearance_changes_};
}
void BattleAttackAnimation::publish_system_sound(BattleEffectFrame& frame) {
    if (pending_system_sound_ && !frame.sound) {
        frame.system_sound = SystemSoundEffect{FF6_ROM_VALUE_EFFECT_SYSTEM_CUE_0};
        pending_system_sound_ = false;
    }
}
BattleEffectFrame BattleAttackAnimation::advance(std::size_t budget) {
    require(!raster_timeline_, "Cannot replace scheduled animation time with isolated passes");
    // Reject atomically, including the suspended task and its pending cue.
    auto next = *this;
    auto frame = next.step(budget, false);
    next.publish_system_sound(frame);
    if (!complete_)
        next.context_.presentation_tick = wrap16(unsigned{context_.presentation_tick} + 1U);
    *this = std::move(next);
    return frame;
}
BattleEffectFrame
BattleAttackAnimation::advance_presentation(std::size_t budget,
                                            std::optional<BattleEffectRasterTiming> timing) {
    require(!raster_timeline_, "Scheduled animation requires the next picture work plan");
    auto next = *this;
    next.raster_timing_ = std::move(timing);
    next.gradient_writes_ = next.mask_writes_ = 0;
    auto frame = next.step(budget, true);
    next.publish_system_sound(frame);
    if (next.raster_timing_)
        require(next.gradient_writes_ == next.raster_timing_->gradient.size() &&
                    next.mask_writes_ == next.raster_timing_->mask.size(),
                "Unused battle raster deadlines");
    next.raster_timing_.reset();
    if (!complete_)
        next.context_.presentation_tick = wrap16(unsigned{context_.presentation_tick} + 1U);
    *this = std::move(next);
    return frame;
}
void BattleAttackAnimation::publish_suspended() {
    // Production uses semantic picture boundaries. A paused picture samples
    // retained gradient/mask writes and scroll buffers; it must not repeat the
    // previous picture's partially sampled rows or resume a script writer.
    require(!raster_timeline_, "Paused scheduled diagnostics require an explicit picture plan");
    if (presentation_timing_) raster_picture_.begin_picture();
    picture_scrolling_.reset();
    publish_backgrounds();
}
BattleEffectFrame BattleAttackAnimation::advance_scheduled(BattleEffectPictureWork work,
                                                           std::size_t budget) {
    auto next = *this;
    if (!next.raster_timeline_) {
        require(!next.presentation_timing_ && !next.pass_started_ && next.frames_ == 0,
                "Scheduled time must begin at the start of an animation body");
        next.raster_timeline_.emplace();
        next.raster_timeline_->reset(next.gradient_.colors, next.mask_publication_rows_);
    }
    next.raster_timeline_->begin_picture(work.picture);
    next.picture_work_ = std::move(work);
    next.next_gradient_plan_ = 0;
    next.raster_picture_ = next.raster_timeline_->raster();
    auto frame = next.step(budget, true);
    next.publish_system_sound(frame);
    require(next.next_gradient_plan_ == next.picture_work_->gradients.size(),
            "Unused scheduled gradient work");
    if (!next.raster_timeline_->writer_pending())
        next.raster_timeline_->finish_picture();
    next.picture_work_.reset();
    if (!complete_)
        next.context_.presentation_tick = wrap16(unsigned{context_.presentation_tick} + 1U);
    *this = std::move(next);
    return frame;
}
bool BattleAttackAnimation::resume_gradient() {
    if (!pending_gradient_)
        return false;
    require(picture_work_.has_value(), "Interrupted gradient needs a picture work plan");
    const auto progress = raster_timeline_->advance_writer();
    raster_picture_ = raster_timeline_->raster();
    if (progress.complete) {
        gradient_ = std::move(*pending_gradient_);
        pending_gradient_.reset();
    }
    return progress.picture_complete;
}
void BattleAttackAnimation::begin_gradient(std::uint8_t operation, std::uint8_t parameter,
                                           bool resuming) {
    unsigned minimum_work = 0;
    if (operation == 0x37) {
        // Clear's 151 stores: five-cycle store, four increments, comparison,
        // taken branch; omit setup and correct the final untaken branch.
        minimum_work = 151U * 19U - 1U;
    } else {
        require(library_->gradients.has_value(), "Battle gradient data is unowned");
        if (operation == 0x33 || operation == 0x39 || operation == 0x4b) {
            const auto amount = operation == 0x39
                ? wrap8(unsigned{gradient_.intensity} + parameter) : parameter;
            minimum_work = battle_gradient_source_work(operation == 0x39, gradient_.phase,
                                                       amount, library_->gradients->ramp).finish;
        }
    }
    // The lower bound does not say how much of an interrupted command remains.
    // Do not charge its complete work again when resuming in the next picture.
    if (!resuming && update_crosses_picture(minimum_work)) {
        gradient_build_pending_ = std::array{operation, parameter};
        return;
    }
    const auto previous = gradient_;
    if (operation == 0x37)
        gradient_.clear();
    else
        gradient_.update(*library_->gradients, operation, parameter);
    publish_gradient();
    if (picture_work_) {
        pending_gradient_ = gradient_;
        gradient_ = previous;
    }
    raster_event(BattleEffectRasterOperation::gradient);
}
void BattleAttackAnimation::publish_gradient() {
    if (!presentation_timing_)
        return;
    if (picture_work_) {
        require(next_gradient_plan_ < picture_work_->gradients.size(),
                "Missing scheduled gradient work");
        const auto& plan = picture_work_->gradients[next_gradient_plan_++];
        require(plan.start >= raster_timeline_->clock() &&
                    plan.stores.size() == gradient_.writes.size(),
                "Backwards or incomplete gradient work");
        require(!raster_floor_ || plan.start > picture_work_->picture.color_samples.back(),
                "Scheduled gradient precedes its after-battlefield barrier");
        std::vector<BattleRasterWriterStep> steps;
        for (std::size_t i = 0; i < plan.stores.size(); ++i)
            steps.push_back({plan.stores[i], BattleRasterWriteKind::gradient,
                             gradient_.writes[i].row, gradient_.writes[i].color, 0});
        require(!steps.empty(), "Empty scheduled gradient");
        raster_timeline_->begin_writer_at(plan.start, std::move(steps), plan.tail);
        return;
    }
    for (const auto write : gradient_.writes) {
        // Every supported F7 floor is below the battlefield. Its exact return
        // line is unnecessary to prove that all 151 row deadlines have passed.
        // Unbarriered work retains the caller's normalized before-picture slot;
        // precise within-picture scheduling must supply the complete deadlines.
        BattleRasterDeadline deadline{
            static_cast<std::uint16_t>(raster_floor_ ? battle_effect_rows : 0)};
        if (raster_timing_) {
            require(gradient_writes_ < raster_timing_->gradient.size(),
                    "Missing gradient write deadline");
            deadline = raster_timing_->gradient[gradient_writes_];
            require(!raster_floor_ || deadline.first_unread_row == battle_effect_rows,
                    "Gradient deadline precedes its after-battlefield barrier");
        }
        ++gradient_writes_;
        raster_picture_.write_gradient(write, deadline);
    }
}
void BattleAttackAnimation::publish_mask() {
    if (!mask_pending_)
        return;
    require(!picture_work_, "Scheduled mask publication needs its own elapsed work plan");
    if (presentation_timing_) {
        // UpdateScrollHDMA copies four groups of 38 rows. Scratch updates
        // never touch the visible mask until this shared publication boundary.
        for (unsigned row = 0; row < 38; ++row)
            for (unsigned group = 0; group < 4; ++group) {
                const unsigned y = row + group * 38;
                if (y >= mask_.rows.size())
                    continue;
                for (const bool right : {false, true}) {
                    BattleRasterDeadline deadline;
                    if (raster_timing_) {
                        require(mask_writes_ < raster_timing_->mask.size(),
                                "Missing mask publication deadline");
                        deadline = raster_timing_->mask[mask_writes_];
                    }
                    ++mask_writes_;
                    const auto span = mask_publication_rows_[y];
                    raster_picture_.write_mask_edge(y, right, right ? span.right : span.left,
                                                    deadline);
                }
            }
    }
    mask_pending_ = false;
}
} // namespace ff6
