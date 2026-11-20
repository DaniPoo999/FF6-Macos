#pragma once
#include "ff6/cutscene_animation.hpp"
#include "ff6/title.hpp"
#include <variant>
#include "rom-materials/title_palette_initial_wait.macros.inc"
#include "rom-materials/title_expansion_initial_height.macros.inc"

namespace ff6 {
using TitleAnimationFrame = CutsceneAnimationFrame;
// The original pointer-based data language is resolved to native resource indices
// while loading. Runtime tasks never receive an original address or callback.
struct TitleAnimationAssets {
    TitleAssets base;
    CutscenePalette town_dark{};
    std::array<CutscenePalette, 6> lightning_colors{};
    std::vector<TitleAnimationFrame> palette_animation;
    std::array<std::vector<TitleAnimationFrame>, 2> lightning_animations;
    std::array<std::optional<std::size_t>, 4> lightning_selection;
    std::vector<std::vector<RasterSprite>> lightning_frames;
    std::vector<RasterCell> cloud_rows;
    std::array<std::int16_t, 256> sine{};
    std::array<std::uint8_t, 256> random{};
};
TitleAnimationAssets load_title_animation_assets(const ContentPack& pack);
struct CutsceneInput {
    bool held = false, previous = false, pressed = false;
    void sample(bool next_held) {
        // Cutscene UpdateCtrl calculates the edge from the preceding sample.
        pressed = held && !previous;
        previous = held;
        held = next_held;
    }
};
enum class TitleStage : std::uint8_t {
    initialize,
    silent_town,
    town,
    reveal_delay,
    logo,
    legal_delay,
    legal,
    darken,
    stop_effects,
    scroll_town,
    hold_town,
    terminate,
    finished = 255
};
struct TitleRowOffset {
    std::int16_t x = 0, y = 0;
};
struct TitleAnimationState {
    TitleStage stage = TitleStage::initialize;
    std::uint16_t remaining = 0, town_y = 0;
    std::uint32_t visits = 0;
    std::uint8_t frame_counter = 0, random_index = 0, lightning_count = 0, intensity = 15;
    bool row_effects = false, hide_legal = false, stop_waves = false;
    bool fading = false, complete = false, skipped = false;
    CutsceneInput input{};
    std::array<CutscenePalette, 16> colors{};
    std::array<std::array<TitleRowOffset, 224>, 3> rows{};
};
enum class TitleSoundRequest { restart_theme, stop };
// Runs after the English splash. Explicit retained RNG/input avoid inventing a
// warm-entry reset. Original unowned palettes are not meaningful native state.
class TitleAnimation {
  public:
    TitleAnimation(TitleAnimationAssets assets, const TitleSplashState& splash,
                   std::uint8_t random_index, CutsceneInput input = {});
    bool advance(bool held_a = false);
    const TitleAnimationState& state() const {
        return published_;
    }
    RasterScene scene() const;
    std::vector<TitleSoundRequest> take_sound_requests();

  private:
    using Cursor = CutsceneAnimationCursor;
    struct Fade {
        unsigned group;
        CutscenePalette target;
        CutscenePaletteFade task;
    };
    struct Cloud {
        bool initialized = false;
    };
    struct PaletteCycle {
        unsigned group;
        unsigned phase = 0;
        std::uint8_t wait = FF6_ROM_VALUE_TITLE_PALETTE_INITIAL_WAIT_0;
        Cursor cursor{};
    };
    struct Lightning {
        bool initialized = false;
        std::size_t animation = 0;
        std::uint8_t x = 0, y = 0;
        Cursor cursor{};
    };
    struct Wave {
        unsigned layer;
        std::uint8_t phase = 0;
    };
    struct Expand {
        unsigned height = FF6_ROM_VALUE_TITLE_EXPANSION_INITIAL_HEIGHT_0;
    };
    struct Legal {
        unsigned group;
    };
    struct TownLights {};
    struct Brightness {
        std::uint8_t level = 15;
    };
    using Task = std::variant<std::monostate, Fade, Cloud, PaletteCycle, Lightning, Wave, Expand,
                              Legal, TownLights, Brightness>;
    TitleAnimationAssets assets_;
    TitleAnimationState state_{};
    TitleAnimationState published_{};
    bool prepared_ = false;
    std::array<Task, 128> tasks_{};
    struct Sprite {
        RasterSprite image;
        unsigned palette;
    };
    std::vector<Sprite> sprites_;
    std::vector<Sprite> published_sprites_;
    std::vector<RasterCell> published_layout_;
    std::vector<TitleSoundRequest> sounds_;
    void create(unsigned priority, Task task);
    void fade(unsigned group, const CutscenePalette& target, std::uint8_t period);
    void advance_stage();
    bool prepare();
    bool run(Task& task);
    std::pair<std::uint8_t, bool> random();
    static void animate(Cursor& cursor, const std::vector<TitleAnimationFrame>& frames);
};
} // namespace ff6
