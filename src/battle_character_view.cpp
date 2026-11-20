#include "ff6/battle_character_view.hpp"
#include "rom-materials/battle_character_initial_styles.macros.inc"
#include "rom-materials/battle_character_running_action.macros.inc"
#include "rom-materials/battle_character_image_copy_offsets.macros.inc"
#include "rom-materials/battle_magitek_rider_offset.macros.inc"
#include "rom-materials/battle_character_projection_origin_y.macros.inc"
#include "rom-materials/battle_character_projection_origin_x.macros.inc"
#include "rom-materials/battle_coliseum_character_y.macros.inc"
#include <algorithm>
#include <stdexcept>
namespace ff6 {
namespace {
// Preserve the native affine form only while every ROM assignment agrees.
// This also authenticates assignments folded into compiler constant pools.
static_assert(FF6_ROM_VALUE_BATTLE_CHARACTER_INITIAL_STYLES_1 ==
              FF6_ROM_VALUE_BATTLE_CHARACTER_INITIAL_STYLES_0 + 1U);
static_assert(FF6_ROM_VALUE_BATTLE_CHARACTER_INITIAL_STYLES_2 ==
              FF6_ROM_VALUE_BATTLE_CHARACTER_INITIAL_STYLES_0 + 2U);
static_assert(FF6_ROM_VALUE_BATTLE_CHARACTER_INITIAL_STYLES_3 ==
              FF6_ROM_VALUE_BATTLE_CHARACTER_INITIAL_STYLES_0 + 3U);
void require(bool value, const char* message) {
    if (!value)
        throw std::invalid_argument(message);
}
void valid_style(BattleSpriteStyle s) {
    require(s.palette < 8 && s.priority < 4, "Invalid character appearance");
}
BattleSpriteStyle facing_style(const BattleCharacterPose& pose) {
    valid_style(pose.appearance);
    require(pose.facing < 4, "Invalid character facing");
    auto style = pose.appearance;
    style.flip_x = style.flip_x || (((pose.facing & 1U) != 0) != (pose.current_frame >= 48));
    style.flip_y = style.flip_y || (pose.facing & 2U) != 0;
    return style;
}
std::uint16_t sum_y(const BattleCharacterPose& pose, std::uint16_t floating, bool jump_pixel) {
    unsigned carry = 0;
    auto y = pose.y;
    // Each addition after the first retains carry. A single signed sum would
    // move ordinary Magitek riders by the wrong pixel at positive coordinates.
    for (auto value : {pose.offset_y, pose.jump_y, floating,
                       static_cast<std::uint16_t>(jump_pixel ? 65535 : 0)}) {
        const unsigned sum = unsigned{y} + value + carry;
        y = wrap16(sum);
        carry = sum > 65535U ? 1U : 0U;
    }
    return y;
}
template <std::size_t N>
RasterSprite sprite(BattleScreenPoint point, BattleSpriteStyle style,
                    const std::array<Tile, N>& frame, unsigned columns,
                    const std::array<Rgb555, 16>& colors) {
    valid_style(style);
    RasterSprite out;
    out.x = point.x;
    out.y = point.y;
    out.width = out.height = static_cast<std::uint16_t>(columns * 8);
    constexpr std::array<std::uint16_t, 4> depths{2, 3, 6, 9};
    out.depth = depths[style.priority];
    out.colors = colors;
    out.flip_x = style.flip_x;
    out.flip_y = style.flip_y;
    for (unsigned y = 0; y < out.height; ++y)
        for (unsigned x = 0; x < out.width; ++x)
            out.pixels.push_back(frame[(y / 8) * columns + x / 8][(y % 8) * 8 + x % 8]);
    return out;
}
} // namespace
BattleCharacterViewData load_battle_character_view_data(const ContentPack& pack) {
    const auto actions = pack.get("battle_character_actions"),
               pieces = pack.get("battle_character_pieces"),
               positions = pack.get("battle_character_positions"),
               flags = pack.get("battle_character_layout_flags"),
               floating = pack.get("battle_character_float_offsets"),
               armor = pack.get("battle_armor_piece_layouts");
    require(actions.size() == 156 && pieces.size() == 64 && positions.size() == 84 &&
                flags.size() == 36 && floating.size() == 8 && armor.size() == 16,
            "Invalid character view resource extent");
    BattleCharacterViewData out;
    for (unsigned i = 0; i < 156; ++i)
        out.actions[i / 4][i % 4] = actions[i];
    for (unsigned facing = 0; facing < 4; ++facing)
        for (unsigned shape = 0; shape < 2; ++shape)
            for (unsigned part = 0; part < 2; ++part) {
                const auto at = facing * 16 + shape * 8 + part * 4;
                require((pieces[at + 2] == 0 || pieces[at + 2] == 2) && (pieces[at + 3] & 63U) == 0,
                        "Invalid character piece selection");
                out.pieces[facing][shape][part] = {
                    static_cast<std::int16_t>(signed8(pieces[at])),
                    static_cast<std::int16_t>(signed8(pieces[at + 1])),
                    static_cast<std::uint8_t>(pieces[at + 2] / 2), (pieces[at + 3] & 64U) != 0,
                    (pieces[at + 3] & 128U) != 0};
            }
    for (unsigned group = 0; group < 4; ++group)
        for (unsigned slot = 0; slot < 4; ++slot) {
            const auto i = group * 4 + slot;
            out.row_offsets[group][slot] = read16(positions, i * 2);
            out.angles[group][slot] = read16(positions, 52 + i * 2);
            require(flags[16 + i] < 2, "Invalid character hand ordering");
            out.swap_hands[group][slot] = flags[16 + i] != 0;
        }
    for (unsigned i = 0; i < 20; ++i) {
        out.y[i / 4][i % 4] = positions[32 + i];
        // Train's fifth group intentionally reads the following hand-order table.
        require((flags[i] & ~64U) == 0, "Invalid character initial facing");
        out.facing[i / 4][i % 4] = flags[i] >> 6U;
    }
    for (unsigned i = 0; i < 4; ++i) {
        require(flags[32 + i] % 2 == 0 && flags[32 + i] < 16, "Invalid armor palette slot");
        out.armor_palettes[i] = flags[32 + i] / 2;
        require(armor[i * 4 + 2] == 12 + i * 64 && armor[i * 4 + 3] == 1,
                "Invalid armor piece ownership");
        out.armor_offsets[i] = {static_cast<std::int16_t>(signed8(armor[i * 4])),
                                static_cast<std::int16_t>(signed8(armor[i * 4 + 1]))};
    }
    for (unsigned i = 0; i < 8; ++i)
        out.floating_offsets[i] = static_cast<std::int16_t>(std::int32_t{floating[i]} - 256);
    return out;
}
void initialize_battle_character_layout(const BattleCharacterViewData& view,
                                        const BattleCharacterGraphicsData& graphics,
                                        const BattleCharacterLayoutRequest& request,
                                        BattleCharacterLayout& state) {
    const auto approach = static_cast<unsigned>(request.approach);
    require(approach < 4, "Invalid character approach");
    auto next = state;
    const auto group = request.magitek_train ? 4U : approach;
    for (unsigned i = 0; i < 4; ++i) {
        auto& p = next.poses[i];
        p.y = view.y[group][i];
        p.facing = view.facing[group][i];
        next.graphics_ids[i] = request.graphics[i];
        prepare_battle_character_graphics(graphics,
                                          {request.graphics[i], request.graphics[i],
                                           request.characters[i], request.green_soldier_disguise},
                                          next.graphics[i]);
        next.swap_hands[i] = view.swap_hands[approach][i] != request.enemy[i];
        p.desired_angle = view.angles[approach][i];
        p.current_angle = wrap16(unsigned{p.desired_angle} + 1U);
        next.back_row[i] = request.back_row[i];
        if (request.back_row[i])
            p.desired_angle = wrap16(unsigned{p.desired_angle} + view.row_offsets[approach][i]);
        // The original word clear spans the jump high byte and animation-x low
        // byte. It does not reset either complete semantic offset.
        p.jump_y &= 255U;
        p.animation_x &= 0xff00U;
        if (request.colosseum && i == 0)
            p.y = FF6_ROM_VALUE_BATTLE_COLISEUM_CHARACTER_Y_0;
        if (request.overrides[i]) {
            require(request.overrides[i]->angle != 65535, "Invalid placement override sentinel");
            p.desired_angle = wrap16(unsigned{request.overrides[i]->angle} + 128U);
            p.y = request.overrides[i]->y;
        }
        p.appearance = {
            static_cast<std::uint8_t>(i + (FF6_ROM_VALUE_BATTLE_CHARACTER_INITIAL_STYLES_0 & 255U)),
            static_cast<std::uint8_t>(FF6_ROM_VALUE_BATTLE_CHARACTER_INITIAL_STYLES_0 >> 8U),
            false, false};
        p.status_action = FF6_ROM_VALUE_BATTLE_CHARACTER_DEFAULT_STATUS_0;
        p.action = p.frame_override = 0;
        p.counter = static_cast<std::uint8_t>(i * 2);
        if (request.enemy[i])
            p.facing ^= 1U;
    }
    unsigned empty = 0;
    while (empty < 4 && request.graphics[empty] != 255)
        ++empty;
    if (empty == 4)
        empty = 0;
    next.armor_palette = view.armor_palettes[empty];
    next.palette_phase = 0;
    if (request.magitek) {
        next.graphics[empty].colors = graphics.magitek_colors;
        next.visible_colors[empty] = graphics.magitek_colors;
    }
    state = std::move(next);
}
std::uint16_t calculate_battle_character_x(std::uint8_t y, std::uint16_t angle) {
    const unsigned distance = wrap8(unsigned{y} + FF6_ROM_VALUE_BATTLE_CHARACTER_PROJECTION_ORIGIN_Y_0), a = angle & 511U;
    if (a >= 256)
        return wrap16(FF6_ROM_VALUE_BATTLE_CHARACTER_PROJECTION_ORIGIN_X_0 + ((a - 256U) * distance >> 8U));
    // Ones-complement magnitude and result are intentional, including the -1.
    return wrap16((FF6_ROM_VALUE_BATTLE_CHARACTER_PROJECTION_ORIGIN_X_0 - 1U) - ((255U - a) * distance >> 8U));
}
void update_battle_character_x(std::array<BattleCharacterPose, 4>& poses) {
    for (auto& p : poses)
        if (p.desired_angle != p.current_angle) {
            p.current_angle = p.desired_angle;
            p.x = calculate_battle_character_x(wrap8(p.y), p.desired_angle);
        }
}
BattleCharacterDraw draw_battle_character(const BattleCharacterViewData& data,
                                          const BattleCharacterDrawContext& context,
                                          BattleCharacterPose& pose) {
    auto next = pose;
    if (context.image_copy)
        next.counter = wrap8(unsigned{next.counter} - 1U);
    const auto blocked = status_mask(BattleStatus::dead) | status_mask(BattleStatus::petrify) |
                         status_mask(BattleStatus::zombie) | status_mask(BattleStatus::sleep) |
                         status_mask(BattleStatus::stop) | status_mask(BattleStatus::morph);
    const bool run = !context.run_animation_blocked &&
                     (context.forced_running_background || context.run_buttons) &&
                     !context.blocking_animation && !context.acting && !context.enemy &&
                     (context.shown_status & blocked) == 0;
    bool jump_pixel = false;
    if (!run && next.frame_override != 0)
        next.current_frame = next.frame_override;
    else {
        const auto action = run                ? (FF6_ROM_VALUE_BATTLE_CHARACTER_RUNNING_ACTION_0 / 4U)
                            : next.action != 0 ? unsigned{next.action}
                                               : unsigned{next.status_action};
        require(action < data.actions.size(), "Invalid character animation action");
        auto phase = unsigned{next.counter} >> (run || next.double_speed ? 2U : 3U);
        if (context.magitek && context.battle_time_stopped)
            phase >>= 1U;
        next.current_frame = data.actions[action][phase & 3U];
        jump_pixel = (next.current_frame & 31U) == 7;
    }
    const auto style = facing_style(next);
    const unsigned orientation = unsigned{style.flip_x} | (unsigned{style.flip_y} << 1U);
    const auto frame = next.current_frame >= 48 ? next.current_frame - 48 : next.current_frame;
    const unsigned shape = frame == 0 ? 1U : 0U;
    const auto floating = wrap16(static_cast<std::uint32_t>(
        battle_character_y_offset(data, context.shown_status, next, context.magitek)));
    const auto image_x = context.image_copy ? (style.flip_x
        ? (65536U - FF6_ROM_VALUE_BATTLE_CHARACTER_IMAGE_COPY_OFFSETS_0)
        : FF6_ROM_VALUE_BATTLE_CHARACTER_IMAGE_COPY_OFFSETS_0) : 0U;
    const auto x = wrap16(unsigned{next.x} + next.animation_x + next.offset_x + image_x);
    const auto y = sum_y(next, floating, jump_pixel);
    next.counter = wrap8(unsigned{next.counter} + 1U);
    BattleCharacterDraw out;
    for (unsigned i = 0; i < 2; ++i) {
        const auto piece = data.pieces[orientation][shape][i];
        require(piece.part < 2, "Invalid character piece");
        auto appearance = style;
        appearance.flip_x = appearance.flip_x || piece.flip_x;
        appearance.flip_y = appearance.flip_y || piece.flip_y;
        out[i] = {project_battle_piece(x, y, piece.x, piece.y), piece.part, appearance};
    }
    pose = next;
    return out;
}
BattleArmorDraw draw_battle_armor(const BattleCharacterViewData& view,
                                  const BattleCharacterGraphicsData& graphics,
                                  const BattleCharacterPose& pose, std::uint8_t slot,
                                  std::uint8_t action, std::uint8_t counter, bool time_stopped,
                                  std::uint8_t palette, std::uint8_t& retained_rider_offset,
                                  BattleCharacterDraw& rider) {
    require(slot < 4 && palette < 8, "Invalid armor draw selection");
    const auto update = select_battle_magitek_frame(graphics, slot, action, counter, time_stopped);
    const auto bob = update ? update->rider_offset_y : retained_rider_offset;
    auto style = facing_style(pose);
    style.palette = palette;
    const auto offset = view.armor_offsets[slot];
    BattleArmorDraw out{
        project_battle_piece(wrap16(unsigned{pose.x} + pose.offset_x + pose.animation_x),
                             wrap16(unsigned{pose.y} + pose.offset_y + pose.jump_y), offset.x,
                             offset.y),
        style, update};
    for (auto& p : rider) {
        const auto y = wrap8(static_cast<std::uint32_t>(p.position.y) + bob);
        p.position.y =
            static_cast<std::int16_t>(y >= 224 ? std::int32_t{y} - 256 : std::int32_t{y});
    }
    retained_rider_offset = bob;
    return out;
}
std::array<RasterSprite, 2> make_battle_character_sprites(const BattleCharacterDraw& draw,
                                                          const BattleCharacterFrame& frame,
                                                          const std::array<Rgb555, 16>& colors) {
    std::array<RasterSprite, 2> out;
    for (unsigned i = 0; i < 2; ++i) {
        require(draw[i].part < 2, "Invalid character part");
        std::array<Tile, 4> tiles;
        std::copy_n(frame.begin() + draw[i].part * 4, 4, tiles.begin());
        out[i] = sprite(draw[i].position, draw[i].style, tiles, 2, colors);
    }
    return out;
}
std::int16_t battle_character_y_offset(const BattleCharacterViewData& data,
                                       std::uint32_t shown_status, const BattleCharacterPose& pose,
                                       bool magitek) {
    if (magitek) {
        // Selected signed-word displacement; decode without relying on
        // implementation-defined unsigned-to-signed narrowing.
        constexpr auto raw = FF6_ROM_VALUE_BATTLE_MAGITEK_RIDER_OFFSET_0;
        return static_cast<std::int16_t>(raw < 32768U ? int(raw) : int(raw) - 65536);
    }
    if ((shown_status & status_mask(BattleStatus::floating)) != 0 && !pose.disable_float &&
        pose.frame_override == 0)
        return data.floating_offsets[(pose.counter >> 3U) & 7U];
    return 0;
}
RasterSprite make_battle_armor_sprite(const BattleArmorDraw& draw,
                                      const std::array<Tile, 16>& frame,
                                      const std::array<Rgb555, 16>& colors) {
    return sprite(draw.position, draw.style, frame, 4, colors);
}
} // namespace ff6
