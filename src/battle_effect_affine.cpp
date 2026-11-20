#include "ff6/battle_effects.hpp"
#include "rom-materials/battle_affine_conversion_limit.macros.inc"
#include "rom-materials/battle_affine_empty_tile.macros.inc"
#include "rom-materials/battle_affine_horizontal_bounds.macros.inc"
#include "rom-materials/final_battle_viewport.macros.inc"
#include "rom-materials/battle_affine_copy_layout.macros.inc"
#include <algorithm>
#include <stdexcept>
#include "rom-materials/battle_summon_affine_positions.macros.inc"
#include "rom-materials/battle_affine_layer_masks.macros.inc"

namespace ff6 {
// The generated fill pattern and scalar clears select the same empty cell.
static_assert(FF6_ROM_VALUE_BATTLE_AFFINE_EMPTY_TILE_0 == FF6_ROM_VALUE_BATTLE_AFFINE_EMPTY_TILE_1);
static_assert(FF6_ROM_VALUE_BATTLE_AFFINE_EMPTY_TILE_0 == FF6_ROM_VALUE_BATTLE_AFFINE_EMPTY_TILE_2);
static_assert(FF6_ROM_VALUE_BATTLE_AFFINE_EMPTY_TILE_0 == FF6_ROM_VALUE_BATTLE_AFFINE_EMPTY_TILE_3);
namespace {
int signed_word(std::uint16_t v) {
    return v < 32768 ? int{v} : int{v} - 65536;
}
int signed_center(std::uint16_t v) {
    const auto n = v & 8191U;
    return n < 4096 ? static_cast<int>(n) : static_cast<int>(n) - 8192;
}
int clipped_difference(int v) {
    const auto bits = static_cast<std::uint32_t>(v);
    return static_cast<int>(bits & 1023U) - ((bits & 8192U) ? 1024 : 0);
}
int truncate_product(int v) {
    // Each center/row product discards six low bits separately, flooring
    // negative values too. Combining products before truncation moves edges.
    return (v >= 0 ? v / 64 : -((-v + 63) / 64)) * 64;
}
} // namespace
AffineLine project_battle_effect_affine(const BattleEffectAffine& t, BattleAnimationPoint origin,
                                        unsigned row) {
    if (row > 255)
        throw std::invalid_argument("Affine battle row exceeds its canonical image");
    const auto sx = signed_word(t.scale_x), sy = signed_word(t.scale_y);
    const auto bx = signed_word(t.cross_x), by = signed_word(t.cross_y);
    const auto cx = signed_center(t.center_x), cy = signed_center(t.center_y);
    const auto ox = signed_center(origin.x);
    const auto oy = signed_center(wrap16(unsigned{origin.y} - 1U));
    // Canonical row zero is the original first displayed scanline. Ordinary
    // battle scroll publication subtracts one; scaled sampling retains both
    // operations rather than cancelling them before product quantization.
    const auto y = t.flip_y ? 255 - static_cast<int>(row + 1) : static_cast<int>(row + 1);
    AffineLine result;
    result.source_x =
        truncate_product(sx * clipped_difference(ox - cx)) +
        truncate_product(bx * clipped_difference(oy - cy)) + truncate_product(bx * y) +
        cx * 256 + (t.flip_x ? sx * 255 : 0);
    result.source_y =
        truncate_product(by * clipped_difference(ox - cx)) +
        truncate_product(sy * clipped_difference(oy - cy)) + truncate_product(sy * y) +
        cy * 256 + (t.flip_x ? by * 255 : 0);
    result.step_x = t.flip_x ? -sx : sx;
    result.step_y = t.flip_x ? -by : by;
    result.enabled = row >= FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 &&
        row < FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1;
    result.left = FF6_ROM_VALUE_BATTLE_AFFINE_HORIZONTAL_BOUNDS_0;
    result.right = FF6_ROM_VALUE_BATTLE_AFFINE_HORIZONTAL_BOUNDS_1 + 1;
    return result;
}
AffineLayer make_battle_summon_affine(const MonsterGraphic& source, bool folded) {
    if (!source.dimensions.width || source.dimensions.width > 16 || !source.dimensions.height ||
        source.dimensions.height > 16)
        throw std::invalid_argument("Invalid affine summon dimensions");
    AffineLayer image;
    image.columns = 128;
    image.rows = 32;
    image.cells.assign(128 * 32, FF6_ROM_VALUE_BATTLE_AFFINE_EMPTY_TILE_0);
    image.tiles.resize(64);
    // The ROM selects the converted subset and the cleared cell identity.
    // The six-bit tile format still owns 64 slots, including an empty slot;
    // a source's extra packed tile must not silently populate that slot.
    std::copy_n(source.packed_tiles.begin(), std::min<std::size_t>(FF6_ROM_VALUE_BATTLE_AFFINE_CONVERSION_LIMIT_0, source.packed_tiles.size()),
                image.tiles.begin());
    std::copy(source.colors.begin(), source.colors.end(), image.colors.begin());
    // Stencil origins and the folded rectangle are selected by ROM operands.
    // The shared extractor validates every row address, clear and cell identity;
    // only original buffer/cell representation remains as format metadata here.
    constexpr unsigned origin = 0xc400, row_bytes = 0x100;
    constexpr unsigned ordinary_row = (FF6_ROM_VALUE_BATTLE_SUMMON_AFFINE_POSITIONS_0 - origin) / row_bytes;
    constexpr unsigned folded_row = (FF6_ROM_VALUE_BATTLE_SUMMON_AFFINE_POSITIONS_1 - origin) / row_bytes;
    constexpr unsigned copy_rows = FF6_ROM_VALUE_BATTLE_AFFINE_COPY_LAYOUT_0;
    constexpr unsigned copy_columns = FF6_ROM_VALUE_BATTLE_AFFINE_COPY_LAYOUT_1;
    constexpr unsigned copy_row = FF6_ROM_VALUE_BATTLE_AFFINE_COPY_LAYOUT_2;
    constexpr unsigned copy_column = FF6_ROM_VALUE_BATTLE_AFFINE_COPY_LAYOUT_3;
    constexpr unsigned target_row = FF6_ROM_VALUE_BATTLE_AFFINE_COPY_LAYOUT_4;
    constexpr unsigned target_column = FF6_ROM_VALUE_BATTLE_AFFINE_COPY_LAYOUT_5;
    static_assert(FF6_ROM_VALUE_BATTLE_AFFINE_COPY_LAYOUT_6 == FF6_ROM_VALUE_BATTLE_AFFINE_EMPTY_TILE_0);
    unsigned tile = 0;
    for (unsigned y = 0; y < source.dimensions.height; ++y)
        for (unsigned x = 0; x < source.dimensions.width; ++x)
            if (source.stencil[y] & (0x8000U >> x)) {
                if (tile >= source.packed_tiles.size() || tile > 63)
                    throw std::invalid_argument("Affine summon exceeds its packed tile owner");
                image.cells[(y + (folded ? folded_row : ordinary_row)) * 128 + x] = tile++;
            }
    if (folded)
        for (unsigned y = 0; y < copy_rows; ++y)
            for (unsigned x = 0; x < copy_columns; ++x) {
                image.cells[(y + target_row) * 128 + x + target_column] = image.cells[(y + copy_row) * 128 + x + copy_column];
                image.cells[(y + copy_row) * 128 + x + copy_column] = FF6_ROM_VALUE_BATTLE_AFFINE_EMPTY_TILE_0;
            }
    image.wrap_y = false;
    return image;
}
void BattleAttackAnimation::initialize_authored_affine(unsigned graphics_offset, unsigned layout_offset) {
    // LoadMode7AnimGfx C2/EC61 selects consecutive 4bpp tiles and copies
    // a 32x32 authored layout into the cleared, horizontally repeating image.
    if (graphics_offset % 32 || graphics_offset / 32 + FF6_ROM_VALUE_BATTLE_AFFINE_CONVERSION_LIMIT_0 > library_->affine_tiles.size() ||
        layout_offset + 1024 > library_->affine_layouts.size() || !images_[1])
        throw std::invalid_argument("Invalid authored affine image ownership");
    AffineLayer image;
    image.columns = 128; image.rows = 32; image.wrap_y = false;
    image.cells.assign(128 * 32, FF6_ROM_VALUE_BATTLE_AFFINE_EMPTY_TILE_0); image.tiles.resize(64);
    std::copy_n(library_->affine_tiles.begin() + graphics_offset / 32, FF6_ROM_VALUE_BATTLE_AFFINE_CONVERSION_LIMIT_0, image.tiles.begin());
    for (unsigned y = 0; y < 32; ++y)
        for (unsigned x = 0; x < 32; ++x) {
            const auto tile = library_->affine_layouts[layout_offset + y * 32 + x];
            if (tile >= 64) throw std::invalid_argument("Authored affine tile exceeds its image");
            image.cells[y * 128 + x] = tile;
        }
    affine_.emplace();
    affine_image_ = std::make_shared<AffineLayer>(std::move(image));
    foreground_frames_cleared_ = true;
    visible_layers_ &= FF6_ROM_VALUE_BATTLE_AFFINE_LAYER_MASKS_0;
    loading_waits_ += 4;
}
void BattleAttackAnimation::initialize_affine(std::size_t identity, bool folded) {
    if (identity >= library_->summons.size())
        throw std::invalid_argument("Affine summon has no resource owner");
    affine_.emplace();
    affine_image_ = std::make_shared<AffineLayer>(
        make_battle_summon_affine(library_->summons[identity], folded));
    load_summon(identity, BattleEffectLayer::foreground);
    // This path publishes the 8192-byte affine image, rather than the ordinary
    // 6144-byte foreground selection used by load_summon above.
    loading_waits_ += 2;
    foreground_frames_cleared_ = true;
    visible_layers_ &= FF6_ROM_VALUE_BATTLE_AFFINE_LAYER_MASKS_1;
}
} // namespace ff6
