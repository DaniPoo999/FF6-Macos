#include "rom-materials/battle_generic_list_scroll.macros.inc"
#include "rom-materials/battle_generic_list_bands.macros.inc"
#include "rom-materials/battle_generic_list_start.macros.inc"
#include "rom-materials/battle_generic_list_extent.macros.inc"
#include "rom-materials/battle_slot_failure_name.macros.inc"
#include "rom-materials/battle_blitz_failure_message.macros.inc"
#include "rom-materials/battle_blitz_failure_attack.macros.inc"
#include "rom-materials/battle_continuation_tools.macros.inc"
#include "rom-materials/battle_continuation_materialize.macros.inc"
#include "rom-materials/battle_continuation_jump_monster.macros.inc"
#include "rom-materials/battle_continuation_jump_character.macros.inc"
#include "rom-materials/battle_continuation_loss_message.macros.inc"
#include "rom-materials/battle_continuation_shock.macros.inc"
#include "rom-materials/battle_continuation_darts.macros.inc"
#include "rom-materials/battle_continuation_red_card.macros.inc"
#include "rom-materials/battle_continuation_escape_message.macros.inc"
#include "rom-materials/battle_continuation_banon_message.macros.inc"
#include "rom-materials/battle_continuation_health.macros.inc"
#include "rom-materials/battle_continuation_wake.macros.inc"
#include "rom-materials/battle_continuation_doom.macros.inc"
#include "rom-materials/battle_continuation_raise.macros.inc"
#include "rom-materials/battle_presentation_morph_prelude.macros.inc"
#include "rom-materials/battle_presentation_control.macros.inc"
#include "rom-materials/battle_presentation_stumble.macros.inc"
#include "rom-materials/battle_presentation_umaro_throw.macros.inc"
#include "rom-materials/battle_presentation_umaro_delta.macros.inc"
#include "rom-materials/battle_presentation_possess.macros.inc"
#include "rom-materials/battle_presentation_jump_unarmed.macros.inc"
#include "rom-materials/battle_presentation_escape.macros.inc"
#include "rom-materials/battle_presentation_health.macros.inc"
#include "rom-materials/battle_presentation_leap.macros.inc"
#include "rom-materials/battle_presentation_sketch.macros.inc"
#include "rom-materials/battle_presentation_dice_delta.macros.inc"
#include "rom-materials/battle_presentation_three_dice.macros.inc"
#include "rom-materials/battle_presentation_jump_monster_miss.macros.inc"
#include "rom-materials/battle_presentation_jump_character_miss.macros.inc"
#include "rom-materials/battle_presentation_jump_monster.macros.inc"
#include "rom-materials/battle_presentation_gil_toss.macros.inc"
#include "rom-materials/battle_presentation_shock.macros.inc"
#include "rom-materials/battle_presentation_runic.macros.inc"
#include "rom-materials/battle_presentation_failed_blitz.macros.inc"
#include "rom-materials/battle_presentation_steal.macros.inc"
#include "rom-materials/battle_presentation_revert_delta.macros.inc"
#include "rom-materials/battle_umaro_storm_attack.macros.inc"
#include "rom-materials/battle_quadra_total_hits.macros.inc"
#include "rom-materials/battle_ordinary_hand_attempts.macros.inc"
#include "rom-materials/battle_offering_hand_attempts.macros.inc"
#include "rom-materials/battle_proxy_cursor_shift.macros.inc"
#include "rom-materials/battle_proxy_center_x.macros.inc"
#include "rom-materials/battle_proxy_center_y.macros.inc"
#include "rom-materials/battle_equipment_cursor_columns.macros.inc"
#include "rom-materials/battle_equipment_cursor_y.macros.inc"
#include "rom-materials/battle_tools_name_start.macros.inc"
#include "rom-materials/battle_tools_name_stride.macros.inc"
#include "rom-materials/battle_dance_name_middle_fields.macros.inc"
#include "rom-materials/battle_dance_name_leading_fields.macros.inc"
#include "rom-materials/battle_dance_cursor_start.macros.inc"
#include "rom-materials/battle_dance_cursor_span.macros.inc"
#include "rom-materials/battle_tools_cursor_columns.macros.inc"
#include "rom-materials/battle_selected_cursor_offset.macros.inc"
#include "rom-materials/battle_lore_cursor_x.macros.inc"
#include "rom-materials/battle_horn_first_threshold.macros.inc"
#include "rom-materials/battle_horn_second_threshold.macros.inc"
#include "rom-materials/battle_dance_fade_limit.macros.inc"
#include "rom-materials/battle_dance_fade_total.macros.inc"
#include "rom-materials/battle_equipment_text_flags.macros.inc"
#include "rom-materials/battle_def_short_offset.macros.inc"
#include "rom-materials/battle_def_window_offset.macros.inc"
#include "rom-materials/battle_def_short_cursor_x.macros.inc"
#include "rom-materials/battle_row_short_cursor_x.macros.inc"
#include "rom-materials/battle_def_window_cursor_x.macros.inc"
#include "rom-materials/battle_row_window_cursor_x.macros.inc"
#include "rom-materials/battle_stance_short_cursor_y.macros.inc"
#include "rom-materials/battle_esper_cursor.macros.inc"
#include "rom-materials/battle_menu_confirm_cue.macros.inc"
#include "rom-materials/battle_menu_error_cue.macros.inc"
#include "rom-materials/battle_menu_active_cue.macros.inc"
#include "rom-materials/battle_menu_move_cue_difference.macros.inc"
#include "rom-materials/battle_bushido_charge_mask.macros.inc"
#include "rom-materials/battle_effect_train_scroll.macros.inc"
#include "rom-materials/battle_effect_dream_scroll.macros.inc"
#include "rom-materials/battle_slot_fade_step.macros.inc"
#include "ff6/battle_menu_session.hpp"
#include "ff6/battle_entry.hpp"
#include "ff6/battle_schedule.hpp"
#include "ff6/battle_time.hpp"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
template <class T> std::size_t index(T value) {
    return static_cast<std::size_t>(value);
}
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
void copy_rows(const TileLayer& from, TileLayer& to, unsigned row, unsigned count) {
    require(from.columns == 32 && to.columns == 32 && from.rows == 8 && to.rows == 8 &&
                from.cells.size() == 256 && to.cells.size() == 256 && row + count <= 8,
            "Invalid battle menu publication extent");
    std::copy_n(from.cells.begin() + row * 32, count * 32, to.cells.begin() + row * 32);
}
void finish_anchor_death(BattleState& battle, unsigned slot) {
    battle.status.targets[slot].pending.apply |= status_mask(BattleStatus::dead);
    std::array<BattleStatusContext, 10> contexts{};
    for (unsigned i = 0; i < 10; ++i)
        contexts[i] = {battle.stats[i].hp, battle.stats[i].blocked_status};
    update_battle_status(battle.status, contexts);
    resolve_battle_status_consequences(battle);
    update_battle_participation(battle);
    update_battle_action_readouts(battle);
}
void whiten_names(BattleMenuSurfaces& surfaces, BattleReadoutRectangle region) {
    for (auto canvas : {BattleTextCanvas::readout, BattleTextCanvas::command}) {
        if (auto& layer = surfaces.text[index(canvas)])
            for (unsigned y = 0; y < region.height; ++y)
                for (unsigned x = 0; x < region.width; ++x)
                    layer->cells.at((region.y + y) * 32 + region.x + x).palette = 0;
    }
}
} // namespace
void prepare_battle_menu_targets(BattleMenuContext& context, std::bitset<4> proxy,
                                BattleApproach approach) {
    // C1/0715..07A2 exposes a scripted enemy character as enemy cursor slot
    // five; C1/7096 maps it back on confirmation. Keep this alias in the menu
    // view only: attacks and animation still use the actual character identity.
    context.enemy_character_proxy = proxy;
    const auto displayed = proxy & context.visible.characters;
    context.eligible.characters &= ~proxy;
    context.visible.characters &= ~proxy;
    if (displayed.none()) return;
    unsigned slot = 0;
    while (!displayed[slot]) ++slot;
    auto& positions = context.positions;
    auto& groups = positions.groups;
    std::erase(groups.monsters_left, 5);
    std::erase(groups.monsters_right, 5);
    groups.monsters_left.push_back(5);
    groups.monsters_on_left.set(5);
    groups.monsters_on_right.reset(5);
    const auto& character = context.anchors[slot];
    auto& alias = context.anchors[9];
    alias = character;
    alias.cursor_x = wrap16(unsigned{character.cursor_x} +
                           (approach == BattleApproach::back ? (65536U - FF6_ROM_VALUE_BATTLE_PROXY_CURSOR_SHIFT_0) : unsigned{FF6_ROM_VALUE_BATTLE_PROXY_CURSOR_SHIFT_0}));
    positions.positions[5] = {wrap16(unsigned{character.center_x} - FF6_ROM_VALUE_BATTLE_PROXY_CENTER_X_0),
                             wrap16(unsigned{character.center_y} - FF6_ROM_VALUE_BATTLE_PROXY_CENTER_Y_0),
                             static_cast<std::uint16_t>(
                                 (static_cast<unsigned>(approach) & 1U) ^ 1U), 0};
    context.visible.monsters.set(5);
    context.eligible.monsters.set(5);
}
BattleMenuSessionAssets load_battle_menu_session_assets(const ContentPack& pack) {
    BattleMenuSessionAssets out{
        load_battle_menu_surface_assets(pack), load_battle_menu_transition_assets(pack),
        load_battle_cursor_assets(pack), load_battle_submission_data(pack).random};
    out.blitz = load_battle_blitz_codes(pack);
    out.slot = load_battle_slot(pack);
    const auto party = load_party_data(pack);
    for (unsigned i = 0; i < party.characters.size(); ++i)
        out.fixed_equipment[i] = party.characters[i].fixed_equipment;
    const auto magic = load_battle_magic(pack);
    const auto items = load_battle_item_effects(pack);
    for (unsigned i = 0; i < out.magic_supported.size(); ++i)
        out.magic_supported[i] = battle_magic_effect_supported(magic.effects[i]);
    for (unsigned i = 232; i < 255; ++i)
        out.items_supported[i] = battle_magic_effect_supported(items.effects[i], true) &&
                                 items.animations[i - 224].has_value();
    for (unsigned i = 0; i < 230; ++i)
        if (items.item_spells[i])
            out.items_supported[i] = battle_magic_effect_supported(magic.effects[*items.item_spells[i]]);
    for (unsigned i = 163; i <= 170; ++i)
        out.tools_supported[i] = true;
    return out;
}
BattleMenuSession::BattleMenuSession(BattleMenuSessionAssets assets, const BattleReadoutView& view,
                                     GameMenuState options, InputState input)
    : assets_(std::move(assets)), work_(make_battle_menu_surfaces(assets_.surfaces, view)),
      published_(work_), transition_(make_battle_command_transition()),
      visible_transition_(transition_), options_(options), input_(input) {
    initialize_input(input_, input_settings_,
                     options.custom_buttons ? options.saved_buttons : default_button_map());
}
void BattleMenuSession::start_text(BattleTextCanvas destination) {
    require(text_rows_left_ == 0, "Battle menu text publication is already pending");
    text_destination_ = destination;
    text_rows_left_ = 8;
    if (!published_.text[index(destination)]) {
        auto blank = *work_.text[index(destination)];
        blank.cells.assign(256, RasterCell{255, 0, 10});
        published_.text[index(destination)] = std::move(blank);
    }
}
void BattleMenuSession::publish(std::uint8_t clock) {
    transfers_.clear();
    append_battle_menu_transfers(transfers_, {style_rows_left_ != 0, text_rows_left_ != 0,
                                              pending_list_row_.has_value(), true});
    // TfrCmdWindowTiles and TfrMenuWindowTiles each copy one clock-selected
    // strip. Only TEXT publication gates window/input queue progress.
    const unsigned row = clock & 7U;
    if (style_rows_left_) {
        if (!published_.windows[index(BattleWindowCanvas::command)])
            published_.windows[index(BattleWindowCanvas::command)] = work_.windows[0];
        copy_rows(*work_.windows[index(BattleWindowCanvas::command)],
                  *published_.windows[index(BattleWindowCanvas::command)], row, 1);
        --style_rows_left_;
    }
    if (text_rows_left_) {
        copy_rows(*work_.text[index(text_destination_)], *published_.text[index(text_destination_)],
                  row, 1);
        --text_rows_left_;
    }
    if (pending_list_row_) {
        copy_rows(*work_.text[index(BattleTextCanvas::list)],
                  *published_.text[index(BattleTextCanvas::list)], *pending_list_row_ * 2U, 2);
        pending_list_row_.reset();
    }
    published_.mp = work_.mp;
    visible_transition_ = transition_;
    publish_battle_cursors(cursors_, assets_.cursors, clock, false);
}
void BattleMenuSession::wait(BattleMenuPhase next) {
    phase_ = BattleMenuPhase::wait;
    after_wait_ = next;
}
void BattleMenuSession::add_sounds(BattleMenuSounds sounds) {
    sounds_.move = wrap8(unsigned{sounds_.move} + sounds.move);
    sounds_.select = wrap8(unsigned{sounds_.select} + sounds.select);
    sounds_.invalid = wrap8(unsigned{sounds_.invalid} + sounds.invalid);
}
BattleCommandList BattleMenuSession::command_list(const BattleState& battle) const {
    require(active_.has_value(), "Battle command menu has no active character");
    if (layout_ != BattleCommandLayout::control)
        return battle.availability.commands[*active_];
    BattleCommandList out;
    for (unsigned i = 0; i < 4; ++i)
        if (const auto& spell = battle.control_commands[*active_][i])
            out.slots[i] = PreparedBattleCommand{spell->identity, spell->targeting, spell->disabled};
    return out;
}
void BattleMenuSession::window_step(const BattleState& battle, const BattleMenuContext& context) {
    if (text_rows_left_ || windows_.empty())
        return;
    const auto task = windows_.front();
    const auto start = [&](unsigned window, unsigned text, BattleWindowMotion motion) {
        begin_battle_menu_transition(transition_, assets_.transitions.windows.at(window),
                                     assets_.transitions.text.at(text), motion,
                                     motion == BattleWindowMotion::closing);
    };
    switch (task) {
    case WindowTask::style:
        layout_ = order_.controlled_monsters[*active_] ? BattleCommandLayout::control
                  : options_.short_commands            ? BattleCommandLayout::short_form
                                                       : BattleCommandLayout::window;
        require(context.text_rows[*active_].has_value(), "Active battle menu has no readout row");
        prepare_battle_command_surface(work_, assets_.surfaces, command_list(battle), layout_,
                                       *context.text_rows[*active_]);
        style_rows_left_ = 8;
        windows_.pop_front();
        break;
    case WindowTask::open_command:
        start_text(BattleTextCanvas::command);
        start(layout_ == BattleCommandLayout::short_form ? 22 : 1, 0, BattleWindowMotion::opening);
        fast_window_ = true;
        active_sound_ = true;
        windows_.front() = WindowTask::wait_open;
        break;
    case WindowTask::wait_open:
    case WindowTask::wait_close:
        if (!transition_.busy())
            windows_.pop_front();
        break;
    case WindowTask::auxiliary:
        work_.text[index(BattleTextCanvas::auxiliary)] = work_.command_work;
        published_.text[index(BattleTextCanvas::auxiliary)] = work_.command_work;
        windows_.pop_front();
        break;
    case WindowTask::open_slot: {
        slot_ = {};
        slot_fade_ = FF6_ROM_VALUE_BATTLE_SLOT_FADE_STEPS_0;
        auto& bank = transition_.text_banks[index(BattleTextBank::list)];
        slot_previous_list_ = bank;
        for (auto& line : bank) line = {BattleTextCanvas::list, 0, 0};
        auto blank = work_.command_work;
        blank.cells.assign(256, RasterCell{255, 0, 10});
        work_.text[index(BattleTextCanvas::list)] = blank;
        published_.text[index(BattleTextCanvas::list)] = std::move(blank);
        start(2, 2, BattleWindowMotion::opening);
        fast_window_ = false;
        windows_.front() = WindowTask::wait_open;
        break;
    }
    case WindowTask::restore_slot:
        transition_.text_banks[index(BattleTextBank::list)] = std::move(slot_previous_list_);
        windows_.pop_front();
        break;
    case WindowTask::open_list: {
        // scr_tmp_line_set resets only vertical offsets, retaining horizontal
        // bindings saved by earlier exchanges. The frozen native layout has a
        // four-pixel bias versus the source buffer origin. Keep that known
        // reconstruction discrepancy while preserving the comparison binary.
        auto& bank = transition_.text_banks[index(BattleTextBank::list)];
        for (unsigned i = FF6_ROM_VALUE_BATTLE_GENERIC_LIST_START_0; i < FF6_ROM_VALUE_BATTLE_GENERIC_LIST_START_0 + FF6_ROM_VALUE_BATTLE_GENERIC_LIST_EXTENT_0 / 4U; ++i)
            bank[i].offset_y = static_cast<std::int16_t>(-FF6_ROM_VALUE_BATTLE_GENERIC_LIST_SCROLL_0 + 4 +
                static_cast<int>((i - FF6_ROM_VALUE_BATTLE_GENERIC_LIST_START_0) / FF6_ROM_VALUE_BATTLE_GENERIC_LIST_BANDS_0 * FF6_ROM_VALUE_BATTLE_GENERIC_LIST_BANDS_1));
        start(choice_kind_ == 2 || choice_kind_ == 12 ? 4 : 3, 2, BattleWindowMotion::opening);
        fast_window_ = false;
        windows_.front() = WindowTask::wait_open;
        break;
    }
    case WindowTask::open_esper:
        prepare_battle_summon_surface(work_, assets_.surfaces, *battle.availability.spells[*active_].esper);
        start_text(BattleTextCanvas::esper);
        start(8, 3, BattleWindowMotion::opening);
        fast_window_ = false;
        windows_.front() = WindowTask::wait_open;
        break;
    case WindowTask::close_esper:
        start(9, 3, BattleWindowMotion::closing);
        fast_window_ = false;
        windows_.front() = WindowTask::wait_close;
        break;
    case WindowTask::open_equipment:
        refresh_equipment_text(battle);
        start(5, 4, BattleWindowMotion::opening);
        fast_window_ = false;
        windows_.front() = WindowTask::wait_open;
        break;
    case WindowTask::close_equipment:
        start(7, 4, BattleWindowMotion::closing);
        fast_window_ = false;
        windows_.front() = WindowTask::wait_close;
        break;
    case WindowTask::open_bushido:
        prepare_battle_bushido_surface(work_, assets_.surfaces, battle.skills.bushido_count, 0);
        start_text(BattleTextCanvas::auxiliary);
        start(18, 9, BattleWindowMotion::opening);
        fast_window_ = false;
        windows_.front() = WindowTask::wait_open;
        break;
    case WindowTask::close_bushido:
        start(19, 9, BattleWindowMotion::closing);
        fast_window_ = false;
        windows_.front() = WindowTask::wait_close;
        break;
    case WindowTask::open_stance: {
        const bool short_form = layout_ == BattleCommandLayout::short_form;
        const auto row = commands_[*active_];
        prepare_battle_stance_surface(work_, assets_.surfaces, row, short_form, defend_choice_);
        start_text(BattleTextCanvas::auxiliary);
        auto window = assets_.transitions.windows.at(short_form ? 23 : 10 + row);
        if (defend_choice_)
            window.binding.offset_x -= short_form ? FF6_ROM_VALUE_BATTLE_DEF_SHORT_OFFSET_0 : FF6_ROM_VALUE_BATTLE_DEF_WINDOW_OFFSET_0;
        begin_battle_menu_transition(transition_, window,
                                     assets_.transitions.text.at(5 + (short_form ? 0 : row)),
                                     BattleWindowMotion::opening);
        fast_window_ = false;
        windows_.front() = WindowTask::wait_open;
        break;
    }
    case WindowTask::close_stance: {
        const bool short_form = layout_ == BattleCommandLayout::short_form;
        const auto row = commands_[*active_];
        start(short_form ? 24 : 14 + row, 5 + (short_form ? 0 : row), BattleWindowMotion::closing);
        // The text-bank exchange owns restoration until the closing transition
        // ends; do not overwrite its auxiliary surface while still sampled.
        fast_window_ = false;
        windows_.front() = WindowTask::wait_close;
        break;
    }
    case WindowTask::close_list:
        start(6, 2, BattleWindowMotion::closing);
        fast_window_ = false;
        windows_.front() = WindowTask::wait_close;
        break;
    case WindowTask::close_command:
        start(0, 0, BattleWindowMotion::closing);
        whiten_names(work_, assets_.surfaces.readout.text_regions[1]);
        whiten_names(published_, assets_.surfaces.readout.text_regions[1]);
        fast_window_ = false;
        windows_.front() = WindowTask::wait_close;
        break;
    case WindowTask::reset_style: {
        // This task restores only the border. Re-rendering command text here
        // would reinterpret Control attack IDs as ordinary command IDs.
        auto border = *work_.windows[index(BattleWindowCanvas::closed)];
        draw_battle_window(assets_.surfaces.readout, assets_.surfaces.windows[0], border);
        work_.windows[index(BattleWindowCanvas::command)] = std::move(border);
        style_rows_left_ = 8;
        windows_.pop_front();
        break;
    }
    case WindowTask::finish:
        finish_battle_menu(order_, *active_, cycle_);
        active_.reset();
        wait_for_menu_ = false;
        cycle_ = BattleMenuCycle::consumed;
        windows_.pop_front();
        break;
    }
}
void BattleMenuSession::stage_magitek(const BattleMenuContext& context) {
    require(!pending_list_row_, "Previous Magitek row has not published");
    prepare_battle_magitek_row(work_, assets_.surfaces, list_row_, context.graphics[*active_]);
    pending_list_row_ = list_row_++;
    if (list_row_ == 4) {
        windows_ = {WindowTask::open_list};
        wait(BattleMenuPhase::magitek);
    } else
        phase_ = BattleMenuPhase::prepare_magitek;
}
void BattleMenuSession::begin_target(std::uint8_t policy, const BattleMenuContext& context) {
    target_return_ = phase_;
    hand_.reset();
    if (initialize_battle_target_cursor(targets_, policy, *active_, context.positions,
                                        context.eligible))
        phase_ = BattleMenuPhase::target;
}
BattleListCursor BattleMenuSession::list_cursor() const {
    if (!active_)
        return {};
    return choice_kind_ == 19 ? dance_cursors_[*active_]
           : choice_kind_ == 16                         ? rage_cursors_[*active_]
           : choice_kind_ == 8                          ? throw_cursors_[*active_]
           : choice_kind_ == 12                         ? lore_cursors_[*active_]
           : choice_kind_ == 2                          ? magic_cursors_[*active_]
           : (choice_kind_ == 1 || choice_kind_ == 9) ? item_cursors_[*active_]
                                                      : lists_[*active_];
}
void BattleMenuSession::stage_choices(const BattleState& battle) {
    require(!pending_list_row_, "Previous choice row has not published");
    const auto first = list_scroll() + list_row_;
    if (choice_kind_ == 2)
        prepare_battle_magic_row(work_, assets_.surfaces, list_row_,
                                 battle.availability.spells[*active_], first * 2);
    else if (choice_kind_ == 12)
        prepare_battle_lore_row(work_, assets_.surfaces, list_row_,
                                battle.availability.spells[*active_].lores.at(first));
    else if (choice_kind_ == 16 || choice_kind_ == 19) {
        auto& canvas = *work_.text[index(BattleTextCanvas::list)];
        for (unsigned y = list_row_ * 2; y < list_row_ * 2 + 2; ++y)
            for (unsigned x = 0; x < 32; ++x) canvas.cells[y * 32 + x] = {255, 0, 10};
        const auto identity = [&](unsigned i) {
            if (choice_kind_ == 19) return i < 8 && battle.skills.dances[i]
                ? static_cast<std::uint8_t>(i) : std::uint8_t{255};
            return i < battle.skills.rages.size() ? battle.skills.rages[i] : std::uint8_t{255};
        };
        // US DrawRageListText: ten-glyph names plus one trailing blank, at
        // columns 4 and 17. The Japanese layout uses different padding.
        const std::uint8_t name = choice_kind_ == 19 ? 23 : 24;
        // DrawDanceListText keeps all eight fixed positions, including unknowns.
        Bytes label{5, (FF6_ROM_VALUE_BATTLE_DANCE_NAME_LEADING_FIELDS_0 & 255), 4, ((FF6_ROM_VALUE_BATTLE_DANCE_NAME_LEADING_FIELDS_0 >> 16) & 255), name, identity(first * 2), 5, (FF6_ROM_VALUE_BATTLE_DANCE_NAME_MIDDLE_FIELDS_0 & 255),
                    4, ((FF6_ROM_VALUE_BATTLE_DANCE_NAME_MIDDLE_FIELDS_0 >> 16) & 255), name, identity(first * 2 + 1), 0};
        draw_battle_text(assets_.surfaces.readout, {}, {0, static_cast<std::uint8_t>(list_row_ * 2), 32, 2},
                        label, canvas, &assets_.surfaces.names, BattleTextLanguage::list);
    } else if (choice_kind_ == 9) {
        // The English Tools list prints two extracted item names, without
        // inventory quantities, in fifteen-cell columns (C1/4BE6).
        for (unsigned column = 0; column < 2; ++column) {
            const auto at = first * 2 + column;
            const auto id =
                at < tool_list_.size() ? tool_list_[at].item->identity : std::uint8_t{255};
            Bytes label{4, FF6_ROM_VALUE_BATTLE_EQUIPMENT_TEXT_FLAGS_0, 14, id, 0};
            auto& canvas = *work_.text[index(BattleTextCanvas::list)];
            for (unsigned y = list_row_ * 2; y < list_row_ * 2 + 2; ++y)
                for (unsigned x = column * FF6_ROM_VALUE_BATTLE_TOOLS_NAME_STRIDE_0; x < column * FF6_ROM_VALUE_BATTLE_TOOLS_NAME_STRIDE_0 + FF6_ROM_VALUE_BATTLE_TOOLS_NAME_STRIDE_0; ++x)
                    canvas.cells[y * 32 + x] = {255, 0, 10};
            draw_battle_text(assets_.surfaces.readout, {},
                             {static_cast<std::uint8_t>(FF6_ROM_VALUE_BATTLE_TOOLS_NAME_START_0 + column * FF6_ROM_VALUE_BATTLE_TOOLS_NAME_STRIDE_0),
                              static_cast<std::uint8_t>(list_row_ * 2), 13, 2},
                             label, canvas, &assets_.surfaces.names, BattleTextLanguage::list);
        }
    } else if (choice_kind_ == 8) {
        auto stack = first < tool_list_.size() ? tool_list_[first] : BattleItemStack{};
        // DrawThrowListText has the same one-column name/quantity layout as
        // Item, but ignores ordinary usability: throwable weapons are enabled.
        if (stack.item) stack.item->usage_flags &= 127U;
        prepare_battle_item_row(work_, assets_.surfaces, list_row_, stack);
    } else {
        auto stack = battle.inventory.items.at(first);
        if (selected_hand_ && stack.item) {
            stack.item->usage_flags |= 128U;
            if (can_exchange_battle_hand(battle.inventory, *active_, *selected_hand_, first,
                                          battle.genji_gloves[*active_])) stack.item->usage_flags &= 127U;
        }
        prepare_battle_item_row(work_, assets_.surfaces, list_row_, stack);
    }
    pending_list_row_ = list_row_++;
    if (list_row_ == 4) {
        if (!reopening_choices_)
            windows_ = {WindowTask::open_list};
        wait(BattleMenuPhase::choices);
    } else
        phase_ = BattleMenuPhase::prepare_choices;
}
void BattleMenuSession::choice_input(BattleState& battle, const BattleMenuContext& context) {
    hand_.reset();
    if (closing_) {
        phase_ = BattleMenuPhase::close_list;
        return;
    }
    auto& cursor = choice_kind_ == 19 ? dance_cursors_[*active_] :
                   choice_kind_ == 12 ? lore_cursors_[*active_] :
                   choice_kind_ == 16 ? rage_cursors_[*active_] :
                   choice_kind_ == 8 ? throw_cursors_[*active_] :
                   choice_kind_ == 2 ? magic_cursors_[*active_] : item_cursors_[*active_];
    auto& scroll = choice_kind_ == 19 ? dance_scroll_[*active_] :
                   choice_kind_ == 12 ? lore_scroll_[*active_] :
                   choice_kind_ == 16 ? rage_scroll_[*active_] :
                   choice_kind_ == 8 ? throw_scroll_[*active_] :
                   choice_kind_ == 2 ? magic_scroll_[*active_] : item_scroll_[*active_];
    if (choice_kind_ == 1 && !cursor.row && !scroll && held(input_.repeating, Button::up) &&
        !selected_item_ && battle.inventory.hands[*active_]) {
        equipment_cursor_ = 0;
        ++sounds_.move;
        hand_.reset();
        windows_ = {WindowTask::open_equipment};
        wait(BattleMenuPhase::equipment);
        return;
    }
    if (choice_kind_ == 2 && !cursor.row && !scroll && held(input_.repeating, Button::up)) {
        const auto& esper = battle.availability.spells[*active_].esper;
        if (esper && !esper->disabled) {
            ++sounds_.move;
            windows_ = {WindowTask::open_esper};
            wait(BattleMenuPhase::esper);
            return;
        }
    }
    const auto moved = move_battle_list_cursor(cursor, choice_kind_ == 1 || choice_kind_ == 8 || choice_kind_ == 12 ? 1 : 2,
                                               input_.repeating);
    if (choice_kind_ != 19 || !moved.scroll) cursor = moved.cursor;
    if (moved.sound)
        ++sounds_.move;
    const unsigned limit = choice_kind_ == 12 ? 20
                           : choice_kind_ == 16 ? 124
                           : choice_kind_ == 9 || choice_kind_ == 19 ? 0
                           : choice_kind_ == 2 ? 23
                                               : 251; // US inventory's original last-row bound.
    if ((moved.scroll > 0 && scroll < limit) || (moved.scroll < 0 && scroll > 0)) {
        scroll = static_cast<std::uint8_t>(int{scroll} + (moved.scroll > 0 ? 1 : -1));
        ++sounds_.move;
        list_row_ = 0;
        reopening_choices_ = true;
        stage_choices(battle);
        return;
    }
    if (held(input_.pressed, Button::b)) {
        ++sounds_.select;
        if (selected_hand_) {
            selected_hand_.reset();
            list_row_ = 0; reopening_choices_ = true; stage_choices(battle);
        } else if (selected_item_)
            selected_item_.reset();
        else {
            windows_ = {WindowTask::close_list};
            wait(BattleMenuPhase::command);
        }
        return;
    }
    const unsigned position = scroll + cursor.row;
    if (held(input_.repeating, Button::a)) {
        item_source_ = BattleItemSource::inventory;
        if (choice_kind_ == 2 || choice_kind_ == 12) {
            const auto& spells = battle.availability.spells[*active_];
            const auto& spell = choice_kind_ == 12 ? spells.lores.at(position)
                : spells.magic.at(position * 2 + cursor.column);
            if (!spell || spell->disabled)
                ++sounds_.invalid;
            else if (!assets_.magic_supported[spell->identity + (choice_kind_ == 12 ? 139U : 0U)]) {
                notice_ = "This spell effect is not implemented yet";
                ++sounds_.invalid;
            } else {
                notice_.clear();
                attack_ = spell->identity;
                ++sounds_.select;
                begin_target(spell->targeting, context);
            }
        } else if (choice_kind_ == 19) {
            const unsigned at = position * 2 + cursor.column;
            if (at >= 8 || !battle.skills.dances[at]) ++sounds_.invalid;
            else {
                attack_ = static_cast<std::uint8_t>(at);
                ++sounds_.select;
                begin_target(2, context);
            }
        } else if (choice_kind_ == 16) {
            const unsigned at = position * 2 + cursor.column;
            if (at >= battle.skills.rages.size()) ++sounds_.invalid;
            else {
                attack_ = battle.skills.rages[at];
                ++sounds_.select;
                begin_target(2, context);
            }
        } else if (choice_kind_ == 8) {
            if (position >= tool_list_.size())
                ++sounds_.invalid;
            else {
                notice_.clear();
                attack_ = tool_list_[position].item->identity;
                ++sounds_.select;
                begin_target(tool_list_[position].item->targeting, context);
            }
        } else if (choice_kind_ == 9) {
            const unsigned at = position * 2 + cursor.column;
            if (at >= tool_list_.size())
                ++sounds_.invalid;
            else if (!assets_.tools_supported[tool_list_[at].item->identity]) {
                notice_ = "This tool effect is not implemented yet";
                ++sounds_.invalid;
            } else {
                attack_ = tool_list_[at].item->identity;
                ++sounds_.select;
                begin_target(tool_list_[at].item->targeting, context);
            }
        } else if (selected_hand_) {
            if (exchange_battle_hand(battle.inventory, *active_, *selected_hand_, position,
                                      battle.genji_gloves[*active_])) {
                battle.equipment_changed.set(*active_);
                ++sounds_.select;
            } else ++sounds_.invalid;
            selected_hand_.reset();
            list_row_ = 0; reopening_choices_ = true; stage_choices(battle);
        } else if (!selected_item_) {
            selected_item_ = position;
            ++sounds_.select;
        } else if (*selected_item_ != position) {
            // The original first selects a stack; confirming another row swaps
            // it. A second confirmation on that same row enters targeting.
            std::swap(battle.inventory.items[*selected_item_], battle.inventory.items[position]);
            selected_item_.reset();
            ++sounds_.select;
            list_row_ = 0;
            reopening_choices_ = true;
            stage_choices(battle);
        } else {
            selected_item_.reset();
            const auto& stack = battle.inventory.items[position];
            if (!stack.item || !stack.quantity || (stack.item->usage_flags & 128U))
                ++sounds_.invalid;
            else if (!assets_.items_supported[stack.item->identity]) {
                notice_ = "This item effect is not implemented yet";
                ++sounds_.invalid;
            } else {
                notice_.clear();
                attack_ = stack.item->identity;
                ++sounds_.select;
                begin_target(stack.item->targeting, context);
            }
        }
    }
    if (phase_ == BattleMenuPhase::choices) {
        const auto x = choice_kind_ == 12 ? std::uint8_t{FF6_ROM_VALUE_BATTLE_LORE_CURSOR_X_0} // C1/8288: English Lore cursor.
                       : choice_kind_ == 19 ? static_cast<std::uint8_t>(FF6_ROM_VALUE_BATTLE_DANCE_CURSOR_START_0 + cursor.column * FF6_ROM_VALUE_BATTLE_DANCE_CURSOR_SPAN_0)
                       : choice_kind_ == 16 ? assets_.cursors.magitek_columns[cursor.column]
                       : choice_kind_ == 9   ? static_cast<std::uint8_t>(FF6_ROM_VALUE_BATTLE_TOOLS_CURSOR_COLUMNS_0 + cursor.column * (FF6_ROM_VALUE_BATTLE_TOOLS_CURSOR_COLUMNS_1 - FF6_ROM_VALUE_BATTLE_TOOLS_CURSOR_COLUMNS_0))
                       : choice_kind_ == 2 ? assets_.surfaces.magic_columns[cursor.column]
                                           : assets_.surfaces.item_column;
        hand_ = BattleHandCursor{x, assets_.cursors.list_rows[cursor.row], false};
    }
}
void BattleMenuSession::refresh_equipment_text(const BattleState& battle) {
    prepare_battle_equipment_surface(work_, assets_.surfaces, *battle.inventory.hands[*active_]);
    start_text(BattleTextCanvas::equipment);
}
void BattleMenuSession::equipment_input(BattleState& battle, const BattleMenuContext& context) {
    hand_.reset();
    if (closing_ || held(input_.repeating, Button::down) ||
        (equipment_cursor_ == 1 && held(input_.repeating, Button::right))) {
        ++sounds_.move;
        windows_ = {WindowTask::close_equipment};
        wait(BattleMenuPhase::prepare_choices);
        list_row_ = 0; reopening_choices_ = true;
        return;
    }
    if (held(input_.repeating, Button::left) && equipment_cursor_) {
        equipment_cursor_ = 0; ++sounds_.move;
    } else if (held(input_.repeating, Button::right) && !equipment_cursor_) {
        equipment_cursor_ = 1; ++sounds_.move;
    }
    if (held(input_.pressed, Button::b)) {
        selected_hand_.reset(); selected_item_.reset(); ++sounds_.select;
    } else if (held(input_.repeating, Button::a)) {
        // C1/8DFC requests a refresh on each permitted hand confirmation,
        // including selecting a hand before an eventual swap or cancellation.
        if (!assets_.fixed_equipment[battle.actors[*active_].value_or(255)])
            battle.equipment_changed.set(*active_);
        if (assets_.fixed_equipment[battle.actors[*active_].value_or(255)]) ++sounds_.invalid;
        else if (!selected_hand_) {
            selected_hand_ = equipment_cursor_; ++sounds_.select;
        } else if (*selected_hand_ != equipment_cursor_) {
            swap_battle_hands(battle.inventory, *active_);
            battle.equipment_changed.set(*active_);
            selected_hand_.reset(); ++sounds_.select;
            refresh_equipment_text(battle);
            wait(BattleMenuPhase::equipment);
        } else {
            selected_hand_.reset();
            const auto& stack = (*battle.inventory.hands[*active_])[equipment_cursor_];
            if (!stack.item || (stack.item->usage_flags & 128U)) ++sounds_.invalid;
            else if (!assets_.items_supported[stack.item->identity]) {
                notice_ = "This item effect is not implemented yet"; ++sounds_.invalid;
            } else {
                notice_.clear(); ++sounds_.select;
                attack_ = stack.item->identity;
                item_source_ = equipment_cursor_ ? BattleItemSource::left_hand : BattleItemSource::right_hand;
                begin_target(stack.item->targeting, context);
            }
        }
    }
    if (phase_ == BattleMenuPhase::equipment)
        hand_ = BattleHandCursor{static_cast<std::uint8_t>(FF6_ROM_VALUE_BATTLE_EQUIPMENT_CURSOR_COLUMNS_0 + equipment_cursor_ * (FF6_ROM_VALUE_BATTLE_EQUIPMENT_CURSOR_COLUMNS_1 - FF6_ROM_VALUE_BATTLE_EQUIPMENT_CURSOR_COLUMNS_0)), FF6_ROM_VALUE_BATTLE_EQUIPMENT_CURSOR_Y_0, false};
}
void BattleMenuSession::input_step(BattleState& battle, const BattleMenuContext& context) {
    switch (phase_) {
    case BattleMenuPhase::idle:
        break;
    case BattleMenuPhase::wait:
        if (windows_.empty())
            phase_ = after_wait_;
        break;
    case BattleMenuPhase::open_command:
        if (!options_.cursor_memory) {
            commands_.fill(0);
            lists_.fill({});
            magic_cursors_.fill({});
            item_cursors_.fill({});
            magic_scroll_.fill(0);
            lore_cursors_.fill({});
            lore_scroll_.fill(0);
            item_scroll_.fill(0);
            throw_cursors_.fill({});
            throw_scroll_.fill(0);
            rage_cursors_.fill({});
            dance_cursors_.fill({}); dance_scroll_.fill(0);
            rage_scroll_.fill(0);
        }
        indicator_ = true;
        windows_ = {WindowTask::style, WindowTask::open_command, WindowTask::auxiliary};
        wait(BattleMenuPhase::command);
        break;
    case BattleMenuPhase::command: {
        hand_.reset();
        const auto result = navigate_battle_commands(commands_[*active_], command_list(battle),
                                                     layout_, input_.repeating, closing_);
        wait_for_menu_ = result.wait_for_menu.value_or(wait_for_menu_);
        if (result.sounds.move || result.decision != BattleMenuDecision::none)
            notice_.clear();
        add_sounds(result.sounds);
        if (result.decision == BattleMenuDecision::close) {
            phase_ = BattleMenuPhase::close_command;
        } else if (result.decision == BattleMenuDecision::next_character ||
                   result.decision == BattleMenuDecision::previous_character) {
            cycle_ = result.decision == BattleMenuDecision::next_character
                         ? BattleMenuCycle::next
                         : BattleMenuCycle::previous;
            closing_ = true;
        } else if (result.decision == BattleMenuDecision::command) {
            notice_.clear();
            draft_ = begin_battle_command(*active_, result.command->identity);
            attack_ = result.attack ? result.attack->attack : 255;
            choice_kind_ = result.command->identity == 23 ? 2 : result.command->identity;
            selected_item_.reset();
            selected_hand_.reset(); item_source_ = BattleItemSource::inventory;
            if (layout_ != BattleCommandLayout::control &&
                (choice_kind_ == 1 || choice_kind_ == 2 || choice_kind_ == 8 ||
                 choice_kind_ == 9 || choice_kind_ == 12 || choice_kind_ == 16 || choice_kind_ == 19)) {
                if (choice_kind_ == 8 || choice_kind_ == 9) {
                    tool_list_.clear();
                    for (const auto& stack : battle.inventory.items)
                        if (stack.item && (stack.item->usage_flags & (choice_kind_ == 8 ? 32U : 64U)))
                            tool_list_.push_back(stack);
                    if (choice_kind_ == 9) item_scroll_[*active_] = 0;
                }
                list_row_ = 0;
                reopening_choices_ = false;
                stage_choices(battle);
            } else if (layout_ != BattleCommandLayout::control && result.command->identity == 29) {
                list_row_ = 0;
                stage_magitek(context);
            } else if (result.command->identity == 15) {
                wait_for_menu_ = false;
                windows_ = {WindowTask::open_slot};
                wait(BattleMenuPhase::slot);
            } else if (result.command->identity == 7 && battle.skills.bushido_count) {
                bushido_charge_ = 0;
                wait_for_menu_ = false;
                windows_ = {WindowTask::open_bushido};
                wait(BattleMenuPhase::bushido);
            } else if (result.command->identity == 10) {
                wait_for_menu_ = false;
                begin_target(2, context); // Self cursor while entering the code.
                blitz_ = {};
                blitz_.previous = input_.held;
                phase_ = BattleMenuPhase::blitz;
            } else if (result.command->identity == 0 || result.command->identity == 5 ||
                       // USA Rev 1 C1/7CE6: Morph/Revert use the command's self-target
                       // descriptor, then the existing effect/animation owner.
                       result.command->identity == 3 || result.command->identity == 4 ||
                       result.command->identity == 18 || result.command->identity == 6 ||
                       result.command->identity == 14 || result.command->identity == 24 ||
                       result.command->identity == 22 || result.command->identity == 11 || result.command->identity == 26 ||
                       result.command->identity == 27 || result.command->identity == 28 ||
                       result.command->identity == 17 || result.command->identity == 13 ||
                       layout_ == BattleCommandLayout::control) {
                wait_for_menu_ = false;
                begin_target(result.targeting, context);
            } else {
                notice_ = "This command menu is not implemented yet";
                wait_for_menu_ = false;
                ++sounds_.invalid;
            }
        } else if (result.decision == BattleMenuDecision::row ||
                   result.decision == BattleMenuDecision::defend) {
            defend_choice_ = result.decision == BattleMenuDecision::defend;
            windows_ = {WindowTask::open_stance};
            wait(BattleMenuPhase::stance);
        }
        if (phase_ == BattleMenuPhase::command && !closing_) {
            const auto p =
                battle_command_cursor_position(assets_.cursors, layout_, commands_[*active_]);
            hand_ = BattleHandCursor{static_cast<std::uint8_t>(p.x), static_cast<std::uint8_t>(p.y),
                                     false};
        }
        break;
    }
    case BattleMenuPhase::esper: {
        hand_.reset();
        if (closing_ || held(input_.pressed, Button::b) ||
            held(input_.repeating, Button::down) || held(input_.repeating, Button::right)) {
            if (held(input_.repeating, Button::right)) magic_cursors_[*active_].column = 0;
            draft_ = begin_battle_command(*active_, 2);
            windows_ = {WindowTask::close_esper};
            wait(BattleMenuPhase::choices);
            ++sounds_.move;
        } else if (held(input_.repeating, Button::a)) {
            const auto& esper = battle.availability.spells[*active_].esper;
            if (!esper || esper->disabled) ++sounds_.invalid;
            else {
                draft_ = begin_battle_command(*active_, 25);
                attack_ = esper->identity;
                ++sounds_.select;
                begin_target(esper->targeting, context);
            }
        } else hand_ = BattleHandCursor{FF6_ROM_VALUE_BATTLE_ESPER_CURSOR_0 & 255, (FF6_ROM_VALUE_BATTLE_ESPER_CURSOR_0 >> 8) & 255, false};
        break;
    }
    case BattleMenuPhase::slot:
        hand_.reset();
        wait_for_menu_ = false;
        if (closing_) phase_ = BattleMenuPhase::close_slot;
        else if (slot_fade_) --slot_fade_;
        else {
            if (held(input_.repeating, Button::a)) {
                ++sounds_.select;
                if (const auto outcome = slot_.confirm(assets_.slot, context.joker_doom,
                                                       assets_.random, graphics_random_)) {
                    BattlePlayerCommand selection{*active_, 15, {*outcome, 0}, {}};
                    submit_battle_player_command(battle, selection);
                    confirmed_ = selection;
                    closing_ = true;
                    phase_ = BattleMenuPhase::close_slot;
                }
            }
            if (phase_ == BattleMenuPhase::slot && held(input_.pressed, Button::b) && slot_.can_cancel()) {
                ++sounds_.select;
                phase_ = BattleMenuPhase::close_slot;
            }
            if (phase_ == BattleMenuPhase::slot) slot_.advance(assets_.slot);
        }
        break;
    case BattleMenuPhase::close_slot:
        hand_.reset();
        if (slot_fade_ < FF6_ROM_VALUE_BATTLE_SLOT_FADE_STEPS_0) ++slot_fade_;
        else {
            windows_ = {WindowTask::close_list, WindowTask::restore_slot};
            wait(BattleMenuPhase::command);
        }
        break;
    case BattleMenuPhase::bushido:
        hand_.reset();
        wait_for_menu_ = false; // C1/7D25: charging never pauses combat time.
        if (closing_) {
            windows_ = {WindowTask::close_bushido};
            wait(BattleMenuPhase::command);
        } else if (held(input_.repeating, Button::a)) {
            BattlePlayerCommand selection{*active_, 7,
                {static_cast<std::uint8_t>(bushido_charge_ >> 5U), 0}, {}};
            submit_battle_player_command(battle, selection);
            confirmed_ = selection;
            closing_ = true;
            ++sounds_.select;
        } else if (held(input_.pressed, Button::b)) {
            windows_ = {WindowTask::close_bushido};
            wait(BattleMenuPhase::command);
            ++sounds_.select;
        } else {
            if (!(context.clock & unsigned{FF6_ROM_VALUE_BATTLE_BUSHIDO_CHARGE_MASK_0})) bushido_charge_ = wrap8(unsigned{bushido_charge_} + 1U);
            if ((bushido_charge_ >> 5U) >= battle.skills.bushido_count) bushido_charge_ = 0;
            prepare_battle_bushido_surface(work_, assets_.surfaces,
                                           battle.skills.bushido_count, bushido_charge_);
            // The original publishes these two short rows every charging frame.
            published_.text[index(BattleTextCanvas::auxiliary)] =
                work_.text[index(BattleTextCanvas::auxiliary)];
        }
        break;
    case BattleMenuPhase::stance: {
        hand_.reset();
        const bool short_form = layout_ == BattleCommandLayout::short_form;
        if (closing_) {
            windows_ = {WindowTask::close_stance};
            wait(BattleMenuPhase::command);
        } else if (held(input_.repeating, Button::a)) {
            BattlePlayerCommand selection{
                *active_, static_cast<std::uint8_t>(defend_choice_ ? 21 : 20), {255, 0}, {}};
            submit_battle_player_command(battle, selection);
            confirmed_ = selection;
            closing_ = true;
            ++sounds_.select;
        } else {
            const auto shoulders = input_.held & (button(Button::l) | button(Button::r));
            const bool cancel =
                short_form
                    ? shoulders != button(defend_choice_ ? Button::r : Button::l)
                    : held(input_.pressed, Button::b) ||
                          input_.pressed == button(defend_choice_ ? Button::left : Button::right);
            if (cancel) {
                ++sounds_.select;
                windows_ = {WindowTask::close_stance};
                wait(BattleMenuPhase::command);
            } else {
                const auto y =
                    short_form
                        ? FF6_ROM_VALUE_BATTLE_STANCE_SHORT_CURSOR_Y_0
                        : assets_.cursors
                              .commands[index(BattleCommandLayout::window)][commands_[*active_]]
                              .y;
                hand_ = BattleHandCursor{static_cast<std::uint8_t>(short_form
                                                                       ? (defend_choice_ ? FF6_ROM_VALUE_BATTLE_DEF_SHORT_CURSOR_X_0 : FF6_ROM_VALUE_BATTLE_ROW_SHORT_CURSOR_X_0)
                                                                       : (defend_choice_ ? FF6_ROM_VALUE_BATTLE_DEF_WINDOW_CURSOR_X_0 : FF6_ROM_VALUE_BATTLE_ROW_WINDOW_CURSOR_X_0)),
                                         static_cast<std::uint8_t>(y), false};
            }
        }
        break;
    }
    case BattleMenuPhase::prepare_choices:
        stage_choices(battle);
        break;
    case BattleMenuPhase::choices:
        choice_input(battle, context);
        break;
    case BattleMenuPhase::equipment:
        equipment_input(battle, context);
        break;
    case BattleMenuPhase::prepare_magitek:
        stage_magitek(context);
        break;
    case BattleMenuPhase::magitek: {
        hand_.reset();
        const auto result = navigate_battle_magitek(lists_[*active_], assets_.surfaces.magitek,
                                                    context.graphics[*active_], input_.repeating,
                                                    input_.pressed, closing_);
        add_sounds(result.sounds);
        if (result.decision == BattleMenuDecision::close)
            phase_ = BattleMenuPhase::close_list;
        else if (result.decision == BattleMenuDecision::cancel) {
            windows_ = {WindowTask::close_list};
            wait(BattleMenuPhase::command);
        } else if (result.decision == BattleMenuDecision::attack) {
            attack_ = result.attack->attack;
            begin_target(result.targeting, context);
        } else {
            const auto p = battle_magitek_cursor_position(assets_.cursors, lists_[*active_]);
            hand_ = BattleHandCursor{static_cast<std::uint8_t>(p.x), static_cast<std::uint8_t>(p.y),
                                     false};
        }
        break;
    }
    case BattleMenuPhase::blitz:
        hand_.reset();
        if (closing_ || held(input_.pressed, Button::b)) {
            cancel_battle_command_target(*draft_, targets_);
            phase_ = BattleMenuPhase::command;
            ++sounds_.select;
        } else {
            blitz_.sample(input_.held);
            if (held(input_.pressed, Button::a)) {
                const auto result = confirm_battle_command(battle, *draft_, targets_,
                                                           blitz_.match(assets_.blitz), {});
                require(result == BattleConfirmation::submitted, "Blitz command was not submitted");
                confirmed_ = draft_->selection;
                closing_ = true;
                phase_ = BattleMenuPhase::command;
                ++sounds_.select;
            }
        }
        break;
    case BattleMenuPhase::target: {
        const auto result = navigate_battle_targets(
            targets_, {input_.repeating, input_.pressed, context.clock, closing_},
            battle.entry.approach, context.positions, context.eligible, assets_.random,
            graphics_random_);
        add_sounds(result.sounds);
        if (result.decision == BattleTargetDecision::cancel) {
            cancel_battle_command_target(*draft_, targets_);
            phase_ = target_return_;
        } else if (result.decision == BattleTargetDecision::confirm) {
            const auto confirmed = confirm_battle_command(battle, *draft_, targets_, attack_,
                                                           {context.enemy_character_proxy, item_source_});
            if (confirmed == BattleConfirmation::submitted) {
                confirmed_ = draft_->selection;
                closing_ = true;
            }
            phase_ = target_return_; // X-Magic returns to the same list for cast two.
        }
        break;
    }
    case BattleMenuPhase::close_list:
        windows_ = {WindowTask::close_list};
        wait(BattleMenuPhase::command);
        break;
    case BattleMenuPhase::close_command:
        indicator_ = false;
        windows_ = {WindowTask::close_command, WindowTask::reset_style};
        wait(BattleMenuPhase::finish);
        break;
    case BattleMenuPhase::finish:
        closing_ = false;
        windows_ = {WindowTask::finish};
        wait(BattleMenuPhase::idle);
        break;
    }
}
std::vector<MusicPlayerCommand> BattleMenuSession::advance(BattleState& battle,
                                                           const BattleMenuContext& context,
                                                           Buttons first, Buttons second) {
    for (const auto request : battle.menu_requests)
        request_battle_menu(order_, request, !battle.menus_disabled);
    battle.menu_requests.clear();
    publish(context.clock);
    if (active_ && !order_.ranks[*active_])
        closing_ = true;
    // DrawCursorSprites sees the previous menu decision, before UpdateMenuInput.
    BattleCursorRequest cursor;
    cursor.menu = hand_;
    if (indicator_ && !context.hide_character_indicator)
        cursor.active_character = active_;
    cursor.targets = targets_;
    cursor.visible = context.visible;
    cursor.clock = context.clock;
    cursor.magitek = context.magitek;
    cursors_.prepared =
        prepare_battle_cursors(cursor, context.anchors, context.positions, context.character_view,
                               context.poses, context.shown_status);
    for (unsigned i = 0; i < (fast_window_ ? 3U : 1U); ++i)
        advance_battle_menu_transition(transition_, text_rows_left_ != 0);
    input_step(battle, context);
    // UpdateSfx services one class in this priority order, retaining other flags.
    std::vector<MusicPlayerCommand> audio;
    if (active_sound_) {
        const bool second_player = options_.multiplayer && active_ &&
                                   (options_.second_player_characters & (1U << *active_));
        audio.push_back(SystemSoundEffect{static_cast<std::uint8_t>(second_player ? FF6_ROM_VALUE_BATTLE_MENU_ACTIVE_CUE_0 + 1 : FF6_ROM_VALUE_BATTLE_MENU_ACTIVE_CUE_0)});
        active_sound_ = false;
    } else if (sounds_.move) {
        audio.push_back(SystemSoundEffect{FF6_ROM_VALUE_BATTLE_MENU_ACTIVE_CUE_0 - FF6_ROM_VALUE_BATTLE_MENU_MOVE_CUE_DIFFERENCE_0});
        sounds_.move = 0;
    } else if (sounds_.invalid) {
        audio.push_back(SystemSoundEffect{FF6_ROM_VALUE_BATTLE_MENU_ERROR_CUE_0});
        sounds_.invalid = 0;
    } else if (sounds_.select) {
        audio.push_back(SystemSoundEffect{FF6_ROM_VALUE_BATTLE_MENU_CONFIRM_CUE_0});
        sounds_.select = 0;
    }
    sample_input(input_, input_settings_, InputContext::battle, first, second,
                 {options_.multiplayer, options_.second_player_characters, active_.value_or(0)});
    if (active_ && (choice_kind_ == 2 || choice_kind_ == 12)) {
        const auto cursor = list_cursor();
        const auto& spell = choice_kind_ == 12
                            ? battle.availability.spells[*active_].lores.at(list_scroll() + cursor.row)
                            : phase_ == BattleMenuPhase::esper ||
                            (phase_ == BattleMenuPhase::target && target_return_ == BattleMenuPhase::esper)
                            ? battle.availability.spells[*active_].esper
                            : battle.availability.spells[*active_].magic.at(
                                (magic_scroll_[*active_] + cursor.row) * 2U + cursor.column);
        prepare_battle_mp(work_, assets_.surfaces, battle.stats[*active_].mp,
                          spell ? spell->mp_cost : 0);
    }
    if (active_ && phase_ == BattleMenuPhase::command && windows_.empty() && !text_rows_left_ &&
        layout_ != BattleCommandLayout::control && battle.availability.changed_commands[*active_]) {
        // Equipment/status changes update capability shading as well as input
        // admission. Repaint when the command canvas is visible again; don't
        // overwrite an Item/equipment/foreground text publication in flight.
        prepare_battle_command_surface(work_, assets_.surfaces, command_list(battle), layout_,
                                       context.text_rows[*active_].value());
        start_text(BattleTextCanvas::command);
        battle.availability.changed_commands.reset(*active_);
    }
    window_step(battle, context);
    const auto ready =
        ready_battle_menu(order_, !battle.menus_disabled && !active_ && windows_.empty() &&
                                      !text_rows_left_ && !confirmed_);
    if (ready) {
        active_ = ready;
        start_text(BattleTextCanvas::readout);
        wait(BattleMenuPhase::open_command);
    }
    return audio;
}
std::vector<TileLayer> BattleMenuSession::layers() const {
    auto out = make_battle_menu_layers(published_, visible_transition_);
    auto reels = make_battle_slot_layers(assets_.slot, assets_.surfaces.slot_tiles,
        *published_.windows[index(BattleWindowCanvas::slot)], slot_, visible_transition_,
        std::min(slot_fade_ * static_cast<unsigned>(FF6_ROM_VALUE_BATTLE_SLOT_FADE_STEP_0), 31U));
    out.insert(out.end(), std::make_move_iterator(reels.begin()), std::make_move_iterator(reels.end()));
    return out;
}
BattleCursorSprites BattleMenuSession::cursor_sprites() const {
    auto result = make_battle_cursor_sprites(cursors_, assets_.cursors);
    if (selected_hand_ && phase_ == BattleMenuPhase::equipment) {
        auto extra = cursors_;
        extra.visible = {};
        extra.visible.menu = BattleHandCursor{static_cast<std::uint8_t>(FF6_ROM_VALUE_BATTLE_EQUIPMENT_CURSOR_COLUMNS_0 + *selected_hand_ * (FF6_ROM_VALUE_BATTLE_EQUIPMENT_CURSOR_COLUMNS_1 - FF6_ROM_VALUE_BATTLE_EQUIPMENT_CURSOR_COLUMNS_0) + FF6_ROM_VALUE_BATTLE_SELECTED_CURSOR_OFFSET_0), FF6_ROM_VALUE_BATTLE_EQUIPMENT_CURSOR_Y_0, false};
        const auto cursor = make_battle_cursor_sprites(extra, assets_.cursors);
        result.after_actors.insert(result.after_actors.begin(), cursor.after_actors.begin(), cursor.after_actors.end());
    }
    if (selected_item_ && phase_ == BattleMenuPhase::choices && *selected_item_ >= list_scroll() &&
        *selected_item_ < list_scroll() + 4) {
        auto extra = cursors_;
        extra.visible = {};
        extra.visible.menu =
            BattleHandCursor{static_cast<std::uint8_t>(assets_.surfaces.item_column + FF6_ROM_VALUE_BATTLE_SELECTED_CURSOR_OFFSET_0),
                             assets_.cursors.list_rows[*selected_item_ - list_scroll()], false};
        const auto cursor = make_battle_cursor_sprites(extra, assets_.cursors);
        result.after_actors.insert(result.after_actors.begin(), cursor.after_actors.begin(),
                                   cursor.after_actors.end());
    }
    return result;
}
void BattleMenuSession::publish_readout_region(const TileLayer& fresh,
                                                BattleReadoutRectangle region) {
    const auto copy = [&](TileLayer& text) {
        for (unsigned y = 0; y < region.height && region.y + y < 8; ++y)
            for (unsigned x = 0; x < region.width; ++x) {
                const auto at = (region.y + y) * 32 + region.x + x;
                text.cells.at(at) = fresh.cells.at(at);
            }
    };
    for (auto* surfaces : {&work_, &published_}) {
        // Command preparation starts from this retained canvas. Refreshing only
        // the visible copies makes the next menu restore old battle-entry HP.
        copy(surfaces->command_work);
        for (auto canvas : {BattleTextCanvas::readout, BattleTextCanvas::command})
            if (auto& text = surfaces->text[index(canvas)])
                copy(*text);
        // Auxiliary text is already normalized past the scratch buffer's
        // leading row, just like the command and closed-menu canvases.
        if (auto& text = surfaces->text[index(BattleTextCanvas::auxiliary)])
            copy(*text);
    }
}
void BattleMenuSession::update_counters(const BattleReadoutView& readout) {
    // Counter publication has its own retained row cadence. Command glyphs
    // and selected-name colors must survive its independent updates.
    const auto fresh = make_battle_readout_layers(assets_.surfaces.readout, readout);
    publish_readout_region(fresh.text, assets_.surfaces.readout.text_regions[3]);
}
void BattleMenuSession::publish_action_readout(const BattleReadoutView& readout) {
    const auto fresh = make_battle_readout_layers(assets_.surfaces.readout, readout);
    // TfrCharHPText updates the HP rectangle in both closed and open menus.
    // An active command/list selection must never discard a resolved outcome.
    // Pending text strips read the refreshed working copy as they publish.
    publish_readout_region(fresh.text, assets_.surfaces.readout.text_regions[2]);
    if (!active_ && windows_.empty() && !text_rows_left_)
        for (auto* surfaces : {&work_, &published_})
            surfaces->text[index(BattleTextCanvas::readout)] = fresh.text;
}
void BattleMenuSession::close_for_victory() {
    if (active_)
        closing_ = true;
    confirmed_.reset();
    hand_.reset();
    indicator_ = false;
}
void BattleMenuPreview::begin_victory(BattleRewards rewards) {
    require(victory_ready_ && !rewards_, "Victory handoff is not ready or already consumed");
    if (rewards.cursed_shield_battles)
        queue_context_.ai.variables[16] = *rewards.cursed_shield_battles;
    rewards_ = std::move(rewards);
    menus_.close_for_victory();
}
void BattleMenuSession::acknowledge_command() {
    require(complete(), "Command acknowledgement requires completed menu closure");
    confirmed_.reset();
    draft_.reset();
}
void BattleMenuPreview::connect_combat(const BattleFormationEntry& formation,
                                       const BattlePreparation& preparation,
                                       const std::array<std::uint8_t, 20>& global_variables,
                                       bool continue_field_music, bool gau_recruited) {
    joker_doom_ = formation.formation.joker_doom;
    for (unsigned i = 0; i < 6; ++i)
        background_monsters_[i] = formation.formation.monsters[i].background_one;

    require(!action_result_, "Cannot connect combat during an action");
    live_ = true;
    continue_field_music_ = continue_field_music;
    formation_continues_music_ = formation.formation.continue_music;
    victory_animation_ = formation.formation.victory_music;
    victory_music_ = battle_victory_music(formation.formation.victory_music,
                                        formation.formation.continue_music, continue_field_music);
    queue_context_.ai.battle_identity = formation.selection.battle_identity;
    queue_context_.ai.gau_recruited = gau_recruited;
    // InitBattle/EndBattle retain AI variables 4..23 across encounters.
    // Only the transient ending/time flags are cleared on entry (C2/2435).
    std::copy(global_variables.begin(), global_variables.end(), queue_context_.ai.variables.begin() + 4);
    queue_context_.ai.variables[11] &= 0x7f;
    queue_context_.ai.variables[12] &= 0x6e;
    for (unsigned i = 0; i < 6; ++i)
        if (const auto& monster = preparation.monsters.slots[i]) {
            execution_context_.monster_identities[i] = monster->identity;
            special_animations_[i] = monster->special_animation;
            queue_context_.ai.allocated[i + 4] = true;
        }
    for (unsigned i = 0; i < 4; ++i) {
        queue_context_.ai.allocated[i] = seed_.rules.shown_characters[i];
        if (const auto& character = preparation.characters.slots[i])
            physical_environment_.block_graphics[i] = character->combat.block_graphics[0];
    }
}
void BattleMenuPreview::replace_monsters(const BattleEntrance& entrance,
                                          const BattleFormationEntry& formation,
                                          const BattlePreparation& preparation, BattleState& battle) {
    joker_doom_ = formation.formation.joker_doom;
    require(battle_change_.has_value(), "No pending battle change");
    // Gfx command 12 replaces monster resources and placement, not the party's
    // poses, menus, retained action queues, AI variables or battlefield image.
    const auto& assets = entrance.scene_assets();
    const auto& seed = entrance.scene_seed();
    assets_.actors.monsters = assets.actors.monsters;
    assets_.actors.monster_graphics_selection = assets.actors.monster_graphics_selection;
    assets_.actors.monster_pieces = assets.actors.monster_pieces;
    seed_.actors.monsters = seed.actors.monsters;
    seed_.actors.monster_colors = seed.actors.monster_colors;
    seed_.actors.monster_styles = seed.actors.monster_styles;
    seed_.rules.shown_monsters = seed.rules.shown_monsters;
    seed_.actors.animation.monster_visible.fill(true);
    seed_.actors.animation.monster_entry_visible.fill(true);
    for (unsigned i = 0; i < 6; ++i) {
        background_monsters_[i] = formation.formation.monsters[i].background_one;
        execution_context_.monster_identities[i].reset();
        queue_context_.ai.allocated[i + 4] = preparation.monsters.slots[i].has_value();
        if (const auto& monster = preparation.monsters.slots[i]) {
            execution_context_.monster_identities[i] = monster->identity;
            special_animations_[i] = monster->special_animation;
        }
    }
    victory_animation_ = formation.formation.victory_music;
    formation_continues_music_ = formation.formation.continue_music;
    battle.monsters_on_left = seed.actors.monsters.groups.monsters_on_left;
    victory_music_ = battle_victory_music(formation.formation.victory_music,
                                        formation.formation.continue_music, continue_field_music_);
    battle.monsters_on_right = seed.actors.monsters.groups.monsters_on_right;
    pending_hide_.reset(); pending_show_.reset(); death_.reset();
    finish_selected_action(battle);
    battle_change_.reset();
}
std::array<std::uint8_t, 20> BattleMenuPreview::global_script_variables() const {
    std::array<std::uint8_t, 20> out{};
    std::copy_n(queue_context_.ai.variables.begin() + 4, out.size(), out.begin());
    return out;
}
std::uint8_t BattleMenuPreview::cursed_shield_battles() const {
    // LearnItemMagic shares global AI variable 16; persistence stores 4..23.
    return queue_context_.ai.variables[16];
}
BattleMenuPreview::BattleMenuPreview(const ContentPack& pack, const BattleEntrance& entrance,
                                     GameMenuState options, InputState input)
    : assets_(entrance.scene_assets()), seed_(entrance.scene_seed()), gauges_(entrance.gauges()),
      menus_(load_battle_menu_session_assets(pack), seed_.readout, options, input),
      magitek_(load_battle_magitek(pack)), magic_(load_battle_magic(pack)),
      dances_(load_battle_dance(pack)),
      rage_monsters_(load_monster_stats(pack)),
      items_(load_battle_item_effects(pack)),
      event_assets_(std::make_shared<BattleEventAssets>(load_battle_event_assets(pack))),
      physical_(load_battle_physical(pack)), equipment_party_(load_party_data(pack)),
      equipment_data_(load_equipment_data(pack)), execution_data_(load_battle_execution(pack)),
      commands_(load_battle_commands(pack)),
      effects_(assets_.effects ? assets_.effects
                               : std::make_shared<BattleEffectLibrary>(load_battle_effects(pack))),
      number_assets_(load_battle_damage_number_assets(pack)),
      death_palette_(load_battle_monster_death_palette(pack)) {
    for (unsigned i = 0; i < dance_backgrounds_.size(); ++i) {
        dance_backgrounds_[i] = load_battle_background(pack, dances_.backgrounds[i]);
        initialize_battle_background_motion(pack, dance_backgrounds_[i]);
    }
    const auto programs = pack.get("battle_monster_transition_programs");
    for (unsigned side = 0; side < 2; ++side)
        for (unsigned i = 0; i < 18; ++i) {
            const auto pointer = read16(programs, side * 36 + i * 2);
            require(pointer >= 0xe4eb && pointer - 0xe4ebU < programs.size(),
                    "Invalid monster transition pointer");
            const auto at = pointer - 0xe4ebU;
            if (programs[at] == 255)
                continue;
            require(at + 6 <= programs.size() && programs[at] == 0 &&
                        programs[at + 1] == (side ? 4 : 3) && programs[at + 4] == 1 &&
                        programs[at + 5] == 255 && read16(programs, at + 2) % 14 == 0,
                    "Unsupported monster transition program");
            transitions_[side][i] = read16(programs, at + 2) / 14;
        }
    require(entrance.complete(), "Battle menu preview requires a completed entrance");
    message_speed_ = options.message_speed;
    victory_assets_ = std::make_shared<BattleVictoryAssets>(load_battle_victory_assets(pack));
}
void BattleMenuPreview::open_message(std::optional<BattleMessagePresentation>& destination,
                                      BattleMessage message) {
    destination.emplace(std::move(message), message_speed_);
    message_window_uses_ = wrap8(unsigned{message_window_uses_} + 1U);
}
BattleEffectContext BattleMenuPreview::effect_context(const BattleState& battle) const {
    BattleEffectContext context;
    context.display = display_;
    context.message_window_uses = message_window_uses_;
    context.battle_type = static_cast<std::uint8_t>(battle.entry.approach);
    context.random_index = menus_.graphics_random_index();
    context.anchors = seed_.actors.anchors;
    context.characters = seed_.actors.characters.layout.poses;
    context.character_back_rows = seed_.actors.characters.layout.back_row;
    context.character_hand_swaps = seed_.actors.characters.layout.swap_hands;
    for (unsigned i = 0; i < 4; ++i)
        context.character_row_distances[i] = static_cast<std::int16_t>(signed16(
            assets_.actors.view.row_offsets.at(context.battle_type).at(i)));
    context.character_identities = battle.actors;
    for (unsigned i = 0; i < 4; ++i)
        context.shown_character_status[i] = seed_.actors.characters.control[i].shown_status;
    context.monsters = seed_.actors.monsters.positions;
    context.monster_facing = seed_.rules.retained_monster_facing;
    for (unsigned i = 0; i < 6; ++i)
        context.monster_facing[i] = battle.status.targets[i + 4].reversed_facing ? 1 : 0;
    context.background_monsters = background_monsters_;
    context.control = seed_.actors.animation;
    context.draw_order = seed_.actors.order.staged;
    context.monster_styles = seed_.actors.monster_styles;
    context.monster_images = std::make_shared<BattleMonsterBackgroundSource>(
        make_battle_monster_background_source(assets_.actors, seed_.actors.publication));
    context.published_character_frames = seed_.actors.publication.character_frames;
    context.published_status_tiles = seed_.actors.publication.status_tiles;
    // C1/03E2 keeps the train/dream terrain's authored scroll selection.
    if (assets_.background.identity == 15 || assets_.background.identity == 44)
        context.scroll_programs[1] = FF6_ROM_VALUE_BATTLE_EFFECT_TRAIN_SCROLL_0;
    else if (assets_.background.identity == 46)
        context.scroll_programs[1] = FF6_ROM_VALUE_BATTLE_EFFECT_DREAM_SCROLL_0;
    for (unsigned i = 0; i < 6; ++i) {
        context.shown_monsters[i] = seed_.rules.shown_monsters[i];
        context.visible_monsters[i] =
            (battle.status.targets[i + 4].active & status_mask(BattleStatus::vanish)) == 0;
    }
    context.magitek = seed_.rules.magitek;
    context.character_colors = battle_character_colors(seed_.actors, assets_.actors);
    context.monster_colors = battle_monster_colors(seed_.actors, assets_.actors);
    context.battlefield_colors.assign(assets_.background.colors.begin(),
                                      assets_.background.colors.end());
    context.freeze_character_palettes = seed_.rules.freeze_palettes;
    context.hide_characters = seed_.rules.hide_characters;
    context.hide_character_indicator = hide_character_indicator_;
    for (unsigned i = 0; i < 6; ++i) {
        context.monster_widths[i] = assets_.actors.monsters.dimensions[i].width;
        context.monster_heights[i] = assets_.actors.monsters.dimensions[i].height;
    }
    return context;
}
void BattleMenuPreview::advance_selected_action(BattleState& battle, const BattleParty& party) {
    if (status_animation_) {
        if (animation_active())
            return;
        animation_.reset();
        status_animation_ = false;
        for (unsigned i = 0; i < 6; ++i)
            if (jumping_monsters_[i]) seed_.rules.shown_monsters[i] = false;
        jumping_monsters_.reset();
        for (unsigned i = 0; i < 4; ++i)
            if (jumping_characters_[i]) seed_.rules.shown_characters[i] = false;
        jumping_characters_.reset();
        if (departing_characters_.any()) {
            for (unsigned i = 0; i < 4; ++i)
                if (departing_characters_[i]) seed_.rules.shown_characters[i] = false;
            departing_characters_.reset();
            update_battle_participation(battle);
            if (!battle.participation.ally_count && (!battle.end_event || battle.end_event == 2)) {
                record_battle_conclusion(queue_context_, BattleConclusion::escaped);
                battle.end_event = 2;
                escape_ready_ = true;
                menus_.close_for_victory();
                return;
            }
        }
    }
    if (event_) {
        if (!event_->complete())
            return;
        display_ = event_->display();
        message_window_uses_ = event_->message_window_uses();
        battle.menus_disabled = event_->menus_disabled();
        event_.reset();
        // C2/147A admits arrivals before AfterAction2; C2/144F retires leavers
        // after it. Their on-screen movement belongs to the event scripts.
        for (unsigned i = 0; i < 4; ++i)
            if (event_arrivals_[i]) {
                auto& status = battle.status.targets[i];
                status.present = true;
                status.active &=
                    ~(status_mask(BattleStatus::dead) | status_mask(BattleStatus::petrify) |
                      status_mask(BattleStatus::imp) | status_mask(BattleStatus::zombie) |
                      status_mask(BattleStatus::hide));
                battle.arrival_override.reset(i);
            }
        if (!immediate_event_)
            finish_selected_action(battle);
        else {
            // Immediate events also pass through AfterAction2. Newly admitted
            // characters must leave their hidden/absent action-blocked state
            // before their ATB can advance (including Gau's own return turn).
            refresh_battle_actions(battle, commands_, magitek_.random, battle.stats[0].level);
        }
        for (unsigned i = 0; i < 4; ++i)
            if (event_departures_[i]) {
                battle.status.targets[i].present = false;
                battle.status.targets[i].active |= status_mask(BattleStatus::hide);
                battle.untargetable.reset(i);
                release_battle_target_links(battle, static_cast<std::uint8_t>(i));
            }
        event_arrivals_.reset();
        event_departures_.reset();
        update_battle_participation(battle);
        update_battle_action_readouts(battle);
        for (unsigned i = 0; i < 4; ++i)
            seed_.actors.characters.control[i].active_status = battle.character_display[i].status;
        update_battle_character_status(assets_.actors.graphics, {seed_.rules.magitek, false},
                                       seed_.actors.characters);
        immediate_event_ = false;
        action_result_.reset();
    }
    if (message_) {
        if (!message_->complete())
            return;
        message_.reset();
    }
    if (attack_name_) {
        if (!attack_name_->complete()) return;
        attack_name_.reset();
    }
    if (defeat_) {
        // LoseBattle closes command windows, waits the ROM-selected duration, then
        // shows the authored defeat message before returning to the field.
        // The caller's event script decides whether this means Game Over.
        if (menus_.phase() != BattleMenuPhase::idle || menus_.active_character()) return;
        if (defeat_waits_) { --defeat_waits_; return; }
        if (!defeat_message_started_) {
            if (!battle.colosseum)
                open_message(message_, format_battle_message(victory_assets_->messages,
                    battle.end_event == 6 ? FF6_ROM_VALUE_BATTLE_CONTINUATION_BANON_MESSAGE_0 : FF6_ROM_VALUE_BATTLE_CONTINUATION_LOSS_MESSAGE_0, {}, party));
            defeat_message_started_ = true;
        }
        return;
    }
    if (action_result_) {
        if (!animation_active())
            ++result_frames_;
        // Repeated hits are queued graphics commands, not separate turns.
        // C1/A4B3 dispatches single numerals in three waits and returns while
        // their independent 64-picture bounce is still visible.
        const bool next_hit_ready = repeated_hit_ + 1 < repeated_count_ &&
                                    animation_ && animation_->complete();
        if (!live_ || (!next_hit_ready && !action_complete()))
            return;
        if (action_result_->rematerialize.any()) {
            // Doom's undead branch queues restoration after the spell; it must
            // not be mistaken for victory when the last visible monster dies.
            auto context = effect_context(battle);
            context.targets.reset();
            context.hits.reset();
            for (unsigned i = 0; i < 6; ++i)
                if (action_result_->rematerialize[i]) {
                    const auto slot = i + 4;
                    battle.stats[slot].hp.current = battle.stats[slot].hp.maximum;
                    auto& status = battle.status.targets[slot];
                    status.active &=
                        ~(status_mask(BattleStatus::dead) | status_mask(BattleStatus::hide));
                    status.monster_alive = status.present = true;
                    status.monster_dead_or_escaped = false;
                    battle.participation.active.set(slot);
                    battle.untargetable.reset(slot);
                    seed_.rules.shown_monsters[i] = true;
                    context.attacker = static_cast<std::uint8_t>(slot);
                    context.targets.set(slot);
                    context.hits.set(slot);
                    context.shown_monsters.set(i);
                    context.control.monster_visible[i] = true;
                    context.control.monster_entry_visible[i] = false;
                }
            update_battle_participation(battle);
            update_battle_action_readouts(battle);
            refresh_battle_actions(battle, commands_, magitek_.random,
                                   battle.stats[context.attacker].level);
            animation_.emplace(
                effects_,
                BattleActionAnimationRequest{BattleActionAnimationKind::monster_effect, FF6_ROM_VALUE_BATTLE_CONTINUATION_MATERIALIZE_0, true},
                context);
            action_result_->rematerialize.reset();
            numbers_.reset();
            numbers_started_ = false;
            death_.reset();
            outcome_waits_.reset();
            result_frames_ = 0;
            return;
        }
        if (repeated_hit_ + 1 < repeated_count_) {
            const auto slot = *execution_context_.executing;
            auto command = *executing_command_;
            if (animation_) repeated_context_ = animation_->context();
            // Each weapon follow-up is still part of the same turn. Preserve
            // messages from earlier strikes (notably theft and weapon break).
            pending_messages_.insert(pending_messages_.end(), action_result_->messages.begin(),
                                     action_result_->messages.end());
            if (sketch_ && !sketch_->executing) {
                sketch_->executing = true;
                command = sketch_->command;
            }
            if (item_continuation_) command = *item_continuation_;
            if (!weapon_sequence_.empty()) {
                weapon_sequence_.pop_front();
                while (weapon_sequence_.size() > 1 && !weapon_sequence_.front().spell &&
                       !battle.stats[slot].hands[weapon_sequence_.front().hand].attack) {
                    weapon_sequence_.pop_front();
                    --repeated_count_;
                }
                require(!weapon_sequence_.empty(), "Missing weapon continuation");
                command = weapon_sequence_.front().command;
            }
            ++repeated_hit_;
            action_result_.reset();
            animation_.reset();
            numbers_started_ = false;
            death_.reset();
            outcome_waits_.reset();
            result_frames_ = 0;
            std::bitset<6> shown;
            for (unsigned i = 0; i < 6; ++i) shown[i] = seed_.rules.shown_monsters[i];
            queue_context_.targets.selectable = battle_target_admission(battle, shown);
            execute_selected_attack(battle, party, slot,
                                    {BattleExecutionDisposition::execute, command});
            return;
        }
        pending_messages_.insert(pending_messages_.end(), action_result_->messages.begin(),
                                 action_result_->messages.end());
        ++completed_actions_;
        action_result_.reset();
        animation_.reset();
        numbers_.reset();
        numbers_started_ = false;
        death_.reset();
        outcome_waits_.reset();
        pending_attacker_.reset();
        result_frames_ = 0;
    }
    if (leap_pending_) {
        leap_complete_ = true;
        menus_.close_for_victory();
        return;
    }
    // CheckBattleEnd is a loop boundary, not a one-shot consequence of a
    // damage result. Death queues pose restoration; checking only before that
    // immediate queue drained stranded a mutual KO after a death counter.
    if (live_) {
        // C2/006F refreshes escape restrictions after every death response,
        // never during its animation. CheckFinalBattle then owns a cleared tier.
        publish_battle_loop_display(battle, queue_context_);
        const auto conclusion = battle_conclusion_at_rest(battle, queue_context_);
        record_battle_conclusion(queue_context_, conclusion);
        switch (conclusion) {
        case BattleConclusion::interrupted:
        case BattleConclusion::engulfed:
            battle.end_event = 8; // Common immediate return, no victory rewards.
            menus_.close_for_victory();
            return;
        case BattleConclusion::escaped:
            battle.end_event = 2;
            escape_ready_ = true;
            menus_.close_for_victory();
            return;
        case BattleConclusion::defeated:
            defeat_ = true;
            battle.menus_disabled = true;
            menus_.close_for_victory();
            return;
        case BattleConclusion::victorious:
            if (queue_veldt_gau_appearance(battle, queue_context_, magitek_.random)) break;
            if (queue_shadow_departure(battle, queue_context_, magitek_.random)) {
                // C2/48BD and C1/0117 suppress both poses and fanfare. The
                // normal reward display and field-song return still run.
                victory_animation_ = false;
                victory_music_ = battle_victory_music(false, formation_continues_music_,
                                                      continue_field_music_);
                break;
            }
            victory_ready_ = true;
            return;
        case BattleConclusion::ongoing:
            break;
        }
    }
    if (!pending_messages_.empty()) {
        const auto next = pending_messages_.front();
        pending_messages_.erase(pending_messages_.begin());
        open_message(message_,
            format_battle_message(victory_assets_->messages, next.identity, next.values, party));
        return;
    }
    if (victory_ready_ || escape_ready_)
        return;
    std::bitset<6> visible_targets;
    for (unsigned i = 0; i < 6; ++i)
        visible_targets[i] = seed_.rules.shown_monsters[i];
    std::uint8_t slot = 0;
    bool counter = false;
    if (live_) {
        if (menus_.complete())
            menus_.acknowledge_command();
        queue_context_.targets.selectable = battle_target_admission(battle, visible_targets);
        queue_context_.ai.selectable = queue_context_.targets.selectable;
        const auto work = take_battle_queue_work(battle, queue_context_);
        if (work.kind == BattleQueueWorkKind::idle)
            return;
        if (work.kind == BattleQueueWorkKind::immediate) {
            const auto* doom = std::get_if<BattleDoomAction>(&battle.immediate_actions.front());
            const auto* relic = std::get_if<BattleRelicSpellAction>(&battle.immediate_actions.front());
            const auto* raise = std::get_if<BattleRaiseAction>(&battle.immediate_actions.front());
            const auto* visibility = std::get_if<BattleMonsterVisibilityAction>(&battle.immediate_actions.front());
            if (doom || relic || raise || visibility) {
                std::uint8_t target = 0;
                BattleQueuedCommand command;
                if (visibility) {
                    // C2/07C8 queues the same monster transition command used
                    // by AI. Immediate ownership preserves normal turns/ATB;
                    // the shared command owns visibility, retained HP and fade.
                    require(visibility->monsters.any(), "Empty immediate monster transition");
                    target = 4;
                    while (!visibility->monsters[target - 4U]) ++target;
                    command = {36, static_cast<std::uint8_t>(visibility->animation == BattleMonsterAnimation::instant ? 0 : 8),
                        static_cast<std::uint16_t>((visibility->monsters.to_ulong() << 8U) |
                            (visibility->effect == BattleMonsterVisibility::hide_keep_battle ? 4U : 2U)), 0};
                } else {
                    // NearFatalEffect selects each automatic spell from its
                    // ROM operand; the native enum only identifies the trigger.
                    constexpr std::array<std::uint8_t, 3> relic_spells{
#include "rom-materials/near_fatal_relic_spells.inc"
                    };
                    // ReraiseEffect C2/0799 queues ordinary Raise through the
                    // same casterless owner as Doom and relic spells. Keep the
                    // death visible until this free self-cast is presented;
                    // pending revival must finish before victory/loss checks.
                    target = raise ? raise->target : doom ? doom->target : relic->target;
                    const auto spell = raise ? std::uint8_t{FF6_ROM_VALUE_BATTLE_CONTINUATION_RAISE_0} : doom ? std::uint8_t{FF6_ROM_VALUE_BATTLE_CONTINUATION_DOOM_0}
                        : relic_spells.at(static_cast<unsigned>(relic->spell));
                    command = {38, spell, 0, 0};
                }
                battle.immediate_actions.erase(battle.immediate_actions.begin());
                // C2/4E91 gives immediate actions their own queue ownership.
                // Preserve any input already selected while presenting its
                // original spell body and updating the shared status effects.
                repeated_hit_ = 0;
                repeated_count_ = 1;
                repeated_context_.reset();
                weapon_sequence_.clear();
                sketch_.reset();
                item_continuation_.reset();
                immediate_attack_ = true;
                execute_selected_attack(battle, party, target,
                    {BattleExecutionDisposition::execute, command});
                return;
            }
            if (std::holds_alternative<BattleEscapeAction>(battle.immediate_actions.front())) {
                battle.immediate_actions.erase(battle.immediate_actions.begin());
                const auto result = consume_battle_escape(battle, escape_held());
                if (result.forbidden)
                    open_message(message_, format_battle_message(victory_assets_->messages, FF6_ROM_VALUE_BATTLE_CONTINUATION_ESCAPE_MESSAGE_0, {}, party));
                if (result.departing.any()) {
                    auto context = effect_context(battle);
                    context.targets.reset();
                    context.needs_step_back.fill(false);
                    for (unsigned i = 4; i-- > 0;) if (result.departing[i]) {
                        context.attacker = static_cast<std::uint8_t>(i);
                        context.targets[i] = true;
                    }
                    context.hits = context.targets;
                    animation_.emplace(effects_, BattleActionAnimationRequest{
                        BattleActionAnimationKind::escape, FF6_ROM_VALUE_BATTLE_PRESENTATION_ESCAPE_0, true}, context);
                    departing_characters_ = result.departing;
                    status_animation_ = true;
                    update_battle_participation(battle);
                }
                return;
            }
            if (const auto* event =
                    std::get_if<BattleScriptedEventAction>(&battle.immediate_actions.front())) {
                event_.emplace(event_assets_, event->identity, effects_, effect_context(battle),
                               message_speed_);
                immediate_event_ = true;
                battle.menus_disabled = true;
                menus_.close_for_victory();
                battle.immediate_actions.erase(battle.immediate_actions.begin());
                return;
            }
            if (const auto* expired =
                    std::get_if<BattleTimerExpiredAction>(&battle.immediate_actions.front())) {
                const auto target = expired->target;
                require(target < 10, "Invalid expired-status participant");
                const bool wake = expired->expired[3] && (battle.status.targets[target].active &
                                                          status_mask(BattleStatus::sleep));
                const std::array<BattleStatus, 4> statuses{
                    BattleStatus::stop, BattleStatus::reflect, BattleStatus::frozen,
                    BattleStatus::sleep};
                for (unsigned i = 0; i < 4; ++i)
                    if (expired->expired[i])
                        battle.status.targets[expired->target].pending.remove |=
                            status_mask(statuses[i]);
                std::array<BattleStatusContext, 10> contexts{};
                for (unsigned i = 0; i < 10; ++i)
                    contexts[i] = {battle.stats[i].hp, battle.stats[i].blocked_status};
                update_battle_status(battle.status, contexts);
                battle.immediate_actions.erase(battle.immediate_actions.begin());
                refresh_battle_actions(battle, commands_, magitek_.random, 0);
                for (unsigned i = 0; i < 4; ++i)
                    seed_.actors.characters.control[i].active_status = battle.character_display[i].status;
                update_battle_character_status(assets_.actors.graphics,
                    {seed_.rules.magitek, false}, seed_.actors.characters);
                if (wake) {
                    // C2/518A emits the authored wake effect only if Psyche's
                    // timer expires while the recipient is still asleep.
                    auto context = effect_context(battle);
                    context.attacker = target;
                    context.targets.set(target);
                    context.hits.set(target);
                    animation_.emplace(effects_,
                                       BattleActionAnimationRequest{
                                           BattleActionAnimationKind::status_effect, FF6_ROM_VALUE_BATTLE_CONTINUATION_WAKE_0, true},
                                       context);
                    status_animation_ = true;
                }
                return;
            }
            if (const auto* restore =
                    std::get_if<BattleRestorePoseAction>(&battle.immediate_actions.front())) {
                require(restore->target < 4, "Pose restoration requires a character");
                seed_.actors.characters.control[restore->target].ready_action = 0;
                update_battle_character_status(
                    assets_.actors.graphics, {seed_.rules.magitek, false}, seed_.actors.characters);
                battle.immediate_actions.erase(battle.immediate_actions.begin());
                return;
            }
            const auto* announcement =
                std::get_if<BattleAnnouncementAction>(&battle.immediate_actions.front());
            require(announcement != nullptr,
                    "Immediate battle action presentation is not implemented yet");
            open_message(message_, format_battle_message(
                                 victory_assets_->messages,
                                 battle_announcement_message(announcement->message), {}, party));
            // Ownership moves from the immediate queue to the active message.
            // Later actions remain queued until its authored display wait ends.
            battle.immediate_actions.erase(battle.immediate_actions.begin());
            return;
        }
        counter = work.kind == BattleQueueWorkKind::retaliation;
        slot = *work.participant;
        if (work.kind == BattleQueueWorkKind::advance) {
            if (prepare_battle_advance(battle, queue_context_, execution_data_.queue, slot) &&
                begin_battle_jump_advance(battle, execution_context_, slot)) {
                auto context = effect_context(battle);
                context.attacker = slot;
                context.targets = context.hits = std::bitset<10>{1U << slot};
                context.needs_step_back.fill(false);
                animation_.emplace(effects_, BattleActionAnimationRequest{
                    BattleActionAnimationKind::command_effect, FF6_ROM_VALUE_BATTLE_CONTINUATION_JUMP_CHARACTER_0, true}, context);
                jumping_characters_.set(slot);
                status_animation_ = true;
            }
            return;
        }
    } else {
        if (!menus_.confirmed())
            return;
        slot = menus_.confirmed()->character;
        auto& turn = battle.turns.at(slot);
        require(menus_.confirmed()->command == 29 && turn.normal.size() == 1,
                "Only one selected Magitek action can execute in this preview");
        if (!pending_attacker_) {
            queue_context_.targets.selectable = battle_target_admission(battle, visible_targets);
            if (!prepare_battle_advance(battle, queue_context_, execution_data_.queue, slot))
                return;
            for (auto& entry : battle.advance_schedule.entries)
                if (entry == slot)
                    entry.reset();
            pending_attacker_ = slot;
        }
        if (!menus_.complete() || !turn.action_ready)
            return;
    }
    auto next = battle;
    auto next_queue = queue_context_;
    auto next_execution = execution_context_;
    auto request =
        counter
            ? begin_battle_retaliation(next, next_queue, next_execution, execution_data_, slot)
            : begin_battle_normal_action(next, next_queue, next_execution, execution_data_, slot);
    if (request.disposition == BattleExecutionDisposition::blocked ||
        request.disposition == BattleExecutionDisposition::no_action) {
        battle = std::move(next);
        queue_context_ = std::move(next_queue);
        execution_context_ = std::move(next_execution);
        if (request.disposition == BattleExecutionDisposition::no_action && slot >= 4 &&
            battle.status.targets[slot].monster_dead_or_escaped &&
            battle.turns[slot].retaliation.empty()) {
            // C2/0062 retires dead sprites when counter decisions drain. An
            // empty decision has no action_result/presentation to do this later.
            seed_.rules.shown_monsters[slot - 4] = false;
        }
        return;
    }
    if (request.disposition == BattleExecutionDisposition::jump_wait) {
        // C2/00E4 retains Jump in the command queue for its landing turn.
        // There is no executing action to finish during this airborne wait.
        if (slot < 4) {
            // Mimic becomes Jump only at execution. Its new advance visit
            // prepares the ordinary takeoff, delay and retained landing.
            battle = std::move(next);
            queue_context_ = std::move(next_queue);
            execution_context_ = std::move(next_execution);
            return;
        }
        auto context = effect_context(next);
        context.attacker = slot;
        context.targets.reset();
        context.targets[slot] = true;
        context.hits = context.targets;
        context.needs_step_back.fill(false);
        animation_.emplace(effects_, BattleActionAnimationRequest{
            BattleActionAnimationKind::monster_effect, FF6_ROM_VALUE_BATTLE_CONTINUATION_JUMP_MONSTER_0, true}, context);
        jumping_monsters_[slot - 4] = true;
        status_animation_ = true;
        battle = std::move(next);
        queue_context_ = std::move(next_queue);
        execution_context_ = std::move(next_execution);
        return;
    }
    if (request.disposition != BattleExecutionDisposition::execute) {
        if (counter)
            finish_battle_retaliation(next, next_queue, next_execution);
        else
            finish_battle_normal_action(next, next_queue, next_execution, execution_data_, {});
        battle = std::move(next);
        queue_context_ = std::move(next_queue);
        execution_context_ = std::move(next_execution);
        return;
    }
    battle = std::move(next);
    queue_context_ = std::move(next_queue);
    execution_context_ = std::move(next_execution);
    repeated_hit_ = 0;
    repeated_count_ = 1;
    repeated_context_.reset();
    weapon_sequence_.clear();
    sketch_.reset();
    item_continuation_.reset();
    execute_selected_attack(battle, party, slot, request);
}
void BattleMenuPreview::execute_selected_attack(BattleState& battle, const BattleParty& party,
    std::uint8_t slot, BattleExecutionRequest request) {
    auto next = battle;
    auto next_queue = queue_context_;
    auto next_execution = execution_context_;
    auto next_physical = physical_environment_;
    if (!repeated_hit_ && !immediate_attack_) mimic_command_ = request.command;
    if (request.command.command == 32) {
        require(!battle_change_, "Another battle change is already pending");
        battle_change_ = BattleChangeRequest{static_cast<std::uint16_t>(request.command.parameters & 0x7fffU),
                                             request.command.attack, (request.command.parameters & 0x8000U) != 0};
        executing_command_ = request.command;
        return;
    }
    // C2/1976 delegates an acquired Control command with the monster as Y.
    // The character still owns execution/ATB, but stats, MP and attack memory
    // belong to the monster. A released link falls back to a new attempt.
    if (slot < 4 && request.command.command == 14 &&
        (next.status.targets[slot].active & status_mask(BattleStatus::control)) &&
        next.links[slot].controls && !next.links[slot].controls->remove_pending) {
        slot = next.links[slot].controls->target;
        require(slot >= 4 && slot < 10, "Invalid controlled attack owner");
        request.command.command = battle_ai_attack_command(execution_data_.queue.ai, request.command.attack);
        request.command.mp_cost = execution_data_.queue.spell_mp[request.command.attack];
    }
    const bool dancing = request.command.command == 19;
    const bool item_summon = repeated_hit_ != 0 && item_continuation_.has_value();
    const auto dance = dancing ? begin_battle_dance(next, slot, dances_, magic_.random)
                               : BattleDanceStart{};
    const bool change_background = dance.background &&
        !dances_.preserved_backgrounds.test(assets_.background.identity);
    const bool raging = request.command.command == 16;
    if (raging) request.command = begin_battle_rage(next, slot, request.command, rage_monsters_, execution_data_.queue.ai);
    const bool sketch_attack = sketch_ && sketch_->executing;
    const auto desperation = !repeated_hit_
        ? choose_battle_desperation(next, physical_, slot, request.command) : std::nullopt;
    if (desperation) request.command = {2, *desperation, request.command.parameters, 0};
    std::optional<BattleUmaroAttack> umaro;
    if (slot < 4 && next.actors[slot] == 13 && request.command.command == 0 &&
        !repeated_hit_ && !raging && !sketch_attack)
        umaro = choose_battle_umaro_attack(next, physical_, slot);
    const bool umaro_storm = umaro && umaro->kind == BattleUmaroAttackKind::storm;
    const bool umaro_physical = umaro && (umaro->kind == BattleUmaroAttackKind::tackle ||
                                          umaro->kind == BattleUmaroAttackKind::ally_throw);
    if (umaro_storm) request.command = {2, FF6_ROM_VALUE_BATTLE_UMARO_STORM_ATTACK_0, request.command.parameters, 0};
    const auto command = request.command;
    next_physical.property_source = sketch_attack ? std::optional{sketch_->source} : std::nullopt;
    const bool bushido = slot < 4 && command.command == 7;
    if (bushido && !repeated_hit_) {
        const auto special = magic_.effects[command.attack].special;
        repeated_count_ = special == 50 ? FF6_ROM_VALUE_BATTLE_QUADRA_TOTAL_HITS_0 : special == 54 ? 2U : 1U;
    }
    if (slot < 4 && !repeated_hit_ && !umaro_physical && (command.command == 0 || command.command == 6 || command.command == 22)) {
        // C2/17F6 chooses Jump's hand before the Horn roll, and retains that
        // hand for every landing. Fight alternates right/left attempts instead.
        const auto& hands = next.stats[slot].hands;
        if (command.command == 22) {
            const std::uint8_t hand = hands[1].attack &&
                (!hands[0].attack || (next.random.next(physical_.targeting.random) & 1U)) ? 1 : 0;
            if (next.stats[slot].relic_effects[0] & 128U) {
                const auto roll = next.random.next(physical_.targeting.random);
                repeated_count_ = 2U + (roll < FF6_ROM_VALUE_BATTLE_HORN_FIRST_THRESHOLD_0 ? 1U : 0U) + (roll < FF6_ROM_VALUE_BATTLE_HORN_SECOND_THRESHOLD_0 ? 1U : 0U);
            }
            for (unsigned i = 0; i < repeated_count_; ++i)
                weapon_sequence_.push_back({command, hand, repeated_count_ - i - 1});
        } else {
            const unsigned attempts = next.stats[slot].relic_effects[2] & 1U ? FF6_ROM_VALUE_BATTLE_OFFERING_HAND_ATTEMPTS_0 : FF6_ROM_VALUE_BATTLE_ORDINARY_HAND_ATTEMPTS_0;
            for (unsigned i = 0; i < attempts; ++i)
                if (hands[i & 1U].attack)
                    weapon_sequence_.push_back({command, static_cast<std::uint8_t>(i & 1U), attempts - i - 1});
            repeated_count_ = std::max(1U, static_cast<unsigned>(weapon_sequence_.size()));
        }
    }
    const bool weapon_spell = !weapon_sequence_.empty() && weapon_sequence_.front().spell;
    const bool control =
        execute_battle_ai_control(next, next_queue.ai, execution_data_.queue.ai, slot, command);
    const bool scripted_sound = command.command == 43 && command.attack == 9;
    if (control || scripted_sound || command.command == 18 || command.command == 33 || command.command == 35) {
        // AI FA/09 -> C2/642D -> DoMonsterAnim C2/E63D. These bytes are a
        // literal pan and cue identity, not a target mask or an attack effect.
        if (scripted_sound)
            script_sound_ = GameSoundEffect{
                static_cast<std::uint8_t>(command.parameters >> 8U),
                static_cast<std::uint8_t>(command.parameters & 255U)};
        if (command.command == 33)
            open_message(message_, format_monster_message(*event_assets_, command.attack, party));
        if (command.command == 35) {
            event_.emplace(event_assets_, command.attack, effects_, effect_context(battle),
                           message_speed_);
            next.menus_disabled = true;
            menus_.close_for_victory(); // Shared orderly closure; does not award victory.
        }
        battle = std::move(next);
        queue_context_ = std::move(next_queue);
        execution_context_ = std::move(next_execution);
        executing_command_ = command;
        action_result_ = BattleAttackResult{slot, command.attack, {}, false};
        if (!event_) {
            finish_selected_action(battle);
            action_result_.reset();
        }
        return;
    }
    if (command.command == 20 || command.command == 21) {
        require(slot < 4, "Player stance command requires a character");
        if (next.deferred_death[slot]) {
            next.deferred_death.reset(slot);
            finish_anchor_death(next, slot);
        }
        if (command.command == 20)
            next.entry.back_rows.flip(slot);
        else
            next.turns[slot].defending = true;
        for (auto& entry : next.ready_schedule.entries)
            if (entry == slot)
                entry.reset();
        battle = std::move(next);
        queue_context_ = std::move(next_queue);
        execution_context_ = std::move(next_execution);
        executing_command_ = command;
        action_result_ = BattleAttackResult{slot, 0, {}, false};
        if (command.command == 20) {
            const auto approach = static_cast<unsigned>(battle.entry.approach);
            const int distance = signed16(assets_.actors.view.row_offsets.at(approach).at(slot));
            require(distance % static_cast<int>(FF6_ROM_VALUE_BATTLE_ROW_MOVEMENT_PARAMETERS_0) == 0, "Row distance cannot be presented in six equal steps");
            auto& layout = seed_.actors.characters.layout;
            layout.back_row[slot] = !layout.back_row[slot];
            battle.entry.displayed_back_rows[slot] = layout.back_row[slot];
            row_motion_ = RowMotion{slot, (layout.back_row[slot] ? distance : -distance) / static_cast<int>(FF6_ROM_VALUE_BATTLE_ROW_MOVEMENT_PARAMETERS_0)};
            layout.poses[slot].action =
                4; // C1/B838: walking-forward action, independent of row direction.
        } else
            finish_selected_action(battle); // DEF has no authored attack animation or sound.
        return;
    }
    // ExecAttack checks silence/Imp against the spell's original cost, then
    // CalcAttackEffect checks the prepared (possibly waived/reduced) cost.
    // Resources can change while a command waits in the shared action queue.
    // C2/37EB clears the spell's base cost before C2/3201 checks Silence/Imp.
    // Magicite and weapon continuations therefore keep working under either
    // status. Ordinary Mimic only waives the queued cost and stays interruptible.
    const bool casting = !item_summon && !weapon_spell && !raging && !umaro_storm &&
        (!sketch_attack || magic_.effects[command.attack].original_mp_cost) &&
        (command.command == 2 || command.command == 23 || command.command == 12 ||
         command.command == 7 || command.command == 25);
    const bool interrupted_cast =
        casting && execution_data_.queue.spell_mp[command.attack] &&
        ((next.status.targets[slot].active & status_mask(BattleStatus::silence)) ||
         ((next.status.targets[slot].active & status_mask(BattleStatus::imp)) &&
          command.attack != 35));
    if (command.command == 30) {
        // C2/19B2 executes Lore with targets cleared by C2/31F2, then starts
        // graphics command 33. Its cursor later publishes a timer-owned Doom;
        // this command must not kill the AI's initially selected target.
        auto lore = command;
        lore.command = 12;
        lore.parameters = 0;
        const bool blocked = execution_data_.queue.spell_mp[command.attack] &&
            (next.status.targets[slot].active &
             (status_mask(BattleStatus::silence) | status_mask(BattleStatus::imp)));
        BattleAttackResult result{slot, command.attack, {}, false};
        if (!blocked)
            result = slot < 4 ? resolve_battle_character_magic(next, slot, lore, magic_)
                              : resolve_battle_monster_magic(next, slot, lore, magic_);
        auto random_index = menus_.graphics_random_index();
        automatic_roulette_ = begin_battle_automatic_roulette(magic_.random, random_index);
        menus_.set_graphics_random_index(random_index);
        open_message(attack_name_, format_battle_ability_name(victory_assets_->messages, command.attack));
        battle = std::move(next);
        queue_context_ = std::move(next_queue);
        execution_context_ = std::move(next_execution);
        executing_command_ = command;
        action_result_ = std::move(result);
        finish_selected_action(battle);
        return;
    }
    if (interrupted_cast && !sketch_attack) {
        battle = std::move(next);
        queue_context_ = std::move(next_queue);
        execution_context_ = std::move(next_execution);
        executing_command_ = command;
        action_result_ = BattleAttackResult{slot, command.attack, {}, false};
        finish_selected_action(battle);
        return;
    }
    // AirAnchorEffect belongs to ExecAttack, after interrupted casting is
    // rejected, and before target effects can arm another delayed death.
    const bool anchor_death = !interrupted_cast && command.command < 30 && next.deferred_death[slot];
    if (anchor_death)
        next.deferred_death.reset(slot);
    BattleAttackResult result;
    BattleActionAnimationRequest presentation;
    bool instant_visibility = false;
    std::optional<BattlePhysicalResult> physical_result;
    numeral_requests_.clear();
    const bool retort_stance = bushido && command.attack == 86 &&
                               !(next.stats[slot].retaliation_flags & 1U);
    if (bushido && command.attack == 86) next.stats[slot].retaliation_flags ^= 1U;
    if (dance.stumbled) {
        result = {slot, command.attack, {{slot, true}}, true};
        result.messages.push_back({4, {}});
        presentation = {BattleActionAnimationKind::command_effect, FF6_ROM_VALUE_BATTLE_PRESENTATION_STUMBLE_0, true, true};
        presentation.command_step_back = true; // C1/BBE1 returns after the authored stumble.
    } else if (interrupted_cast) {
        // A silenced copied spell has no effect, but Sketch has already moved
        // the caster. The common empty-continuation path must return that step.
        result = {slot, command.attack, {}, false};
        presentation = {BattleActionAnimationKind::outcome_only, 0, true, true};
    } else if (retort_stance) {
        // C2/1847 arms Retort without its strike; C1/B9AF skips the body.
        request.command.parameters = choose_battle_targets(next, slot, command.parameters,
            battle_attack_target_rules(execution_data_.queue.preparation, command.command, command.attack),
            next_queue.targets, execution_data_.queue.ai.random, next.random);
        result = resolve_battle_bushido_magic(next, slot, request.command, magic_, false, true);
        presentation = {BattleActionAnimationKind::bushido, command.attack, false, true};
        presentation.stance_only = true;
    } else if (command.command == 3 || command.command == 4) {
        result = resolve_battle_morph(next, slot, command, magitek_);
        presentation = {BattleActionAnimationKind::command_effect,
                        static_cast<std::uint16_t>(command.command == 3 ? FF6_ROM_VALUE_BATTLE_PRESENTATION_MORPH_PRELUDE_0 : (FF6_ROM_VALUE_BATTLE_PRESENTATION_MORPH_PRELUDE_0 + FF6_ROM_VALUE_BATTLE_PRESENTATION_REVERT_DELTA_0)), true, true};
    } else if (command.command == 17) {
        request.command.parameters = choose_battle_targets(next, slot, command.parameters,
            battle_attack_target_rules(execution_data_.queue.preparation, 17, 255),
            next_queue.targets, execution_data_.queue.ai.random, next.random);
        result = resolve_battle_leap(next, slot, request.command, magitek_);
        presentation = {BattleActionAnimationKind::command_effect, FF6_ROM_VALUE_BATTLE_PRESENTATION_LEAP_0, true, true};
    } else if (command.command == 10 && (command.attack & 128U)) {
        // Invalid/unlearned codes still consume the turn and the command's
        // prelude, but have no hit targets and cannot trigger if_attack(Pummel).
        result = {slot, FF6_ROM_VALUE_BATTLE_BLITZ_FAILURE_ATTACK_0, {{slot, false}}, false};
        result.messages.push_back({FF6_ROM_VALUE_BATTLE_BLITZ_FAILURE_MESSAGE_0, {}});
        presentation = {BattleActionAnimationKind::command_effect, FF6_ROM_VALUE_BATTLE_PRESENTATION_FAILED_BLITZ_0, true, true};
    } else if (command.command == 38) {
        result = resolve_battle_immediate_spell(next, slot, command.attack, magic_);
        presentation = {BattleActionAnimationKind::status_effect, command.attack, true, true};
        for (const auto& target : result.targets)
            if (target.miss_notice)
                numeral_requests_.push_back({target.slot, BattleDamageNumberKind::miss, 0});
    } else if (command.command == 34 || command.command == 45) {
        result = command.command == 45 ? resolve_battle_seize_drain(next, slot, command, magic_)
                                       : resolve_battle_periodic(next, slot, command, magic_);
        presentation = {BattleActionAnimationKind::outcome_only, 0, true, true};
        for (const auto& target : result.targets)
            if (target.display_amount)
                numeral_requests_.push_back({target.slot,
                    target.healing ? BattleDamageNumberKind::healing : BattleDamageNumberKind::damage,
                    target.amount});
    } else if (command.command == 14 || command.command == 24) {
        request.command.parameters = choose_battle_targets(next, slot, command.parameters,
            battle_attack_target_rules(execution_data_.queue.preparation, command.command, command.attack),
            next_queue.targets, execution_data_.queue.ai.random, next.random);
        result = command.command == 14 ? resolve_battle_control(next, slot, request.command, magic_)
                                      : resolve_battle_gil_toss(next, slot, request.command, magic_);
        presentation = {BattleActionAnimationKind::command_effect,
                        static_cast<std::uint16_t>(command.command == 14 ? FF6_ROM_VALUE_BATTLE_PRESENTATION_CONTROL_0 : FF6_ROM_VALUE_BATTLE_PRESENTATION_GIL_TOSS_0), true, true};
        for (const auto& target : result.targets)
            if (target.miss_notice || target.display_amount)
                numeral_requests_.push_back({target.slot,
                    target.miss_notice ? BattleDamageNumberKind::miss : BattleDamageNumberKind::damage,
                    target.amount});
    } else if (command.command == 11) {
        require(slot < 4, "Runic stance requires a character");
        next.stats[slot].retaliation_flags |= 4;
        result = {slot, 0, {{slot, true}}, false};
        presentation = {BattleActionAnimationKind::command_effect, FF6_ROM_VALUE_BATTLE_PRESENTATION_RUNIC_0, true, true};
    } else if (command.command == 26 || command.command == 27) {
        // C2/171A..1725: command targeting with a built-in spell effect.
        request.command.attack = command.command == 26 ? FF6_ROM_VALUE_BATTLE_CONTINUATION_HEALTH_0 : FF6_ROM_VALUE_BATTLE_CONTINUATION_SHOCK_0;
        request.command.parameters = choose_battle_targets(
            next, slot, command.parameters,
            battle_attack_target_rules(execution_data_.queue.preparation, command.command,
                                       request.command.attack),
            next_queue.targets, execution_data_.queue.ai.random, next.random);
        result = resolve_battle_innate_magic(next, slot, request.command, magic_);
        // CmdAnimPropPtrs maps Shock directly to definition 130; its own
        // sword gesture releases the wave, without a Magic casting prelude.
        presentation = command.command == 26
            ? BattleActionAnimationRequest{BattleActionAnimationKind::health, FF6_ROM_VALUE_BATTLE_PRESENTATION_HEALTH_0, false, true}
            : BattleActionAnimationRequest{BattleActionAnimationKind::command_effect, FF6_ROM_VALUE_BATTLE_PRESENTATION_SHOCK_0, true, true};
        for (const auto& target : result.targets)
            if (target.miss_notice || target.display_amount)
                numeral_requests_.push_back({target.slot,
                                             target.miss_notice ? BattleDamageNumberKind::miss
                                             : target.healing   ? BattleDamageNumberKind::healing
                                                                : BattleDamageNumberKind::damage,
                                             target.amount});
    } else if (request.command.command == 29) {
        if (live_)
            request.command.parameters = choose_battle_targets(
                next, slot, request.command.parameters,
                battle_attack_target_rules(execution_data_.queue.preparation,
                                           request.command.command, request.command.attack),
                next_queue.targets, execution_data_.queue.ai.random, next.random);
        result = resolve_battle_magitek(next, slot, request.command, magitek_);
        presentation = {BattleActionAnimationKind::magitek, result.attack, false, true};
        for (const auto& target : result.targets)
            if (target.miss_notice || target.display_amount)
                numeral_requests_.push_back({target.slot,
                                             target.miss_notice ? BattleDamageNumberKind::miss
                                             : target.healing   ? BattleDamageNumberKind::healing
                                                                : BattleDamageNumberKind::damage,
                                             target.amount});
    } else if (command.command == 1 || item_summon ||
               (command.command == 8 && items_.throw_spells[command.attack]) || (slot < 4 && (!sketch_attack || !(magic_.effects[command.attack].flags & 1U)) &&
               (((raging || sketch_attack) && command.command != 0 && !(magic_.effects[command.attack].flags & 1U)) || command.command == 2 || command.command == 23 || command.command == 12 || command.command == 15 || command.command == 25 || command.command == 19 || command.command == 1 ||
                (command.command == 9 && items_.tool_spells.at(command.attack - 163)) ||
                ((command.command == 10 || bushido) && !(magic_.effects[command.attack].flags & 1U))))) {
        if (!weapon_spell || !weapon_sequence_.front().same_target) {
            auto targeting = battle_attack_target_rules(execution_data_.queue.preparation,
                                                        command.command, command.attack);
            // ExecAttack's copied-action path chooses anew using the copied
            // spell's side/group policy, not the monster selected for Sketch.
            targeting.policy.force_retarget |= sketch_attack || item_summon;
            // ShowAttackName_07 precedes ChooseTarget. Its one-use bypass
            // retains Joker's selected side, including an enemy character.
            if (command.command == 15 && command.attack == 148)
                next_queue.targets.skip_next_filter = true;
            request.command.parameters =
                choose_battle_targets(next, slot, command.parameters,
                                      targeting,
                                      next_queue.targets, execution_data_.queue.ai.random, next.random);
        }
        const bool item = command.command == 1;
        const auto item_spell = item ? items_.item_spells[command.attack] : std::nullopt;
        const bool tool = command.command == 9;
        const bool thrown = command.command == 8;
        result = weapon_spell ? resolve_battle_weapon_magic(next, slot, request.command, magic_,
                                                            weapon_sequence_.front().same_target)
                 : command.command == 15 ? resolve_battle_slot(next, slot, request.command, magic_)
                 : item_summon ? resolve_battle_item_summon(next, slot, request.command, magic_)
                 : command.command == 25 ? resolve_battle_summon(next, slot, request.command, magic_)
                 : sketch_attack ? resolve_battle_sketch_magic(next, slot, sketch_->source, request.command, magic_)
                 : umaro_storm ? resolve_battle_umaro_storm(next, slot, request.command, magic_)
                 : dancing ? resolve_battle_dance_magic(next, slot, request.command, magic_)
                 : raging ? resolve_battle_rage_magic(next, slot, request.command, magic_)
                 : thrown ? resolve_battle_throw_magic(next, slot, request.command, magic_,
                                                       *items_.throw_spells[command.attack])
                 : tool   ? resolve_battle_tool_magic(next, slot, request.command, magic_,
                                                    *items_.tool_spells.at(command.attack - 163))
                 : item_spell ? resolve_battle_item_magic(next, slot, request.command, magic_, *item_spell,
                                                         items_.reusable_items[command.attack])
                 : item ? resolve_battle_item(next, slot, request.command, items_)
                 : bushido ? resolve_battle_bushido_magic(next, slot, request.command, magic_, repeated_hit_ != 0)
                 : command.command == 10
                     ? resolve_battle_blitz_magic(next, slot, request.command, magic_)
                     : resolve_battle_character_magic(next, slot, request.command, magic_);
        const auto definition = thrown ? items_.throw_animations[command.attack]
                                : tool ? std::optional<std::uint16_t>{static_cast<std::uint16_t>(
                                             FF6_ROM_VALUE_BATTLE_CONTINUATION_TOOLS_0 + command.attack - 163)}
                                : item_spell ? std::optional<std::uint16_t>{*item_spell}
                                : item ? items_.animations.at(command.attack - 224)
                                       : std::optional<std::uint16_t>{command.attack};
        require(definition.has_value(), "Item has no animation definition");
        presentation = {slot >= 4              ? BattleActionAnimationKind::monster_effect
                        : raging || dancing || sketch_attack || weapon_spell || umaro_storm || command.command == 15 ? BattleActionAnimationKind::rage_magic
                        : tool || thrown        ? BattleActionAnimationKind::command_effect
                        : item && !item_spell   ? BattleActionAnimationKind::item
                        : bushido               ? BattleActionAnimationKind::bushido
                        : command.command == 10 ? BattleActionAnimationKind::blitz
                                                : BattleActionAnimationKind::magic,
                        *definition, tool || thrown, true};
        presentation.slot_pre_cast = command.command == 15;
        if (result.follow_up_summon) {
            item_continuation_ = BattleQueuedCommand{25, *result.follow_up_summon, 0, 0};
            repeated_count_ = repeated_hit_ + 2;
            presentation.defer_return = true;
        }
        // ExecCmd keeps the item user's pre-cast disabled for the continuation.
        // The item gesture already owns the forward step and its final return.
        if (item_summon) presentation.skip_pre_cast = true;
        if ((raging || sketch_attack) && command.command == 27)
            presentation = {BattleActionAnimationKind::command_effect, FF6_ROM_VALUE_BATTLE_PRESENTATION_SHOCK_0, true, true};
        for (const auto& target : result.targets)
            if (target.miss_notice || target.display_amount)
                numeral_requests_.push_back({target.slot,
                                             target.miss_notice ? BattleDamageNumberKind::miss
                                             : target.healing   ? BattleDamageNumberKind::healing
                                                                : BattleDamageNumberKind::damage,
                                             target.amount});
    } else if (slot >= 4 && !(magic_.effects[command.attack].flags & 1U) &&
               (command.command == 2 || command.command == 23 || command.command == 12 || command.command == 7)) {
        if (magic_.effects[command.attack].special != 68)
            request.command.parameters =
            choose_battle_targets(next, slot, command.parameters,
                                  battle_attack_target_rules(execution_data_.queue.preparation,
                                                             command.command, command.attack),
                                  next_queue.targets, execution_data_.queue.ai.random, next.random);
        result = resolve_battle_monster_magic(next, slot, request.command, magic_);
        presentation = {BattleActionAnimationKind::monster_effect, result.attack, true, true};
        for (const auto& target : result.targets)
            if (target.miss_notice || target.display_amount)
                numeral_requests_.push_back({target.slot,
                                             target.miss_notice ? BattleDamageNumberKind::miss
                                             : target.healing   ? BattleDamageNumberKind::healing
                                                                : BattleDamageNumberKind::damage,
                                             target.amount});
    } else if (command.command == 43) {
        require(command.attack < effects_->monster_misc.size() && command.attack != 9,
                "Unknown misc. monster animation");
        const auto definition = effects_->monster_misc[command.attack];
        const auto selected = static_cast<std::uint8_t>(command.parameters);
        result = {slot, command.attack, {}, true};
        for (unsigned i = 0; i < 10; ++i)
            if (definition.all_characters ? i < 4 && next.status.targets[i].present
                                          : i >= 4 && (selected & (1U << (i - 4))))
                result.targets.push_back({static_cast<std::uint8_t>(i), true});
        presentation = {BattleActionAnimationKind::monster_effect, definition.effect, true, true};
    } else if (command.command == 36) {
        const unsigned operation = command.parameters & 255U;
        require(slot >= 4 && operation <= 4 && command.attack < 18,
                "Unsupported monster visibility operation");
        const bool show = operation == 0 || operation == 2;
        const std::bitset<6> selected(command.parameters >> 8U);
        result = {slot, command.attack, {}, false};
        for (unsigned i = 0; i < 6; ++i) {
            if (!selected[i] || !next_queue.ai.allocated[i + 4])
                continue;
            auto& status = next.status.targets[i + 4];
            if (operation == 0)
                next.stats[i + 4].hp.current = next.stats[i + 4].hp.maximum;
            if (operation == 4)
                next.ending_inhibited = true; // C2/4FC2: hide, but retain the battle.
            if (show) {
                next.ending_inhibited = false; // C2/4FEC: a replacement releases the hold.
                status.present = true;
                status.active &=
                    ~(status_mask(BattleStatus::hide) | status_mask(BattleStatus::dead));
                status.monster_alive = true;
                status.monster_dead_or_escaped = false;
                next.participation.active.set(i + 4);
                next.untargetable.reset(i + 4);
                pending_show_.set(i);
                next.departed_monsters.reset(i);
            } else {
                status.active |= status_mask(BattleStatus::hide);
                status.monster_alive = false;
                if (operation == 1) {
                    status.present = false;
                    next.participation.active.reset(i + 4);
                }
                pending_hide_.set(i);
            }
            result.targets.push_back({static_cast<std::uint8_t>(i + 4), true});
        }
        const auto property = transitions_[show ? 1 : 0][command.attack];
        // The authored FF-only transition returns without an effect body.
        // Keep the same visibility/queue handoff as animated transitions.
        const bool special = command.attack == 13 || command.attack == 17;
        instant_visibility = !property && !special;
        if (special) {
            presentation = {BattleActionAnimationKind::monster_transition, command.attack, true, true};
            presentation.monster_entering = show;
        } else if (property)
            presentation = {BattleActionAnimationKind::monster_effect, *property, true, true};
    } else if (request.command.command == 0 || command.command == 6 || command.command == 10 || command.command == 9 || command.command == 28 ||
               command.command == 8 || command.command == 22 ||
               command.command == 5 || command.command == 13 ||
               (bushido && (magic_.effects[command.attack].flags & 1U)) ||
               ((slot >= 4 || sketch_attack) && (magic_.effects[command.attack].flags & 1U) &&
                (command.command == 2 || command.command == 23 || command.command == 12 || command.command == 7))) {
        const auto previous_targets = next_physical.targets.backup_targets;
        next_physical.targets = next_queue.targets;
        next_physical.random_retarget = sketch_attack || (bushido && repeated_count_ == FF6_ROM_VALUE_BATTLE_QUADRA_TOTAL_HITS_0) ||
                                        (command.command == 22 && repeated_hit_ != 0) ||
                                        ((command.command == 0 || command.command == 6) && !umaro_physical && (next.stats[slot].relic_effects[2] & 1U));
        next_physical.weapon_hand = weapon_sequence_.empty() ? std::nullopt
                                          : std::optional{weapon_sequence_.front().hand};
        next_physical.remaining_hits = weapon_sequence_.empty() ? repeated_count_ - repeated_hit_ - 1
                                                               : weapon_sequence_.front().remaining;
        next_physical.counterattack = battle.retaliation_action;
        next_physical.clock = clock();
        if (next_physical.random_retarget)
            next_physical.targets.backup_targets = repeated_hit_ ? previous_targets : 0;
        std::bitset<10> facing_right;
        for (unsigned i = 0; i < 4; ++i)
            facing_right[i] = (seed_.actors.characters.layout.poses[i].facing & 1U) != 0;
        for (unsigned i = 0; i < 6; ++i)
            facing_right[i + 4] = ((seed_.actors.monsters.positions[i].flip ^
                                    seed_.rules.retained_monster_facing[i] ^ 1U) &
                                   1U) != 0;
        next_physical.hit_from_behind =
            battle_rear_attack_targets(next.entry.approach, slot, facing_right);
        if (command.command == 8)
            require(!battle_attack_target_rules(execution_data_.queue.preparation, 8, command.attack).spell &&
                        items_.throw_animations[command.attack].has_value(),
                    "Physical Throw requires a physical item and animation");
        auto technique = magic_.effects[command.attack];
        if (technique.special == 50) technique.special = 255;
        if (command.command == 13) {
            technique = {};
            technique.flags = 1;
            technique.options = 34;
            technique.behavior = 32;
            technique.special = 85;
        }
        const auto physical =
            umaro_physical ? resolve_battle_umaro_physical(next, next_physical, physical_, *umaro,
                                                          request.command)
            : command.command == 8
                ? resolve_battle_physical_technique(next, next_physical, physical_, slot,
                                                    request.command, items_.thrown[command.attack])
            : command.command == 28
                ? resolve_battle_possess(next, next_physical, physical_, slot, request.command)
            : command.command == 5
                ? resolve_battle_steal(next, next_physical, physical_, slot, request.command)
            : command.command == 9
                ? resolve_battle_tool_physical(next, next_physical, physical_, slot,
                                               request.command, items_.tools[command.attack - 163])
            : (command.command == 13 || command.command == 10 || command.command == 2 || command.command == 23 || command.command == 12 ||
               command.command == 7)
                ? resolve_battle_physical_technique(next, next_physical, physical_, slot,
                                                    request.command, technique)
            : raging || sketch_attack ? resolve_battle_rage_physical(next, next_physical, physical_, slot, request.command)
            : slot < 4 ? resolve_battle_character_physical(next, next_physical, physical_, slot,
                                                           request.command)
                       : resolve_battle_monster_physical(next, next_physical, physical_, slot,
                                                         request.command);
        physical_result = physical;
        if (physical.sketch_source) {
            const auto attack = *physical.sketch_attack;
            sketch_ = SketchContinuation{*physical.sketch_source,
                {battle_ai_attack_command(execution_data_.queue.ai, attack), attack, 0, 0}, false};
            ++repeated_count_;
            next.stats[slot].monster_special = next.stats[*physical.sketch_source].monster_special;
            next.rage_special_animations[slot] = special_animations_[*physical.sketch_source - 4];
        }
        if (physical.followup_spell && !weapon_sequence_.empty()) {
            auto targets = request.command.parameters;
            if (physical.followup_same_target) {
                targets = 0;
                for (unsigned i = 0; i < 10; ++i)
                    if (physical.targets[i]) targets |= 1U << (i < 4 ? i : i + 4);
            }
            weapon_sequence_.insert(weapon_sequence_.begin() + 1,
                {{2, *physical.followup_spell, targets, 0}, 0, 0, true, physical.followup_same_target});
            ++repeated_count_;
        }
        result.rematerialize = physical.rematerialize;
        result.attacker = slot;
        result.attack = request.command.attack;
        result.messages = physical.messages;
        result.suppress_retaliation = physical.suppress_retaliation;
        for (const auto& impact : physical.impacts) {
            result.targets.push_back({impact.slot, impact.hit, impact.healing.has_value(),
                                      impact.healing.value_or(impact.damage.value_or(0)),
                                      battle.stats[impact.slot].hp.current,
                                      next.stats[impact.slot].hp.current,
                                      battle.status.targets[impact.slot].active,
                                      next.status.targets[impact.slot].active});
            if (!impact.hit || impact.damage || impact.healing)
                numeral_requests_.push_back({impact.slot,
                                             !impact.hit      ? BattleDamageNumberKind::miss
                                             : impact.healing ? BattleDamageNumberKind::healing
                                                              : BattleDamageNumberKind::damage,
                                             impact.healing.value_or(impact.damage.value_or(0))});
        }
        presentation = {(raging || sketch_attack) && physical.special ? BattleActionAnimationKind::character_special
                        : slot < 4           ? BattleActionAnimationKind::character_fight
                        : physical.special ? BattleActionAnimationKind::monster_special
                                           : BattleActionAnimationKind::monster_fight,
                        static_cast<std::uint16_t>(
                            physical.special
                                ? (slot < 4 ? next.rage_special_animations[slot] : special_animations_[slot - 4])
                                : physical.weapon_animation),
                        false, true, physical.critical};
        if (slot >= 4 && (command.command == 2 || command.command == 23 || command.command == 12 || command.command == 7))
            presentation = {BattleActionAnimationKind::monster_effect, command.attack, true, true};
        if (umaro_physical) {
            const bool tackle = umaro->kind == BattleUmaroAttackKind::tackle;
            presentation = {BattleActionAnimationKind::command_effect,
                            static_cast<std::uint16_t>(tackle ? (FF6_ROM_VALUE_BATTLE_PRESENTATION_UMARO_THROW_0 + FF6_ROM_VALUE_BATTLE_PRESENTATION_UMARO_DELTA_0) : FF6_ROM_VALUE_BATTLE_PRESENTATION_UMARO_THROW_0), true, true, physical.critical};
            presentation.command_step_back = tackle;
        }
        if (command.command == 22) {
            // C1/BA24 selects the landing or missed-Jump script. Its source
            // image remains available even while the ordinary sprite is hidden.
            if (slot < 4) seed_.rules.shown_characters[slot] = true;
            else pending_show_.set(slot - 4);
            std::uint16_t landing = physical.hits.any() ? FF6_ROM_VALUE_BATTLE_PRESENTATION_JUMP_MONSTER_0 : FF6_ROM_VALUE_BATTLE_PRESENTATION_JUMP_MONSTER_MISS_0;
            if (slot < 4) {
                landing = FF6_ROM_VALUE_BATTLE_PRESENTATION_JUMP_CHARACTER_MISS_0;
                if (physical.targets.any()) {
                    landing = FF6_ROM_VALUE_BATTLE_PRESENTATION_JUMP_UNARMED_0; // Unarmed when neither hand supplies a weapon.
                    if (next.inventory.hands[slot]) for (const auto& hand : *next.inventory.hands[slot])
                        if (hand.item && (hand.item->usage_flags & 16U)) {
                            landing = items_.jump_animations[hand.item->identity];
                            break;
                        }
                }
            }
            presentation = {slot < 4 ? BattleActionAnimationKind::command_effect
                                     : BattleActionAnimationKind::monster_effect,
                            landing, true, true};
            presentation.command_step_back = slot < 4;
            presentation.defer_return = repeated_hit_ + 1 < repeated_count_;
        }
        if (command.command == 0 && physical.thrown_weapon)
            presentation = {BattleActionAnimationKind::command_effect,
                            *items_.throw_animations[next.stats[slot].hands[physical.weapon_hand].item], true, true};
        if (command.command == 0 && physical.dice[0])
            presentation = {BattleActionAnimationKind::command_effect,
                            static_cast<std::uint16_t>(physical.dice[2] ? FF6_ROM_VALUE_BATTLE_PRESENTATION_THREE_DICE_0 : (FF6_ROM_VALUE_BATTLE_PRESENTATION_THREE_DICE_0 + FF6_ROM_VALUE_BATTLE_PRESENTATION_DICE_DELTA_0)), true, true};
        presentation.capture = physical.capture;
        presentation.weapon_death_effect = physical.death_animation;
        if (command.command == 8)
            presentation = {slot < 4 ? BattleActionAnimationKind::command_effect
                                     : BattleActionAnimationKind::monster_effect,
                            *items_.throw_animations[command.attack], true, true};
        if (command.command == 8 && items_.throw_weapon_animation[command.attack])
            presentation = {slot < 4 ? BattleActionAnimationKind::character_fight
                                     : BattleActionAnimationKind::monster_fight,
                            *items_.throw_animations[command.attack], false, true};
        if (command.command == 10)
            presentation = {BattleActionAnimationKind::blitz, command.attack, false, true};
        if (slot < 4 && command.command == 7)
            presentation = {BattleActionAnimationKind::bushido, command.attack, false, true};
        if (command.command == 9)
            presentation = {BattleActionAnimationKind::command_effect,
                            static_cast<std::uint16_t>(
                                physical.chainsaw_death ? FF6_ROM_VALUE_BATTLE_CONTINUATION_TOOLS_1 : FF6_ROM_VALUE_BATTLE_CONTINUATION_TOOLS_0 + command.attack - 163),
                            true, true};
        if (command.command == 13)
            presentation = {BattleActionAnimationKind::command_effect, FF6_ROM_VALUE_BATTLE_PRESENTATION_SKETCH_0, true, true};
        if (command.command == 5)
            presentation = {BattleActionAnimationKind::command_effect, FF6_ROM_VALUE_BATTLE_PRESENTATION_STEAL_0, true, true};
        if (command.command == 28) {
            presentation = {BattleActionAnimationKind::command_effect, FF6_ROM_VALUE_BATTLE_PRESENTATION_POSSESS_0, true, true};
            presentation.command_step_back = true;
            if (physical.hits.any()) result.departures.set(slot);
        }
    } else
        throw std::invalid_argument("Battle command consumer not implemented: " +
                                    std::to_string(request.command.command) + " attacker " + std::to_string(slot) +
                                    " attack " + std::to_string(command.attack));
    // MagicCmdAnim C1/ABEA presents Red Card with three Darts weapon bodies.
    // It remains one magic resolution and one result/counter publication.
    if (command.command == 2 && command.attack == FF6_ROM_VALUE_BATTLE_CONTINUATION_RED_CARD_0) {
        presentation = {BattleActionAnimationKind::character_fight, FF6_ROM_VALUE_BATTLE_CONTINUATION_DARTS_0, true, true};
        presentation.body_repetitions = 3;
    }
    if (sketch_attack)
        for (const auto& target : result.targets) {
            auto& memory = next.attack_memory[target.slot];
            if (memory.command && memory.command->attacker == slot) memory.command->value = 13;
        }
    if (anchor_death)
        finish_anchor_death(next, slot);
    if (sketch_) {
        presentation.defer_return = !sketch_->executing;
        if (sketch_attack) presentation.skip_pre_cast = true;
    }
    if (!weapon_sequence_.empty()) {
        presentation.defer_return = repeated_hit_ + 1 < repeated_count_;
        if (weapon_spell) presentation.skip_pre_cast = true;
    }
    if (bushido) {
        presentation.skip_pre_cast = repeated_hit_ != 0;
        presentation.defer_return = repeated_hit_ + 1 < repeated_count_;
        if (command.attack == 89 && repeated_hit_) {
            // Empowerer's MP pass uses Mimic's empty body, then the common return.
            presentation.kind = BattleActionAnimationKind::bushido;
            presentation.stance_only = true;
        }
    }
    presentation.runic_absorb = result.runic;
    // Rage dispatches GetCmdForAI before queuing graphics (C2/1554).
    // Its chosen spell therefore uses Magic/Lore/Shock/Fight presentation.
    // Lore also comes from enemy-controlled character scripts. C1/B8E5
    // selects its gesture by command/actor type, not by Rage ownership.
    presentation.dance_pre_cast = dancing && !dance.stumbled;
    presentation.lore_pre_cast = slot < 4 && command.command == 12 &&
                                 magic_.effects[command.attack].special != 25;
    if (raging && magic_.effects[command.attack].special == 25)
        presentation.skip_pre_cast = true;
    presentation.explode_attacker = (command.command == 2 || command.command == 23 || command.command == 12) &&
                                    magic_.effects[command.attack].special == 25;
    if (result.golem_hp) next_physical.golem_hp = *result.golem_hp;
    for (unsigned i = 0; i < 10; ++i) {
        if (result.seize_started[i]) next_queue.ai.personal_variables[i] |= 128;
        if (result.seize_released[i]) next_queue.ai.personal_variables[i] &= 127;
        if (result.preferred_targets[i]) next_queue.ai.preferred_targets[i] = result.preferred_targets[i];
    }
    if (raging) normalize_battle_immunity(next.stats[slot], next.morph.permanent);
    presentation.command_step_back = presentation.command_step_back ||
                                     (presentation.kind == BattleActionAnimationKind::command_effect &&
                                     (command.command == 8 || command.command == 9 ||
                                      command.command == 10 || command.command == 11 || command.command == 13 ||
                                      command.command == 27));
    presentation.projectile_targets = result.projectile_targets;
    presentation.projectile_hits = result.projectile_hits;
    if (std::ranges::any_of(result.projectile_targets, [](auto target) { return target.has_value(); }))
        presentation.skip_pre_cast = true; // C1/A98F uses the simple volley body.
    for (const auto& reflection : result.reflections) {
        presentation.reflection_targets[reflection.source] = reflection.target;
        presentation.reflection_hits[reflection.source] = reflection.hit;
    }
    for (const auto& returned : result.drain_returns)
        numeral_requests_.push_back(
            {returned.slot,
             returned.healing ? BattleDamageNumberKind::healing : BattleDamageNumberKind::damage,
             returned.amount});
    numeral_requests_ = merge_battle_damage_numbers(numeral_requests_);
    // ExecAIRetal (C2/4B86) consumes the retaliation chain, not the actor's
    // ready normal turn. A counter can happen after ATB becomes ready; deleting
    // that schedule entry strands its still-ready action indefinitely. Immediate
    // effects and periodic damage likewise do not own the ordinary turn.
    if (!next.retaliation_action && !immediate_attack_ && command.command != 34 &&
        command.command != 38 && command.command != 45)
        for (auto& entry : next.ready_schedule.entries)
            if (entry == slot)
                entry.reset();
    // ExecAttack applies each hit's statuses, but AfterAction1 handles death
    // departures and resulting actions only after all repeated attacks finish.
    if (repeated_hit_ + 1 == repeated_count_) resolve_battle_status_consequences(next);
    update_battle_participation(next);
    update_battle_action_readouts(next);
    for (auto& target : result.targets) {
        target.hp_after = next.stats[target.slot].hp.current;
        target.status_after = next.status.targets[target.slot].active;
    }
    if (command.command == 22 && result.targets.empty()) {
        // The missed-Jump script still has to land an airborne character when
        // no targets remain or the retained weapon broke during Dragon Horn.
        result.targets.push_back({slot, false});
    }
    if (result.targets.empty() && repeated_context_ &&
        std::ranges::any_of(repeated_context_->needs_step_back, [](bool pending) { return pending; })) {
        // A weapon's follow-up can lose its target to the preceding strike.
        // It still owns the caster's return; never strand that deferred step.
        result.targets.push_back({slot, false});
        presentation = {BattleActionAnimationKind::outcome_only, 0, true, true};
        presentation.defer_return = repeated_hit_ + 1 < repeated_count_;
    }
    if (result.targets.empty() || instant_visibility) {
        repeated_count_ = repeated_hit_ + 1;
        battle = std::move(next);
        queue_context_ = std::move(next_queue);
        execution_context_ = std::move(next_execution);
        executing_command_ = command;
        if (mimic_command_) mimic_command_->parameters = physical_result
            ? physical_result->selected_targets : request.command.parameters;
        action_result_ = std::move(result);
        if (instant_visibility)
            for (unsigned i = 0; i < 6; ++i)
                if (pending_show_[i]) {
                    seed_.actors.animation.monster_visible[i] = true;
                    seed_.actors.animation.monster_entry_visible[i] = true;
                }
        finish_selected_action(battle);
        return;
    }
    auto context = effect_context(battle);
    if (change_background) {
        const auto& colors = dance_backgrounds_[next.selections.dances[slot]].colors;
        context.battlefield_colors.assign(colors.begin(), colors.end());
    }
    context.vertical_sound_pan = (dancing && !dance.stumbled) || command.command == 13 || sketch_attack;
    context.attacker = umaro_physical && umaro->ally ? *umaro->ally : slot;
    context.repeated_hit = static_cast<std::uint8_t>(repeated_hit_);
    context.jump_again = command.command == 22 && repeated_hit_ + 1 < repeated_count_ &&
        (weapon_sequence_.size() < 2 || weapon_sequence_[1].command.command == 22);
    if (repeated_context_) {
        context.needs_step_back = repeated_context_->needs_step_back;
        context.step_back_facing = repeated_context_->step_back_facing;
        context.saved_attacker_direction = repeated_context_->saved_attacker_direction;
    }
    context.shown_monsters |= pending_show_;
    for (unsigned i = 0; i < 6; ++i)
        if (pending_show_[i]) {
            // Entry animations publish their final ordinary sprite explicitly.
            // Reset the prior exit's hide gate before copying the source image.
            context.control.monster_visible[i] = true;
            context.control.monster_entry_visible[i] = false;
        }
    const auto direct_count = result.targets.size() - result.reflections.size();
    for (std::size_t i = 0; i < direct_count; ++i) {
        const auto& target = result.targets[i];
        context.targets[target.slot] = true;
        context.hits[target.slot] = target.hit;
        presentation.reflecting_targets[target.slot] = target.reflected;
    }
    // Drains create an extra numeral on the attacker, not another hit effect.
    if (physical_result && physical_result->targets.any()) {
        presentation.covered_target = physical_result->covered_target;
        context.targets = physical_result->targets;
        context.hits = physical_result->hits;
        context.block_animations = physical_result->block_animations;
        context.dice = physical_result->dice;
        context.weapon_left_hand = physical_result->weapon_hand != 0;
    }
    if (command.command == 13 && sketch_)
        context.sketch_monster = next.monster_information[sketch_->source - 4].identity;
    try {
        // Validate the complete path before publishing its first frame/cue. An
        // unsupported command must not strand actors halfway through an effect.
        std::optional<BattleMessage> name;
        if (command.command == 26 || command.command == 27)
            name = format_battle_command_name(victory_assets_->messages, command.command);
        else if (command.command == 8)
            name = format_battle_item_name(victory_assets_->messages, command.attack);
        else if (command.command == 15)
            name = format_battle_ability_name(victory_assets_->messages,
                                              command.attack == 148 ? FF6_ROM_VALUE_BATTLE_SLOT_FAILURE_NAME_0 : command.attack);
        else if (command.command == 25 || desperation || (bushido && !retort_stance && !repeated_hit_) ||
            (command.command == 10 && command.attack >= 93 && command.attack <= 100) ||
            ((raging || sketch_attack || umaro_storm) && command.command != 0))
            name = bushido ? format_battle_bushido_name(victory_assets_->messages, command.attack)
                          : format_battle_ability_name(victory_assets_->messages, command.attack);
        // The name finishes before loading the prelude. Reserve that surface
        // use in the candidate, then commit it only after validation succeeds.
        if (name) context.message_window_uses = wrap8(unsigned{message_window_uses_} + 1U);
        BattleActionAnimation candidate(effects_, presentation, context);
        auto validation = candidate;
        auto validation_actors = seed_.actors;
        auto validation_rules = seed_.rules;
        for (unsigned frame = 0; !validation.complete(); ++frame) {
            require(frame < 2048, "Attack animation exceeded its validation frame limit");
            validation.advance(assets_.actors, validation_actors, validation_rules);
            if (validation.phase() == BattleActionAnimationPhase::outcomes)
                validation.resume_return();
            auto scene =
                compose_battle_scene(assets_.background, assets_.readouts, seed_.readout, {});
            validation.compose(scene);
        }
        animation_ = std::move(candidate);
        if (change_background)
            dance_transition_ = DanceTransition{0, next.selections.dances[slot]};
        if (name) open_message(attack_name_, std::move(*name));
        animation_notice_ = "Attack presentation completed";
    } catch (const std::invalid_argument& error) {
        if (live_)
            throw std::invalid_argument("Battle participant " + std::to_string(slot) + ", attack " +
                                        std::to_string(request.command.attack) + ", presentation " +
                                        std::to_string(presentation.attack) + ": " + error.what());
        animation_notice_ = std::string("Effect resolved; animation pending: ") + error.what();
    }
    battle = std::move(next);
    physical_environment_ = std::move(next_physical);
    queue_context_ = std::move(next_queue);
    execution_context_ = std::move(next_execution);
    executing_command_ = request.command;
    if (mimic_command_) mimic_command_->parameters = physical_result
        ? physical_result->selected_targets : request.command.parameters;
    action_result_ = std::move(result);
    leap_pending_ = battle.end_event == 4;
    for (unsigned i = 0; i < 6; ++i)
        if (action_result_->departures[i + 4]) pending_hide_.set(i);
    if (repeated_hit_ + 1 == repeated_count_ &&
        (command.command < 30 || command.command == 34 || command.command == 38 || command.command == 45))
        queue_retaliations(battle, physical_result.has_value(),
                           physical_result ? physical_result->interceptor_target : std::nullopt);
    if (!animation_)
        finish_selected_action(battle);
}
void BattleMenuPreview::finish_selected_action(BattleState& battle) {
    require(executing_command_.has_value() && (immediate_attack_ || execution_context_.executing.has_value()),
            "No selected action awaiting its outcome handoff");
    if (repeated_hit_ + 1 < repeated_count_) return;
    const auto slot = immediate_attack_ ? action_result_->attacker : *execution_context_.executing;
    // AfterAction2 follows the attack/outcome graphics caller. ATB reset and
    // actor return follow that refresh; doing them before the body fills the
    // visible caster gauge early and admits subsequent work prematurely.
    refresh_battle_actions(battle, commands_, magitek_.random, battle.stats[slot].level);
    refresh_battle_control_commands(battle, magic_);
    publish_battle_items(battle.inventory);
    // FixPlayerAttack reserves an Item/Throw selection only on submission.
    // Once its action finishes, a subsequent Mimic owns no such inventory item.
    if (!immediate_attack_ && slot < 4 && mimic_command_ &&
        (mimic_command_->command == 1 || mimic_command_->command == 8))
        battle.selections.items[slot] = 255;
    if (immediate_attack_)
        immediate_attack_ = false;
    else if (battle.retaliation_action) {
        remember_battle_mimic_action(execution_context_, execution_data_, slot, mimic_command_);
        finish_battle_retaliation(battle, queue_context_, execution_context_);
    } else {
        const bool returns = finish_battle_normal_action(battle, queue_context_, execution_context_,
                                                         execution_data_, mimic_command_);
        require(returns || live_, "Selected action preview unexpectedly retained a command chain");
    }
    for (unsigned i = 0; i < 6; ++i) {
        if (pending_hide_[i])
            seed_.rules.shown_monsters[i] = false;
        if (pending_show_[i])
            seed_.rules.shown_monsters[i] = true;
    }
    pending_hide_.reset();
    pending_show_.reset();
    if (action_result_)
        for (unsigned i = 0; i < 4; ++i)
            if (action_result_->departures[i]) seed_.rules.shown_characters[i] = false;
    if (action_result_ && action_result_->jumping.any()) {
        begin_battle_summon_jumps(battle, queue_context_, action_result_->jumping);
        for (unsigned i = 0; i < 4; ++i)
            if (action_result_->jumping[i]) seed_.rules.shown_characters[i] = false;
        for (unsigned i = 0; i < 6; ++i)
            if (action_result_->jumping[i + 4]) seed_.rules.shown_monsters[i] = false;
    }
    update_battle_participation(battle);
    update_battle_action_readouts(battle);
    executing_command_.reset();
    mimic_command_.reset();
}
void BattleMenuPreview::queue_retaliations(BattleState& battle, bool physical,
                                          std::optional<std::uint8_t> interceptor) {
    if (live_) {
        // CheckRetal admits decisions here; ExecAIRetal evaluates each one at
        // execution, after earlier counters have changed the battlefield.
        for (unsigned reverse = 10; reverse; --reverse) {
            const auto target = reverse - 1;
            auto& memory = battle.attack_memory[target];
            if (!memory.retaliation_pending && !battle.status.targets[target].died)
                continue;
            if (!action_result_->suppress_retaliation) {
                if (memory.retaliation_pending && memory.retaliation_attacker &&
                    *memory.retaliation_attacker != target)
                    queue_context_.ai.last_hit_targets[target] = *memory.retaliation_attacker;
                queue_battle_ai_retaliation(battle, static_cast<std::uint8_t>(target));
            }
            if (!action_result_->suppress_retaliation)
                queue_battle_retort(battle, static_cast<std::uint8_t>(target), physical);
            if (interceptor == target && !action_result_->suppress_retaliation)
                queue_battle_interceptor(battle, execution_data_.queue,
                                         static_cast<std::uint8_t>(target));
            memory.retaliation_pending = false;
        }
    }
}
void BattleMenuPreview::refresh_equipment(BattleState& battle) {
    const auto changed = refresh_battle_equipment(battle, equipment_party_, equipment_data_, commands_);
    for (unsigned slot = 0; slot < 4; ++slot)
        if (changed[slot]) physical_environment_.block_graphics[slot] = battle.block_graphics[slot][0];
}
std::vector<MusicPlayerCommand> BattleMenuPreview::advance(BattleState& battle,
                                                           const BattleParty& party,
                                                           Buttons buttons, Buttons second) {
    refresh_equipment(battle);
    if (!battle.terrain_background) battle.terrain_background = assets_.background.identity;
    if (dance_transition_ && dance_transition_->age == FF6_ROM_VALUE_BATTLE_DANCE_FADE_TOTAL_0 / 2) dance_transition_.reset();
    if (dance_transition_ && dance_transition_->age == FF6_ROM_VALUE_BATTLE_DANCE_FADE_LIMIT_0 / 2)
        assets_.background = dance_backgrounds_[dance_transition_->dance];
    seed_.rules.run_buttons = escape_held() && !battle.menus_disabled && !animation_active();
    for (unsigned i = 0; i < 6; ++i)
        seed_.rules.retained_monster_facing[i] = battle.status.targets[i + 4].reversed_facing ? 1 : 0;
    advance_selected_action(battle, party);
    auto& actor = seed_.actors;
    if (rewards_ && !menus_.active_character() && menus_.phase() == BattleMenuPhase::idle) {
        if (!victory_)
            victory_.emplace(victory_assets_, *rewards_, party, actor, seed_.rules, message_speed_,
                             victory_animation_, victory_music_);
        auto audio = victory_->advance(assets_.actors, actor, seed_.rules, buttons);
        visible_ =
            compose_battle_scene(assets_.background, assets_.readouts, seed_.readout,
                                 make_battle_actor_sprites(assets_.actors, actor.publication),
                                 {0, 1, 256, (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1)}, actor.clock);
        victory_->compose(visible_, assets_.readouts, seed_.readout);
        return audio;
    }
    if (!menu_characters_) {
        menu_characters_.emplace();
        for (unsigned i = 0; i < 4; ++i)
            (*menu_characters_)[i] =
                party.slots[i].present && party.slots[i].graphics && !party.slots[i].enemy;
    }
    std::vector<MusicPlayerCommand> event_audio;
    if (event_) {
        if (menus_.complete())
            menus_.acknowledge_command();
        if (!menus_.active_character() && menus_.phase() == BattleMenuPhase::idle)
            event_audio = event_->advance(actor, seed_.rules, party, buttons, assets_.actors);
        apply_battle_actor_appearance_changes(assets_.actors, seed_.rules,
                                              event_->take_appearance_changes(), actor);
        for (const auto change : event_->take_character_changes()) {
            const auto slot = change.character;
            if (change.menu) {
                (*menu_characters_)[slot] = change.enabled;
                gauges_ = {}; // Rows now identify different participants.
            } else if (change.enabled) {
                event_arrivals_.set(slot);
                battle.arrival_override.set(slot);
            } else {
                event_departures_.set(slot);
                battle.untargetable.set(slot);
                menus_.close_for_victory();
            }
        }
    }
    for (unsigned i = 0; i < 4; ++i)
        seed_.rules.condemned[i] = battle.turns[i].condemned;
    const auto clock = wrap8(actor.clock);
    std::optional<GameSoundEffect> death_sound;
    if (animation_ && animation_->phase() == BattleActionAnimationPhase::outcomes &&
        outcome_waits_) {
        if (death_ && !death_->complete())
            death_sound = death_->advance();
        else if (*outcome_waits_) {
            --*outcome_waits_;
            if (*outcome_waits_ == 3) {
                std::bitset<6> dying;
                for (unsigned i = 0; i < 6; ++i)
                    dying[i] = seed_.rules.shown_monsters[i] &&
                               actor.animation.monster_entry_visible[i] &&
                               actor.animation.monster_visible[i] &&
                               battle.status.targets[i + 4].monster_dead_or_escaped &&
                               !action_result_->rematerialize[i] &&
                               battle.turns[i + 4].retaliation.empty() && executing_command_ &&
                               (executing_command_->command < 30 || executing_command_->command == 34 ||
                                executing_command_->command == 38 || executing_command_->command == 45);
                if (dying.any() && repeated_hit_ + 1 == repeated_count_)
                    death_.emplace(death_palette_, dying, actor.monsters.positions);
            }
            if (!*outcome_waits_) {
                const auto hidden = pending_hide_ | (death_ ? death_->hidden() : std::bitset<6>{});
                finish_selected_action(battle);
                animation_->resume_return(hidden);
            }
        }
    }
    if (animation_ && animation_->phase() == BattleActionAnimationPhase::outcomes && !numbers_started_) {
        if (numbers_) numbers_->submit(numeral_requests_);
        else numbers_.emplace(number_assets_, numeral_requests_);
        numbers_started_ = true;
    }
    if (numbers_) {
        std::bitset<10> shown;
        for (unsigned i = 0; i < 10; ++i)
            shown[i] = i < 4 ? seed_.rules.shown_characters[i] : seed_.rules.shown_monsters[i - 4];
        if (death_)
            for (unsigned i = 0; i < 6; ++i)
                if (death_->hidden()[i])
                    shown[i + 4] = false;
        numbers_->advance(actor.anchors, shown, actor.order.monster_masks);
        // After numeral dispatch, the original caller services four ordinary
        // battle frames before STEP_BACK. Its first frame runs monster deaths;
        // the remaining three resume afterward, independently of the
        // still-bouncing numeral tasks.
        if (numbers_started_ && numbers_->dispatch_complete() && !outcome_waits_)
            outcome_waits_ = 4;
    }
    if (row_motion_) {
        auto& motion = *row_motion_;
        auto& pose = actor.characters.layout.poses[motion.character];
        if (motion.waits) {
            --motion.waits;
            pose.desired_angle =
                wrap16(static_cast<std::uint32_t>(int{pose.desired_angle} + motion.increment));
            if (!motion.waits)
                pose.action = 0;
        } else {
            row_motion_.reset();
            finish_selected_action(battle);
        }
    }
    const bool advancing_action = animation_ && !animation_->complete() && !attack_name_ && !dance_transition_;
    if (!advancing_action)
        publish_battle_actor_scene(actor, assets_.actors, seed_.rules.hide_characters);
    auto displayed_party = party;
    for (unsigned i = 0; i < 4; ++i)
        displayed_party.slots[i].present = (*menu_characters_)[i];
    auto readout = make_battle_readout_view(displayed_party, battle, seed_.hide_names,
                                            seed_.readout.show_gauges);
    readout.window = seed_.readout.window;
    readout.font_color = seed_.readout.font_color;
    // WAIT_FRAME copies HP/MP after numeral dispatch, before STEP_BACK and
    // while numeral tasks can still be bouncing. Keep the pre-impact readout
    // during casting/body playback, then refresh independently of menu input.
    if (!action_result_ || !animation_ || (outcome_waits_ && *outcome_waits_ < 4)) {
        seed_.readout = readout;
        menus_.publish_action_readout(readout);
    }
    // C1/01FB WAIT_FRAME also visits status graphics while idle. Initial
    // Vanish transitions share one prior sheet; later party members must keep
    // receiving visits after the entrance, without needing another attack.
    const bool idle_status_visit = !action_result_ && !animation_active() && !event_;
    if (idle_status_visit ||
        (action_result_ && !animation_active() && repeated_hit_ + 1 == repeated_count_)) {
        for (unsigned i = 0; i < 6; ++i) {
            seed_.rules.shown_monsters[i] =
                battle.status.targets[i + 4].present && !battle.away_from_battle[i + 4] &&
                (!battle.status.targets[i + 4].monster_dead_or_escaped ||
                 !battle.turns[i + 4].retaliation.empty()) &&
                !(battle.status.targets[i + 4].active & status_mask(BattleStatus::hide));
            seed_.rules.invisible_monsters[i] =
                (battle.status.targets[i + 4].active & status_mask(BattleStatus::vanish)) != 0;
        }
        for (unsigned i = 0; i < 4; ++i)
            actor.characters.control[i].active_status = battle.character_display[i].status;
        update_battle_character_status(assets_.actors.graphics, {seed_.rules.magitek, false},
                                       actor.characters);
    }
    std::array<BattleCounterReadout, 4> counters{};
    for (unsigned i = 0; i < 4; ++i)
        if (readout.characters[i])
            counters[i] = readout.characters[i]->counters;
    advance_battle_gauge_publication(gauges_, counters);
    for (unsigned i = 0; i < 4; ++i)
        if (seed_.readout.characters[i])
            seed_.readout.characters[i]->counters = gauges_.visible[i];
    menus_.update_counters(seed_.readout);
    // Actor preparation advances the logical animation clock and anchors. The
    // cursor and strip phase use the interrupt's preceding clock value.
    std::optional<GameSoundEffect> attack_sound;
    std::optional<PlaybackVolumeRamp> transition_volume;
    std::optional<SystemSoundEffect> interception_sound;
    if (advancing_action) {
        const auto frame = animation_->advance(assets_.actors, actor, seed_.rules);
        attack_sound = frame.sound;
        transition_volume = frame.volume;
        interception_sound = frame.system_sound;
        const auto& context = animation_->context();
        if (context.character_back_rows)
            for (unsigned i = 0; i < 4; ++i)
                battle.entry.displayed_back_rows[i] = (*context.character_back_rows)[i];
        menus_.set_graphics_random_index(context.random_index);
        hide_character_indicator_ = context.hide_character_indicator;
        display_ = context.display;
    } else
        prepare_battle_actor_scene(assets_.actors, seed_.rules, actor);
    BattleMenuContext context;
    context.joker_doom = joker_doom_;
    context.anchors = actor.anchors;
    context.positions = actor.monsters;
    context.character_view = assets_.actors.view;
    context.poses = actor.characters.layout.poses;
    context.graphics = actor.characters.layout.graphics_ids;
    context.clock = clock;
    context.magitek = seed_.rules.magitek;
    // The original gates only the selected-character arrow. Menu hands and
    // target eligibility are independent of this temporary drawing state.
    context.hide_character_indicator = hide_character_indicator_ || seed_.rules.hide_characters;
    unsigned row = 0;
    for (unsigned i = 0; i < 4; ++i) {
        context.shown_status[i] = actor.characters.control[i].shown_status;
        if (!seed_.hide_names && (*menu_characters_)[i])
            context.text_rows[i] = static_cast<std::uint8_t>(row++);
        // Build physical visibility first; the menu alias is applied below.
        context.visible.characters[i] = seed_.rules.shown_characters[i];
        context.eligible.characters[i] =
            context.visible.characters[i] && battle.participation.targetable[i];
    }
    for (unsigned i = 0; i < 6; ++i) {
        context.visible.monsters[i] = seed_.rules.shown_monsters[i];
        context.eligible.monsters[i] = context.visible.monsters[i] &&
                                       battle.participation.targetable[i + 4] &&
                                       battle.monster_targets[i];
    }
    if (automatic_roulette_ && !attack_name_) {
        auto eligible = context.visible;
        for (unsigned i = 0; i < 4; ++i)
            eligible.characters[i] = eligible.characters[i] &&
                !(battle.status.targets[i].active & status_mask(BattleStatus::dead));
        auto random_index = menus_.graphics_random_index();
        if (const auto target = advance_battle_automatic_roulette(*automatic_roulette_, eligible,
                clock, magic_.random, random_index))
            battle.time.roulette_target = *target;
        menus_.set_graphics_random_index(random_index);
        if (automatic_roulette_->complete) automatic_roulette_.reset();
    }
    prepare_battle_menu_targets(context, seed_.enemy_character_proxy, battle.entry.approach);
    auto audio = menus_.advance(battle, context, buttons, second);
    refresh_equipment(battle);
    audio.insert(audio.end(), event_audio.begin(), event_audio.end());
    // UpdateSfx gives the one pending animation cue priority over menu sounds.
    if (script_sound_) {
        audio.clear();
        audio.push_back(*script_sound_);
        script_sound_.reset();
    }
    if (interception_sound) {
        audio.clear();
        audio.push_back(*interception_sound);
    }
    if (attack_sound) {
        audio.clear();
        audio.push_back(*attack_sound);
    }
    if (death_sound) {
        require(!attack_sound, "Death and attack cues have overlapping owners");
        audio.clear();
        audio.push_back(*death_sound);
    }
    if (transition_volume) audio.push_back(*transition_volume);
    auto groups = make_battle_actor_sprite_groups(assets_.actors, actor.publication,
                                                  event_       ? event_->actor_palettes()
                                                  : animation_ ? animation_->actor_palettes()
                                                               : BattleActorPaletteOverrides{});
    if (death_)
        death_->apply_to(groups);
    if (numbers_)
        numbers_->add_to(groups);
    visible_ = compose_battle_scene(assets_.background, assets_.readouts, seed_.readout, {},
                                  {0, 1, 256, (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1)}, clock);
    if (dance_transition_) {
        // ChangeDanceBattleBG C1/BAC6 subtracts 0..30 then 30..2 from each
        // terrain color component. Actors and menu colors remain untouched.
        const auto age = dance_transition_->age++;
        const unsigned amount = age < FF6_ROM_VALUE_BATTLE_DANCE_FADE_LIMIT_0 / 2 ? age * 2 : (FF6_ROM_VALUE_BATTLE_DANCE_FADE_TOTAL_0 / 2 - age) * 2;
        for (auto& palette : visible_.layers.front().palettes) for (auto& color : palette) {
            const auto darken = [&](unsigned shift) {
                const unsigned value = (color >> shift) & 31U;
                return (value > amount ? value - amount : 0U) << shift;
            };
            color = static_cast<Rgb555>(darken(0) | darken(5) | darken(10));
        }
    }
    visible_.layers.resize(1);
    for (auto& layer : menus_.layers())
        visible_.layers.push_back(std::move(layer));
    if (event_)
        event_->compose_effects(visible_, groups);
    else if (animation_ && !dance_transition_)
        animation_->compose(visible_, groups);
    else
        for (const auto& group : groups)
            for (const auto& piece : group.sprites)
                visible_.objects->sprites.push_back(piece.image);
    if (!event_ && !animation_)
        apply_battle_effect_display(display_, visible_);
    if (death_)
        death_->compose(visible_);
    if (!actor.animation.fixed_draw_order)
        stage_battle_draw_order(actor.order, actor.anchors);
    auto cursors = menus_.cursor_sprites();
    if (automatic_roulette_ && automatic_roulette_->target)
        if (const auto sprite = menus_.roulette_sprite(*automatic_roulette_->target,
                actor.anchors, actor.monsters))
            cursors.before_actors.push_back(*sprite);
    auto& objects = visible_.objects->sprites;
    // Actor pixels retain the battlefield clip. Menu hands share their object
    // ordering but extend into the lower windows, so a global field-only clip
    // would erase them. Clip in destination space before opening the plane.
    for (auto& sprite : objects)
        for (unsigned y = 0; y < sprite.height; ++y)
            for (unsigned x = 0; x < sprite.width; ++x) {
                const auto dx =
                    sprite.x + static_cast<int>(sprite.flip_x ? sprite.width - 1 - x : x);
                const auto dy =
                    sprite.y + static_cast<int>(sprite.flip_y ? sprite.height - 1 - y : y);
                if (dx < FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_0 || dx >= FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_1 + 1 ||
                    dy < FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 || dy >= (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1))
                    sprite.pixels[y * sprite.width + x] = 0;
            }
    objects.insert(objects.begin(), cursors.before_actors.begin(), cursors.before_actors.end());
    objects.insert(objects.end(), cursors.after_actors.begin(), cursors.after_actors.end());
    visible_.objects->lines.assign(224, SpriteLine{});
    if (message_) {
        compose_battle_message(visible_, assets_.readouts, seed_.readout, victory_assets_->messages,
                               message_->message(), false, message_->scroll());
        message_->advance(buttons);
    }
    if (attack_name_) {
        compose_battle_message(visible_, assets_.readouts, seed_.readout, victory_assets_->messages,
                               attack_name_->message());
        attack_name_->advance(buttons);
    }
    if (event_)
        event_->compose(visible_, assets_.readouts, seed_.readout, victory_assets_->messages);
    return audio;
}
} // namespace ff6
