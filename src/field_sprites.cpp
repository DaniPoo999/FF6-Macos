#include "rom-materials/field_character_x_origin.macros.inc"
#include "rom-materials/field_character_upper_y.macros.inc"
#include "rom-materials/field_character_y_cull.macros.inc"
#include "rom-materials/field_viewport_bounds.macros.inc"
#include "ff6/field_sprites.hpp"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::invalid_argument(message);
}
void valid_pose(std::uint8_t value) {
    require(value < 128 && (value & 63U) < 58, "Undefined field sprite pose");
}
std::size_t first_slot(std::uint8_t event_visits) {
    return (std::size_t{event_visits} % 4) * 6;
}
} // namespace
std::uint8_t select_field_pose(const FieldPoseTables& tables, const FieldPoseInput& input,
                               std::uint8_t previous) {
    require(input.facing <= FieldFacing::left && input.ride <= FieldRide::raft,
            "Invalid field pose direction or ride");
    std::uint8_t selected = previous;
    if (input.special) {
        const auto& special = *input.special;
        require(special.speed < 4 && special.offset < tables.special.size() &&
                    special.cycle <= FieldSpecialCycle::four,
                "Invalid special pose selection");
        const auto phase = unsigned{input.logic_count} >> (2U + special.speed);
        unsigned offset = 0;
        switch (special.cycle) {
        case FieldSpecialCycle::still:
            break;
        case FieldSpecialCycle::alternating_flip:
            offset = (phase & 1U) * 64U;
            break;
        case FieldSpecialCycle::two:
            offset = phase & 1U;
            break;
        case FieldSpecialCycle::four:
            offset = phase & 3U;
            break;
        }
        selected = wrap8(unsigned{tables.special[special.offset]} + offset);
    } else {
        const auto facing = input.party_on_ladder ? FieldFacing::up : input.facing;
        const auto base = static_cast<std::size_t>(facing) * 4;
        unsigned phase = 0;
        if (input.moving_x)
            phase = (unsigned{input.x} & 255U) / 8;
        else if (input.moving_y)
            phase = (unsigned{input.y} & 255U) / 8;
        if (input.ride == FieldRide::raft)
            selected = tables.walking[base];
        else if (input.ride != FieldRide::none)
            selected = tables.vehicle[base + phase % 4];
        else if (input.walking && (input.moving_x || input.moving_y)) {
            if (input.diagonal_tile && input.moving_x && input.moving_y)
                phase = unsigned{input.vblank_count} / 4;
            selected = tables.walking[base + phase % 4];
        }
    }
    valid_pose(selected);
    return selected;
}
FieldSpriteTiles FieldSpriteAssets::pose(std::uint8_t graphics_id, std::uint8_t pose_id) const {
    valid_pose(pose_id);
    require(graphics_id < graphics.size(), "Invalid field sprite graphics identity");
    FieldSpriteTiles result;
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto index = std::uint64_t{graphics[graphics_id]} + pose_tiles[pose_id & 63U][i];
        require(index < tiles.size(), "Field sprite pose exceeds known graphics");
        result[i] = tiles[static_cast<std::size_t>(index)];
    }
    return result;
}
FieldSpriteAssets load_field_sprites(const ContentPack& pack) {
    const auto selectors = pack.get("field_object_pose_selection");
    const auto tables = pack.get("field_sprite_pose_tables");
    const auto raw = pack.get("map_sprite_map_sprite_gfx_4bpp");
    const auto vehicles = pack.get("field_overlay_vehicle_gfx_4bpp");
    const auto colors = pack.get("map_sprite_map_sprite_pal_pal");
    require(selectors.size() == 44 && tables.size() == 1612 && raw.size() == 208896 &&
                vehicles.size() == 8192 && colors.size() == 1024,
            "Invalid field sprite resource extent");
    FieldSpriteAssets assets;
    std::ranges::copy(selectors.first(16), assets.selection.vehicle.begin());
    std::ranges::copy(selectors.subspan(16, 16), assets.selection.walking.begin());
    std::ranges::copy(selectors.subspan(32, 4), assets.selection.standing.begin());
    std::ranges::copy(selectors.subspan(36, 8), assets.selection.special.begin());
    for (auto value : selectors)
        valid_pose(value);
    for (std::size_t i = 0; i < 128; ++i) {
        require((tables[i] & ~64U) == 0 && (tables[128 + i] & ~64U) == 0,
                "Invalid field sprite flip flag");
        assets.upper_flip[i] = tables[i] != 0;
        assets.lower_flip[i] = tables[128 + i] != 0;
    }
    for (std::size_t pose = 0; pose < 58; ++pose)
        for (std::size_t tile = 0; tile < 6; ++tile) {
            const auto offset = read16(tables, 256 + pose * 12 + tile * 2);
            require(offset % 32 == 0, "Unaligned field sprite tile window");
            assets.pose_tiles[pose][tile] = offset / 32;
        }
    // The canonical pools are adjacent. Preserve cross-resource reads while
    // keeping all pointer normalization inside content loading.
    assets.tiles = decode_tiles(raw, 4);
    const auto vehicle_tiles = decode_tiles(vehicles, 4);
    assets.tiles.insert(assets.tiles.end(), vehicle_tiles.begin(), vehicle_tiles.end());
    for (std::size_t i = 0; i < assets.graphics.size(); ++i) {
        const auto address = std::uint32_t{read16(tables, 952 + i * 2)} |
                             (std::uint32_t{tables[1282 + i * 2]} << 16U);
        require(address >= 0xd50000 && address < 0xd83000 && address % 32 == 0 &&
                    tables[1283 + i * 2] == 32,
                "Invalid field sprite graphics pointer or tile size");
        assets.graphics[i] = (address - 0xd50000) / 32;
        for (const auto& pose : assets.pose_tiles)
            for (auto tile : pose)
                require(std::uint64_t{assets.graphics[i]} + tile < assets.tiles.size(),
                        "Field sprite read exceeds classified graphics");
    }
    for (std::size_t palette = 0; palette < 32; ++palette)
        for (std::size_t color = 0; color < 16; ++color)
            assets.palettes[palette][color] = read16(colors, palette * 32 + color * 2) & 0x7fffU;
    return assets;
}
void latch_field_poses(std::uint8_t event_visits, const FieldSpriteQueue& queue,
                       std::span<FieldSpriteObject> objects) {
    const auto first = first_slot(event_visits);
    for (auto i = first; i < first + 6; ++i)
        if (queue[i]) {
            require(*queue[i] < objects.size(), "Invalid field sprite queue owner");
            valid_pose(objects[*queue[i]].pending_pose);
        }
    for (auto i = first; i < first + 6; ++i)
        if (queue[i])
            objects[*queue[i]].pose = objects[*queue[i]].pending_pose;
}
void FieldSpritePublication::publish(std::uint8_t event_visits, const FieldSpriteAssets& assets,
                                     const FieldSpriteQueue& queue,
                                     std::span<const FieldSpriteObject> objects) {
    const auto first = first_slot(event_visits);
    // Validate and prepare the entire group before publishing any replacement.
    auto next = visible_;
    for (auto i = first; i < first + 6; ++i)
        if (queue[i]) {
            require(*queue[i] < objects.size(), "Invalid field sprite queue owner");
            const auto& object = objects[*queue[i]];
            next[i] = assets.pose(object.graphics, object.pose);
        }
    visible_ = std::move(next);
}
std::vector<RasterSprite>
make_field_character(const FieldSpriteAssets& assets, const FieldSpriteTiles& tiles,
                     std::uint8_t latched_pose, const FieldSpritePosition& position,
                     FieldSpriteAppearance upper, FieldSpriteAppearance lower,
                     const std::array<std::array<Rgb555, 16>, 8>& colors, CompositeBlend mix) {
    valid_pose(latched_pose);
    require(upper.palette < 8 && lower.palette < 8 && upper.priority < 4 && lower.priority < 4,
            "Invalid field character appearance");
    // Both background-scrolling paths select the same origin and cull offsets.
    const auto x = wrap16(unsigned{position.x} + 65536U - position.scroll_x +
                          FF6_ROM_VALUE_FIELD_CHARACTER_X_ORIGIN_0);
    // Original starts the first vertical subtraction with borrow; subsequent
    // subtractions explicitly clear it. Keep the resulting extra -1 pixel.
    const auto bottom = wrap16(unsigned{position.y} + 4U * 65536U - position.scroll_y - 1U -
                               position.shake_y - position.jump_y);
    if (wrap16(unsigned{x} + FF6_ROM_VALUE_FIELD_CHARACTER_X_ORIGIN_0) >= 256 ||
        wrap16(unsigned{bottom} + FF6_ROM_VALUE_FIELD_CHARACTER_Y_CULL_0) >= 256)
        return {};
    const auto screen_x = x >= 32768 ? std::int32_t{x} - 65536 : std::int32_t{x};
    const std::array appearances{upper, lower};
    const std::array flips{assets.upper_flip[latched_pose], assets.lower_flip[latched_pose]};
    constexpr std::array<std::uint16_t, 4> depths{2, 3, 6, 9};
    std::vector<RasterSprite> result;
    for (std::size_t part = 0; part < 2; ++part) {
        const auto appearance = appearances[part];
        RasterSprite sprite;
        sprite.width = sprite.height = 16;
        sprite.pixels.resize(256, 0);
        for (std::size_t y = 0; y < (part == 0 ? 16U : 8U); ++y)
            for (std::size_t x_pixel = 0; x_pixel < 16; ++x_pixel)
                sprite.pixels[y * 16 + x_pixel] =
                    tiles[part * 4 + y / 8 * 2 + x_pixel / 8][y % 8 * 8 + x_pixel % 8];
        sprite.x = screen_x;
        const auto raw_y = wrap8(unsigned{bottom} + 256U -
            (part == 0 ? static_cast<unsigned>(FF6_ROM_VALUE_FIELD_CHARACTER_UPPER_Y_0) : 0U));
        sprite.y = raw_y >= 224 ? std::int32_t{raw_y} - 256 : std::int32_t{raw_y};
        sprite.colors = colors[appearance.palette];
        sprite.depth = depths[appearance.priority];
        sprite.flip_x = appearance.flip_x || flips[part];
        sprite.flip_y = appearance.flip_y;
        sprite.mix = appearance.palette >= 4 ? mix : CompositeBlend{};
        sprite.allow_color_effect = appearance.palette >= 4;
        result.push_back(std::move(sprite));
    }
    return result;
}
SpriteLayer make_field_sprite_layer(std::vector<RasterSprite> sprites, FieldLayerStyle style) {
    SpriteLayer layer;
    layer.sprites = std::move(sprites);
    layer.visible = style.visible;
    layer.operand = style.operand;
    layer.clip_operand = false;
    layer.lines.resize(224);
    for (std::size_t y = 0; y < 224; ++y) {
        layer.lines[y].left = FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_0;
        layer.lines[y].right = FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_1 + 1;
        layer.lines[y].enabled = y >= FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_3 && y < FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_2;
    }
    return layer;
}
} // namespace ff6
