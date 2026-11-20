#pragma once
#include "rom-materials/field_dialogue_initial_x.macros.inc"
#include "ff6/field_events.hpp"
#include "ff6/input.hpp"
#include "ff6/raster.hpp"

namespace ff6 {
struct FieldDialogueAssets {
    Bytes script, item_names, spell_names;
    std::vector<std::uint32_t> messages;
    // Uncompressed, single-line glyph strings, excluding their terminators.
    std::vector<Bytes> titles;
    std::array<std::uint8_t, 256> widths{}, pairs{};
    std::array<Glyph, 224> glyphs{};
    std::array<std::array<Tile, 28>, 8> skins{};
    std::array<std::array<Rgb555, 7>, 8> colors{};
    std::array<std::uint8_t, 288> window_tiles{};
    std::array<std::uint8_t, 200> shades{};
    std::array<std::uint8_t, 16> ending_shades{};
    std::array<std::uint8_t, 128> window_scroll{};
    std::array<std::uint8_t, 72> text_scroll{};
    std::array<std::uint8_t, 12> cursor{};
};
FieldDialogueAssets load_field_dialogue(const ContentPack& pack);
// Map-entry presentation used by the ending: two fade edges and a reserved
// caption area. Dialogue subsequently owns its opening/closing strips.
void compose_field_ending_bars(RasterScene& scene, const FieldDialogueAssets& assets);
struct FieldDialogueValues {
    std::array<std::array<std::uint8_t, 6>, 14> names{};
    std::uint32_t gil = 0;
    std::uint8_t item = 0, spell = 0;
};
enum class DialogueWindow { closed, opening, closing };
struct FieldDialogueStatus {
    DialogueWindow window = DialogueWindow::closed;
    std::uint8_t phase = 0, x = FF6_ROM_VALUE_FIELD_DIALOGUE_INITIAL_X_0, line = 0, block = 0;
    std::uint8_t key_wait = 0, clear_region = 0, selection = 0, choices = 0;
    std::uint16_t pause = 0, key_timer = 0;
    std::uint32_t position = 0;
    bool text_active = false, end_pending = false, bottom = false, text_only = false;
    bool ending_bars = false;
    bool operator==(const FieldDialogueStatus&) const = default;
};
// English field dialogue. Logical text, staged 16x16 publication and window
// bands have separate lifetimes. No machine-code, device memory or registers.
class FieldDialogue {
  public:
    const FieldDialogueStatus& status() const {
        return state_;
    }
    bool open() const {
        return state_.window != DialogueWindow::closed;
    }
    bool text_ready() const {
        return state_.key_wait == 1;
    }
    const std::array<std::uint8_t, 256 * 64>& pixels() const {
        return pixels_;
    }
    const std::array<std::uint8_t, 9>& bands() const {
        return bands_;
    }
    std::uint8_t title_remaining() const {
        return title_remaining_;
    }
    // Fresh map entry only. A title does not hold a dialogue event wait open.
    void show_map_title(const FieldDialogueAssets& assets, std::uint8_t title);
    void close_map_title();
    void show(const FieldDialogueAssets& assets, EventDialogue message, bool ending_bars = false);
    // Exactly one ordinary field visit. Failure preserves the prior boundary.
    void advance(const FieldDialogueAssets& assets, const FieldDialogueValues& values,
                 Buttons buttons);
    // Cursor, glyph block, then clearing, in original visible-publication order.
    void publish();
    // Ending text-only windows preserve the fade strips and hide the field.
    bool consume(const FieldDialogueAssets& assets, FieldEventRunner& events,
                 EventFlagAccess& flags);
    void compose(RasterScene& scene, const FieldDialogueAssets& assets, std::uint8_t skin,
                 const std::array<Rgb555, 128>& colors,
                 std::optional<std::array<std::size_t, 2>> underlay = std::nullopt) const;

  private:
    struct Position {
        std::uint8_t line = 0, block = 0;
    };
    FieldDialogueStatus state_;
    std::uint8_t title_remaining_ = 0;
    Bytes buffer_;
    std::size_t buffer_at_ = 0;
    bool choice_latched_ = false, text_pending_ = false, cursor_pending_ = false;
    std::array<Position, 8> choices_{};
    Position destination_, cursor_destination_;
    std::array<std::uint32_t, 16> ink_{}, shadow_{};
    std::array<std::uint8_t, 256> pending_{};
    std::array<std::uint8_t, 256 * 64> pixels_{};
    // Zero restores the underlying field; otherwise retain the opening phase
    // which last wrote this strip. Closing removes outer strips without scaling.
    std::array<std::uint8_t, 9> bands_{};
    void update_text(const FieldDialogueAssets&, const FieldDialogueValues&, Buttons);
    void update_window();
    std::uint8_t word_width(const FieldDialogueAssets&, const FieldDialogueValues&) const;
    void paint(const FieldDialogueAssets&, std::uint8_t glyph);
    void stage();
    void draw(const FieldDialogueAssets&, std::uint8_t glyph);
    void newline(const FieldDialogueAssets&, bool page);
    void draw_buffer(const FieldDialogueAssets&);
    void draw_cursor(const FieldDialogueAssets&, Buttons);
};
} // namespace ff6
