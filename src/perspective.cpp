#include "ff6/perspective.hpp"

namespace ff6 {
namespace {
std::int32_t floor_multiple_64(std::int32_t value) {
    const auto remainder = value % 64;
    return value - (remainder < 0 ? remainder + 64 : remainder);
}
std::int32_t projection_offset(std::int32_t difference) {
    // Retain ten low bits and the sign of the thirteen-bit coordinate
    // difference. In particular, -1024 stays -1024, while +1024 becomes zero.
    const auto remainder = difference % 1024;
    const auto low = remainder < 0 ? remainder + 1024 : remainder;
    return difference < 0 ? low - 1024 : low;
}
} // namespace

PerspectiveScales make_perspective_scales(PerspectiveDepth depth) {
    if (depth.rows == 0 || depth.rows > 224)
        throw std::invalid_argument("Perspective depth ramp needs 1..224 rows");
    const bool descending = depth.far_depth < depth.near_depth;
    const auto distance =
        descending ? depth.near_depth - depth.far_depth : depth.far_depth - depth.near_depth;
    const auto magnitude = static_cast<unsigned>(distance) / depth.rows;
    PerspectiveScales result;
    result.depth_step = wrap16(descending ? 65536U - magnitude : magnitude);
    result.next_depth = depth.near_depth;
    result.rows.reserve(depth.rows);
    for (unsigned row = 0; row < depth.rows; ++row) {
        const unsigned divisor = result.next_depth >> 8U;
        // The original integer divider returns 0xffff for a zero divisor.
        // Its signed scale interpretation is -1, not infinity or a clamp to 0.
        const unsigned quotient = divisor == 0 ? 65535U : depth.numerator / divisor;
        const auto signed_scale =
            quotient < 32768U ? static_cast<int>(quotient) : static_cast<int>(quotient) - 65536;
        result.rows.push_back(static_cast<std::int16_t>(signed_scale));
        result.next_depth = wrap16(unsigned{result.next_depth} + result.depth_step);
    }
    return result;
}

AffineLine make_perspective_line(const PerspectiveView& view, std::int16_t scale,
                                 std::uint16_t sample_y) {
    return make_affine_perspective_line(view, scale, 0, 0, scale, sample_y);
}

AffineLine make_affine_perspective_line(const PerspectiveView& view,
    std::int16_t xx, std::int16_t xy, std::int16_t yx, std::int16_t yy,
    std::uint16_t sample_y) {
    for (const auto coordinate : {view.center_x, view.center_y, view.scroll_x, view.scroll_y})
        if (coordinate < -4096 || coordinate > 4095)
            throw std::invalid_argument("Perspective view coordinate exceeds thirteen bits");
    if (sample_y > 255)
        throw std::invalid_argument("Perspective view sample row exceeds 255");
    const auto dx = projection_offset(std::int32_t{view.scroll_x} - view.center_x);
    const auto dy = projection_offset(std::int32_t{view.scroll_y} - view.center_y);
    AffineLine result;
    // Quantize each product separately, before addition. Multiplication avoids
    // shifting negative coordinates, and these bounded products fit int32.
    result.source_x = floor_multiple_64(xx * dx) + floor_multiple_64(xy * dy) +
                      floor_multiple_64(std::int32_t{xy} * sample_y) +
                      std::int32_t{view.center_x} * 256;
    result.source_y = floor_multiple_64(yx * dx) + floor_multiple_64(yy * dy) +
                      floor_multiple_64(std::int32_t{yy} * sample_y) +
                      std::int32_t{view.center_y} * 256;
    result.step_x = xx;
    result.step_y = yx;
    return result;
}
} // namespace ff6
