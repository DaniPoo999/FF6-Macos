#pragma once
#include "ff6/codecs.hpp"
#include <optional>

namespace ff6 {
// Canonical colors precede display scaling and RGB8 conversion. Bit 15 is invalid.
using Rgb555 = std::uint16_t;
enum class ColorBlend { none, add, subtract, average, half_difference };
Rgb555 blend_color(Rgb555 base, Rgb555 operand, ColorBlend operation);
// Optional linear five-bit projection used by the Mesen comparison profile.
// Keep this separate: other display profiles retain more brightness precision.
Rgb555 quantize_intensity_rgb555(Rgb555 color, std::uint8_t intensity);

// Resolve the operand plane before blending. Absence is distinct from black.
struct CompositeBlend {
    ColorBlend covered = ColorBlend::none, uncovered = ColorBlend::none;
    bool operator==(const CompositeBlend&) const = default;
};
// With an absent secondary image, use full-strength color math. A fixed
// operand remains present even where no secondary image covers the pixel.
constexpr CompositeBlend composite_blend(bool subtract, bool halve, bool fixed_operand) {
    const auto full = subtract ? ColorBlend::subtract : ColorBlend::add;
    const auto half = subtract ? ColorBlend::half_difference : ColorBlend::average;
    return {halve ? half : full, halve && fixed_operand ? half : full};
}
struct RasterCoverage {
    bool visible = true, operand = false;
};
struct RasterLine {
    std::int32_t offset_x = 0, offset_y = 0;
    std::uint16_t left = 0, right = 256; // Half-open destination clip.
    ColorBlend blend = ColorBlend::none;
    Rgb555 operand = 0;
    bool enabled = true; // Suppress both visible and operand coverage for this row.
    std::optional<RasterCoverage> coverage = std::nullopt; // Absent inherits the layer's coverage.
    // Hold a source row and repeat horizontally sampled texels before clipping,
    // priority and color math. Coordinates remain in destination space before
    // adding this row's scroll offsets; objects have independent sampling.
    std::uint8_t sample_width = 1;
    std::optional<std::int32_t> sample_row = std::nullopt;
};
struct RasterCell {
    std::uint32_t tile = 0, palette = 0;
    std::uint16_t depth = 1; // Larger depth wins; first submitted wins ties.
    bool flip_x = false, flip_y = false;
    // A retained resource may not have an owner yet. Reject when sampled;
    // keeping an off-screen unknown is distinct from inventing a blank tile.
    bool available = true;
};
struct TileLayer {
    std::uint16_t columns = 0, rows = 0;
    std::vector<RasterCell> cells;
    std::vector<Tile> tiles;
    std::vector<std::array<Rgb555, 16>> palettes;
    std::vector<RasterLine> lines;
    bool wrap_x = true, wrap_y = true;
    bool visible = true, operand = false;
    bool clip_operand = true; // False keeps operand coverage outside visible row clips.
    CompositeBlend mix;
};
struct AffineLine {
    // Source position at destination x=0, followed by the per-pixel increment.
    // All four values have eight fractional bits. Negative fractions floor.
    std::int32_t source_x = 0, source_y = 0, step_x = 256, step_y = 0;
    std::uint16_t left = 0, right = 256;
    bool enabled = true;
    std::optional<RasterCoverage> coverage = std::nullopt;
    std::uint8_t sample_width = 1;
};
struct AffineLayer {
    // Ordinary row-major tile identities and eight-bit indexed texels. Neither
    // the layout nor the tile pool is a device memory image.
    std::uint16_t columns = 0, rows = 0, depth = 1;
    std::vector<std::uint32_t> cells;
    std::vector<Tile> tiles;
    std::array<Rgb555, 256> colors{};
    std::vector<AffineLine> lines;
    bool wrap_x = true, wrap_y = true;
    bool visible = true, operand = false, clip_operand = true;
    CompositeBlend mix;
};
struct RasterSprite {
    std::int32_t x = 0, y = 0;
    std::uint16_t width = 0, height = 0, depth = 1;
    Bytes pixels; // Row-major palette indices, zero transparent.
    std::array<Rgb555, 16> colors{};
    bool flip_x = false, flip_y = false;
    CompositeBlend mix;
    bool allow_color_effect = true;
};
// Replace ordinary layer/composite blending with a fixed color effect on a
// completed row. Transparent backdrop participation is explicitly selected.
struct RasterColorEffect {
    Rgb555 operand = 0;
    ColorBlend operation = ColorBlend::none;
    bool include_backdrop = false;
};
struct SpriteLine {
    std::uint16_t left = 0, right = 256;
    bool enabled = true;
    std::optional<RasterCoverage> coverage = std::nullopt;
};
struct SpriteLayer {
    // First opaque sprite wins the object plane, independently of its depth
    // against backgrounds. Depth must never reorder overlapping objects.
    std::vector<RasterSprite> sprites;
    std::vector<SpriteLine> lines;
    bool visible = true, operand = false, clip_operand = true;
};
struct RasterScene {
    std::uint16_t width = 256, height = 224;
    Rgb555 backdrop = 0;
    std::vector<TileLayer> layers;
    std::vector<std::uint8_t> intensity; // Exactly one 0..15 value per output row.
    Rgb555 operand_fallback = 0;
    CompositeBlend backdrop_mix;
    std::optional<SpriteLayer> objects;
    // An empty vector inherits each winning pixel's blend. Otherwise one
    // optional override per row, including backdrop pixels and the side clips.
    std::vector<std::optional<CompositeBlend>> blend_rows{};
    // Tile layers precede affine layers on equal depth. Within each collection,
    // first submitted wins ties; objects retain their separate ordering rule.
    std::vector<AffineLayer> affine_layers{};
    // Optional fixed operand per output row. Covered operand pixels still win;
    // absence continues to use operand_fallback for the entire scene.
    std::vector<Rgb555> operand_fallback_rows{};
    // Optional half-open region on each row where color blending is allowed.
    // Visibility and operand coverage remain independent of this effect mask.
    std::vector<std::array<std::uint16_t, 2>> blend_spans{};
    std::vector<std::optional<RasterColorEffect>> color_effect_rows{};
};
struct CanonicalFrame {
    std::uint16_t width, height;
    std::vector<Rgb555> colors;
    std::vector<std::uint8_t> intensity;
};
// Semantic layers/coordinates/effects only. No register or device-memory inputs.
// Palette index zero is transparent; backdrop is explicit. Validate before drawing.
CanonicalFrame render_scene(const RasterScene& scene);
} // namespace ff6
