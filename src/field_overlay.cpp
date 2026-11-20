#include "ff6/field_overlay.hpp"
#include "rom-materials/field_overlay_horizontal_origin.macros.inc"
#include "rom-materials/field_overlay_hidden_piece.macros.inc"
#include "rom-materials/field_viewport_bounds.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool value, const char* why) {
    if (!value)
        throw std::invalid_argument(why);
}
std::uint8_t offset(std::uint8_t value, int displacement) {
    return wrap8(static_cast<std::uint32_t>(int{value} + displacement + 65536));
}
bool permits_top(std::uint8_t level, std::uint8_t top) {
    if (level == 2)
        return (top & 7U) != 1;
    if (level == 3)
        return top == 0xf7 || (top & 2U) != 0;
    return top == 0xf7 || (top & 7U) == 1;
}
FieldOverlayPiece piece(std::uint8_t x, std::uint8_t y, std::uint8_t control, bool allowed,
                        bool lower) {
    FieldOverlayPiece result{x, y, {}, false, false, true, 0};
    if (allowed && control != 255) {
        result.graphic = control & 63U;
        result.flip_x = (control & 64U) != 0;
        result.flip_y = (control & 128U) != 0;
        result.lower = lower;
    }
    return result;
}
} // namespace
FieldOverlaySet load_field_overlay(const ContentPack& pack, std::uint8_t id) {
    const auto pointers = pack.get("field_overlay_overlay_prop_ptr_bin");
    const auto properties = pack.get("field_overlay_overlay_prop_bin_lz");
    const auto formations = pack.get("field_overlay_overlay_tilemap_bin");
    const auto graphics = pack.get("field_overlay_overlay_gfx_1bpp");
    require(pointers.size() == 90 && id < 45 && formations.size() == 1536 &&
                graphics.size() == 2432,
            "Invalid terrain overlay resources");
    const auto start = read16(pointers, std::size_t{id} * 2);
    const auto end = id == 44 ? properties.size() : read16(pointers, std::size_t{id} * 2 + 2);
    require(start < end && end <= properties.size(), "Invalid overlay property extent");
    const auto decoded = decompress_lzss(slice(properties, start, end - start), 272);
    require(decoded.size() == 272, "Invalid decoded overlay properties");
    FieldOverlaySet result;
    std::copy_n(decoded.begin() + 16, 256, result.controls.begin());
    for (std::size_t slot = 0; slot < 16; ++slot) {
        const auto formation = decoded[slot];
        // Unused slots contain 255. The original still loads unrelated bytes for
        // them; no descriptor uses those slots. Do not package executable bytes
        // to reproduce that unused load. A visible unresolved piece fails below.
        if (formation == 255)
            continue;
        require(formation < 192, "Overlay formation exceeds the content table");
        const auto first = slot / 8 * 32 + slot % 8 * 2;
        for (std::size_t part = 0; part < 4; ++part) {
            const auto source = read16(formations, std::size_t{formation} * 8 + part * 2);
            const auto rows = slice(graphics, source, 8);
            Tile tile{};
            for (unsigned y = 0; y < 8; ++y)
                for (unsigned x = 0; x < 8; ++x)
                    tile[y * 8 + x] = (rows[y] >> (7U - x)) & 1U;
            result.graphics[first + part / 2 * 16 + part % 2] = tile;
        }
    }
    for (const auto control : result.controls) {
        if (control == 255)
            continue;
        const auto first = control & 63U;
        require((first & 17U) == 0 && result.graphics[first] && result.graphics[first + 1] &&
                    result.graphics[first + 16] && result.graphics[first + 17],
                "Overlay descriptor uses an unresolved formation");
    }
    return result;
}
FieldOverlayLayouts load_field_overlay_layouts(const ContentPack& pack) {
    FieldOverlayLayouts result;
    const auto decode = [](ByteView data, auto& rows) {
        require(data.size() == rows.size() * rows[0].size() * 3,
                "Invalid overlay placement table extent");
        std::size_t at = 0;
        for (auto& row : rows)
            for (auto& target : row) {
                target = {static_cast<std::int16_t>(signed8(data[at])),
                          static_cast<std::int16_t>(signed8(data[at + 1])), data[at + 2]};
                require(target.local_cell < 12, "Overlay placement exceeds local terrain cache");
                at += 3;
            }
    };
    decode(pack.get("field_overlay_stair_layouts"), result.stairs);
    decode(pack.get("field_overlay_cardinal_layouts"), result.cardinal);
    return result;
}
void update_field_overlay(FieldOverlayState& state, const FieldOverlaySet& assets,
                          const FieldOverlayLayouts& layouts, const FieldMap& map,
                          const FieldObject& player, const FieldPlayerTerrain& terrain,
                          const FieldLocalTiles& local, const FieldOverlayView& view) {
    if (player.movement.mode != 2)
        return;
    require(player.x <= 0xffffff && player.y <= 0xffffff, "Invalid overlay player position");
    const auto direction = player.move_direction;
    const auto current = local.cells[7].flags;
    auto x = wrap8((player.x >> 8U) + 65536U - view.origin[0] + FF6_ROM_VALUE_FIELD_OVERLAY_HORIZONTAL_ORIGIN_0);
    auto y = wrap8((player.y >> 8U) + 65536U - view.origin[1] - 1U);
    const auto control = [&](std::size_t cell) {
        require(cell < 12, "Overlay placement exceeds local terrain cache");
        return assets.controls[local.cells[cell].tile];
    };
    const auto flags = [&](std::size_t cell) {
        require(cell < 12, "Overlay placement exceeds local terrain cache");
        return map.tile_properties[local.cells[cell].tile][0];
    };
    FieldOverlayState next;
    if ((current & 192U) && !(current & 4U) && (direction == 0 || direction >= 5)) {
        require(direction <= 8, "Unsupported overlay stair direction");
        const auto& layout = layouts.stairs[direction == 0 ? 0 : direction - 4];
        for (std::size_t i = 0; i < 3; ++i) {
            const auto c = control(layout[i].local_cell);
            next[i] = piece(offset(x, layout[i].x), offset(y, layout[i].y), c,
                            !(flags(layout[i].local_cell) & 4U) || terrain.level != 1, false);
            // Stair absence retains flips, and never selects the lower band.
            next[i].flip_x = (c & 64U) != 0;
            next[i].flip_y = (c & 128U) != 0;
            next[i].lower = false;
        }
        // The original writes one selected byte four times, not an empty descriptor. Preserve
        // the hidden piece so unusual scroll/shake cannot silently discard it.
        next[3] = {FF6_ROM_VALUE_FIELD_OVERLAY_HIDDEN_PIECE_0,
                   FF6_ROM_VALUE_FIELD_OVERLAY_HIDDEN_PIECE_1,
                   FF6_ROM_VALUE_FIELD_OVERLAY_HIDDEN_PIECE_2,
                   bool{FF6_ROM_VALUE_FIELD_OVERLAY_HIDDEN_PIECE_3},
                   bool{FF6_ROM_VALUE_FIELD_OVERLAY_HIDDEN_PIECE_4},
                   bool{FF6_ROM_VALUE_FIELD_OVERLAY_HIDDEN_PIECE_5},
                   FF6_ROM_VALUE_FIELD_OVERLAY_HIDDEN_PIECE_6};
    } else {
        require(direction <= 8, "Unsupported overlay direction");
        if (view.follow_rate[0] < 0)
            x = offset(x, 16);
        if (view.follow_rate[1] < 0)
            y = offset(y, 16);
        const bool bridge = (current & 4U) != 0;
        next[0] = piece(x, y, control(7), !bridge || terrain.level != 1, bridge);
        next[1] = piece(
            x, offset(y, -16), control(4),
            bridge ? terrain.level != 1 : permits_top(current & 3U, local.cells[4].flags), bridge);
        if (direction >= 5) {
            // Bridge stairs legitimately enter NotStairs (C0/C8D6). Its
            // destination table C0/CC7D contains only four cardinal rows; the
            // diagonal selectors instead read instructions and unrelated RAM.
            // There is no destination mask defined for those rows. Retain the
            // two valid current-tile masks and leave the undefined pieces empty.
            // Do not turn that original out-of-bounds read into a movement stop
            // or package executable bytes as terrain data. See FIELD_OVERLAYS.
            state = next;
            return;
        }
        const auto& layout = layouts.cardinal[direction == 0 ? 0 : direction - 1];
        const auto destination = flags(layout[0].local_cell);
        const bool destination_bridge = (destination & 4U) != 0;
        const bool lower_allowed =
            !destination_bridge || (bridge ? terrain.level != 1 : (current & 3U) != 1);
        bool upper_allowed;
        if (destination_bridge) {
            const auto level = current & 7U;
            upper_allowed = level != 1 && (level == 2 || terrain.level != 1);
        } else {
            upper_allowed = permits_top(destination & 3U, flags(layout[1].local_cell));
        }
        for (std::size_t i = 0; i < 2; ++i)
            next[i + 2] = piece(
                offset(x, layout[i].x), offset(y, layout[i].y), control(layout[i].local_cell),
                direction != 0 && (i == 0 ? lower_allowed : upper_allowed), destination_bridge);
    }
    state = next;
}
void clear_field_overlay(FieldOverlayState& state) {
    for (auto& entry : state)
        entry.graphic.reset();
}
FieldOverlayFrame project_field_overlay(const FieldOverlayState& state,
                                        const FieldOverlayView& view) {
    FieldOverlayFrame frame;
    std::array<int, 2> alignment{};
    for (std::size_t axis = 0; axis < 2; ++axis) {
        // The original tests the signed integer byte, not the entire 8.8 rate.
        // A positive fractional-only rate therefore follows the stationary rule.
        alignment[axis] = view.follow_rate[axis] >= 256
                              ? -static_cast<int>((unsigned{view.origin[axis]} + 65535U) % 16 + 1)
                              : -int{view.origin[axis] % 16};
    }
    for (std::size_t i = 0; i < state.size(); ++i) {
        auto entry = state[i];
        if (!entry.graphic)
            continue;
        require(*entry.graphic < 64 && entry.palette < 8, "Invalid terrain mask appearance");
        entry.x = offset(entry.x, alignment[0]);
        entry.y = offset(entry.y, alignment[1] - view.shake_y);
        frame[entry.lower ? 1 : 0][i] = entry;
    }
    return frame;
}
FieldOverlaySprites make_field_overlay_sprites(const FieldOverlaySet& assets,
                                               const FieldOverlayFrame& frame,
                                               const std::array<std::array<Rgb555, 16>, 8>& colors,
                                               CompositeBlend mix) {
    FieldOverlaySprites result;
    for (std::size_t band = 0; band < 2; ++band)
        for (const auto& maybe : frame[band]) {
            if (!maybe || !maybe->graphic)
                continue;
            const auto& entry = *maybe;
            require(*entry.graphic < 64 && entry.palette < 8, "Invalid terrain mask appearance");
            const auto y = entry.y >= 224 ? int{entry.y} - 256 : int{entry.y};
            // InitPPU/IRQ's shared field boundary also clips these pieces.
            // Scrolling can project the stair routine's hidden descriptor
            // into the blank band. It still contributes no pixels, so its unused tile
            // must not be demanded merely because it intersects the framebuffer.
            if (y + 16 <= 0 || y >= FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_2)
                continue;
            RasterSprite sprite;
            sprite.x = entry.x;
            sprite.y = y;
            sprite.width = sprite.height = 16;
            sprite.depth = 2;
            sprite.flip_x = entry.flip_x;
            sprite.flip_y = entry.flip_y;
            sprite.colors = colors[entry.palette];
            sprite.mix = entry.palette >= 4 ? mix : CompositeBlend{};
            sprite.allow_color_effect = entry.palette >= 4;
            sprite.pixels.resize(256);
            const unsigned base = *entry.graphic;
            require(base / 16 < 3, "Terrain mask extends beyond loaded graphics");
            for (unsigned row = 0; row < 16; ++row)
                for (unsigned col = 0; col < 16; ++col) {
                    const unsigned tile = (base / 16 + row / 8) * 16 + (base + col / 8) % 16;
                    if (!assets.graphics[tile])
                        throw std::invalid_argument("Visible terrain mask uses an unrepresented unused graphic: tile=" +
                            std::to_string(tile) + " base=" + std::to_string(base) +
                            " y=" + std::to_string(y));
                    sprite.pixels[row * 16 + col] = (*assets.graphics[tile])[row % 8 * 8 + col % 8];
                }
            result.bands[band].push_back(std::move(sprite));
        }
    return result;
}
} // namespace ff6
