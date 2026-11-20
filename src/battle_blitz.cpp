#include "ff6/battle_blitz.hpp"
#include "rom-materials/battle_blitz_chord_deadline.macros.inc"
#include <stdexcept>

namespace ff6 {
BattleBlitzCodes load_battle_blitz_codes(const ContentPack& pack) {
    const auto data = pack.get("data_c47a40_blitz_code");
    if (data.size() != 96)
        throw std::invalid_argument("Invalid Blitz code table");
    constexpr std::array<Buttons, 15> masks{65535,
                                            button(Button::a),
                                            button(Button::b),
                                            button(Button::x),
                                            button(Button::y),
                                            button(Button::l),
                                            button(Button::r),
                                            button(Button::down) | button(Button::left),
                                            button(Button::down),
                                            button(Button::down) | button(Button::right),
                                            button(Button::right),
                                            button(Button::up) | button(Button::right),
                                            button(Button::up),
                                            button(Button::up) | button(Button::left),
                                            button(Button::left)};
    BattleBlitzCodes out;
    for (unsigned i = 0; i < 8; ++i) {
        const auto length = data[i * 12 + 11];
        if (length < 4 || length > 20 || (length & 1U))
            throw std::invalid_argument("Invalid Blitz code length");
        out[i].length = length / 2;
        for (unsigned j = 0; j < 10; ++j) {
            const auto value = data[i * 12 + j];
            if (value >= masks.size())
                throw std::invalid_argument("Invalid Blitz button identity");
            out[i].inputs[j] = masks[value];
        }
    }
    return out;
}
void BattleBlitzInput::sample(Buttons held) {
    if (held && held != previous) {
        deadline = FF6_ROM_VALUE_BATTLE_BLITZ_CHORD_DEADLINE_0;
        chords[cursor & 15U] |= held;
        cursor = wrap8(unsigned{cursor} + 1U);
    }
    previous = held;
    deadline = wrap8(unsigned{deadline} - 1U);
    if (!deadline)
        chords[0] = 0;
}
std::uint8_t BattleBlitzInput::match(const BattleBlitzCodes& codes) const {
    // C1/6F8D accepts either cardinal component of a diagonal and permits A
    // on the final direction or as the following chord. First matching code wins.
    for (unsigned i = 0; i < codes.size(); ++i) {
        const auto& code = codes[i];
        if (code.length < 2 || code.length > 10)
            continue;
        const unsigned directions = code.length - 1;
        bool matches = true;
        for (unsigned j = 0; j < directions; ++j)
            matches = matches && (chords[j] & code.inputs[j]) != 0;
        if (matches && held(chords[directions - 1] | chords[directions], Button::a))
            return static_cast<std::uint8_t>(i);
    }
    return 255;
}
} // namespace ff6
