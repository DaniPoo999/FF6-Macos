#include "ff6/field_display.hpp"
#include "rom-materials/field_pyramid_phase_step.macros.inc"
#include "rom-materials/field_pyramid_phase_mask.macros.inc"
#include "rom-materials/field_pyramid_height.macros.inc"
#include "rom-materials/field_pyramid_side_angle.macros.inc"
#include "rom-materials/field_pyramid_vertical_radius.macros.inc"
#include "rom-materials/field_pyramid_horizontal_radius.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
int edge_step(FieldWindowPoint from, FieldWindowPoint to) {
    const int height = int{to[1]} - from[1];
    if (height <= 0)
        throw std::invalid_argument("Triangle edge must descend");
    // CalcWindowPos divides the unsigned magnitude then restores the sign.
    // Signed C++ division likewise truncates toward zero, without a shift of
    // a negative value or an overflowing narrow intermediate.
    return (int{to[0]} - from[0]) * 256 / height;
}
} // namespace
FieldWindowSpans field_triangle_spans(std::array<FieldWindowPoint, 3> points) {
    std::ranges::sort(points, [](auto a, auto b) {
        return a[1] < b[1] || (a[1] == b[1] && a[0] < b[0]);
    });
    const auto top = points[0], middle = points[1], bottom = points[2];
    FieldWindowSpans spans{};
    if (top[1] == bottom[1])
        return spans;
    const bool flat_top = top[1] == middle[1];
    const int long_step = edge_step(top, bottom);
    int short_step = flat_top ? edge_step(middle, bottom) : edge_step(top, middle);
    // With a flat top the short edge starts at its rightmost vertex. Otherwise
    // its slope relative to the long edge determines the side of the triangle.
    const bool short_left = !flat_top && short_step < long_step;
    std::uint16_t long_x = static_cast<std::uint16_t>(unsigned{top[0]} * 256U);
    std::uint16_t short_x = static_cast<std::uint16_t>(unsigned{flat_top ? middle[0] : top[0]} * 256U);
    for (unsigned row = top[1]; row < bottom[1] && row < spans.size(); ++row) {
        if (!flat_top && row == middle[1])
            short_step = edge_step(middle, bottom);
        // UpdatePyramid advances before publishing each row. At the middle
        // vertex it retains fractional error instead of snapping to the vertex.
        long_x = wrap16(static_cast<std::uint32_t>(int{long_x} + long_step + 65536));
        short_x = wrap16(static_cast<std::uint32_t>(int{short_x} + short_step + 65536));
        const unsigned left = (short_left ? short_x : long_x) >> 8U;
        const unsigned right = (short_left ? long_x : short_x) >> 8U;
        if (left <= right)
            spans[row] = {static_cast<std::uint16_t>(left), static_cast<std::uint16_t>(right + 1U)};
    }
    return spans;
}
std::array<std::uint8_t, 256> load_field_window_sine(const ContentPack& pack) {
    const auto tables = pack.get("data_c2fc6d_btlgfx_lookup_tables");
    if (tables.size() != 768)
        throw std::invalid_argument("Invalid field window sine resource");
    std::array<std::uint8_t, 256> result;
    std::copy_n(tables.begin() + 512, result.size(), result.begin());
    return result;
}
void FieldPyramid::advance(std::uint8_t center_x, std::uint8_t center_y,
                           const std::array<std::uint8_t, 256>& sine) {
    if (!object)
        return;
    phase = wrap16(unsigned{phase} + FF6_ROM_VALUE_FIELD_PYRAMID_PHASE_STEP_0);
    const auto angle = wrap8((phase & FF6_ROM_VALUE_FIELD_PYRAMID_PHASE_MASK_0) + ((phase >> 8U) * 2U));
    const auto scaled = [&](unsigned direction, unsigned radius) {
        // CalcSine/CalcCosine bias the signed coefficient before multiplying;
        // subtracting half the radius afterwards preserves downward rounding.
        const unsigned biased = wrap8(unsigned{sine[wrap8(direction)]} + 128U);
        return static_cast<int>(radius * biased / 256U) - static_cast<int>(radius / 2U);
    };
    const auto base = [&](unsigned direction) -> FieldWindowPoint {
        return {wrap8(static_cast<unsigned>(int{center_x} + scaled(direction + 64U, FF6_ROM_VALUE_FIELD_PYRAMID_HORIZONTAL_RADIUS_0) + 256)),
                wrap8(static_cast<unsigned>(int{center_y} + scaled(direction, FF6_ROM_VALUE_FIELD_PYRAMID_VERTICAL_RADIUS_0) + 256))};
    };
    spans = field_triangle_spans({FieldWindowPoint{center_x, wrap8(unsigned{center_y} + (256U - FF6_ROM_VALUE_FIELD_PYRAMID_HEIGHT_0))},
                                  base(unsigned{angle} + (256U - FF6_ROM_VALUE_FIELD_PYRAMID_SIDE_ANGLE_0)),
                                  base(unsigned{angle} + FF6_ROM_VALUE_FIELD_PYRAMID_SIDE_ANGLE_0)});
}
} // namespace ff6
