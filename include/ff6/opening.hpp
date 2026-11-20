#pragma once
#include "ff6/cutscene_animation.hpp"
#include "ff6/opening_landscape.hpp"
#include "ff6/title.hpp"
#include "ff6/title_animation.hpp"
#include "rom-materials/opening_initial_scroll_packed.macros.inc"
#include "rom-materials/opening_initial_center_y.macros.inc"
#include "rom-materials/opening_bob_initial_wait.macros.inc"
#include "rom-materials/opening_initial_brightness.macros.inc"
#include <variant>

namespace ff6 {
struct OpeningPalette {
    CutscenePalette colors{};
    // Original fade slots leave their target's low address byte behind as a
    // snow particle's fractional y when reused. Resolve this quirk at loading;
    // no runtime palette pointer or emulated task memory is required.
    std::uint8_t particle_fraction = 0;
};
using OpeningSprite = CutsceneSprite;
struct OpeningArmorPart {
    std::vector<CutsceneAnimationFrame> animation;
    std::uint8_t offset_x = 0, offset_y = 0;
};
struct OpeningOffset {
    std::uint8_t value = 0, duration = 0;
};
struct OpeningCaption {
    std::size_t cell = 0;
    std::uint8_t attributes = 0, scroll = 0;
    Bytes glyphs; // Includes documented row controls and the zero terminator.
};
struct OpeningAssets {
    AffineLayer landscape;
    TileLayer town, text;
    std::vector<Tile> object_tiles;
    std::vector<std::vector<OpeningSprite>> sprite_frames;
    std::array<OpeningArmorPart, 6> armor;
    std::array<std::vector<CutsceneAnimationFrame>, 7> snow;
    std::array<std::size_t, 8> snow_selection{};
    std::vector<OpeningOffset> armor_offsets;
    std::vector<OpeningCaption> captions;
    std::vector<CutsceneAnimationFrame> caption_animation;
    std::array<std::array<std::uint8_t, 4>, 8> caption_scrolls{};
    OpeningPalette black, font, armor_palette, snow_palette, town_light, town_dark;
    std::array<OpeningPalette, 3> landscape_palettes;
    std::array<std::uint8_t, 256> random{};
};
OpeningAssets load_opening_assets(const ContentPack& pack);
enum class OpeningStage : std::uint8_t { initialize, approach, credits, ending, finished = 255 };
// The packed constructor word and independently used center must agree.
static_assert((FF6_ROM_VALUE_OPENING_INITIAL_SCROLL_PACKED_0 >> 16U) ==
              FF6_ROM_VALUE_OPENING_INITIAL_CENTER_Y_0);
struct OpeningState {
    OpeningStage stage = OpeningStage::initialize;
    std::uint16_t remaining = 0, landscape_scroll = 0,
                  town_scroll = (FF6_ROM_VALUE_OPENING_INITIAL_SCROLL_PACKED_0 & 65535U),
                  center_y = FF6_ROM_VALUE_OPENING_INITIAL_CENTER_Y_0;
    std::uint32_t visits = 0;
    std::uint8_t frame_counter = 0, random_index = 0,
                 intensity = FF6_ROM_VALUE_OPENING_INITIAL_BRIGHTNESS_0,
                 bob = 0, active_tasks = 0;
    // Authored starting coordinates are private ROM-generated inputs.
    std::array<std::uint8_t, 3> armor_x{
#include "rom-materials/opening_armor_x.inc"
    }, armor_y{
#include "rom-materials/opening_armor_y.inc"
    };
    std::array<CutscenePalette, 16> colors{};
    CutsceneInput input{};
    bool fading = false, complete = false, skipped = false;
};
class OpeningAnimation {
  public:
    explicit OpeningAnimation(OpeningAssets assets, std::uint8_t random_index = 0);
    bool advance(bool held_a = false);
    // True after preparing completion following the final published image.
    // A parent can issue SoundOff immediately, without an invented extra visit.
    bool finished() const {
        return state_.complete;
    }
    const OpeningState& state() const {
        return published_;
    }
    RasterScene scene() const;
    const std::vector<OpeningSprite>& sprites() const {
        return published_sprites_;
    }
    const std::array<RasterCell, 320>& text_cells() const {
        return published_text_;
    }

  private:
    struct Fade {
        unsigned group;
        OpeningPalette target;
        CutscenePaletteFade task;
    };
    struct Armor {
        unsigned actor, part;
        bool initialized = false;
        CutsceneAnimationCursor cursor{};
    };
    struct Bob {
        bool initialized = false;
        std::size_t frame = 0;
        std::uint8_t wait = FF6_ROM_VALUE_OPENING_BOB_INITIAL_WAIT_0;
    };
    struct Snow {
        std::size_t animation;
        std::uint16_t x, y, dx, dy;
    };
    struct SnowSpawner {};
    struct Scroll {};
    struct Move {
        unsigned actor;
        std::uint8_t phase = 0, wait, duration;
        std::uint16_t y = 0, rate;
    };
    struct Caption {
        bool initialized = false;
        CutsceneAnimationCursor cursor{};
    };
    struct TownLights {};
    struct Brightness {
        std::uint8_t level = 15;
    };
    using Task = std::variant<std::monostate, Fade, Armor, Bob, Snow, SnowSpawner, Scroll, Move,
                              Caption, TownLights, Brightness>;
    struct Slot {
        Task task{};
        // Only the visible particle-fraction consequence of original slot reuse
        // is retained. These are not a generic task register file or RAM image.
        std::uint8_t particle_x = 0, particle_y = 0;
    };
    OpeningAssets assets_;
    OpeningState state_{}, published_{};
    std::array<Slot, 128> slots_{};
    std::array<RasterCell, 320> text_{}, published_text_{};
    std::vector<OpeningSprite> sprites_, published_sprites_;
    std::array<std::uint8_t, 4> live_text_scroll_{};
    std::uint16_t live_center_y_ = FF6_ROM_VALUE_OPENING_INITIAL_CENTER_Y_0;
    bool started_ = false;
    std::size_t create(unsigned priority, Task task);
    void fade(unsigned group, const OpeningPalette& target, std::uint8_t period);
    void initialize();
    void prepare(bool advance_stage);
    bool run(std::size_t slot);
    std::uint8_t random();
    void draw(std::size_t resource, std::uint8_t x, std::uint8_t y);
    void caption(const OpeningCaption& text);
};
} // namespace ff6
