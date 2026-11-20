#include "rom-materials/field_sprite_baseline.macros.inc"
#include "rom-materials/field_object_center_x.macros.inc"
#include "rom-materials/field_magitek_rider_height.macros.inc"
#include "rom-materials/field_raft_rider_height.macros.inc"
#include "rom-materials/field_chocobo_rider_height.macros.inc"
#include "rom-materials/field_vehicle_clip_bias.macros.inc"
#include "rom-materials/field_chocobo_clip_bias.macros.inc"
#include "rom-materials/field_chocobo_rider_offset.macros.inc"
#include "rom-materials/field_rider_priority.macros.inc"
#include "rom-materials/field_relocated_hidden_coordinate.macros.inc"
#include "rom-materials/field_vehicle_graphics_size.macros.inc"
#include "rom-materials/field_vehicle_first_tile.macros.inc"
#include "rom-materials/field_portrait_clear_tiles.macros.inc"
#include "rom-materials/field_portrait_palette.macros.inc"
#include "ff6/field_object_view.hpp"
#include "ff6/field_overlay.hpp"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::invalid_argument(message);
}
std::uint16_t pixel(std::uint32_t position) {
    require(position <= 0xffffff, "Field position exceeds unsigned 16.8 range");
    return static_cast<std::uint16_t>(position >> 8U);
}
std::uint32_t moved(std::uint32_t position, std::int16_t velocity) {
    pixel(position);
    return static_cast<std::uint32_t>(std::int64_t{position} + velocity) & 0xffffffU;
}
std::array<FieldSpriteObject, 49> pose_objects(const FieldObjects& field) {
    std::array<FieldSpriteObject, 49> result;
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = {field.objects[i].graphics, field.objects[i].pending_pose,
                     field.objects[i].pose};
    return result;
}
std::uint8_t ordered_object(const FieldObjects& field, std::size_t i) {
    require(field.update_count <= field.update_order.size() && field.update_order[i] &&
                *field.update_order[i] < field.objects.size(),
            "Invalid field update order");
    return *field.update_order[i];
}
FieldSpriteAppearance appearance(const FieldAppearance& value) {
    require(!value.tile_bank, "Ordinary field character uses an unresolved graphics bank");
    return {value.palette, value.priority, value.flip_x, value.flip_y};
}
std::int32_t vertical(std::uint32_t value) {
    const auto byte = wrap8(value);
    return byte >= 224 ? std::int32_t{byte} - 256 : std::int32_t{byte};
}
struct Subtraction {
    std::uint16_t value;
    bool no_borrow;
};
Subtraction subtract(std::uint16_t value, unsigned amount, bool no_borrow = true) {
    const auto rhs = amount + (no_borrow ? 0U : 1U);
    return {wrap16(65536U + value - rhs), unsigned{value} >= rhs};
}
using Piece = std::optional<RasterSprite>;
struct Pieces {
    std::vector<Piece> upper, lower;
};
RasterSprite quad(const std::array<Tile, 4>& tiles, FieldSpriteAppearance appearance,
                  const FieldObjectView& view, std::int32_t x, std::int32_t y) {
    require(appearance.palette < 8 && appearance.priority < 4, "Invalid field vehicle appearance");
    RasterSprite result;
    result.x = x;
    result.y = y;
    result.width = result.height = 16;
    result.pixels.resize(256);
    for (std::size_t row = 0; row < 16; ++row)
        for (std::size_t column = 0; column < 16; ++column)
            result.pixels[row * 16 + column] =
                tiles[row / 8 * 2 + column / 8][row % 8 * 8 + column % 8];
    result.colors = view.colors[appearance.palette];
    constexpr std::array<std::uint16_t, 4> depths{2, 3, 6, 9};
    result.depth = depths[appearance.priority];
    result.flip_x = appearance.flip_x;
    result.flip_y = appearance.flip_y;
    result.mix = appearance.palette >= 4 ? view.style.mix : CompositeBlend{};
    result.allow_color_effect = appearance.palette >= 4;
    return result;
}
Pieces magitek(const FieldObject& object, const FieldSpriteTiles& tiles,
               const FieldObjectViewAssets& assets, const FieldObjectView& view) {
    require(object.facing < 4, "Invalid Magitek facing");
    const auto px = pixel(object.x), py = pixel(object.y);
    const auto x = signed16(
        wrap16(unsigned{px} + 65536U - wrap16(static_cast<std::uint32_t>(view.origins[0].x))));
    if (x < -32 || x >= 288)
        return {};
    const auto baseline =
        wrap16(unsigned{py} + 3U * 65536U - wrap16(static_cast<std::uint32_t>(view.origins[0].y)) -
               1U - view.shake_y);
    const auto bottom = subtract(baseline, FF6_ROM_VALUE_FIELD_SPRITE_BASELINE_0);
    // These three geometry operations deliberately share the original borrow.
    const auto top = subtract(bottom.value, 16, bottom.no_borrow);
    const auto rider = subtract(top.value, FF6_ROM_VALUE_FIELD_MAGITEK_RIDER_HEIGHT_0, top.no_borrow);
    if (wrap16(unsigned{rider.value} + FF6_ROM_VALUE_FIELD_VEHICLE_CLIP_BIAS_0) >= 256)
        return {};
    const unsigned phase_position = object.velocity_x != 0 ? px : py;
    const auto& formation =
        assets.magitek[std::size_t{object.facing} * 4 + (phase_position / 8) % 4];
    Pieces result;
    result.upper.resize(3);
    result.lower.resize(3);
    if (object.rider_visible) {
        std::array<Tile, 4> upper;
        std::copy_n(tiles.begin(), 4, upper.begin());
        result.upper[0] = quad(upper, {object.lower.palette, FF6_ROM_VALUE_FIELD_RIDER_PRIORITY_0, object.facing == 1, false}, view,
                               x + FF6_ROM_VALUE_FIELD_OBJECT_CENTER_X_0, vertical(unsigned{rider.value} + (phase_position / 8) % 2));
    }
    result.upper[1] =
        quad(formation[0].tiles, formation[0].appearance, view, x, vertical(top.value));
    result.upper[2] =
        quad(formation[1].tiles, formation[1].appearance, view, x + 16, vertical(top.value));
    result.lower[0] =
        quad(formation[2].tiles, formation[2].appearance, view, x, vertical(bottom.value));
    result.lower[1] =
        quad(formation[3].tiles, formation[3].appearance, view, x + 16, vertical(bottom.value));
    return result;
}
RasterSprite vehicle_piece(std::uint16_t descriptor, const FieldObjectViewAssets& assets,
                           const FieldObjectView& view, int x, int y) {
    const unsigned character = descriptor & 511U;
    require(character >= 256 && character <= 494 && (character & 15U) < 15,
            "Invalid vehicle graphics reference");
    const unsigned index = character - 256U;
    std::array<Tile, 4> tiles;
    constexpr std::array<unsigned, 4> offsets{0, 1, 16, 17};
    for (unsigned i = 0; i < 4; ++i) {
        const auto& tile = assets.special_tiles[index + offsets[i]];
        require(tile.has_value(), "Unresolved vehicle graphics");
        tiles[i] = *tile;
    }
    return quad(tiles,
                {static_cast<std::uint8_t>((descriptor >> 9U) & 7U),
                 static_cast<std::uint8_t>((descriptor >> 12U) & 3U),
                 (descriptor & 16384U) != 0, (descriptor & 32768U) != 0},
                view, x, y);
}
Pieces raft(const FieldObject& object, const FieldSpriteTiles& rider_tiles,
            const FieldObjectViewAssets& assets, const FieldObjectView& view) {
    require(object.facing < 4, "Invalid raft facing");
    const auto x = signed16(wrap16(unsigned{pixel(object.x)} + 65536U -
                                   wrap16(static_cast<std::uint32_t>(view.origins[0].x))));
    const auto baseline =
        wrap16(unsigned{pixel(object.y)} + 3U * 65536U -
               wrap16(static_cast<std::uint32_t>(view.origins[0].y)) - 1U - view.shake_y);
    const auto bottom = subtract(baseline, FF6_ROM_VALUE_FIELD_SPRITE_BASELINE_0);
    const auto top = subtract(bottom.value, 16, bottom.no_borrow);
    const auto rider = subtract(top.value, FF6_ROM_VALUE_FIELD_RAFT_RIDER_HEIGHT_0, top.no_borrow);
    if (x < -32 || x >= 288 || wrap16(unsigned{rider.value} + FF6_ROM_VALUE_FIELD_VEHICLE_CLIP_BIAS_0) >= 256)
        return {};
    // DrawRaft reserves three entries in each ordering band. The rider's two
    // pieces precede the hull's upper-left piece; facing selects hull imagery,
    // while right-facing alone mirrors the ordinary rider graphics.
    Pieces pieces;
    pieces.upper.resize(3);
    pieces.lower.resize(3);
    if (object.rider_visible) {
        const FieldSpriteAppearance look{object.lower.palette, FF6_ROM_VALUE_FIELD_RIDER_PRIORITY_0, object.facing == 1, false};
        pieces.upper[0] = quad({rider_tiles[0], rider_tiles[1], rider_tiles[2], rider_tiles[3]},
                               look, view, x + FF6_ROM_VALUE_FIELD_OBJECT_CENTER_X_0, vertical(rider.value));
        pieces.upper[1] = quad({rider_tiles[4], rider_tiles[5], Tile{}, Tile{}}, look, view,
                               x + FF6_ROM_VALUE_FIELD_OBJECT_CENTER_X_0, vertical(unsigned{rider.value} + 16U));
    }
    pieces.upper[2] = vehicle_piece(assets.raft[0][object.facing], assets, view,
                                     x, vertical(top.value));
    pieces.lower[0] = vehicle_piece(assets.raft[1][object.facing], assets, view,
                                     x + 16, vertical(top.value));
    pieces.lower[1] = vehicle_piece(assets.raft[2][object.facing], assets, view,
                                     x, vertical(bottom.value));
    pieces.lower[2] = vehicle_piece(assets.raft[3][object.facing], assets, view,
                                     x + 16, vertical(bottom.value));
    return pieces;
}
Pieces chocobo(const FieldObject& object, const FieldSpriteTiles& rider_tiles,
               const FieldObjectViewAssets& assets, const FieldObjectView& view) {
    require(object.facing < 4, "Invalid chocobo facing");
    const auto px = pixel(object.x), py = pixel(object.y);
    const auto x = signed16(
        wrap16(unsigned{px} + 65536U - wrap16(static_cast<std::uint32_t>(view.origins[0].x))));
    const auto baseline =
        wrap16(unsigned{py} + 3U * 65536U - wrap16(static_cast<std::uint32_t>(view.origins[0].y)) -
               1U - view.shake_y);
    const auto bottom = subtract(baseline, FF6_ROM_VALUE_FIELD_SPRITE_BASELINE_0);
    const auto top = subtract(bottom.value, 16, bottom.no_borrow);
    const auto rider = subtract(top.value, FF6_ROM_VALUE_FIELD_CHOCOBO_RIDER_HEIGHT_0, top.no_borrow);
    if (x + FF6_ROM_VALUE_FIELD_OBJECT_CENTER_X_0 < -32 || x + FF6_ROM_VALUE_FIELD_OBJECT_CENTER_X_0 >= 288 || wrap16(unsigned{rider.value} + FF6_ROM_VALUE_FIELD_CHOCOBO_CLIP_BIAS_0) >= 256)
        return {};
    const unsigned phase = ((object.velocity_x ? px : py) / 8) % 4;
    const unsigned direction = object.facing == 0   ? 0
                               : object.facing == 2 ? 1
                               : object.facing == 1 ? 2
                                                    : 3;
    const auto& data = assets.chocobo[direction];
    const auto word = [&](unsigned table) { return read16(data, table * 8 + phase * 2); };
    const auto body = [&](std::uint16_t descriptor, int bx, int by) {
        return vehicle_piece(descriptor, assets, view, bx, by);
    };
    Pieces pieces;
    pieces.upper.resize(3);
    pieces.lower.resize(3);
    std::array<Tile, 4> upper;
    std::copy_n(rider_tiles.begin(), 4, upper.begin());
    if (direction < 2) {
        pieces.upper[0] =
            body(word(3), x + FF6_ROM_VALUE_FIELD_OBJECT_CENTER_X_0 + signed16(word(0)), vertical(unsigned{top.value} + word(1)));
        pieces.upper[2] = body(word(5), x + FF6_ROM_VALUE_FIELD_OBJECT_CENTER_X_0, vertical(top.value));
        pieces.lower[0] = body(word(6), x + FF6_ROM_VALUE_FIELD_OBJECT_CENTER_X_0, vertical(bottom.value));
        if (object.rider_visible)
            pieces.upper[1] = quad(upper, {object.lower.palette, FF6_ROM_VALUE_FIELD_RIDER_PRIORITY_0, false, false}, view, x + FF6_ROM_VALUE_FIELD_OBJECT_CENTER_X_0,
                                   vertical(unsigned{rider.value} + word(2)));
    } else {
        const bool right = direction == 2;
        const auto upper_front = word(1);
        const auto upper_back = right ? word(2) : word(0);
        const auto lower_front = word(3);
        const auto lower_back = right ? word(4) : word(2);
        pieces.upper[2] = body(upper_front, x + 16, vertical(top.value));
        pieces.lower[2] = body(upper_back, x, vertical(top.value));
        pieces.lower[0] = body(lower_front, x + 16, vertical(bottom.value));
        pieces.lower[1] = body(lower_back, x, vertical(bottom.value));
        if (object.rider_visible) {
            const auto bob = read16(assets.chocobo[2], phase * 2);
            const int rx = x + FF6_ROM_VALUE_FIELD_OBJECT_CENTER_X_0 + (right ? -FF6_ROM_VALUE_FIELD_CHOCOBO_RIDER_OFFSET_0 : FF6_ROM_VALUE_FIELD_CHOCOBO_RIDER_OFFSET_0);
            const auto ry = vertical(unsigned{rider.value} + bob);
            pieces.upper[0] = quad(upper, {object.lower.palette, FF6_ROM_VALUE_FIELD_RIDER_PRIORITY_0, right, false}, view, rx, ry);
            pieces.upper[1] = quad({rider_tiles[4], rider_tiles[5], Tile{}, Tile{}},
                                   {object.lower.palette, FF6_ROM_VALUE_FIELD_RIDER_PRIORITY_0, right, false}, view, rx, ry + 16);
        }
    }
    return pieces;
}
Pieces special(const FieldObjects& field, const FieldObject& object,
               const FieldObjectViewAssets& assets, const FieldObjectView& view) {
    require(object.interaction_data <= 0x3ffffU, "Invalid special NPC graphics link");
    const auto link = object.interaction_data;
    const bool linked = (link & 0x20000U) != 0;
    const auto& anchor = linked ? field.objects[16 + ((link >> 8U) & 31U)] : object;
    const unsigned offset = ((link >> 13U) & 7U) * (linked ? 16U : 2U);
    const bool shift_y = (link & 0x10000U) != 0;
    const auto origin = view.origins[object.scroll_with_background2 ? 1 : 0];
    const auto x = signed16(wrap16(unsigned{pixel(anchor.x)} + 65536U -
                                   wrap16(static_cast<std::uint32_t>(origin.x)) +
                                   (shift_y ? 0U : offset) + FF6_ROM_VALUE_FIELD_OBJECT_CENTER_X_0));
    // The source explicitly begins the scroll subtraction with borrow, then
    // clears it for shake, jump and the special piece's eight-pixel baseline.
    const auto y = wrap16(unsigned{pixel(anchor.y)} + (shift_y ? offset : 0U) + 4U * 65536U -
                          wrap16(static_cast<std::uint32_t>(origin.y)) - 1U - view.shake_y -
                          anchor.jump_y - FF6_ROM_VALUE_FIELD_SPRITE_BASELINE_0);
    if (x < -32 || x >= 256 || wrap16(unsigned{y} + 32U) >= 256)
        return {};
    const unsigned cycle =
        object.special_pose ? static_cast<unsigned>(object.special_pose->cycle) : 0;
    require(cycle < 4, "Invalid special NPC frame cycle");
    constexpr std::array<unsigned, 4> masks{0, 0, 1, 3};
    const unsigned side = (object.movement.options & 2U) != 0 ? 32U : 16U;
    // Special graphics are selected only for the no-vehicle encoding, so their
    // source animation-speed bits are zero. The flip bit does not gate cycling.
    require(object.ride == FieldRide::none, "Conflicting special NPC vehicle encoding");
    const auto first =
        wrap8((link & 127U) * 2U + ((view.vblank_count >> 2U) & masks[cycle]) * (side / 8U));
    const auto& appearance = object.upper;
    require(appearance.palette < 8 && appearance.priority < 4, "Invalid special NPC appearance");
    RasterSprite sprite;
    sprite.x = x;
    sprite.y = vertical(y);
    sprite.width = sprite.height = static_cast<std::uint16_t>(side);
    sprite.pixels.resize(side * side);
    for (unsigned row = 0; row < side; ++row)
        for (unsigned column = 0; column < side; ++column) {
            // Tile rows and columns wrap independently inside this game-owned
            // 16-by-16 atlas. This preserves frames crossing an atlas edge.
            const auto tile =
                ((first / 16U + row / 8U) % 16U) * 16U + (unsigned{first} + column / 8U) % 16U;
            require(assets.special_tiles[tile].has_value(), "Special NPC uses unresolved graphics");
            sprite.pixels[row * side + column] =
                (*assets.special_tiles[tile])[row % 8U * 8U + column % 8U];
        }
    sprite.colors = view.colors[appearance.palette];
    constexpr std::array<std::uint16_t, 4> depths{2, 3, 6, 9};
    sprite.depth = depths[appearance.priority];
    sprite.flip_x = appearance.flip_x || object.walking_animation;
    sprite.flip_y = appearance.flip_y;
    sprite.mix = appearance.palette >= 4 ? view.style.mix : CompositeBlend{};
    sprite.allow_color_effect = appearance.palette >= 4;
    // The original decrements the common band's cursor but writes only the
    // upper entry. Reserve its empty lower entry for later actors and party fixup.
    return {{std::move(sprite)}, {std::nullopt}};
}
void relocate_party_piece(Piece& piece) {
    if (!piece)
        return;
    piece->x = wrap8(static_cast<std::uint32_t>(piece->x));
    if (piece->width == 32) {
        // FixPlayerSpritePriority copies only low descriptor bytes. Its reserved
        // slot keeps the cleared size flag, so even a special party piece is 16x16.
        std::vector<std::uint8_t> pixels(256);
        for (unsigned row = 0; row < 16; ++row)
            std::copy_n(piece->pixels.begin() + row * 32, 16, pixels.begin() + row * 16);
        piece->pixels = std::move(pixels);
        piece->width = piece->height = 16;
    }
}
void hide_relocated_piece(Piece& piece) {
    if (piece && piece->height == 32) {
        // The source hides with selected low coordinates, leaving size and high X intact.
        // This is fully outside for ordinary 16-pixel pieces, but a large piece
        // still wraps into the top edge. Keep that defined rare caller behavior.
        piece->x = piece->x < 0 ? FF6_ROM_VALUE_FIELD_RELOCATED_HIDDEN_COORDINATE_0 - 256
                                    : FF6_ROM_VALUE_FIELD_RELOCATED_HIDDEN_COORDINATE_0;
        piece->y = FF6_ROM_VALUE_FIELD_RELOCATED_HIDDEN_COORDINATE_0 - 256;
    } else
        piece.reset();
}
void publish_special_atlas(FieldObjectViewAssets& assets,
                           std::array<std::optional<Tile>, 256> atlas) {
    auto vehicle = assets.magitek;
    for (auto& frame : vehicle)
        for (auto& part : frame) {
            if (!part.atlas_tile)
                continue;
            const auto first = *part.atlas_tile;
            require(first + 17U < atlas.size(), "Vehicle piece exceeds its graphics atlas");
            for (unsigned tile = 0; tile < 4; ++tile) {
                const auto index = first + tile / 2 * 16 + tile % 2;
                require(atlas[index].has_value(), "Vehicle piece uses unresolved graphics");
                part.tiles[tile] = *atlas[index];
            }
        }
    assets.special_tiles = std::move(atlas);
    assets.magitek = std::move(vehicle);
}
} // namespace
FieldObjectViewAssets load_field_object_view(const ContentPack& pack) {
    FieldObjectViewAssets assets;
    assets.sprites = load_field_sprites(pack);
    const auto portraits = decode_tiles(pack.get("menu_graphics_portrait_gfx_4bpp"), 4);
    const auto colors = pack.get("menu_graphics_portrait_pal_pal");
    const auto order = decode_portrait_order(pack.get("field_portrait_tile_order"));
    require(portraits.size() >= 16 * 25 && colors.size() >= 16 * 32,
            "Truncated field portrait resources");
    for (unsigned image = 0; image < 16; ++image) {
        for (unsigned tile = 0; tile < 25; ++tile)
            assets.portraits[image][tile] = portraits[image * 25 + order[tile]];
        for (unsigned color = 0; color < 16; ++color)
            assets.portrait_colors[image][color] = read16(colors, image * 32 + color * 2) & 0x7fffU;
    }
    constexpr std::array<const char*,4> directions{"up","down","right","left"};
    constexpr std::array<unsigned,4> sizes{56,56,40,32};
    for (unsigned i=0;i<4;++i) {
        const auto bytes=pack.get(std::string("field_chocobo_")+directions[i]);
        require(bytes.size()==sizes[i],"Invalid chocobo frame table");
        assets.chocobo[i].assign(bytes.begin(),bytes.end());
    }
    const auto jumps = pack.get("field_jump_offsets");
    const auto formations = pack.get("field_magitek_formations");
    const auto raft_tiles = pack.get("field_raft_formations");
    require(raft_tiles.size() == 32, "Invalid raft formation table");
    for (unsigned part = 0; part < 4; ++part)
        for (unsigned facing = 0; facing < 4; ++facing)
            assets.raft[part][facing] = read16(raft_tiles, part * 8 + facing * 2);
    require(jumps.size() == 128 && formations.size() == 128, "Invalid field object view tables");
    std::ranges::copy(jumps, assets.jump_offsets.begin());
    // Only this prefix of the vehicle resource is published by the original.
    const auto tiles = decode_tiles(slice(pack.get("field_overlay_vehicle_gfx_4bpp"), 0, FF6_ROM_VALUE_FIELD_VEHICLE_GRAPHICS_SIZE_0), 4);
    std::copy(tiles.begin(), tiles.end(), assets.special_tiles.begin() + (FF6_ROM_VALUE_FIELD_VEHICLE_FIRST_TILE_0 - 256));
    for (std::size_t frame = 0; frame < 16; ++frame)
        for (std::size_t part = 0; part < 4; ++part) {
            const auto descriptor = read16(formations, part * 32 + frame * 2);
            const auto character = descriptor & 511U;
            require(character >= FF6_ROM_VALUE_FIELD_VEHICLE_FIRST_TILE_0 && character + 17 <= 511 && (character & 15U) < 15,
                    "Magitek formation exceeds loaded vehicle graphics");
            auto& output = assets.magitek[frame][part];
            output.atlas_tile = static_cast<std::uint16_t>(character - 256);
            const auto first = character - FF6_ROM_VALUE_FIELD_VEHICLE_FIRST_TILE_0;
            output.tiles = {tiles[first], tiles[first + 1], tiles[first + 16], tiles[first + 17]};
            output.appearance = {static_cast<std::uint8_t>((descriptor >> 9U) & 7U),
                                 static_cast<std::uint8_t>((descriptor >> 12U) & 3U),
                                 (descriptor & 0x4000U) != 0, (descriptor & 0x8000U) != 0};
        }
    return assets;
}
void prepare_field_special_graphics(FieldObjects& field, FieldObjectViewAssets& assets,
                                    std::size_t npc_count) {
    require(npc_count <= 32, "Too many special NPC definitions");
    auto next = field;
    auto atlas = assets.special_tiles;
    for (std::size_t i = 16; i < 16 + npc_count; ++i) {
        auto& object = next.objects[i];
        if (!object.special_graphics)
            continue;
        require(object.graphics < assets.sprites.graphics.size(), "Invalid special NPC graphic");
        object.movement.options |= 1U; // Shared special-graphics passability flag.
        object.walking_animation = (object.interaction_data & 128U) != 0;
        const unsigned side = (object.movement.options & 2U) != 0 ? 4 : 2;
        const unsigned frames =
            object.special_pose ? static_cast<unsigned>(object.special_pose->cycle) + 1 : 1;
        require(frames <= 4, "Invalid special NPC animation extent");
        const unsigned first = (object.interaction_data & 127U) * 2;
        auto source = std::size_t{assets.sprites.graphics[object.graphics]};
        // The source stores successive frames contiguously; destinations keep
        // the atlas's sixteen-tile row stride. Preserve overlaps between frames
        // and later definitions rather than treating each actor as a texture.
        for (unsigned frame = 0; frame < frames; ++frame)
            for (unsigned y = 0; y < side; ++y)
                for (unsigned x = 0; x < side; ++x) {
                    const auto destination = first + frame * side + y * 16 + x;
                    require(destination < atlas.size() && source < assets.sprites.tiles.size(),
                            "Special NPC graphics exceed the supported atlas or content");
                    atlas[destination] = assets.sprites.tiles[source++];
                }
    }

    publish_special_atlas(assets, std::move(atlas));
    field = std::move(next);
}
void prepare_field_portrait(FieldObjectViewAssets& assets, std::uint8_t identity,
                           std::array<std::array<Rgb555, 16>, 8>& pending_colors) {
    require(identity < assets.portraits.size(), "Field portrait identity outside source table");
    auto atlas = assets.special_tiles;
    // C0/5104 clears only the first half of the special/vehicle atlas, then
    // places the 5x5 image in its sixteen-tile row stride. Linked special NPCs
    // display that shared image; there is no separate portrait overlay actor.
    std::fill_n(atlas.begin(), FF6_ROM_VALUE_FIELD_PORTRAIT_CLEAR_TILES_0, Tile{});
    for (unsigned tile = 0; tile < 25; ++tile)
        atlas[tile / 5 * 16 + tile % 5] = assets.portraits[identity][tile];
    publish_special_atlas(assets, std::move(atlas));
    pending_colors[FF6_ROM_VALUE_FIELD_PORTRAIT_PALETTE_0] = assets.portrait_colors[identity];
}
void step_field_objects(FieldObjects& field, const FieldObjectViewAssets& assets,
                        FieldObjectClocks clocks) {
    require(field.update_count <= field.update_order.size(), "Invalid field update count");
    auto next = field;
    for (std::size_t i = 0; i < std::min<unsigned>(24, field.update_count); ++i) {
        const auto id = ordered_object(next, i);
        auto& object = next.objects[id];
        object.x = moved(object.x, object.velocity_x);
        object.y = moved(object.y, object.velocity_y);
        std::uint8_t height = 0;
        if ((object.jump_counter & 63U) != 0) {
            require(object.jump_counter < assets.jump_offsets.size(), "Undefined field jump curve");
            height = assets.jump_offsets[object.jump_counter];
            --object.jump_counter;
        }
        object.jump_y = static_cast<std::uint16_t>((object.jump_y & 0xff00U) | height);
        FieldPoseInput pose;
        pose.facing = static_cast<FieldFacing>(object.facing);
        pose.ride = object.ride;
        pose.walking = object.walking_animation;
        pose.moving_x = object.velocity_x != 0;
        pose.moving_y = object.velocity_y != 0;
        pose.x = pixel(object.x);
        pose.y = pixel(object.y);
        pose.diagonal_tile = clocks.diagonal_tile;
        pose.party_on_ladder = clocks.party_on_ladder && next.update_order[0] == id;
        pose.logic_count = clocks.logic;
        pose.vblank_count = clocks.vblank;
        pose.special = object.special_pose;
        object.pending_pose =
            select_field_pose(assets.sprites.selection, pose, object.pending_pose);
        if (object.x % 4096 == 0 && object.y % 4096 == 0)
            object.velocity_x = object.velocity_y = 0;
    }
    field = std::move(next);
}
void latch_field_object_poses(FieldObjects& field) {
    auto poses = pose_objects(field);
    latch_field_poses(field.event_visits, field.animation_queue, poses);
    for (std::size_t i = 0; i < poses.size(); ++i)
        field.objects[i].pose = poses[i].pose;
}
void publish_field_objects(const FieldObjects& field, const FieldObjectViewAssets& assets,
                           FieldSpritePublication& publication) {
    publication.publish(field.event_visits, assets.sprites, field.animation_queue,
                        pose_objects(field));
}
SpriteLayer make_field_object_layer(const FieldObjects& field, const FieldObjectViewAssets& assets,
                                    const FieldSpritePublication& publication,
                                    const FieldObjectView& view,
                                    const FieldOverlaySprites* overlays) {
    require(field.update_count <= field.update_order.size(), "Invalid field update count");
    std::array<Pieces, 3> groups;
    for (std::size_t i = 0; i < std::min<unsigned>(24, field.update_count); ++i) {
        const auto& object = field.objects[ordered_object(field, i)];
        if (!object.membership.visible)
            continue;
        Pieces pieces;
        auto group = object.draw_group;
        require(static_cast<unsigned>(group) < 4, "Invalid field draw group");
        if (static_cast<unsigned>(group) == 3)
            group = FieldObject::DrawGroup::low;
        if (object.special_graphics)
            pieces = special(field, object, assets, view);
        else {
            require(object.animation_slot && *object.animation_slot < 24,
                    "Field object has no valid animation slot");
            // InitObjGfx clears sprite pixels. A newly created/shown object
            // can precede its six-slot publication visit; its unused slot is
            // transparent until then, while reused slots retain their pixels.
            const auto tiles = publication.visible()[*object.animation_slot].value_or(FieldSpriteTiles{});
            if (object.special_pose || object.ride == FieldRide::none) {
                const auto origin = view.origins[object.scroll_with_background2 ? 1 : 0];
                auto ordinary = make_field_character(
                    assets.sprites, tiles, object.pose,
                    {pixel(object.x), pixel(object.y), wrap16(static_cast<std::uint32_t>(origin.x)),
                     wrap16(static_cast<std::uint32_t>(origin.y)), view.shake_y, object.jump_y},
                    appearance(object.upper), appearance(object.lower), view.colors,
                    view.style.mix);
                if (!ordinary.empty()) {
                    pieces.upper.push_back(std::move(ordinary[0]));
                    pieces.lower.push_back(std::move(ordinary[1]));
                }
            } else if (object.ride == FieldRide::magitek) {
                group = FieldObject::DrawGroup::normal;
                pieces = magitek(object, tiles, assets, view);
            } else if (object.ride == FieldRide::chocobo) {
                group = FieldObject::DrawGroup::normal;
                pieces = chocobo(object, tiles, assets, view);
            } else if (object.ride == FieldRide::raft) {
                group = FieldObject::DrawGroup::normal;
                pieces = raft(object, tiles, assets, view);
            } else
                throw std::invalid_argument("Field vehicle rendering is not implemented: object " +
                    std::to_string(ordered_object(field, i)) + " ride " + std::to_string(unsigned(object.ride)) +
                    " at " + std::to_string(pixel(object.x)) + "," + std::to_string(pixel(object.y)));
        }
        auto& target = groups[static_cast<std::size_t>(group)];
        const auto capacity = group == FieldObject::DrawGroup::normal ? 40U : 8U;
        require(target.upper.size() + pieces.upper.size() <= capacity,
                "Field sprite ordering band overflow");
        target.upper.insert(target.upper.begin(), pieces.upper.begin(), pieces.upper.end());
        target.lower.insert(target.lower.begin(), pieces.lower.begin(), pieces.lower.end());
    }
    Piece party_upper, party_lower;
    auto& normal = groups[0];
    if (field.update_count != 0) {
        const auto& party = field.objects[ordered_object(field, 0)];
        if (party.membership.visible && party.ride == FieldRide::none && !normal.upper.empty()) {
            // The source relocates these reserved entries, not an identity search.
            party_upper = normal.upper.back();
            hide_relocated_piece(normal.upper.back());
            party_lower = normal.lower.back();
            hide_relocated_piece(normal.lower.back());
            relocate_party_piece(party_upper);
            relocate_party_piece(party_lower);
        }
    }
    std::vector<RasterSprite> result;
    const auto append = [&](const std::vector<Piece>& pieces) {
        for (const auto& piece : pieces)
            if (piece)
                result.push_back(*piece);
    };
    append(groups[1].upper);
    append(groups[1].lower);
    append(normal.upper);
    if (overlays)
        result.insert(result.end(), overlays->bands[0].begin(), overlays->bands[0].end());
    if (!view.party_low_priority && party_upper)
        result.push_back(*party_upper);
    append(normal.lower);
    if (overlays)
        result.insert(result.end(), overlays->bands[1].begin(), overlays->bands[1].end());
    if (view.party_low_priority && party_upper)
        result.push_back(*party_upper);
    if (party_lower)
        result.push_back(*party_lower);
    append(groups[2].upper);
    append(groups[2].lower);
    return make_field_sprite_layer(std::move(result), view.style);
}
} // namespace ff6
