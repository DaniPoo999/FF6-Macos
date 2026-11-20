#include "ff6/battle_effects.hpp"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
const std::array<Rgb555, 16>& BattleAttackAnimation::effect_colors(BattleEffectLayer layer,
                                                                 bool base) const {
    const auto owner = static_cast<unsigned>(layer);
    if (owner >= 3)
        throw std::invalid_argument("Effect colors require a sprite, foreground or overlay plane");
    if (const auto& image = images_[owner])
        return base ? image->base_colors : image->colors;
    return base ? imageless_base_colors_[owner] : imageless_colors_[owner];
}
void BattleEffectColorOffset::update(std::uint8_t parameter, bool relative, bool subtraction) {
    unsigned value = parameter & 31U;
    if (relative) {
        // The original takes ONE previous component (red, then green, then
        // blue), adjusts it, and assigns that value to every selected channel.
        // This differs from independently incrementing all selected channels.
        if (!(parameter & 224U))
            throw std::invalid_argument("Relative palette offset needs a selected color channel");
        const auto first = parameter & 128U ? 0U : parameter & 64U ? 1U : 2U;
        const int delta = int(parameter & 15U) * ((parameter & 16U) ? -1 : 1);
        value = static_cast<unsigned>(std::clamp(int(channels[first]) + delta, 0, 31));
    } else
        channels = {};
    for (unsigned i = 0; i < 3; ++i)
        if (parameter & (128U >> i))
            channels[i] = static_cast<std::uint8_t>(value);
    subtract = subtraction;
}
Rgb555 BattleEffectColorOffset::apply(Rgb555 color) const {
    unsigned out = 0;
    for (unsigned i = 0; i < 3; ++i) {
        const int base = (color >> (i * 5U)) & 31U;
        const int value = std::clamp(base + int(channels[i]) * (subtract ? -1 : 1), 0, 31);
        out |= static_cast<unsigned>(value) << (i * 5U);
    }
    return static_cast<Rgb555>(out);
}
bool BattleAttackAnimation::palette_command(std::uint8_t command, std::uint8_t parameter) {
    unsigned owner = 0;
    bool relative = false, subtract = false;
    switch (command) {
    case 0xb1:
        subtract = true;
        break;
    case 0xb2:
        break;
    case 0xb7:
        relative = true;
        break;
    case 0xb8:
        relative = subtract = true;
        break;
    case 0xcc:
        owner = 1;
        subtract = true;
        break;
    case 0xcd:
        owner = 1;
        break;
    // Follow the code's IncPal/DecPal destinations, not the reversed CE comment.
    case 0xce:
        owner = 1;
        relative = true;
        break;
    case 0xcf:
        owner = 1;
        relative = subtract = true;
        break;
    case 0xaa:
        owner = 2;
        subtract = true;
        break;
    case 0xab:
        owner = 2;
        break;
    case 0xb3:
        owner = 2;
        relative = true;
        break;
    case 0xb4:
        owner = 2;
        relative = subtract = true;
        break;
    case 0xaf:
        owner = 3;
        subtract = true;
        break;
    case 0xb0:
        owner = 3;
        break;
    case 0xb5:
        owner = 3;
        relative = true;
        break;
    case 0xb6:
        owner = 3;
        relative = subtract = true;
        break;
    case 0xb9:
    case 0xba:
    case 0xbb:
    case 0xbc:
        owner = 5;
        relative = command == 0xbb || command == 0xbc;
        subtract = command == 0xb9 || command == 0xbc;
        break;
    case 0xfb:
    case 0xfc:
    case 0xfd:
    case 0xfe:
        owner = 4;
        relative = command == 0xfd || command == 0xfe;
        subtract = command == 0xfb || command == 0xfe;
        break;
    default:
        return false;
    }
    auto& offset = color_offsets_[owner];
    // IncPal/DecPal both visit every selected color, even when its resulting
    // components stay unchanged. Keep this preceding work for a later mask,
    // gradient or wave; do not turn a palette command into a fitted delay.
    // C1/EB53/EB71 + FCAC/FD13, shortest per-component clamp paths only.
    // BG1 also updates eight colors in the following background bank. Those
    // writes still cost work even when no current image uses that second bank.
    constexpr std::array<unsigned, 6> color_counts{16, 24, 4, 48, 64, 48};
    const unsigned colors = owner == 4 && subtract ? 48U : color_counts[owner];
    const unsigned batches = owner == 1 ? 2U : owner == 4 ? 4U : 1U;
    accrue_update_work(colors * 142U - batches, colors * 34U);
    offset.update(parameter, relative, subtract);
    if (owner == 1 && copied_background_base_)
        for (unsigned i = 0; i < 16; ++i)
            (*copied_background_colors_)[i] = offset.apply((*copied_background_base_)[i]);
    if (owner < 3) {
        auto& colors = images_[owner] ? images_[owner]->colors : imageless_colors_[owner];
        const auto& base = effect_colors(static_cast<BattleEffectLayer>(owner), true);
        for (unsigned i = 0; i < (owner == 2 ? 4U : 16U); ++i)
            colors[i] = offset.apply(base[i]);
    }
    if (owner == 1)
        for (unsigned i = 0; i < 8; ++i)
            secondary_colors_[i] = offset.apply(secondary_base_colors_[i]);
    if (owner == 3)
        for (unsigned bank = 0; bank < base_battlefield_colors_.size(); ++bank)
            for (unsigned i = 0; i < 16; ++i)
                context_.battlefield_colors[bank][i] = offset.apply(base_battlefield_colors_[bank][i]);
    if (owner == 4) {
        // FB/FC/FD/FE operate on all four character banks, regardless of actor
        // visibility. DecPal writes only twelve colors; IncPal writes sixteen.
        // Both read CopyPal's animation-start snapshot, not the previous fade.
        for (unsigned bank = 0; bank < 4; ++bank)
            for (unsigned i = 0; i < (subtract ? 12U : 16U); ++i)
                context_.character_colors[bank][i] = offset.apply(base_character_colors_[bank][i]);
    }
    if (owner == 5) {
        // B9..BC always write all 48 colors from CopyPal's snapshot. Palette
        // aliases follow each monster's selected bank, independently of targets
        // and of the temporary effect-palette substitution (bank 11).
        for (unsigned bank = 0; bank < 3; ++bank)
            for (unsigned i = 0; i < 16; ++i)
                context_.monster_colors[bank][i] = offset.apply(base_monster_colors_[bank][i]);
    }
    return true;
}
BattleActorPaletteOverrides BattleAttackAnimation::actor_palettes(
    const BattleAttackAnimation* published_selection) const {
    BattleActorPaletteOverrides out;
    const auto& selection = published_selection ? *published_selection : *this;
    if (!selection.complete_ && selection.monster_palette_shift_)
        for (unsigned i = 0; i < 6; ++i) {
            const auto bank = (selection.context_.monster_styles[i].palette + selection.monster_palette_shift_) & 7U;
            if (bank < 4)
                throw std::invalid_argument("Shifted monster palette has no character bank owner");
            out[i + 4] = context_.character_colors[bank - 4];
        }
    if (!selection.complete_ && images_[0])
        for (unsigned i = 0; i < 10; ++i)
            if (selection.recolored_targets_[i])
                out[i] = images_[0]->colors;
    return out;
}
} // namespace ff6
