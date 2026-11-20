#include "ff6/event_timer.hpp"
#include "rom-materials/field_timer_priority.macros.inc"
#include "rom-materials/battle_timer_priority.macros.inc"
#include "rom-materials/battle_text_layout.macros.inc"
#include "rom-materials/field_viewport_bounds.macros.inc"
#include "rom-materials/field_timer_position.macros.inc"

namespace ff6 {
EventTimerGraphics load_event_timer_graphics(const ContentPack& pack) {
    const auto font = decode_tiles(pack.get("menu_graphics_small_font_2bpp"), 2);
    EventTimerGraphics result;
    for (unsigned i = 0; i < 10; ++i)
        result.glyphs[i] = font.at(0xb4 + i);
    result.glyphs[10] = font.at(0xc1);
    const auto colors = pack.get("menu_graphics_battle_font_pal_pal");
    for (unsigned i = 0; i < 4; ++i)
        result.battle_colors[i] = read16(colors, i * 2U);
    return result;
}
std::array<unsigned, 5> event_timer_digits(std::uint16_t frames) {
    const unsigned seconds = frames / 60U, minutes = seconds / 60U;
    return {minutes / 10U, minutes % 10U, 10, seconds / 10U % 6U, seconds % 10U};
}
void draw_field_event_timer(RasterScene& scene, const EventTimerGraphics& graphics,
                            std::uint16_t frames, const std::array<Rgb555, 16>& colors) {
    // DrawTimer selects the coordinates and sprite priority. Native depth
    // ranks preserve relative layer ordering; the 2bpp plane conversion is fixed.
    RasterSprite sprite;
    sprite.x = FF6_ROM_VALUE_FIELD_TIMER_POSITION_0; sprite.y = FF6_ROM_VALUE_FIELD_TIMER_POSITION_1 + 1;
    sprite.width = 40; sprite.height = 8;
    constexpr auto priority = FF6_ROM_VALUE_FIELD_TIMER_PRIORITY_0;
    sprite.depth = priority == 0 ? 2 : priority * 3;
    sprite.colors = colors;
    sprite.pixels.resize(320);
    const auto digits = event_timer_digits(frames);
    for (unsigned column = 0; column < digits.size(); ++column)
        for (unsigned y = 0; y < 8; ++y)
            for (unsigned x = 0; x < 8; ++x) {
                const auto pixel = graphics.glyphs[digits[column]][y * 8 + x];
                sprite.pixels[y * 40 + column * 8 + x] = pixel ^ ((pixel >> 1U) & 1U);
            }
    if (!scene.objects) {
        scene.objects.emplace();
        scene.objects->clip_operand = false;
        scene.objects->lines.resize(scene.height);
        for (unsigned y = 0; y < scene.height; ++y) {
            auto& line = scene.objects->lines[y];
            line.left = FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_0;
            line.right = FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_1 + 1;
            line.enabled = y >= FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_3 && y < FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_2;
        }
    }
    scene.objects->sprites.insert(scene.objects->sprites.begin(), std::move(sprite));
}
void draw_battle_event_timer(RasterScene& scene, const EventTimerGraphics& graphics,
                             std::uint16_t frames, Rgb555 font_color) {
    // The ROM selects the text-canvas cell and its visible-band geometry.
    // Extraction maps that cell to native screen coordinates; the font is 8px.
    TileLayer layer;
    layer.columns = 5; layer.rows = 1;
    layer.wrap_x = layer.wrap_y = false;
    layer.tiles.assign(graphics.glyphs.begin(), graphics.glyphs.end());
    layer.tiles.emplace_back();
    layer.palettes.push_back(graphics.battle_colors);
    layer.palettes[0][3] = font_color;
    const auto digits = event_timer_digits(frames);
    for (unsigned i = 0; i < digits.size(); ++i)
        layer.cells.push_back({i == 0 && digits[i] == 0 ? 11U : digits[i], 0, 1 + 9 * FF6_ROM_VALUE_BATTLE_TIMER_PRIORITY_0});
    layer.lines.resize(scene.height);
    for (unsigned y = 0; y < scene.height; ++y) {
        auto& line = layer.lines[y];
        line.enabled = y >= FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_6 && y < FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_6 + 8;
        line.offset_x = -FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_5;
        line.offset_y = -FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_6;
    }
    scene.layers.insert(scene.layers.begin(), std::move(layer));
}
} // namespace ff6
