#include "ff6/battle_character_motion.hpp"
#include "rom-materials/battle_vector_magitek_raise.macros.inc"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace ff6 {
BattleCharacterMotionData load_battle_character_motion(const ContentPack& pack) {
    BattleCharacterMotionData data;
    const auto sine = pack.get("data_c2fc6d_btlgfx_lookup_tables");
    const auto angles = pack.get("battle_effect_angles");
    const auto walking = pack.get("battle_character_walk_directions");
    const auto jumping = pack.get("battle_character_jump_directions");
    if (sine.size() != 768 || angles.size() != 1024 || walking.size() != 16 || jumping.size() != 16)
        throw std::invalid_argument("Invalid character motion tables");
    for (unsigned i = 0; i < 256; ++i) data.sine[i] = read16(sine, i * 2);
    std::ranges::copy(angles, data.angles.begin());
    std::ranges::copy(walking, data.walking.begin());
    std::ranges::copy(jumping, data.jumping.begin());
    return data;
}
std::uint8_t battle_vector_angle(std::span<const std::uint8_t, 1024> angles,
    BattleAnimationPoint from, BattleAnimationPoint to) {
    const int dx = int{wrap8(to.x)} - wrap8(from.x), dy = int{wrap8(to.y)} - wrap8(from.y);
    int angle = angles[unsigned(std::abs(dy)) / 8 * 32 + unsigned(std::abs(dx)) / 8];
    // Equal coordinates take the negative quadrant, including a zero vector.
    if (dx <= 0) angle = dy <= 0 ? 128 + angle : 128 - angle;
    else if (dy <= 0) angle = -angle;
    return wrap8(static_cast<std::uint32_t>(angle));
}
int battle_sine16(std::span<const std::uint16_t, 256> sine, std::uint8_t phase,
                  std::uint16_t radius) {
    const auto coefficient = sine[phase];
    const bool negative = (coefficient & 0x8000U) != 0;
    const auto magnitude = negative ? coefficient ^ 65535U : unsigned{coefficient};
    // CalcSine16 uses one's-complement magnitudes and the low radius byte.
    const auto result = static_cast<int>((magnitude * (radius & 255U) * 2U) >> 16U);
    return negative ? -result : result;
}
BattleCharacterLine make_battle_character_line(BattleAnimationPoint from,
                                               BattleAnimationPoint to, bool magitek) {
    const auto x0 = wrap8(from.x), y0 = wrap8(from.y), x1 = wrap8(to.x),
               y1 = wrap8(unsigned{to.y} - (magitek ? FF6_ROM_VALUE_BATTLE_VECTOR_MAGITEK_RAISE_0 : 0U));
    BattleCharacterLine line;
    // _c2dcc8 retains zero direction on equal endpoints. The stepping
    // helper nevertheless takes its nonnegative branch for a zero-length line.
    line.direction_x = (x1 > x0) - (x1 < x0);
    line.direction_y = (y1 > y0) - (y1 < y0);
    line.distance_x = static_cast<std::uint8_t>(std::abs(int{x1} - x0));
    line.distance_y = static_cast<std::uint8_t>(std::abs(int{y1} - y0));
    line.remaining = std::max(line.distance_x, line.distance_y);
    return line;
}
BattleLineStep advance_battle_line(BattleCharacterLine& line, BattleAnimationPoint& displacement,
    std::span<const std::uint16_t, 256> sine, std::uint8_t speed, std::uint8_t height, bool reverse) {
    if (!speed) throw std::invalid_argument("Character line motion requires nonzero speed");
    const bool horizontal = line.distance_x >= line.distance_y;
    const unsigned major = std::max(line.distance_x, line.distance_y),
                   minor = std::min(line.distance_x, line.distance_y);
    unsigned residual = line.remainder, minor_steps = 0;
    for (unsigned i = 0; i < speed && minor; ++i) {
        residual += minor;
        if (residual >= major) { residual -= major; ++minor_steps; }
    }
    line.remainder = wrap8(residual);
    const int sign = reverse ? -1 : 1;
    const auto add = [](std::uint16_t value, int delta) {
        return wrap16(static_cast<std::uint32_t>(int{value} + delta));
    };
    displacement.x = add(displacement.x, sign * (line.direction_x < 0 ? -1 : 1) * int(horizontal ? speed : minor_steps));
    displacement.y = add(displacement.y, sign * (line.direction_y < 0 ? -1 : 1) * int(horizontal ? minor_steps : speed));
    // CalcCharVecPos (linked C1/E0C9) samples remaining distance BEFORE decrementing it. Truncate the
    // reciprocal and low product word separately; substituting division changes
    // visible arc samples, especially on short and coincident vectors.
    const unsigned reciprocal = major ? 65535U / major : 65535U;
    const auto phase = static_cast<std::uint8_t>(128U +
        (((wrap8(static_cast<std::uint32_t>(line.remaining)) * reciprocal) >> 8U) & 255U) / 2U);
    const auto arc = add(0, battle_sine16(sine, phase, height));
    line.remaining -= speed;
    if (line.remaining >= 0) return {arc, false};
    if (!reverse) line.remaining = static_cast<std::int32_t>(major);
    return {0, true};
}
bool advance_battle_character_line(BattleCharacterLine& line, BattleCharacterPose& pose,
    BattleAnimationPoint& offset, std::span<const std::uint16_t, 256> sine,
    std::uint8_t speed, std::uint8_t height, bool reverse) {
    BattleAnimationPoint displacement{pose.offset_x, pose.offset_y};
    const auto result = advance_battle_line(line, displacement, sine, speed, height, reverse);
    // The character script retains the task's final x offset on return even
    // though the actor's displacement clears. Its y offset becomes zero.
    offset.x = displacement.x;
    if (result.complete && reverse) displacement = {};
    pose.offset_x = displacement.x;
    pose.offset_y = displacement.y;
    pose.jump_y = result.arc;
    offset.y = wrap16(unsigned{displacement.y} + result.arc);
    return result.complete;
}
} // namespace ff6
