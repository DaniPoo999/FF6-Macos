#include "ff6/battle_effects.hpp"
#include "rom-materials/battle_gradient_geometry.macros.inc"
#include "rom-materials/battle_gradient_blue_channels.macros.inc"
#include "rom-materials/battle_gradient_channel_offset.macros.inc"
#include "rom-materials/battle_gradient_wide_half.macros.inc"
#include <stdexcept>

namespace ff6 {
// The compact native loop requires evenly spaced copied bands. Validate every
// original destination and endpoint rather than deriving unverified data from
// the first band's spacing. These private inputs never ship in Builder.
static_assert(FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_4 ==
              2 * FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_3);
static_assert(FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_5 ==
              3 * FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_3);
static_assert(FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_2 ==
              FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_3 + 1);
static_assert(FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_1 +
                  FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_6 ==
              FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_0);
BattleEffectGradientData decode_battle_effect_gradients(ByteView bytes) {
    if (bytes.size() != 48)
        throw std::invalid_argument("Invalid battle gradient tables");
    BattleEffectGradientData data;
    for (unsigned i = 0; i < 32; ++i) {
        if (bytes[i] > 31)
            throw std::invalid_argument("Gradient ramp exceeds five-bit intensity");
        data.ramp[i] = bytes[i];
    }
    for (unsigned i = 0; i < 16; ++i) {
        if ((bytes[32 + i] & 31U) || !bytes[32 + i])
            throw std::invalid_argument("Invalid gradient component selection");
        data.channels[i] = bytes[32 + i] >> 5U;
    }
    return data;
}
void BattleEffectGradient::update(const BattleEffectGradientData& data, std::uint8_t command,
                                  std::uint8_t parameter) {
    if (command != 0x33 && command != 0x39 && command != 0x4b && command != 0x5f)
        throw std::invalid_argument("Invalid battle gradient operation");
    const bool blue = command == 0x39 || command == 0x5f;
    published = true;
    if (blue)
        intensity = wrap8(unsigned{intensity} + parameter);
    const auto amount = blue ? intensity : parameter;
    unsigned color = intensity >> 5U;
    unsigned position = phase & 31U;
    Rgb555 retained = 0;
    rows[0] = 0;
    colors[0] = {7, 0};
    for (unsigned y = 1; y < rows.size(); ++y) {
        const unsigned ramp =
            command == 0x5f
                ? std::min(y - 1, unsigned{FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_0} - y) / 4U
                : data.ramp[position];
        // The original subtracts as a byte and tests its sign bit. Preserve
        // that wrap, including component-selection spill for unusual inputs.
        auto value = wrap8(ramp - amount);
        if (value & 128U)
            value = 0;
        const auto channels = blue ? unsigned{FF6_ROM_VALUE_BATTLE_GRADIENT_BLUE_CHANNELS_0}
                                   : data.channels[color + (command == 0x4b
                                         ? FF6_ROM_VALUE_BATTLE_GRADIENT_CHANNEL_OFFSET_0 : 0)];
        const auto selected = channels | (value >> 5U);
        colors[y] = {static_cast<std::uint8_t>(selected), static_cast<std::uint8_t>(value & 31U)};
        for (unsigned component = 0; component < 3; ++component)
            if (selected & (1U << component)) {
                const unsigned shift = component * 5U;
                retained =
                    static_cast<Rgb555>((retained & ~(31U << shift)) | ((value & 31U) << shift));
            }
        rows[y] = retained;
        position = (position + 1U) & 31U;
        if (!position)
            color = (color + 1U) & 7U;
    }
    phase = wrap8(unsigned{phase} + 1U);
    if (!blue)
        intensity = wrap8(unsigned{intensity} + 1U);
    writes.clear();
    const auto write = [&](unsigned y) {
        if (y < colors.size())
            writes.push_back({static_cast<std::uint16_t>(y), colors[y]});
    };
    write(0);
    if (command == 0x39) {
        // The original writes four bands together, then copies a partial band.
        for (unsigned y = 1; y <= FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_2 - 1; ++y)
            for (unsigned group = 0; group < 4; ++group)
                write(y + group * FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_3);
        for (unsigned y = FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_1 + 1U;
             y < colors.size(); ++y)
            write(y);
    } else if (command == 0x5f) {
        for (unsigned y = 1; y <= FF6_ROM_VALUE_BATTLE_GRADIENT_WIDE_HALF_0; ++y) {
            write(y);
            // The first mirrored row is outside the battlefield; the central
            // row is visited twice. Preserve the original writer ordering.
            write(FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_0 + 1 - y);
        }
    } else {
        for (unsigned y = 1; y < colors.size(); ++y)
            write(y);
    }
}
void BattleEffectGradient::clear() {
    rows.fill(0);
    colors.fill({7, 0});
    writes.clear();
    for (unsigned y = 0; y < rows.size(); ++y)
        writes.push_back({static_cast<std::uint16_t>(y), colors[y]});
}
} // namespace ff6
