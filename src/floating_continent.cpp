#include "ff6/floating_continent.hpp"
#include "rom-materials/continent_zoom_gate.macros.inc"
#include "rom-materials/continent_scroll_gate.macros.inc"
#include "rom-materials/continent_tint_channels.macros.inc"
#include "rom-materials/continent_blend_mode.macros.inc"
#include "ff6/perspective.hpp"
#include <algorithm>

namespace ff6 {
FloatingContinentAssets decode_floating_continent_assets(ByteView graphics, ByteView palette) {
    if (graphics.size() != 0x900 || palette.size() != 32)
        throw std::invalid_argument("Invalid Floating Continent graphics/palette extent");
    FloatingContinentAssets assets;
    auto& terrain = assets.terrain;
    terrain.columns = terrain.rows = 128;
    terrain.wrap_x = terrain.wrap_y = false;
    terrain.tiles = decode_tiles(graphics.first(0x7c0), 4);
    terrain.cells.resize(128 * 128);
    terrain.lines.resize(224);
    // LoadFloatingContGfx prepares 62 tiles and ten rows of 32 map cells.
    // The original conversion also reads two unused scratch tiles. No layout
    // cell addresses them, so they are not invented as native resources.
    for (unsigned y = 0; y < 10; ++y)
        for (unsigned x = 0; x < 32; ++x) {
            const auto tile = graphics[0x7c0 + y * 32 + x];
            if (tile >= terrain.tiles.size())
                throw std::invalid_argument("Floating Continent references an unprepared tile");
            terrain.cells[y * 128 + x] = tile;
        }
    for (unsigned i = 0; i < 16; ++i)
        assets.palette[i] = read16(palette, i * 2) & 0x7fffU;
    return assets;
}
FloatingContinentAssets load_floating_continent_assets(const ContentPack& pack) {
    return decode_floating_continent_assets(
        decompress_lzss(pack.get("cutscene_assets_floating_cont_bin_lz")),
        pack.get("floating_continent_palette"));
}
FloatingContinentRise::FloatingContinentRise(FloatingContinentAssets assets)
    : assets_(std::move(assets)) {
    if (assets_.terrain.lines.size() != 224)
        throw std::invalid_argument("Invalid Floating Continent presentation rows");
}
void FloatingContinentRise::next_phase() {
    switch (phase_) {
    case ContinentRisePhase::revealing:
        phase_ = ContinentRisePhase::rising;
        remaining_ = FF6_ROM_VALUE_CONTINENT_PHASE_WAITS_1;
        tint_enabled_ = true;
        break;
    case ContinentRisePhase::rising:
        phase_ = ContinentRisePhase::receding;
        remaining_ = FF6_ROM_VALUE_CONTINENT_PHASE_WAITS_2;
        break;
    case ContinentRisePhase::receding:
        phase_ = ContinentRisePhase::finishing;
        // The authored one-visit acceleration is followed by its final
        // publication and termination visit before the source-selected exit fade.
        remaining_ = FF6_ROM_VALUE_CONTINENT_PHASE_WAITS_3 + 2;
        break;
    case ContinentRisePhase::finishing:
        phase_ = ContinentRisePhase::fading;
        remaining_ = FF6_ROM_VALUE_CONTINENT_PHASE_WAITS_4;
        break;
    case ContinentRisePhase::fading:
        phase_ = ContinentRisePhase::complete;
        intensity_ = 0;
        break;
    case ContinentRisePhase::complete:
        break;
    }
}
bool FloatingContinentRise::advance() {
    if (complete()) return false;
    if (!remaining_) next_phase();
    if (complete()) return false;

    // The scale is published before movement tasks; vertical scroll and the
    // shared threshold palette fade are published after those tasks. Preserve
    // these visible owners without reproducing an interrupt/device scheduler.
    if (phase_ != ContinentRisePhase::fading &&
        !(phase_ == ContinentRisePhase::finishing && remaining_ == 1))
        visible_scale_ = scale_;
    fade_.advance(colors_, assets_.palette);
    if (phase_ == ContinentRisePhase::receding) ++scale_;
    else if (phase_ == ContinentRisePhase::finishing && remaining_ == FF6_ROM_VALUE_CONTINENT_PHASE_WAITS_3 + 2) scale_ += 2;
    else if ((updates_ & FF6_ROM_VALUE_CONTINENT_ZOOM_GATE_0) == 0) ++scale_;
    if ((updates_ & FF6_ROM_VALUE_CONTINENT_SCROLL_GATE_0) == 0) ++scroll_y_;

    if (tint_enabled_) {
        // Decode source channel selections independently of their color-register
        // selector bits. The source compares before writing then increments;
        // the native clamp therefore uses the last included blue value.
        if (!tint_wait_) {
            const unsigned red = tint_steps_;
            const unsigned green = tint_steps_ + static_cast<unsigned>(FF6_ROM_VALUE_CONTINENT_TINT_CHANNELS_2);
            const unsigned blue = std::min(static_cast<unsigned>(FF6_ROM_VALUE_CONTINENT_TINT_CHANNELS_0) - 1U,
                tint_steps_ + static_cast<unsigned>(FF6_ROM_VALUE_CONTINENT_TINT_CHANNELS_1));
            tint_ = static_cast<Rgb555>(red | (green << 5U) | (blue << 10U));
            ++tint_steps_;
            tint_wait_ = FF6_ROM_VALUE_CONTINENT_TINT_PERIOD_0;
        }
        --tint_wait_;
    }
    if (phase_ == ContinentRisePhase::fading)
        intensity_ = static_cast<std::uint8_t>(remaining_);
    --remaining_;
    ++updates_;
    return true;
}
RasterScene FloatingContinentRise::scene() const {
    RasterScene result;
    result.backdrop = colors_[0];
    result.intensity.assign(result.height, intensity_);
    result.operand_fallback = tint_;
    result.affine_layers.push_back(assets_.terrain);
    auto& terrain = result.affine_layers.back();
    std::copy(colors_.begin(), colors_.end(), terrain.colors.begin());
    if (tint_enabled_) terrain.mix = {
        static_cast<ColorBlend>(FF6_ROM_VALUE_CONTINENT_BLEND_MODE_0),
        static_cast<ColorBlend>(FF6_ROM_VALUE_CONTINENT_BLEND_MODE_0)};
    for (unsigned y = 0; y < result.height; ++y)
        terrain.lines[y] = make_perspective_line({FF6_ROM_VALUE_CONTINENT_VIEW_0,
            FF6_ROM_VALUE_CONTINENT_VIEW_1,
            static_cast<std::int16_t>(FF6_ROM_VALUE_CONTINENT_VIEW_2), scroll_y_}, visible_scale_,
                                                static_cast<std::uint16_t>(y + 1));
    return result;
}
} // namespace ff6
