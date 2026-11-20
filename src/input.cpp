#include "ff6/input.hpp"
#include <bit>
#include "rom-materials/default_button_mapping.macros.inc"

namespace ff6 {
namespace {
constexpr Buttons all_buttons = 4095;
constexpr Buttons directions =
    button(Button::up) | button(Button::down) | button(Button::left) | button(Button::right);
constexpr Buttons battle_secondary =
    button(Button::a) | button(Button::x) | button(Button::l) | button(Button::r);
void validate_mapping(const ButtonMap& mapping) {
    for (const auto target : mapping) {
        const auto value = button(target);
        if (value > button(Button::select) || !std::has_single_bit(value))
            throw std::runtime_error("Controller mappings require a single action button");
    }
}
std::uint8_t decrement(std::uint8_t value) {
    return wrap8(std::uint32_t{value} + 255U);
}
bool repeat_due(std::uint8_t& delay, std::uint8_t& interval, std::uint8_t period) {
    if (delay != 0) {
        delay = decrement(delay);
        if (delay != 0)
            return false;
    }
    interval = decrement(interval);
    if (interval != 0)
        return false;
    interval = period;
    return true;
}
Buttons mapped(Buttons physical, const ButtonMap& mapping) {
    Buttons result = physical & directions;
    for (std::size_t i = 0; i < mapping.size(); ++i)
        if ((physical & (1U << i)) != 0)
            result = static_cast<Buttons>(result | button(mapping[i]));
    return result;
}
Button decode_action(std::uint8_t index) {
    switch (index) {
    case 0:
        return Button::start;
    case 1:
        return Button::a;
    case 2:
        return Button::b;
    case 3:
        return Button::x;
    case 4:
        return Button::y;
    case 5:
        return Button::l;
    case 6:
        return Button::r;
    case 7:
        return Button::select;
    default:
        throw std::runtime_error("Invalid original controller mapping index");
    }
}
} // namespace

ButtonMap default_button_map() {
    // Select acting as R is observable original behavior, including in menus.
    // C3/A5D1 supplies the packed defaults. Encoding is independent of those
    // choices; private generated literals preserve the existing native binary.
    return {static_cast<Button>(FF6_ROM_VALUE_DEFAULT_BUTTON_MAPPING_0),
            static_cast<Button>(FF6_ROM_VALUE_DEFAULT_BUTTON_MAPPING_1),
            static_cast<Button>(FF6_ROM_VALUE_DEFAULT_BUTTON_MAPPING_2),
            static_cast<Button>(FF6_ROM_VALUE_DEFAULT_BUTTON_MAPPING_3),
            static_cast<Button>(FF6_ROM_VALUE_DEFAULT_BUTTON_MAPPING_4),
            static_cast<Button>(FF6_ROM_VALUE_DEFAULT_BUTTON_MAPPING_5),
            static_cast<Button>(FF6_ROM_VALUE_DEFAULT_BUTTON_MAPPING_6),
            static_cast<Button>(FF6_ROM_VALUE_DEFAULT_BUTTON_MAPPING_7)};
}
ButtonMap decode_button_map(ByteView packed) {
    if (packed.size() != 4)
        throw std::runtime_error("Controller mapping requires four bytes");
    ButtonMap mapping{};
    for (std::size_t i = 0; i < packed.size(); ++i) {
        mapping[i * 2] = decode_action(static_cast<std::uint8_t>(packed[i] >> 4U));
        mapping[i * 2 + 1] = decode_action(static_cast<std::uint8_t>(packed[i] & 15U));
    }
    return mapping;
}
void initialize_input(InputState& state, InputSettings& settings, const ButtonMap& mapping) {
    validate_mapping(mapping);
    settings = {mapping, static_cast<std::uint8_t>(FF6_ROM_VALUE_INPUT_REPEAT_INITIALIZERS_0 & 255U), static_cast<std::uint8_t>(FF6_ROM_VALUE_INPUT_REPEAT_INITIALIZERS_0 >> 8U)};
    state.confirm_delay = FF6_ROM_VALUE_INPUT_CONFIRM_DELAY_0;
    state.confirm_interval = static_cast<std::uint8_t>(FF6_ROM_VALUE_INPUT_REPEAT_INITIALIZERS_0 >> 8U);
}
void sample_input(InputState& state, const InputSettings& settings, InputContext context,
                  Buttons first, Buttons second, ControllerRouting routing) {
    validate_mapping(settings.mapping);
    if ((first & ~all_buttons) != 0 || (second & ~all_buttons) != 0)
        throw std::runtime_error("Unsupported physical button bits");
    if (context != InputContext::field && context != InputContext::menu &&
        context != InputContext::battle)
        throw std::runtime_error("Invalid input sampling context");
    Buttons physical = first;
    if (context == InputContext::battle && routing.multiplayer) {
        if (routing.active_character >= 4)
            throw std::runtime_error("Battle controller character index is outside the party");
        if ((routing.second_player_characters & (1U << routing.active_character)) != 0)
            physical = second;
    }
    const auto previous = state.previous;
    state.held = mapped(physical, settings.mapping);
    state.pressed = static_cast<Buttons>(state.held & ~previous);
    state.previous = state.held;
    state.raw = first | second;
    if (context == InputContext::field)
        return;

    // Any mapped change resets the delay, not just a change of direction.
    if (state.held != previous) {
        state.menu_delay = settings.repeat_delay;
        state.menu_interval = settings.repeat_period;
        state.repeating = state.pressed;
    } else {
        state.repeating = repeat_due(state.menu_delay, state.menu_interval, settings.repeat_period)
                              ? state.held
                              : state.pressed;
    }
    if (context != InputContext::battle)
        return;

    if (held(state.held, Button::a)) {
        if (repeat_due(state.confirm_delay, state.confirm_interval, settings.repeat_period)) {
            state.repeating |= button(Button::a);
            return;
        }
    } else {
        state.confirm_delay = FF6_ROM_VALUE_INPUT_CONFIRM_DELAY_0;
        state.confirm_interval = settings.repeat_period;
    }
    // A/X/L/R share the original secondary action group. Even X/L/R generic
    // repeats are suppressed here except on the samples where A repeats.
    state.repeating = static_cast<Buttons>((state.repeating & ~battle_secondary) |
                                           (state.pressed & battle_secondary));
}
} // namespace ff6
