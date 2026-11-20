#include "ff6/battle_menu_transition.hpp"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
constexpr std::array<unsigned, 6> bank_sizes{64, 72, 80, 128, 40, 48};
void require(bool ok, const char* message) {
    if (!ok)
        throw std::invalid_argument(message);
}
unsigned bank_index(BattleTextBank bank) {
    const auto index = static_cast<unsigned>(bank);
    require(index < bank_sizes.size(), "Invalid battle menu text bank");
    return index;
}
std::int16_t signed_word(std::uint16_t word) {
    return static_cast<std::int16_t>(word < 32768 ? int{word} : int{word} - 65536);
}
BattleWindowBinding window_binding(unsigned index, std::uint16_t x, std::uint16_t y, unsigned top) {
    BattleWindowCanvas canvas;
    unsigned base_x = 0, base_y = 0;
    switch (index) {
    case 0:
    case 21:
    case 25:
        canvas = BattleWindowCanvas::closed;
        break;
    case 1:
    case 6:
    case 22:
        canvas = BattleWindowCanvas::command;
        base_y = 80;
        break;
    case 2:
        canvas = BattleWindowCanvas::slot;
        base_x = 256;
        base_y = 128;
        break;
    case 3:
    case 7:
    case 20:
    case 26:
        canvas = BattleWindowCanvas::list;
        base_y = 192;
        break;
    case 4:
    case 9:
    case 27:
        canvas = BattleWindowCanvas::magic;
        base_x = 256;
        break;
    case 5:
        canvas = BattleWindowCanvas::equipment;
        base_x = 256;
        base_y = 64;
        break;
    case 8:
        canvas = BattleWindowCanvas::esper;
        base_x = 256;
        base_y = 192;
        break;
    case 10:
    case 11:
    case 12:
    case 13:
    case 23:
        canvas = BattleWindowCanvas::row_def;
        break;
    case 18:
        canvas = BattleWindowCanvas::bushido;
        base_y = 64;
        break;
    case 14:
    case 15:
    case 16:
    case 17:
    case 19:
    case 24:
        require(x == 0 && y == 0, "Unexpected foreground window restoration");
        return {}; // Reveal the underlying battlefield; it retains its own sampling.
    default:
        throw std::invalid_argument("Invalid battle window layout");
    }
    // Decode original data coordinates into a named canvas. The renderer never
    // sees original addresses, scroll registers or a hardware tilemap atlas.
    const auto local_y = (top + unsigned{y} + 1U + 256U - base_y) & 255U;
    return {canvas, signed_word(wrap16(unsigned{x} - base_x)),
            static_cast<std::int16_t>(static_cast<int>(local_y) - static_cast<int>(top))};
}
} // namespace
BattleMenuTransitionAssets load_battle_menu_transition_assets(const ContentPack& pack) {
    const auto sizes = pack.get("battle_window_sizes");
    const auto positions = pack.get("battle_window_positions");
    const auto text = pack.get("battle_text_transitions");
    require(sizes.size() == 112 && positions.size() == 112 && text.size() == 88,
            "Invalid battle menu transition resources");
    BattleMenuTransitionAssets out;
    for (unsigned i = 0; i < out.windows.size(); ++i) {
        const auto top = read16(sizes, i * 4), steps = read16(sizes, i * 4 + 2);
        require(top % 4 == 0 && steps > 0 && top / 4U + steps * 8U <= 224,
                "Invalid battle window extent");
        const bool foreground = (i >= 10 && i <= 19) || i == 23 || i == 24;
        out.windows[i] = {
            static_cast<std::uint16_t>(top / 4U), static_cast<std::uint8_t>(steps),
            foreground ? BattleWindowPlane::foreground : BattleWindowPlane::menu,
            window_binding(i, read16(positions, i * 4), read16(positions, i * 4 + 2), top / 4U)};
    }
    // Scoped pointer decoding for the six documented source banks. These values
    // are content-format provenance only; native state owns separate typed banks.
    constexpr std::array<unsigned, 6> original_bases{0x4e75, 0x4f75, 0x5095,
                                                     0x51d5, 0x53d5, 0x5475};
    for (unsigned i = 0; i < out.text.size(); ++i) {
        const auto source = read16(text, i * 8), destination = read16(text, i * 8 + 2);
        const auto rows = read16(text, i * 8 + 4);
        require(destination >= 0x4af5 && (destination - 0x4af5) % 4 == 0 && rows >= 2 &&
                    rows <= 8 && (destination - 0x4af5) / 4U + rows * 8U <= 224 &&
                    read16(text, i * 8 + 6) == 0,
                "Invalid battle text transition extent");
        bool found = false;
        for (unsigned bank = 0; bank < original_bases.size(); ++bank) {
            if (source < original_bases[bank] || (source - original_bases[bank]) % 4 != 0)
                continue;
            const auto line = (source - original_bases[bank]) / 4;
            if (line + rows * 8U > bank_sizes[bank])
                continue;
            out.text[i] = {static_cast<BattleTextBank>(bank), static_cast<std::uint8_t>(line),
                           static_cast<std::uint8_t>(rows),
                           static_cast<std::uint16_t>((destination - 0x4af5) / 4)};
            found = true;
            break;
        }
        require(found, "Unowned battle text transition source");
    }
    return out;
}
BattleMenuTransition::BattleMenuTransition() {
    for (unsigned i = 0; i < text_banks.size(); ++i)
        text_banks[i].resize(bank_sizes[i]);
}
void begin_battle_menu_transition(BattleMenuTransition& state, BattleWindowLayout window,
                                  BattleTextLayout text, BattleWindowMotion motion,
                                  bool defer_text) {
    require(!state.busy(), "Battle menu transition is already active");
    require(motion == BattleWindowMotion::opening || motion == BattleWindowMotion::closing,
            "Invalid battle window motion");
    require(window.plane == BattleWindowPlane::menu ||
                window.plane == BattleWindowPlane::foreground,
            "Invalid battle window plane");
    require(window.steps > 0 && unsigned{window.top} + unsigned{window.steps} * 8U <= 224,
            "Battle window travel exceeds display");
    const auto bank = bank_index(text.bank);
    require(text.rows >= 2 && unsigned{text.top} + unsigned{text.rows} * 8U <= 224 &&
                unsigned{text.source_line} + unsigned{text.rows} * 8U <=
                    state.text_banks[bank].size(),
            "Battle text travel exceeds its surface");
    const bool close = motion == BattleWindowMotion::closing;
    const auto window_first = close ? window.steps * 8 - 4 : (window.steps - 1) * 4;
    const auto window_second = close ? 0 : window_first + 4;
    const auto text_first = close ? text.rows * 8 - 8 : (text.rows - 1) * 4;
    const auto text_second = close ? 4 : text_first + 4;
    BattleTextTravel text_travel{text,
                                 static_cast<std::int16_t>(text_first),
                                 static_cast<std::int16_t>(text_second),
                                 static_cast<std::uint8_t>(text.rows - 1),
                                 defer_text,
                                 false,
                                 std::vector<BattleTextBinding>((text.rows - 1U) * 8U)};
    state.window_travel =
        BattleWindowTravel{window, static_cast<std::int16_t>(window_first),
                           static_cast<std::int16_t>(window_second), window.steps};
    state.text_travel = std::move(text_travel);
}
void advance_battle_menu_transition(BattleMenuTransition& state, bool publication_pending) {
    if (publication_pending)
        return;
    // Validate both tracks before writing either. Public retained state may be
    // restored by future save/replay readers; malformed cursors cannot index out
    // of bounds or partially advance the other track.
    const auto valid_band = [](int first, int second, unsigned extent) {
        return first >= 0 && second >= 0 && static_cast<unsigned>(first) + 4 <= extent &&
               static_cast<unsigned>(second) + 4 <= extent;
    };
    if (state.window_travel) {
        const auto& t = *state.window_travel;
        require(t.layout.top <= 224 && t.remaining > 0 && t.remaining <= t.layout.steps &&
                    (t.layout.plane == BattleWindowPlane::menu ||
                     t.layout.plane == BattleWindowPlane::foreground) &&
                    valid_band(t.first, t.second, 224U - t.layout.top),
                "Invalid retained window travel");
    }
    if (state.text_travel) {
        const auto& t = *state.text_travel;
        const auto size = state.text_banks[bank_index(t.layout.bank)].size();
        require(
            t.layout.rows >= 2 && t.remaining > 0 && t.remaining < t.layout.rows &&
                t.layout.top <= 224 && t.layout.source_line <= size &&
                unsigned{t.layout.top} + t.layout.rows * 8U <= 224 &&
                unsigned{t.layout.source_line} + t.layout.rows * 8U <= size &&
                t.backup.size() == (t.layout.rows - 1U) * 8U &&
                valid_band(t.first, t.second, 224U - t.layout.top) &&
                valid_band(t.first, t.second, static_cast<unsigned>(size - t.layout.source_line)),
            "Invalid retained text travel");
    }
    if (state.window_travel) {
        auto& travel = *state.window_travel;
        auto& lines =
            travel.layout.plane == BattleWindowPlane::menu ? state.windows : state.foreground;
        for (int band : {travel.first, travel.second})
            std::fill_n(lines.begin() + travel.layout.top + band, 4, travel.layout.binding);
        travel.first = static_cast<std::int16_t>(travel.first - 4);
        travel.second = static_cast<std::int16_t>(travel.second + 4);
        if (--travel.remaining == 0)
            state.window_travel.reset();
    }
    if (state.text_travel) {
        auto& travel = *state.text_travel;
        if (travel.deferred) {
            travel.deferred = false;
            return;
        }
        auto& bank = state.text_banks[bank_index(travel.layout.bank)];
        if (!travel.backed_up) {
            std::copy_n(state.text.begin() + travel.layout.top + 4, travel.backup.size(),
                        travel.backup.begin());
            travel.backed_up = true;
        }
        for (int band : {travel.first, travel.second})
            std::copy_n(bank.begin() + travel.layout.source_line + band, 4,
                        state.text.begin() + travel.layout.top + band);
        travel.first = static_cast<std::int16_t>(travel.first - 4);
        travel.second = static_cast<std::int16_t>(travel.second + 4);
        if (--travel.remaining == 0) {
            std::copy(travel.backup.begin(), travel.backup.end(),
                      bank.begin() + travel.layout.source_line + 4);
            state.text_travel.reset();
        }
    }
}
} // namespace ff6
