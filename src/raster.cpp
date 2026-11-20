#include "ff6/raster.hpp"
#include <algorithm>

namespace ff6 {
namespace {
void valid_color(Rgb555 color) {
    if (color > 0x7fff)
        throw std::runtime_error("Canonical color exceeds RGB555");
}
void valid_blend(ColorBlend operation) {
    switch (operation) {
    case ColorBlend::none:
    case ColorBlend::add:
    case ColorBlend::subtract:
    case ColorBlend::average:
    case ColorBlend::half_difference:
        return;
    }
    throw std::runtime_error("Invalid color blend operation");
}
int component(int first, int second, ColorBlend operation) {
    switch (operation) {
    case ColorBlend::none:
        return first;
    case ColorBlend::add:
        return std::min(31, first + second);
    case ColorBlend::subtract:
        return std::max(0, first - second);
    case ColorBlend::average:
        // Divide before saturation: bright operands must not saturate to 31 first.
        return (first + second) / 2;
    case ColorBlend::half_difference:
        return std::max(0, first - second) / 2;
    }
    throw std::runtime_error("Invalid color blend operation");
}
std::int64_t coordinate(std::int64_t value, std::int64_t extent, bool wrap) {
    if (!wrap)
        return value >= 0 && value < extent ? value : -1;
    const auto residue = value % extent;
    return residue < 0 ? residue + extent : residue;
}
std::int64_t whole_texel(std::int64_t fixed) {
    // C++ division truncates toward zero. Sampling requires floor, including
    // the interval [-255,-1], which belongs to texel -1 rather than texel 0.
    return fixed / 256 - (fixed % 256 < 0 ? 1 : 0);
}
void validate(const RasterScene& scene) {
    if (scene.width == 0 || scene.height == 0 || scene.width > 4096 || scene.height > 4096 ||
        scene.layers.size() > 512 || scene.affine_layers.size() > 512 ||
        scene.layers.size() + scene.affine_layers.size() > 512 ||
        scene.intensity.size() != scene.height)
        throw std::runtime_error("Invalid raster scene dimensions");
    valid_color(scene.backdrop);
    valid_color(scene.operand_fallback);
    if (!scene.color_effect_rows.empty() && scene.color_effect_rows.size() != scene.height)
        throw std::runtime_error("Invalid raster color-effect extent");
    for (const auto& effect : scene.color_effect_rows)
        if (effect) {
            valid_color(effect->operand);
            valid_blend(effect->operation);
        }
    if (!scene.operand_fallback_rows.empty() && scene.operand_fallback_rows.size() != scene.height)
        throw std::runtime_error("Invalid raster row operand extent");
    for (auto color : scene.operand_fallback_rows)
        valid_color(color);
    valid_blend(scene.backdrop_mix.covered);
    valid_blend(scene.backdrop_mix.uncovered);
    if (!scene.blend_rows.empty() && scene.blend_rows.size() != scene.height)
        throw std::runtime_error("Invalid raster row blend extent");
    if (!scene.blend_spans.empty() && scene.blend_spans.size() != scene.height)
        throw std::runtime_error("Invalid raster blend-mask extent");
    for (const auto span : scene.blend_spans)
        if (span[0] > span[1] || span[1] > scene.width)
            throw std::runtime_error("Invalid raster blend-mask interval");
    for (const auto& blend : scene.blend_rows)
        if (blend) {
            valid_blend(blend->covered);
            valid_blend(blend->uncovered);
        }
    for (auto intensity : scene.intensity)
        if (intensity > 15)
            throw std::runtime_error("Invalid raster intensity");
    for (const auto& layer : scene.affine_layers) {
        if (layer.columns == 0 || layer.rows == 0 || layer.columns > 512 || layer.rows > 512 ||
            layer.cells.size() != std::size_t{layer.columns} * layer.rows ||
            layer.lines.size() != scene.height || layer.tiles.empty() || layer.depth == 0)
            throw std::runtime_error("Invalid affine layer dimensions or depth");
        for (const auto tile : layer.cells)
            if (tile >= layer.tiles.size())
                throw std::runtime_error("Invalid affine tile reference");
        for (const auto color : layer.colors)
            valid_color(color);
        for (const auto& line : layer.lines)
            if (line.left > line.right || line.right > scene.width || line.sample_width == 0 || line.sample_width > 16)
                throw std::runtime_error("Invalid affine clip or sample width");
        valid_blend(layer.mix.covered);
        valid_blend(layer.mix.uncovered);
    }
    if (scene.objects) {
        const auto& layer = *scene.objects;
        if (layer.sprites.size() > 512 || layer.lines.size() != scene.height)
            throw std::runtime_error("Invalid sprite layer dimensions");
        for (const auto& line : layer.lines)
            if (line.left > line.right || line.right > scene.width)
                throw std::runtime_error("Invalid sprite clip");
        for (const auto& sprite : layer.sprites) {
            if (sprite.width == 0 || sprite.height == 0 || sprite.width > 64 ||
                sprite.height > 64 ||
                sprite.pixels.size() != std::size_t{sprite.width} * sprite.height ||
                sprite.depth == 0)
                throw std::runtime_error("Invalid sprite dimensions or depth");
            for (auto pixel : sprite.pixels)
                if (pixel > 15)
                    throw std::runtime_error("Invalid sprite palette index");
            for (auto color : sprite.colors)
                valid_color(color);
            valid_blend(sprite.mix.covered);
            valid_blend(sprite.mix.uncovered);
        }
    }
    for (const auto& layer : scene.layers) {
        valid_blend(layer.mix.covered);
        valid_blend(layer.mix.uncovered);
        if (layer.columns == 0 || layer.rows == 0 || layer.columns > 512 || layer.rows > 512 ||
            layer.cells.size() != std::size_t{layer.columns} * layer.rows ||
            layer.lines.size() != scene.height || layer.tiles.empty() || layer.palettes.empty())
            throw std::runtime_error("Invalid tile layer dimensions");
        for (const auto& cell : layer.cells)
            if (cell.tile >= layer.tiles.size() || cell.palette >= layer.palettes.size() ||
                cell.depth == 0)
                throw std::runtime_error("Invalid tile layer reference");
        for (const auto& tile : layer.tiles)
            for (auto index : tile)
                if (index > 15)
                    throw std::runtime_error("Raster tile needs a four-bit palette index");
        for (const auto& palette : layer.palettes)
            for (auto color : palette)
                valid_color(color);
        for (const auto& line : layer.lines) {
            if (line.left > line.right || line.right > scene.width)
                throw std::runtime_error("Invalid raster clip");
            if (line.sample_width == 0 || line.sample_width > 16)
                throw std::runtime_error("Invalid raster sample width");
            valid_color(line.operand);
            valid_blend(line.blend);
        }
    }
}
} // namespace

Rgb555 blend_color(Rgb555 base, Rgb555 operand, ColorBlend operation) {
    valid_color(base);
    valid_color(operand);
    valid_blend(operation);
    std::uint32_t result = 0;
    for (unsigned shift = 0; shift < 15; shift += 5) {
        const auto channel = component((base >> shift) & 31, (operand >> shift) & 31, operation);
        result |= static_cast<std::uint32_t>(channel) << shift;
    }
    return static_cast<Rgb555>(result);
}
Rgb555 quantize_intensity_rgb555(Rgb555 color, std::uint8_t intensity) {
    valid_color(color);
    if (intensity > 15)
        throw std::runtime_error("Invalid raster intensity");
    std::uint32_t result = 0;
    for (unsigned shift = 0; shift < 15; shift += 5)
        result |= ((((std::uint32_t{color} >> shift) & 31U) * intensity) / 15U) << shift;
    return static_cast<Rgb555>(result);
}
CanonicalFrame render_scene(const RasterScene& scene) {
    validate(scene);
    const auto count = std::size_t{scene.width} * scene.height;
    CanonicalFrame output{scene.width, scene.height, std::vector<Rgb555>(count, scene.backdrop),
                          scene.intensity};
    std::vector<std::uint16_t> depths(count, 0), operand_depths(count, 0);
    std::vector<Rgb555> operands(count, scene.operand_fallback);
    for (std::size_t y = 0; y < scene.operand_fallback_rows.size(); ++y)
        std::fill_n(operands.begin() + static_cast<std::ptrdiff_t>(y * scene.width), scene.width,
                    scene.operand_fallback_rows[y]);
    std::vector<CompositeBlend> mixes(count, scene.backdrop_mix);
    std::vector<bool> effect_allowed(count, true);
    for (const auto& layer : scene.layers) {
        const std::int64_t width = std::int64_t{layer.columns} * 8;
        const std::int64_t height = std::int64_t{layer.rows} * 8;
        for (unsigned y = 0; y < scene.height; ++y) {
            const auto& line = layer.lines[y];
            if (!line.enabled)
                continue;
            const auto sy =
                coordinate(std::int64_t{line.sample_row.value_or(static_cast<std::int32_t>(y))} +
                               line.offset_y,
                           height, layer.wrap_y);
            if (sy < 0)
                continue;
            const auto coverage =
                line.coverage.value_or(RasterCoverage{layer.visible, layer.operand});
            const unsigned left = coverage.operand && !layer.clip_operand ? 0 : line.left;
            const unsigned right =
                coverage.operand && !layer.clip_operand ? scene.width : line.right;
            for (unsigned x = left; x < right; ++x) {
                const auto sx = coordinate(std::int64_t{x - x % line.sample_width} + line.offset_x,
                                           width, layer.wrap_x);
                if (sx < 0)
                    continue;
                const auto target = std::size_t{y} * scene.width + x;
                const auto cell_index = static_cast<std::size_t>(sy / 8) * layer.columns +
                                        static_cast<std::size_t>(sx / 8);
                const auto& cell = layer.cells[cell_index];
                const bool visible = coverage.visible && x >= line.left && x < line.right &&
                                     cell.depth > depths[target];
                const bool operand = coverage.operand && cell.depth > operand_depths[target];
                if (!visible && !operand)
                    continue;
                if (!cell.available)
                    throw std::runtime_error("Raster sample needs an unowned retained tile at " +
                                             std::to_string(sx) + "," + std::to_string(sy) +
                                             " (output " + std::to_string(x) + "," +
                                             std::to_string(y) + ")");
                const auto tx = static_cast<std::size_t>(sx % 8);
                const auto ty = static_cast<std::size_t>(sy % 8);
                const auto index = layer.tiles[cell.tile][(cell.flip_y ? 7 - ty : ty) * 8 +
                                                          (cell.flip_x ? 7 - tx : tx)];
                if (index == 0)
                    continue;
                const bool blend = scene.blend_spans.empty() ||
                    (x >= scene.blend_spans[y][0] && x < scene.blend_spans[y][1]);
                const auto color = blend_color(layer.palettes[cell.palette][index], line.operand,
                    blend && (scene.color_effect_rows.empty() || !scene.color_effect_rows[y])
                        ? line.blend : ColorBlend::none);
                if (visible) {
                    output.colors[target] = color;
                    depths[target] = cell.depth;
                    mixes[target] = layer.mix;
                }
                if (operand) {
                    operands[target] = color;
                    operand_depths[target] = cell.depth;
                }
            }
        }
    }
    for (const auto& layer : scene.affine_layers) {
        const std::int64_t width = std::int64_t{layer.columns} * 8;
        const std::int64_t height = std::int64_t{layer.rows} * 8;
        for (unsigned y = 0; y < scene.height; ++y) {
            const auto& line = layer.lines[y];
            if (!line.enabled)
                continue;
            const auto coverage =
                line.coverage.value_or(RasterCoverage{layer.visible, layer.operand});
            const unsigned left = coverage.operand && !layer.clip_operand ? 0 : line.left;
            const unsigned right =
                coverage.operand && !layer.clip_operand ? scene.width : line.right;
            for (unsigned x = left; x < right; ++x) {
                const auto sx = coordinate(
                    whole_texel(std::int64_t{line.source_x} + std::int64_t{x - x % line.sample_width} * line.step_x), width,
                    layer.wrap_x);
                const auto sy = coordinate(
                    whole_texel(std::int64_t{line.source_y} + std::int64_t{x - x % line.sample_width} * line.step_y),
                    height, layer.wrap_y);
                if (sx < 0 || sy < 0)
                    continue;
                const auto cell = static_cast<std::size_t>(sy / 8) * layer.columns +
                                  static_cast<std::size_t>(sx / 8);
                const auto texel =
                    static_cast<std::size_t>(sy % 8) * 8 + static_cast<std::size_t>(sx % 8);
                const auto index = layer.tiles[layer.cells[cell]][texel];
                if (index == 0)
                    continue;
                const auto target = std::size_t{y} * scene.width + x;
                if (coverage.visible && x >= line.left && x < line.right &&
                    layer.depth > depths[target]) {
                    output.colors[target] = layer.colors[index];
                    depths[target] = layer.depth;
                    mixes[target] = layer.mix;
                }
                if (coverage.operand && layer.depth > operand_depths[target]) {
                    operands[target] = layer.colors[index];
                    operand_depths[target] = layer.depth;
                }
            }
        }
    }
    if (scene.objects) {
        const auto& layer = *scene.objects;
        std::vector<bool> claimed(count, false);
        for (const auto& sprite : layer.sprites) {
            // Widen before adding dimensions, including hostile INT32 endpoints.
            const auto left = std::max<std::int64_t>(0, sprite.x);
            const auto right =
                std::min<std::int64_t>(scene.width, std::int64_t{sprite.x} + sprite.width);
            const auto top = std::max<std::int64_t>(0, sprite.y);
            const auto bottom =
                std::min<std::int64_t>(scene.height, std::int64_t{sprite.y} + sprite.height);
            for (auto y = top; y < bottom; ++y) {
                const auto& line = layer.lines[static_cast<std::size_t>(y)];
                if (!line.enabled)
                    continue;
                const auto coverage =
                    line.coverage.value_or(RasterCoverage{layer.visible, layer.operand});
                for (auto x = left; x < right; ++x) {
                    const auto target =
                        static_cast<std::size_t>(y) * scene.width + static_cast<std::size_t>(x);
                    if (claimed[target])
                        continue;
                    const auto sx = static_cast<std::size_t>(x - sprite.x);
                    const auto sy = static_cast<std::size_t>(y - sprite.y);
                    const auto tx = sprite.flip_x ? sprite.width - 1U - sx : sx;
                    const auto ty = sprite.flip_y ? sprite.height - 1U - sy : sy;
                    const auto index = sprite.pixels[ty * sprite.width + tx];
                    if (index == 0)
                        continue;
                    claimed[target] = true;
                    const bool in_clip = x >= line.left && x < line.right;
                    if (coverage.visible && in_clip && sprite.depth > depths[target]) {
                        output.colors[target] = sprite.colors[index];
                        depths[target] = sprite.depth;
                        mixes[target] = sprite.mix;
                        effect_allowed[target] = sprite.allow_color_effect;
                    }
                    if (coverage.operand && (!layer.clip_operand || in_clip) &&
                        sprite.depth > operand_depths[target]) {
                        operands[target] = sprite.colors[index];
                        operand_depths[target] = sprite.depth;
                    }
                }
            }
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (!scene.blend_spans.empty()) {
            const auto span = scene.blend_spans[i / scene.width];
            if (i % scene.width < span[0] || i % scene.width >= span[1])
                continue;
        }
        if (!scene.color_effect_rows.empty()) {
            if (const auto& effect = scene.color_effect_rows[i / scene.width]) {
                if (effect_allowed[i] && (depths[i] || effect->include_backdrop))
                    output.colors[i] = blend_color(output.colors[i], effect->operand,
                                                    effect->operation);
                continue;
            }
        }
        const auto mix = scene.blend_rows.empty()
                             ? mixes[i]
                             : scene.blend_rows[i / scene.width].value_or(mixes[i]);
        output.colors[i] = blend_color(output.colors[i], operands[i],
                                       operand_depths[i] ? mix.covered : mix.uncovered);
    }
    return output;
}
} // namespace ff6
