#include "ff6/battle_fade.hpp"
#include "rom-materials/battle_entry_fade_arithmetic.macros.inc"
#include <algorithm>
#include <stdexcept>
namespace ff6 {
namespace {
void validate(const BattleEntryFade& fade) {
    if (fade.brightness > 15 || fade.remaining > FF6_ROM_VALUE_BATTLE_ENTRY_FADE_ORIGIN_0 || (fade.remaining & 1U) != 0 ||
        std::ranges::any_of(fade.subtraction, [](auto c) { return c > 31; }))
        throw std::invalid_argument("Invalid battle entry fade state");
}
} // namespace
void advance_battle_entry_fade(BattleEntryFade& fade) {
    validate(fade);
    if (fade.complete())
        return;
    fade.brightness = static_cast<std::uint8_t>(std::min(15U, unsigned{fade.brightness} + 1U));
    const auto upper = unsigned{fade.remaining}, lower = 152U - upper;
    for (unsigned i = 0; i < upper / 2; ++i) {
        const auto amount = static_cast<std::uint8_t>(std::min(31U, i));
        fade.subtraction[upper - 1 - i * 2] = fade.subtraction[upper - i * 2] = amount;
        fade.subtraction[lower + i * 2] = fade.subtraction[lower + 1 + i * 2] = amount;
    }
    fade.remaining = static_cast<std::uint8_t>(upper - 2);
}
void apply_battle_entry_fade(const BattleEntryFade& fade, RasterScene& scene) {
    validate(fade);
    if (scene.height < 152 || !scene.affine_layers.empty() || !scene.blend_rows.empty())
        throw std::invalid_argument("Unsupported battle fade composition");
    for (const auto& layer : scene.layers)
        if (layer.lines.size() < 151)
            throw std::invalid_argument("Incomplete battle layer lines");
    std::vector<Rgb555> operands(scene.height);
    for (unsigned y = 0; y < 151; ++y)
        operands[y] = static_cast<Rgb555>(unsigned{fade.subtraction[y]} * 1057U);
    std::vector<std::uint8_t> intensity(scene.height, fade.brightness);
    for (auto& layer : scene.layers)
        for (unsigned y = 0; y < 151; ++y) {
            layer.lines[y].blend = static_cast<ColorBlend>(FF6_ROM_VALUE_BATTLE_ENTRY_FADE_ARITHMETIC_0);
            layer.lines[y].operand = operands[y];
        }
    scene.operand_fallback_rows = std::move(operands);
    scene.intensity = std::move(intensity);
}
} // namespace ff6
