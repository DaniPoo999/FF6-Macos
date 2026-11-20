#include "ff6/battle_effect_raster.hpp"
#include <stdexcept>

namespace ff6 {
namespace {
void validate(unsigned row, BattleRasterDeadline deadline) {
    if (row >= battle_effect_rows || deadline.first_unread_row > battle_effect_rows)
        throw std::invalid_argument("Battle raster row or deadline exceeds the battlefield");
}
} // namespace
void BattleEffectRaster::reset(const Colors& colors, const Mask& mask) {
    for (const auto color : colors)
        if (color.components > 7 || color.value > 31)
            throw std::invalid_argument("Invalid battle gradient component update");
    for (const auto span : mask)
        if (span.left > 256 || span.right > 256)
            throw std::invalid_argument("Battle mask edge exceeds the picture");
    retained_colors_ = colors;
    retained_mask_ = mask;
    begin_picture();
}
void BattleEffectRaster::begin_picture() {
    sampled_colors_ = retained_colors_;
    sampled_mask_ = retained_mask_;
    last_deadline_.fill(0);
}
void BattleEffectRaster::write_gradient(BattleGradientWrite write, BattleRasterDeadline deadline) {
    validate(write.row, deadline);
    if (write.color.components > 7 || write.color.value > 31)
        throw std::invalid_argument("Invalid battle gradient component update");
    if (deadline.first_unread_row < last_deadline_[0])
        throw std::invalid_argument("Gradient deadlines cannot move backwards within a picture");
    last_deadline_[0] = deadline.first_unread_row;
    retained_colors_[write.row] = write.color;
    if (write.row >= deadline.first_unread_row)
        sampled_colors_[write.row] = write.color;
}
void BattleEffectRaster::write_mask_edge(unsigned row, bool right, std::uint16_t edge,
                                         BattleRasterDeadline deadline) {
    validate(row, deadline);
    if (edge > 256)
        throw std::invalid_argument("Battle mask edge exceeds the picture");
    auto& previous = last_deadline_[right ? 2 : 1];
    if (deadline.first_unread_row < previous)
        throw std::invalid_argument("Mask deadlines cannot move backwards within a picture");
    previous = deadline.first_unread_row;
    (right ? retained_mask_[row].right : retained_mask_[row].left) = edge;
    if (row >= deadline.first_unread_row)
        (right ? sampled_mask_[row].right : sampled_mask_[row].left) = edge;
}
std::array<Rgb555, battle_effect_rows> BattleEffectRaster::colors() const {
    std::array<Rgb555, battle_effect_rows> result{};
    Rgb555 retained = 0;
    for (unsigned y = 0; y < result.size(); ++y) {
        const auto update = sampled_colors_[y];
        for (unsigned component = 0; component < 3; ++component)
            if (update.components & (1U << component)) {
                const unsigned shift = component * 5U;
                retained = static_cast<Rgb555>((retained & ~(31U << shift)) |
                                               (unsigned{update.value} << shift));
            }
        result[y] = retained;
    }
    return result;
}
} // namespace ff6
