#pragma once
#include "ff6/input.hpp"
#include <SDL3/SDL.h>
#include <span>

// SDL uses physical positions: Nintendo B/A/Y/X are south/east/west/north.
// Keep the SNES layout, including genuine L/R shoulders and +/- as Start/Select.
struct MenuBinding {
    ff6::Button button;
    SDL_Scancode key;
    SDL_GamepadButton pad;
};
constexpr std::array menu_bindings{
    MenuBinding{ff6::Button::a, SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_EAST},
    MenuBinding{ff6::Button::b, SDL_SCANCODE_BACKSPACE, SDL_GAMEPAD_BUTTON_SOUTH},
    MenuBinding{ff6::Button::up, SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP},
    MenuBinding{ff6::Button::down, SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    MenuBinding{ff6::Button::left, SDL_SCANCODE_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    MenuBinding{ff6::Button::right, SDL_SCANCODE_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT}};
constexpr std::array scene_bindings{
    menu_bindings[0],
    menu_bindings[1],
    menu_bindings[2],
    menu_bindings[3],
    menu_bindings[4],
    menu_bindings[5],
    MenuBinding{ff6::Button::x, SDL_SCANCODE_X, SDL_GAMEPAD_BUTTON_NORTH},
    MenuBinding{ff6::Button::y, SDL_SCANCODE_Y, SDL_GAMEPAD_BUTTON_WEST},
    MenuBinding{ff6::Button::l, SDL_SCANCODE_Q, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    MenuBinding{ff6::Button::r, SDL_SCANCODE_W, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    MenuBinding{ff6::Button::start, SDL_SCANCODE_RSHIFT, SDL_GAMEPAD_BUTTON_START},
    MenuBinding{ff6::Button::select, SDL_SCANCODE_LCTRL, SDL_GAMEPAD_BUTTON_BACK}};
inline ff6::Buttons mapped_buttons(SDL_Gamepad* controller,
                            std::span<const MenuBinding> bindings = menu_bindings,
                            bool keyboard = true) {
    ff6::Buttons result = 0;
    const auto* keys = SDL_GetKeyboardState(nullptr);
    for (const auto binding : bindings)
        if ((keyboard && keys[binding.key]) ||
            (controller && SDL_GetGamepadButton(controller, binding.pad)))
            result |= ff6::button(binding.button);
    return result;
}
