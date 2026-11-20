#pragma once
#include "ff6/ending_book.hpp"
#include "ff6/party.hpp"
#include "rom-materials/ending_character_default_priority.macros.inc"

namespace ff6 {
enum class EndingCharacterScene { shadow, cyan, mog, brothers, gogo, gau, terra,
                                  locke_celes, relm, strago, setzer, umaro };
bool is_ending_character_segment(std::uint8_t selector);
EndingCharacterScene ending_character_segment(std::uint8_t selector);
// Names follow character properties, including characters absent from the
// current party. This is the same lookup used by DrawEndingCharName.
std::array<std::uint8_t, 6> ending_player_name(const Roster& roster, unsigned character);
enum class EndingDecoration { apple, coin, dancer, glimmer, eyes, pendant, sword,
                              cards, skull, moogle_idle, moogle_walk, moogle_jump,
                              moogle_stop0, moogle_stop1, moogle_stop2, moogle_stop3, count };
struct EndingSpriteAnimation {
    std::vector<CutsceneAnimationFrame> frames;
    std::vector<std::vector<CutsceneSprite>> groups;
};
struct EndingCharacterName {
    TileLayer player;
    std::array<std::vector<CutsceneSprite>, 2> full;
    std::array<int, 2> x{};
};
struct EndingCharacterAssets {
    EndingCharacterScene kind = EndingCharacterScene::shadow;
    EndingBookAssets background;
    CutscenePalette decoration_colors{};
    std::vector<EndingCharacterName> names;
    std::vector<CutsceneSprite> as;
    std::array<EndingSpriteAnimation, static_cast<unsigned>(EndingDecoration::count)> animations;
    std::array<int, 16> jump{};
};
EndingCharacterAssets load_ending_character_assets(const ContentPack& pack,
                                                   EndingCharacterScene kind, const Roster& roster);
class EndingCharacters {
  public:
    explicit EndingCharacters(EndingCharacterAssets assets);
    bool advance();
    bool complete() const;
    unsigned updates() const { return updates_; }
    RasterScene scene() const;
  private:
    struct Palette {
        CutscenePalette colors{}, target{};
        CutscenePaletteFade fade{FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0};
        void to(const CutscenePalette& next, std::uint8_t period);
        void advance() { fade.advance(colors, target); }
    };
    enum class Movement { ordinary, coin, waiting, jumping, resting, walking, arrival, stopped };
    struct Object {
        EndingDecoration animation;
        CutsceneAnimationCursor cursor;
        std::uint16_t x = 0, y = 0;
        int vx = 0, vy = 0;
        unsigned priority = FF6_ROM_VALUE_ENDING_CHARACTER_DEFAULT_PRIORITY_0, counter = 0, jump = 0, final_pose = 0;
        bool scroll = false, remove_on_hold = false, alive = true;
        Movement movement = Movement::ordinary;
    };
    EndingCharacterAssets assets_;
    std::array<Palette, 8> background_, objects_;
    std::vector<Object> decorations_;
    CutsceneAnimationCursor book_{};
    unsigned updates_ = 0;
    bool paired() const;
    unsigned fade_start() const;
    int scroll() const;
    void spawn(EndingDecoration animation, int x, int y, bool scroll, unsigned priority = FF6_ROM_VALUE_ENDING_CHARACTER_DEFAULT_PRIORITY_0);
    void set_animation(Object& object, EndingDecoration animation);
    void advance_object(Object& object);
    void caption_in();
    void caption_out();
};
} // namespace ff6
