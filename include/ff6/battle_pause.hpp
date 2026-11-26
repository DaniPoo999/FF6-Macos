#pragma once
#include "ff6/input.hpp"
#include <optional>

namespace ff6 {
struct BattlePauseState {
    bool paused = false, sound_disabled = false, flashback = false, forbidden = false;
    Buttons pressed = 0;
};
struct BattlePauseDecision {
    // The resume visit also skips active work; input/presentation still run.
    bool skip_active_updates = false;
    std::optional<bool> music_pause;
};
inline BattlePauseDecision update_battle_pause(BattlePauseState& state) {
    BattlePauseDecision result{state.paused, {}};
    if (state.sound_disabled || (!state.paused && (state.flashback || state.forbidden)))
        return result;
    // CheckPause compares the mapped NEW high-byte group, not held/repeat or
    // physical buttons. A/X/L/R do not interfere; default physical Select is R.
    constexpr Buttons group = button(Button::b) | button(Button::y) | button(Button::select) |
        button(Button::start) | button(Button::up) | button(Button::down) |
        button(Button::left) | button(Button::right);
    if ((state.pressed & group) == button(Button::start)) {
        state.paused = !state.paused;
        result.skip_active_updates = true;
        result.music_pause = state.paused;
    }
    return result;
}
} // namespace ff6
