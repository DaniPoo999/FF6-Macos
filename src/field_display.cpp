#include "rom-materials/field_flash_setup.macros.inc"
#include "rom-materials/field_palette_flash_bias.macros.inc"
#include "rom-materials/field_shake_decay_numerator.macros.inc"
#include "rom-materials/field_shake_decay_denominator.macros.inc"
#include "ff6/field_display.hpp"
#include "rom-materials/field_viewport_bounds.macros.inc"
#include <limits>

namespace ff6 {
namespace {
void validate_colors(const std::array<Rgb555, 128>& colors) {
    for (auto color : colors)
        if (color > 0x7fff)
            throw std::invalid_argument("Invalid field palette color");
}
} // namespace
void FieldFlashlight::advance(std::uint8_t center_x, std::uint8_t center_y) {
    if (!active) return;
    if (target > 62 || radius > 62 || (target & 1) || (radius & 1))
        throw std::invalid_argument("Invalid field flashlight radius");
    if (radius < target) radius += 2;
    else if (radius > target) radius -= 2;
    spans.fill({0, 0});
    if (!radius) { active = false; return; }
    // C0/UpdateFlashlight uses two fixed-point accumulators and a truncated
    // reciprocal. A Euclidean circle would change the authored pixel contour.
    std::array<unsigned, 64> widths{};
    const unsigned inverse = 256U / radius;
    int x = int{radius} * 256 + 128;
    unsigned y = 0;
    while (x >= 0) {
        const unsigned row = y >> 8;
        x -= static_cast<int>(row * inverse);
        if (x < 0) break;
        if (row >= widths.size()) throw std::logic_error("Invalid flashlight contour");
        widths[row] = static_cast<unsigned>(x) >> 8;
        y += widths[row] * inverse;
    }
    auto put = [&](int row, unsigned width) {
        if (row < 0 || row >= static_cast<int>(spans.size())) return;
        spans[static_cast<std::size_t>(row)] = {
            static_cast<std::uint16_t>(std::max(0, int{center_x} - static_cast<int>(width))),
            static_cast<std::uint16_t>(std::min(256U, unsigned{center_x} + width + 1U))};
    };
    // The top loop omits width[0]; the bottom begins one row above center_y.
    // This asymmetry is visible, so retain it rather than mirroring a circle.
    for (unsigned i = 1; i < radius; ++i)
        put(int{center_y} - 1 - static_cast<int>(i), widths[i]);
    for (unsigned i = 0; i < radius; ++i)
        put(int{center_y} - 1 + static_cast<int>(i), widths[i]);
}
void FieldFlashlight::apply(RasterScene& scene, std::uint8_t mode) const {
    FieldWindowMask{active, spans}.apply(scene, mode);
}
void FieldWindowMask::apply(RasterScene& scene, std::uint8_t mode) const {
    if (!active) return;
    if (mode > 3 || scene.layers.size() < 3 || scene.height != 224)
        throw std::invalid_argument("Invalid field window scene");
    const auto interval = [&](std::size_t row) {
        return row >= FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_3 && row < FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_2 ? spans[row - FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_3] : std::array<std::uint16_t, 2>{0, 0};
    };
    auto intersect = [&](auto& line, std::array<std::uint16_t, 2> span) {
        line.left = std::max(line.left, span[0]);
        line.right = std::max(line.left, std::min(line.right, span[1]));
    };
    // InitColorMath and SetWindowSelect restrict blending to the shared
    // aperture even when a layer itself is not clipped. This supplies the
    // pyramid's solid tint behind its BG3 texture, without tinting the room.
    scene.blend_spans.resize(scene.height);
    for (std::size_t row = 0; row < scene.height; ++row) {
        auto span = interval(row);
        span[0] = std::max<std::uint16_t>(FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_0, span[0]);
        span[1] = std::max(span[0], std::min<std::uint16_t>(FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_1 + 1, span[1]));
        scene.blend_spans[row] = span;
    }
    if (mode == 0) return;
    if (mode == 1) {
        // Tent roof: reveal the interior through BG2, retaining BG1/objects.
        // Split only visible coverage; the map's color operand is unmasked.
        auto right = scene.layers[1];
        right.operand = false;
        for (std::size_t row = 0; row < scene.height; ++row) {
            if (right.lines[row].coverage) right.lines[row].coverage->operand = false;
            const auto span = interval(row);
            intersect(scene.layers[1].lines[row], {0, span[0]});
            intersect(right.lines[row], {span[1], scene.width});
        }
        scene.layers.push_back(std::move(right));
    } else {
        for (unsigned layer = mode == 3 ? 2U : 0U; layer < 3; ++layer)
            for (std::size_t row = 0; row < scene.height; ++row)
                intersect(scene.layers[layer].lines[row], interval(row));
        if (mode == 2 && scene.objects)
            for (std::size_t row = 0; row < scene.objects->lines.size(); ++row)
                intersect(scene.objects->lines[row], interval(row));
    }
}
void FieldColorEffect::flash(std::uint8_t colors) {
    channels = colors & 224U;
    counter = static_cast<std::uint8_t>(FF6_ROM_VALUE_FIELD_FLASH_SETUP_0);
    rate = static_cast<std::uint8_t>(FF6_ROM_VALUE_FIELD_FLASH_SETUP_0 >> 8U);
    target = 0;
    subtract = increasing = false;
}
void FieldColorEffect::ramp(std::uint8_t control, bool subtraction) {
    // EventCmd_56/57 tests the current fractional intensity, not the new
    // command's direction. The second command fades the existing effect out.
    if (counter) {
        increasing = false;
        return;
    }
    channels = control & 224U;
    target = static_cast<std::uint8_t>((control & 7U) * 4U + 3U);
    constexpr std::array<std::uint8_t, 4> rates{
#include "rom-materials/field_color_rates.inc"
    };
    rate = rates[(control >> 3U) & 3U];
    counter = 0;
    subtract = subtraction;
    increasing = true;
}
void FieldColorEffect::apply(RasterScene& scene) {
    if (!rate) return;
    if (increasing) {
        counter = wrap8(unsigned{counter} + rate);
        // UpdateFixedColor discards the fractional overshoot once above the
        // target, holding the authored five-bit value at all supported rates.
        if (counter > unsigned{target} * 8U) counter &= 248U;
    } else {
        if (!counter) { rate = 0; return; }
        counter = wrap8(unsigned{counter} - rate);
    }
    const unsigned intensity = unsigned{counter} >> 3U;
    Rgb555 color = 0;
    for (unsigned i = 0; i < 3; ++i)
        if (channels & (32U << i))
            color |= static_cast<Rgb555>(unsigned{intensity} << (5 * i));
    // EventCmd_55/UpdateFixedColor replaces ordinary subscreen color math
    // for this interval. The map's retained composition is used again after
    // the flash; sprites and dialogue are not brightened.
    scene.backdrop_mix = {};
    for (auto& layer : scene.layers) {
        layer.operand = false;
        layer.mix = {};
        for (auto& row : layer.lines) {
            row.blend = subtract ? ColorBlend::subtract : ColorBlend::add;
            row.operand = color;
            if (row.coverage)
                row.coverage->operand = false;
        }
    }
    if (scene.objects) {
        scene.objects->operand = false;
        for (auto& sprite : scene.objects->sprites)
            sprite.mix = {};
        for (auto& row : scene.objects->lines)
            if (row.coverage)
                row.coverage->operand = false;
    }
}
Rgb555 filter_field_color(Rgb555 original, std::uint8_t channels) {
    if (channels > 7) throw std::invalid_argument("Invalid field palette filter channels");
    const unsigned average = ((original & 31U) + ((original >> 5U) & 31U) +
                              ((original >> 10U) & 31U)) / 3U;
    return static_cast<Rgb555>(((channels & 1U) ? average : 0U) |
                              ((channels & 2U) ? average << 5U : 0U) |
                              ((channels & 4U) ? average << 10U : 0U));
}
void FieldDisplay::filter_palette(std::uint8_t channels, std::uint8_t first, std::uint8_t last) {
    if (channels > 7 || first > last || last >= 128)
        throw std::invalid_argument("Invalid background filter range");
    for (unsigned i = first; i <= last; ++i)
        palette_.pending[i] = filter_field_color(palette_.base[i], channels);
}
void modify_field_background_palette(FieldPaletteState& palette, std::uint8_t control,
                                     std::uint8_t first, std::uint8_t last) {
    if (first >= 128 || last >= 128) throw std::invalid_argument("Invalid background palette range");
    const auto mode = control >> 5U;
    if (mode == 0 || mode == 7) { palette.pending = palette.base; return; }
    constexpr std::array<unsigned, 4> intensities{
#include "rom-materials/field_color_intensities.inc"
    };
    auto next = palette.pending;
    const unsigned count = ((unsigned{last} + 128U - first) & 127U) + 1U;
    for (unsigned step = 0; step < count; ++step) {
        const unsigned i = (unsigned{first} + step) & 127U;
        if (mode == 3 || mode == 6) {
            // ColorUnInc/UnDec stop on equality, not on crossing the base.
            // Mixed scripted effects and animated palettes can put a channel
            // on either side. Their byte/word wrapping and the comparison's
            // borrow/carry visibly spill into adjacent components on recombine.
            const auto current = palette.pending[i], original = palette.base[i];
            const unsigned red = current & 31U, red_base = original & 31U;
            const unsigned blue = (current >> 8U) & 124U, blue_base = (original >> 8U) & 124U;
            const unsigned green = current & 992U, green_base = original & 992U;
            const bool increase = mode == 6;
            const auto r = red == red_base ? red : wrap8(increase ? red + 1U : red - 1U);
            const auto b = blue == blue_base ? blue : wrap8(increase
                ? blue + 4U + unsigned(blue > blue_base)
                : blue - 4U - unsigned(blue < blue_base));
            const auto g = green == green_base ? green : wrap16(increase
                ? green + 32U + unsigned(green > green_base)
                : green - 32U - unsigned(green < green_base));
            next[i] = static_cast<Rgb555>((r | (b << 8U) | g) & 0x7fffU);
            continue;
        }
        unsigned color = 0;
        for (unsigned channel = 0; channel < 3; ++channel) {
            auto value = (palette.pending[i] >> (channel * 5)) & 31U;
            if (mode == 2) {
                // BGColorIncFlash replaces the supplied mask/intensity with
                // full-channel additions, saturating every selected color white.
                value = 31;
            } else if (mode == 5) {
                // InitColorMod complements the selected intensity; DecFlash
                // adds four before subtracting it from the current color.
                const auto amount =
                    (31U + FF6_ROM_VALUE_FIELD_PALETTE_FLASH_BIAS_0) - ((control & (16U >> channel)) ? intensities[control & 3U] : 0U);
                if (channel == 2) {
                    // Blue uses an eight-bit shifted subtraction and tests its
                    // sign. Preserve the visible wrap for differences below -128.
                    const auto shifted = wrap8(value * 4U - amount * 4U);
                    value = (shifted & 128U) ? 0U : shifted >> 2U;
                } else {
                    value = value > amount ? value - amount : 0U;
                }
            } else if (mode == 1) {
                const auto target = (control & (16U >> channel)) ? intensities[control & 3U] : 0;
                if (value < target) ++value;
            } else if (mode == 4) {
                const auto target = 31U - ((control & (16U >> channel)) ? intensities[control & 3U] : 0U);
                if (value > target) --value;
            }
            color |= value << (channel * 5);
        }
        next[i] = static_cast<Rgb555>(color);
    }
    palette.pending = next;
}
void modify_field_sprite_palettes(std::array<std::array<Rgb555, 16>, 8>& colors,
                                  const std::array<std::array<Rgb555, 16>, 8>& base,
                                  std::uint8_t control, std::uint8_t first, std::uint8_t last) {
    // Field SpriteColor* uses the same channel arithmetic as BGColor*, but
    // its range is relative to the eight sprite palettes. Restore modes 0/7
    // restore all 128 colors even when the event supplied a narrower range.
    FieldPaletteState palette{};
    for (unsigned i = 0; i < 128; ++i) {
        palette.base[i] = base[i / 16][i % 16];
        palette.pending[i] = colors[i / 16][i % 16];
    }
    modify_field_background_palette(palette, control, first, last);
    for (unsigned i = 0; i < 128; ++i)
        colors[i / 16][i % 16] = palette.pending[i];
}
void FieldShake::advance(bool odd_frame, const std::array<std::uint8_t, 256>& random,
                         std::uint8_t& random_index) {
    // Field UpdateShake: alternating displacement and rest frames, with fixed-eight
    // decay. The authored recurrence uses the amplitude masks for frequency too.
    constexpr std::array<std::uint8_t, 4> masks{
#include "rom-materials/field_shake_masks.inc"
    };
    if (odd_frame) {
        amplitude = static_cast<std::uint8_t>(unsigned{amplitude} * FF6_ROM_VALUE_FIELD_SHAKE_DECAY_NUMERATOR_0 / FF6_ROM_VALUE_FIELD_SHAKE_DECAY_DENOMINATOR_0);
        offsets.fill(0);
        return;
    }
    const auto strength = control & 3U;
    const auto frequency = (control >> 2U) & 3U;
    const auto draw = [&] { random_index = wrap8(unsigned{random_index} + 1U); return random[random_index]; };
    if (strength && !frequency) {
        control &= 252U;
        amplitude = masks[strength];
    } else if (strength && !(draw() & masks[frequency])) amplitude = draw() & masks[strength];
    for (unsigned i = 0; i < offsets.size(); ++i)
        if (control & (16U << i)) offsets[i] = amplitude;
}
void apply_field_mosaic(RasterScene& scene, std::uint8_t block_size) {
    if (block_size == 0 || block_size > 16)
        throw std::invalid_argument("Invalid field mosaic block size");
    for (auto& layer : scene.layers)
        for (std::size_t row = 0; row < layer.lines.size(); ++row) {
            auto& line = layer.lines[row];
            line.sample_width = block_size;
            line.sample_row = static_cast<std::int32_t>(row - row % block_size);
        }
}
RasterScene make_field_display_scene(const FieldMap& map,
                                     const std::array<FieldOrigin, 3>& logical_origins,
                                     std::uint8_t intensity, const FieldMapView* view,
                                     std::array<bool, 3> include_inactive) {
    if (intensity > 15)
        throw std::invalid_argument("Invalid field display intensity");
    auto origins = logical_origins;
    for (auto& origin : origins) {
        if (origin.x < std::numeric_limits<std::int32_t>::min() + FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_0 ||
            origin.y == std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("Field display origin exceeds coordinate range");
        origin.x -= FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_0;
        origin.y += 1;
    }
    auto scene = make_field_background_scene(map, origins, 256, 224, view, include_inactive);
    for (std::size_t row = 0; row < scene.height; ++row)
        scene.intensity[row] = row < FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_2 ? intensity : 0;
    for (auto& layer : scene.layers) {
        layer.clip_operand = false;
        for (std::size_t row = 0; row < layer.lines.size(); ++row) {
            auto& line = layer.lines[row];
            line.left = FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_0;
            line.right = FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_1 + 1;
            // The initial seven-row mask removes layers from both planes. The
            // lower band is blanked after the original visible-field transfer.
            line.enabled = row >= FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_3 && row < FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_2;
        }
    }
    return scene;
}
FieldDisplay::FieldDisplay(FieldMap map, FieldAnimationAssets animation, bool third_phase,
                           std::optional<FieldPaletteState> palette)
    : map_(std::move(map)), original_composition_(map_.composition), animation_(std::move(animation)),
      palette_(palette.value_or(FieldPaletteState{map_.colors, map_.colors})),
      third_phase_(third_phase) {
    validate_colors(map_.colors);
    validate_colors(palette_.base);
    validate_colors(palette_.pending);
}
void FieldDisplay::modify_palette(std::uint8_t control, std::uint8_t first, std::uint8_t last) {
    modify_field_background_palette(palette_, control, first, last);
}
void FieldDisplay::restore_color_math() {
    // Event 54 restores the map's saved blend/sub-layer selection. Palette
    // modifications are independent and must survive this reset.
    map_.composition = original_composition_;
}
void FieldDisplay::load_palette(std::uint8_t destination, const std::array<Rgb555, 16>& colors) {
    if (destination >= 8)
        throw std::invalid_argument("Invalid background palette destination");
    for (auto color : colors)
        if (color > 0x7fff)
            throw std::invalid_argument("Invalid loaded palette color");
    // Event 60 replaces both the base and current colors; later filters must
    // start from this palette, rather than the map's original colors.
    std::copy(colors.begin(), colors.end(), palette_.base.begin() + destination * 16);
    std::copy(colors.begin(), colors.end(), palette_.pending.begin() + destination * 16);
}
void FieldDisplay::advance_palette() {
    animation_.advance_palette(palette_.base, palette_.pending);
}
void FieldDisplay::refresh_text_color(Rgb555 font_color) {
    set_field_text_colors(palette_.base, palette_.pending, font_color);
}
void FieldDisplay::set_window_colors(const std::array<Rgb555, 7>& colors) {
    set_field_window_colors(palette_.base, palette_.pending, colors);
}
void FieldDisplay::commit(std::uint8_t intensity) {
    if (displaying_ || intensity > 15)
        throw std::invalid_argument("Invalid field display commit");
    map_.colors = palette_.pending;
    third_phase_ = !third_phase_;
    intensity_ = intensity;
    displaying_ = true;
}
RasterScene FieldDisplay::scene(const std::array<FieldOrigin, 3>& logical_origins,
                                std::array<bool, 3> include_inactive) const {
    if (!displaying_)
        throw std::invalid_argument("Field display has no committed frame");
    return make_field_display_scene(map_, logical_origins, intensity_, view_ ? &*view_ : nullptr,
                                    include_inactive);
}
void FieldDisplay::commit(std::uint8_t intensity, const FieldMapView& view) {
    commit(intensity);
    view_ = view;
}
void FieldDisplay::finish() {
    if (!displaying_)
        throw std::invalid_argument("Field display has no frame to finish");
    animation_.advance_backgrounds(map_, third_phase_ ? BackgroundAnimationPhase::third
                                                      : BackgroundAnimationPhase::first_and_second);
    displaying_ = false;
}
} // namespace ff6
