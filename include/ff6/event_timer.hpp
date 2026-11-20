#pragma once
#include "ff6/content.hpp"
#include "ff6/raster.hpp"
#include "rom-materials/battle_timer_refresh.macros.inc"

namespace ff6 {
struct EventTimerGraphics {
    std::array<Tile, 11> glyphs{}; // Decimal digits and colon.
    std::array<Rgb555, 16> battle_colors{};
};
struct BattleEventTimerDisplay {
    std::uint16_t frames = 0;
    std::uint8_t updates_remaining = 0;
    // C2/B92D: refresh now when zero, otherwise decrement and retain the text.
    void advance(std::uint16_t counter) {
        if (updates_remaining)
            --updates_remaining;
        else {
            frames = counter;
            updates_remaining = FF6_ROM_VALUE_BATTLE_TIMER_REFRESH_0;
        }
    }
};
EventTimerGraphics load_event_timer_graphics(const ContentPack&);
std::array<unsigned, 5> event_timer_digits(std::uint16_t frames);
void draw_field_event_timer(RasterScene&, const EventTimerGraphics&, std::uint16_t frames,
                            const std::array<Rgb555, 16>& colors);
void draw_battle_event_timer(RasterScene&, const EventTimerGraphics&, std::uint16_t frames,
                             Rgb555 font_color);
} // namespace ff6
