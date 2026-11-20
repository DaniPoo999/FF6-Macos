#pragma once
#include "ff6/battle_character_view.hpp"

namespace ff6 {
struct BattleAnimationPoint {
    std::uint16_t x = 0, y = 0;
    bool operator==(const BattleAnimationPoint&) const = default;
};
// Integer line interpolation shared by entrances and attack scripts. The arc
// is sampled independently of the horizontal/vertical step accumulator.
struct BattleCharacterLine {
    int direction_x = 0, direction_y = 0;
    std::uint8_t distance_x = 0, distance_y = 0, remainder = 0;
    std::int32_t remaining = 0;
    bool operator==(const BattleCharacterLine&) const = default;
};
struct BattleCharacterMotionData {
    std::array<std::uint16_t, 256> sine{};
    std::array<std::uint8_t, 1024> angles{};
    std::array<std::uint8_t, 16> walking{}, jumping{};
};
BattleCharacterMotionData load_battle_character_motion(const ContentPack& pack);
std::uint8_t battle_vector_angle(std::span<const std::uint8_t, 1024> angles,
    BattleAnimationPoint from, BattleAnimationPoint to);
int battle_sine16(std::span<const std::uint16_t, 256> sine, std::uint8_t phase,
                  std::uint16_t radius);
BattleCharacterLine make_battle_character_line(BattleAnimationPoint from,
                                               BattleAnimationPoint to, bool magitek);
// Returns true after the final step. Like the original, a step at zero remaining
// distance is still published; completion is the subsequent underflow.
struct BattleLineStep { std::uint16_t arc = 0; bool complete = false; };
// Shared DDA and arc sampling used by character and monster movement scripts.
BattleLineStep advance_battle_line(BattleCharacterLine& line, BattleAnimationPoint& displacement,
    std::span<const std::uint16_t, 256> sine, std::uint8_t speed, std::uint8_t height, bool reverse);
bool advance_battle_character_line(BattleCharacterLine& line, BattleCharacterPose& pose,
    BattleAnimationPoint& offset, std::span<const std::uint16_t, 256> sine,
    std::uint8_t speed, std::uint8_t height, bool reverse);
} // namespace ff6
