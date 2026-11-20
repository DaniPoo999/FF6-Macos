#include "ff6/battle_character_presentation.hpp"
#include "rom-materials/battle_status_pose_choices.macros.inc"
#include "rom-materials/battle_status_sprite_choices.macros.inc"
#include "rom-materials/battle_status_graphics_choices.macros.inc"
#include "rom-materials/battle_vanish_transition_duration.macros.inc"
#include "rom-materials/battle_vanish_outline_selection.macros.inc"
#include "rom-materials/battle_vanish_outline_phase.macros.inc"
#include "rom-materials/battle_zombie_skin_selection.macros.inc"
#include "rom-materials/battle_berserk_skin_selection.macros.inc"
#include "rom-materials/battle_skin_first_color_slot.macros.inc"
#include "rom-materials/battle_skin_second_color_slot.macros.inc"
#include "rom-materials/battle_character_frame_refresh_mask.macros.inc"
#include "rom-materials/battle_status_outline_ramp_mask.macros.inc"
#include "rom-materials/battle_status_outline_half_period.macros.inc"
#include <algorithm>
#include <stdexcept>
namespace ff6 {
namespace {
bool has(std::uint32_t value, BattleStatus bit) {
    return (value & status_mask(bit)) != 0;
}
Rgb555 outline_color(const BattleCharacterPaletteData& data, unsigned identity,
                     std::uint8_t phase) {
    const auto shifted = wrap8(unsigned{phase} + data.outline_delay[phase & 3U]);
    const auto ramp = unsigned{
        (shifted & (unsigned{FF6_ROM_VALUE_BATTLE_STATUS_OUTLINE_RAMP_MASK_0} << 1U)) >> 1U};
    const auto amount = (shifted & FF6_ROM_VALUE_BATTLE_STATUS_OUTLINE_HALF_PERIOD_0) != 0
                            ? 31U - ramp : ramp;
    const auto source = data.outline[identity];
    unsigned result = 0;
    for (unsigned shift : {0U, 5U, 10U}) {
        const auto component = (source >> shift) & 31U;
        result |= (component > amount ? component - amount : 0U) << shift;
    }
    return static_cast<Rgb555>(result);
}
std::uint8_t status_action(std::uint32_t value, std::uint8_t ready) {
    if (has(value, BattleStatus::dead))
        return FF6_ROM_VALUE_BATTLE_STATUS_POSE_CHOICES_0;
    if (has(value, BattleStatus::sleep))
        return FF6_ROM_VALUE_BATTLE_STATUS_POSE_CHOICES_1;
    if (has(value, BattleStatus::confuse))
        return FF6_ROM_VALUE_BATTLE_STATUS_POSE_CHOICES_2;
    if (has(value, BattleStatus::control))
        return FF6_ROM_VALUE_BATTLE_STATUS_POSE_CHOICES_3;
    if (has(value, BattleStatus::poison) || has(value, BattleStatus::near_fatal))
        return FF6_ROM_VALUE_BATTLE_STATUS_POSE_CHOICES_1;
    return ready != 0 ? ready : FF6_ROM_VALUE_BATTLE_STATUS_POSE_CHOICES_4;
}
std::uint8_t status_sprite(std::uint32_t value) {
    if (has(value, BattleStatus::sleep))
        return FF6_ROM_VALUE_BATTLE_STATUS_SPRITE_CHOICES_0;
    if (has(value, BattleStatus::confuse))
        return FF6_ROM_VALUE_BATTLE_STATUS_SPRITE_CHOICES_1;
    if (has(value, BattleStatus::berserk) || has(value, BattleStatus::rage))
        return FF6_ROM_VALUE_BATTLE_STATUS_SPRITE_CHOICES_2;
    if (has(value, BattleStatus::poison))
        return FF6_ROM_VALUE_BATTLE_STATUS_SPRITE_CHOICES_3;
    if (has(value, BattleStatus::blind))
        return FF6_ROM_VALUE_BATTLE_STATUS_SPRITE_CHOICES_4;
    if (has(value, BattleStatus::silence))
        return FF6_ROM_VALUE_BATTLE_STATUS_SPRITE_CHOICES_5;
    return 0;
}
} // namespace
BattleCharacterPaletteData load_battle_character_palette_data(const ContentPack& pack) {
    const auto fixed = pack.get("battle_character_condition_palettes"),
               effects = pack.get("battle_character_palette_effects");
    if (fixed.size() != 46 || effects.size() != 32)
        throw std::invalid_argument("Invalid character condition palette extent");
    BattleCharacterPaletteData out;
    for (unsigned i = 0; i < 11; ++i) {
        out.petrify[i] = read16(fixed, i * 2) & 32767U;
        // Petrify's twelfth word is unused; frozen begins after it.
        out.frozen[i] = read16(fixed, 24 + i * 2) & 32767U;
    }
    for (unsigned i = 0; i < 4; ++i)
        out.outline_delay[i] = effects[i];
    for (unsigned i = 0; i < 6; ++i)
        out.skin[i / 2][i % 2] = read16(effects, 4 + i * 2) & 32767U;
    for (unsigned i = 0; i < 8; ++i)
        out.outline[i] = read16(effects, 16 + i * 2) & 32767U;
    return out;
}
void update_battle_character_palette(const BattleCharacterPaletteData& data, std::uint8_t counter,
                                     BattleCharacterPresentation& state) {
    const auto slot = (unsigned{state.layout.palette_phase} + 1U) & 3U;
    auto& c = state.control[slot];
    auto& colors = state.layout.visible_colors[slot];
    colors = state.layout.graphics[slot].colors;
    if (c.vanish_counter != 0) {
        c.vanish_counter = wrap8(unsigned{c.vanish_counter} - 2U);
        if (c.vanish_counter == 0)
            state.transition_busy = false;
        colors[1] = outline_color(
            data, FF6_ROM_VALUE_BATTLE_VANISH_OUTLINE_SELECTION_0,
            wrap8(unsigned{c.vanish_counter} * 4U + FF6_ROM_VALUE_BATTLE_VANISH_OUTLINE_PHASE_0));
        return;
    }
    const auto conditions = c.shown_status;
    if (has(conditions, BattleStatus::petrify)) {
        std::ranges::copy(data.petrify, colors.begin() + 1);
        return;
    }
    if (has(conditions, BattleStatus::frozen))
        std::ranges::copy(data.frozen, colors.begin() + 1);
    std::optional<unsigned> skin;
    if (has(conditions, BattleStatus::poison))
        skin = 0;
    else if (has(conditions, BattleStatus::zombie))
        skin = FF6_ROM_VALUE_BATTLE_ZOMBIE_SKIN_SELECTION_0;
    else if (has(conditions, BattleStatus::berserk))
        skin = FF6_ROM_VALUE_BATTLE_BERSERK_SKIN_SELECTION_0;
    if (skin) {
        colors[FF6_ROM_VALUE_BATTLE_SKIN_FIRST_COLOR_SLOT_0] = data.skin[*skin][0];
        colors[FF6_ROM_VALUE_BATTLE_SKIN_SECOND_COLOR_SLOT_0] = data.skin[*skin][1];
        return;
    }
    constexpr std::array<BattleStatus, 6> priority{BattleStatus::reflect, BattleStatus::safe,
                                                   BattleStatus::shell,   BattleStatus::stop,
                                                   BattleStatus::haste,   BattleStatus::slow};
    // The first row is selected by the original zero-clear. Remaining
    // selected rows are data operands, not encoding or status-bit definitions.
    constexpr std::array<unsigned, 6> identities{0,
#include "rom-materials/battle_status_outline_choices.inc"
    };
    for (unsigned i = 0; i < priority.size(); ++i)
        if (has(conditions, priority[i])) {
            colors[1] = outline_color(data, identities[i], counter);
            break;
        }
}
void update_battle_character_status(const BattleCharacterGraphicsData& data,
                                    BattleCharacterAppearanceRules rules,
                                    BattleCharacterPresentation& state) {
    auto next = state;
    const auto slot = next.status_cursor & 3U;
    auto& c = next.control[slot];
    if (c.vanish_counter == 0) {
        auto& graphics = next.layout.graphics[slot];
        auto& identity = next.layout.graphics_ids[slot];
        auto wanted = c.base_graphics;
        if (!rules.disable_status_graphics) {
            if (has(c.active_status, BattleStatus::morph))
                wanted = FF6_ROM_VALUE_BATTLE_STATUS_GRAPHICS_CHOICES_0;
            else if (has(c.active_status, BattleStatus::imp))
                wanted = FF6_ROM_VALUE_BATTLE_STATUS_GRAPHICS_CHOICES_1;
        }
        const auto load = [&](std::uint8_t image) {
            prepare_battle_character_graphics(
                data, {image, c.base_graphics, c.character, rules.green_soldier_disguise},
                graphics);
        };
        if (wanted != identity) {
            identity = wanted;
            load(wanted);
            c.outlined = false;
        }
        const bool outline =
            !rules.disable_status_graphics && has(c.active_status, BattleStatus::vanish);
        if (outline != c.outlined && !next.transition_busy) {
            next.transition_busy = true;
            next.previous_sheet = graphics.sheet;
            if (outline) {
                // The four bitplane operations retain exactly palette index 1.
                for (auto& tile : graphics.sheet)
                    for (auto& pixel : tile)
                        pixel = pixel == 1 ? 1 : 0;
            } else
                load(identity);
            c.outlined = outline;
            c.vanish_counter = FF6_ROM_VALUE_BATTLE_VANISH_TRANSITION_DURATION_0;
        }
        c.frame_frozen = false;
        next.layout.poses[slot].status_action = status_action(c.active_status, c.ready_action);
        c.status_sprite = status_sprite(c.active_status);
        c.shown_status = c.active_status;
    }
    next.status_cursor = wrap8(unsigned{next.status_cursor} + 1U);
    state = std::move(next);
}
BattleCharacterImageWork publish_battle_character_frames(bool magitek,
                                                         BattleCharacterPresentation& state) {
    auto frames = state.frames;
    std::array<std::uint8_t, 4> previous;
    // Shared entry costs five cycles; the final slot's untaken loop branch
    // saves one. Source boundaries and independent derivation are documented in
    // docs/BATTLE_RASTER_SCHEDULE.md. This records work without advancing time.
    BattleCharacterImageWork work{4, 0, 0};
    for (unsigned slot = 0; slot < 4; ++slot) {
        const auto& pose = state.layout.poses[slot];
        const auto& graphics = state.layout.graphics[slot];
        const auto& control = state.control[slot];
        previous[slot] = graphics.previous_frame;
        if (control.frame_frozen) {
            work.source_cycles += 31;
            continue;
        }
        const bool periodic_refresh =
            ((unsigned{pose.counter} - 1U) & FF6_ROM_VALUE_BATTLE_CHARACTER_FRAME_REFRESH_MASK_0) == 0;
        if (!periodic_refresh && pose.current_frame == previous[slot]) {
            work.source_cycles += 54;
            continue;
        }
        work.refreshed_slots |= static_cast<std::uint8_t>(1U << slot);
        work.source_cycles += periodic_refresh ? 24U : 35U;
        work.source_cycles += pose.current_frame >= 48 ? 18U : 15U;
        work.source_cycles += 23; // Prepare frame and transition arguments.
        previous[slot] = pose.current_frame;
        const auto frame = pose.current_frame >= 48 ? pose.current_frame - 48 : pose.current_frame;
        if (frame >= 32)
            throw std::invalid_argument("Unresolved out-of-sheet character frame publication");
        auto& target = frames[slot];
        if (control.vanish_counter == 0) {
            publish_battle_character_frame(graphics.sheet, static_cast<std::uint8_t>(frame),
                                           magitek, target);
            work.source_cycles += 20U + (magitek ? 1444U : 1905U);
            work.source_bytes += magitek ? 192 : 256;
        } else {
            // Four eight-row strips form a vertical wipe from the shared prior
            // sheet to the current sheet. Transition copies all rows in Magitek.
            const unsigned new_rows = 32U - (control.vanish_counter & 31U);
            for (unsigned tile = 0; tile < 8; ++tile)
                for (unsigned y = 0; y < 8; ++y) {
                    const auto& source = (tile / 2 * 8 + y < new_rows)
                                             ? graphics.sheet[unsigned(frame) * 8 + tile]
                                             : state.previous_sheet[unsigned(frame) * 8 + tile];
                    std::copy_n(source.begin() + y * 8, 8, target[tile].begin() + y * 8);
                }
            // Three whole strips and one eight-row split strip always copy a
            // full frame. A mid-strip source switch costs eight extra cycles;
            // the last strip phase needs nine more for source-bank setup.
            work.source_cycles += 32U + 2045U;
            if ((control.vanish_counter & 7U) != 0)
                work.source_cycles += 8;
            if (((control.vanish_counter >> 3U) & 3U) == 3)
                work.source_cycles += 9;
            work.source_bytes += 256;
        }
        if (control.condemned_overlay) {
            std::copy_n(control.condemned_overlay->begin(), 2, target.begin());
            work.source_cycles += 601;
            work.source_bytes += 64;
        } else
            work.source_cycles += 22;
        work.source_cycles += 18; // Restore/traverse to the next slot.
    }
    state.frames = std::move(frames);
    for (unsigned slot = 0; slot < 4; ++slot)
        state.layout.graphics[slot].previous_frame = previous[slot];
    return work;
}
void initialize_battle_character_presentation(const BattleCharacterViewData& view,
                                              const BattleCharacterGraphicsData& graphics,
                                              const BattleCharacterPaletteData& palettes,
                                              const BattleCharacterLayoutRequest& request,
                                              BattleCharacterAppearanceRules rules,
                                              std::uint8_t& counter,
                                              BattleCharacterPresentation& state) {
    auto next = state;
    auto clock = counter;
    initialize_battle_character_layout(view, graphics, request, next.layout);
    update_battle_character_x(next.layout.poses);
    for (unsigned i = 0; i < 4; ++i) {
        next.control[i].base_graphics = request.graphics[i];
        next.control[i].character = request.characters[i];
    }
    next.layout.palette_phase = wrap8(unsigned{next.layout.palette_phase} - 1U);
    for (unsigned i = 0; i < 4; ++i) {
        update_battle_character_palette(palettes, clock, next);
        update_battle_character_status(graphics, rules, next);
        next.layout.palette_phase = wrap8(unsigned{next.layout.palette_phase} + 1U);
        clock = wrap8(unsigned{clock} + 1U);
    }
    state = std::move(next);
    counter = clock;
}
} // namespace ff6
