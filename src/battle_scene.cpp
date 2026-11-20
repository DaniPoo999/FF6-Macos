#include "ff6/battle_scene.hpp"
#include "rom-materials/battlefield_wave_banks.macros.inc"
#include "rom-materials/battlefield_wave_phase_terms.macros.inc"
#include "ff6/battle_character_motion.hpp"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
void initialize_battle_background_motion(const ContentPack& pack, BattleBackground& background) {
    if (!background.definition.wave) return;
    const auto data = pack.get("data_c2fc6d_btlgfx_lookup_tables");
    if (data.size() != 768) throw std::invalid_argument("Invalid battle wave arithmetic table");
    std::array<std::uint16_t, 256> sine;
    for (unsigned i = 0; i < sine.size(); ++i) sine[i] = read16(data, i * 2);
    // InitSineBuf banks contain word samples and advance amplitude by two.
    // Selected bank offsets and the initial/step phases come from the ROM.
    const std::uint16_t amplitude = background.identity == 46
        ? FF6_ROM_VALUE_BATTLEFIELD_WAVE_BANKS_0 / 32U : FF6_ROM_VALUE_BATTLEFIELD_WAVE_BANKS_1 / 32U;
    for (unsigned i = 0; i < background.wave_samples.size(); ++i)
        background.wave_samples[i] = static_cast<std::int16_t>(
            battle_sine16(sine, wrap8(FF6_ROM_VALUE_BATTLEFIELD_WAVE_PHASE_TERMS_0 / 2U +
                                    i * (FF6_ROM_VALUE_BATTLEFIELD_WAVE_PHASE_TERMS_1 / 2U)), amplitude) - 2);
}
RasterScene compose_battle_scene(const BattleBackground& background,
                                 const BattleReadoutAssets& readout_assets,
                                 const BattleReadoutView& readout,
                                 std::span<const RasterSprite> pieces,
                                 BattleBackgroundView viewport, std::uint16_t clock) {
    if (viewport.width != 256 || viewport.height != (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1))
        throw std::invalid_argument("Closed battle scene requires the battlefield viewport");
    RasterScene scene;
    scene.intensity.assign(224, 15);
    auto layer = make_battle_background_plane(background, viewport);
    layer.lines.resize(224);
    for (unsigned y = viewport.height; y < 224; ++y)
        layer.lines[y].enabled = false;
    // InitBattleGfx supplies inclusive endpoints; native clips use an exclusive end.
    for (unsigned y = 0; y < viewport.height; ++y) {
        if (background.definition.wave) {
            // The authored heat/dream waves repeat every 32 rows and advance
            // once per four presentation frames. They replace base scroll.
            const auto displacement = background.wave_samples[(clock / 4U + y) & 31U];
            layer.lines[y].offset_y = displacement + 1;
            if (background.identity != 46 && background.identity != 55)
                layer.lines[y].offset_x = displacement;
        }
        // InitBattleGfx/HDMA7Tbl leave the first four display rows disabled.
        layer.lines[y].enabled = y >= FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0;
        layer.lines[y].left = FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_0;
        layer.lines[y].right = FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_1 + 1;
    }
    scene.layers.push_back(std::move(layer));
    auto menu = make_battle_readout_layers(readout_assets, readout);
    scene.layers.push_back(std::move(menu.windows));
    scene.layers.push_back(std::move(menu.text));
    scene.objects.emplace();
    scene.objects->sprites.assign(pieces.begin(), pieces.end());
    scene.objects->lines.resize(224);
    for (unsigned y = 0; y < 224; ++y) {
        auto& line = scene.objects->lines[y];
        line.enabled = y >= FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 && y < (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1);
        line.left = FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_0;
        line.right = FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_1 + 1;
    }
    return scene;
}
} // namespace ff6
