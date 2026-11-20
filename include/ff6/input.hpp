#pragma once
#include "rom-materials/input_repeat_initializers.macros.inc"
#include "rom-materials/input_confirm_delay.macros.inc"
#include "ff6/bytes.hpp"
#include <array>

namespace ff6 {
enum class Button : std::uint16_t {
    a = 1,
    b = 2,
    x = 4,
    y = 8,
    l = 16,
    r = 32,
    start = 64,
    select = 128,
    up = 256,
    down = 512,
    left = 1024,
    right = 2048
};
using Buttons = std::uint16_t;
constexpr Buttons button(Button value) {
    return static_cast<Buttons>(value);
}
constexpr bool held(Buttons buttons, Button value) {
    return (buttons & button(value)) != 0;
}
using ButtonMap = std::array<Button, 8>; // Physical A, B, X, Y, L, R, Start, Select.
ButtonMap default_button_map();
ButtonMap decode_button_map(ByteView packed);

enum class InputContext { field, menu, battle };
struct InputSettings {
    ButtonMap mapping = default_button_map();
    std::uint8_t repeat_delay = static_cast<std::uint8_t>(FF6_ROM_VALUE_INPUT_REPEAT_INITIALIZERS_0 & 255U);
    std::uint8_t repeat_period = static_cast<std::uint8_t>(FF6_ROM_VALUE_INPUT_REPEAT_INITIALIZERS_0 >> 8U);
};
struct InputState {
    Buttons raw = 0, held = 0, pressed = 0, repeating = 0, previous = 0;
    std::uint8_t confirm_delay = FF6_ROM_VALUE_INPUT_CONFIRM_DELAY_0,
                 confirm_interval = static_cast<std::uint8_t>(FF6_ROM_VALUE_INPUT_REPEAT_INITIALIZERS_0 >> 8U);
    std::uint8_t menu_delay = 0, menu_interval = 0;
    bool operator==(const InputState&) const = default;
};
struct ControllerRouting {
    bool multiplayer = false;
    std::uint8_t second_player_characters = 0;
    std::uint8_t active_character = 0;
};
// Reinitialization preserves button history and menu counters, as the game does.
void initialize_input(InputState& state, InputSettings& settings, const ButtonMap& mapping);
// One logical sample. Host device-event frequency must not drive these counters.
void sample_input(InputState& state, const InputSettings& settings, InputContext context,
                  Buttons first, Buttons second, ControllerRouting routing = {});
} // namespace ff6
