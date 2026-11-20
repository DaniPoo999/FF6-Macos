#pragma once
#include "ff6/raster.hpp"
#include "rom-materials/opening_perspective_numerator.macros.inc"
#include "rom-materials/opening_perspective_far_depth.macros.inc"

namespace ff6 {
struct PerspectiveDepth {
    std::uint16_t rows = 224;
    std::uint16_t numerator = FF6_ROM_VALUE_OPENING_PERSPECTIVE_NUMERATOR_0;
    std::uint16_t near_depth = 0, far_depth = FF6_ROM_VALUE_OPENING_PERSPECTIVE_FAR_DEPTH_0;
};
struct PerspectiveScales {
    std::vector<std::int16_t> rows;               // Signed scale with eight fractional bits.
    std::uint16_t depth_step = 0, next_depth = 0; // Explicit modulo-65536 values.
};
// Source-backed unrotated depth ramp, used by the opening landscape. Rows must
// be 1..224. Other world-map producers/rotation are not implemented by this API.
PerspectiveScales make_perspective_scales(PerspectiveDepth depth);

struct PerspectiveView {
    std::int16_t center_x = 0, center_y = 0;
    std::int16_t scroll_x = 0, scroll_y = 0;
};
// Produce one semantic sampling row. Coordinates are signed thirteen-bit
// values; sample_y is the original view's vertical coordinate (first visible
// line is 1). Clipping and palette/compositing belong to the scene producer.
// See docs/PERSPECTIVE.md for the offset and product precision rules.
AffineLine make_perspective_line(const PerspectiveView& view, std::int16_t scale,
                                 std::uint16_t sample_y);
// Rotation is resolved by the scene producer. These are ordinary signed
// 8-fraction-bit matrix coefficients, not a device-register interface.
AffineLine make_affine_perspective_line(const PerspectiveView& view,
    std::int16_t xx, std::int16_t xy, std::int16_t yx, std::int16_t yy,
    std::uint16_t sample_y);
} // namespace ff6
