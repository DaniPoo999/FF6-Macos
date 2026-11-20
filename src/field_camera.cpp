#include "ff6/field_camera.hpp"
#include "rom-materials/field_camera_minimum_x.macros.inc"
#include "rom-materials/field_camera_shared_edge.macros.inc"

namespace ff6 {
FieldScrollLimits field_scroll_limits(const FieldMapDefinition& definition) {
    if (definition.scroll_bounds[0] == 0 && definition.scroll_bounds[1] == 0)
        return {};
    // CalcScrollRange's viewport choices come from authenticated ROM operands.
    constexpr auto minimum_x = FF6_ROM_VALUE_FIELD_CAMERA_MINIMUM_X_0;
    constexpr auto edge = FF6_ROM_VALUE_FIELD_CAMERA_SHARED_EDGE_0;
    return {FieldScrollLimit{minimum_x, wrap8(unsigned{definition.scroll_bounds[0]} + (256U - edge))},
            FieldScrollLimit{edge, wrap8(unsigned{definition.scroll_bounds[1]} + (256U - edge))}};
}
FieldScroll initialize_field_scroll(const FieldMapDefinition& definition,
                                    const FieldParallax& parallax, std::uint8_t x, std::uint8_t y) {
    const auto initial = initialize_field_camera(definition, parallax, x, y);
    FieldScroll scroll;
    scroll.layers[0][0].center = initial.center_x;
    scroll.layers[0][1].center = initial.center_y;
    for (std::size_t layer = 0; layer < 3; ++layer) {
        const auto& origin = initial.scroll[layer];
        scroll.layers[layer][0].position =
            std::uint32_t{wrap16(static_cast<std::uint32_t>(origin.x))} * 256U;
        scroll.layers[layer][1].position =
            std::uint32_t{wrap16(static_cast<std::uint32_t>(origin.y))} * 256U;
        if (layer == 0)
            continue;
        for (std::size_t axis = 0; axis < 2; ++axis) {
            const auto index = (layer - 1) * 2 + axis;
            const unsigned center = axis == 0 ? initial.center_x : initial.center_y;
            const unsigned extent =
                axis == 0 ? definition.layouts[layer].width : definition.layouts[layer].height;
            const auto position = wrap16(unsigned{definition.offsets[index]} * 16U +
                                         center * parallax.multiplier[index]);
            scroll.layers[layer][axis].center =
                static_cast<std::uint8_t>((position / 16U) & (extent - 1U));
            scroll.layers[layer][axis].background_rate = parallax.autonomous_speed[index];
        }
    }
    return scroll;
}
void update_field_scroll_rate(FieldScroll& scroll, const FieldScrollLimits& limits,
                              const std::array<std::uint8_t, 4>& multipliers,
                              const FieldScrollFocus& focus) {
    if (focus.locked && !focus.caller_is_camera)
        return;
    for (std::size_t axis = 0; axis < 2; ++axis) {
        auto velocity = focus.locked ? focus.camera_speed[axis] : focus.party_speed[axis];
        if (!focus.locked && limits[axis].maximum != 255) {
            const auto tile = wrap8(unsigned{focus.party_tile[axis]} + (velocity < 0 ? 255U : 0U));
            if (tile < limits[axis].minimum || tile >= limits[axis].maximum)
                velocity = 0;
        }
        scroll.layers[0][axis].follow_rate = velocity;
        // Original multiplication uses only the low byte after shifting. Do not
        // multiply first or use arithmetic right shift for negative values.
        const unsigned magnitude =
            static_cast<unsigned>(velocity < 0 ? -int{velocity} : int{velocity});
        const unsigned truncated = (magnitude / 16U) & 255U;
        for (std::size_t layer = 1; layer < 3; ++layer) {
            const int product = static_cast<int>(truncated * multipliers[(layer - 1) * 2 + axis]);
            const auto bits = wrap16(static_cast<std::uint32_t>(velocity < 0 ? -product : product));
            scroll.layers[layer][axis].follow_rate = static_cast<std::int16_t>(signed16(bits));
        }
    }
}
std::array<std::array<bool, 2>, 3> advance_field_scroll(FieldScroll& scroll) {
    std::array<std::array<bool, 2>, 3> crossings{};
    for (std::size_t layer = 0; layer < 3; ++layer)
        for (std::size_t axis = 0; axis < 2; ++axis) {
            auto& value = scroll.layers[layer][axis];
            const auto sum =
                wrap16(static_cast<std::uint32_t>(int{value.follow_rate} + value.background_rate));
            const auto step = signed16(sum);
            const auto phase = value.position & 4095U;
            const bool crossed = step >= 0 ? phase + static_cast<unsigned>(step) >= 4096U
                                           : phase < static_cast<unsigned>(-step);
            if (crossed)
                value.center = wrap8(unsigned{value.center} + (step < 0 ? 255U : 1U));
            value.position += static_cast<std::uint32_t>(step);
            crossings[layer][axis] = crossed;
        }
    return crossings;
}
std::array<FieldOrigin, 3> field_scroll_origins(const FieldScroll& scroll) {
    std::array<FieldOrigin, 3> origins;
    for (std::size_t layer = 0; layer < 3; ++layer)
        origins[layer] = {
            static_cast<std::int32_t>((scroll.layers[layer][0].position >> 8U) & 65535U),
            static_cast<std::int32_t>((scroll.layers[layer][1].position >> 8U) & 65535U)};
    return origins;
}
} // namespace ff6
