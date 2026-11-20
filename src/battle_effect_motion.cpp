#include "ff6/battle_effects.hpp"
#include "rom-materials/battle_boomerang_lift.macros.inc"
#include "rom-materials/battle_boomerang_phase.macros.inc"
#include "rom-materials/battle_character_jump_height.macros.inc"
#include <cstdlib>
#include <stdexcept>

namespace ff6 {
int scale_battle_effect_sine8(const BattleEffectLibrary& library, std::uint8_t phase,
                              std::uint8_t radius) {
    const unsigned value = library.sine[phase];
    const auto magnitude = ((value & 128U ? value ^ 255U : value) * radius) >> 8U;
    return (value & 128U) ? -int(magnitude) : int(magnitude);
}
BattleEffectVector measure_battle_effect_vector(const BattleEffectLibrary& library, std::uint8_t x0,
                                                std::uint8_t y0, std::uint8_t x1, std::uint8_t y1) {
    const int dx = int{x1} - x0, dy = int{y1} - y0;
    const auto width = static_cast<unsigned>(std::abs(dx));
    const auto height = static_cast<unsigned>(std::abs(dy));
    const auto cell = (height / 8) * 32 + width / 8;
    return {battle_vector_angle(library.angles, {x0, y0}, {x1, y1}), static_cast<std::uint8_t>(width),
            library.distances[cell]};
}
int scale_battle_effect_sine(const BattleEffectLibrary& library, std::uint8_t phase,
                             std::uint16_t radius) {
    return battle_sine16(library.sine16, phase, radius);
}
void BattleAttackAnimation::set_vector(BattleEffectTask& task, std::uint8_t x0, std::uint8_t y0,
                                       std::uint8_t x1, std::uint8_t y1) {
    if (task.character_line)
        throw std::invalid_argument("Character-line to projectile motion remains unowned");
    const auto vector = measure_battle_effect_vector(*library_, x0, y0, x1, y1);
    task.vector_angle = vector.angle;
    task.vector_dx = vector.horizontal_distance;
    task.vector_length = vector.length;
    task.radius = 0;
}
void BattleAttackAnimation::move_vector(BattleEffectTask& task) const {
    if (task.character_line)
        throw std::invalid_argument("Character-line projection reuse remains unowned");
    task.script.offset = {wrap16(static_cast<std::uint32_t>(scale_battle_effect_sine(
                              *library_, wrap8(unsigned{task.vector_angle} + 64U), task.radius))),
                          wrap16(static_cast<std::uint32_t>(scale_battle_effect_sine(
                              *library_, task.vector_angle, task.radius)))};
}
void BattleAttackAnimation::move_boomerang(BattleEffectTask& task) const {
    task.radius = wrap16(static_cast<std::uint32_t>(
        scale_battle_effect_sine(*library_, task.motion_phase, task.vector_length)));
    move_vector(task);
    const auto phase = wrap8((unsigned{task.motion_phase} + 64U) * 2U +
                             unsigned{FF6_ROM_VALUE_BATTLE_BOOMERANG_PHASE_0});
    task.script.offset.y = wrap16(static_cast<std::uint32_t>(
        int{task.script.offset.y} + scale_battle_effect_sine(
            *library_, phase, FF6_ROM_VALUE_BATTLE_BOOMERANG_LIFT_0)));
}
bool BattleAttackAnimation::move_character_jump(BattleEffectTask& task, std::uint8_t speed,
                                                bool backwards) {
    if (task.script.attacker >= 4 || speed == 0)
        throw std::invalid_argument("Character jump needs a character and nonzero speed");
    move_vector(task);
    auto& pose = context_.characters[task.script.attacker];
    pose.offset_x = task.script.offset.x;
    pose.offset_y = task.script.offset.y;
    // CalcThreadVecPos samples before advancing. Its reciprocal is truncated
    // first, then the LOW product word supplies the phase's high byte. The
    // original divide-by-zero result is FFFF, including coincident endpoints.
    const unsigned divisor = std::min<unsigned>(task.vector_length, 255);
    const unsigned reciprocal = divisor ? 65535U / divisor : 65535U;
    const auto phase = static_cast<std::uint8_t>(
        128U + (((unsigned{wrap8(task.radius)} * reciprocal) >> 8U) & 255U) / 2U);
    pose.jump_y =
        wrap16(static_cast<std::uint32_t>(scale_battle_effect_sine(
            *library_, phase, FF6_ROM_VALUE_BATTLE_CHARACTER_JUMP_HEIGHT_0)));
    const auto return_radius = wrap16(unsigned{task.radius} + 65536U - speed);
    const bool finished =
        backwards ? (return_radius & 32768U) != 0 : task.radius >= task.vector_length;
    if (backwards || !finished)
        task.radius = backwards ? return_radius : wrap16(unsigned{task.radius} + speed);
    if (finished) {
        pose.jump_y = 0;
        if (backwards)
            pose.offset_x = pose.offset_y = 0;
        // The thread retains its last projected offset on both completion
        // paths, independently of the character's reset return position.
    } else
        task.script.offset.y = wrap16(unsigned{task.script.offset.y} + pose.jump_y);
    return finished;
}
} // namespace ff6
