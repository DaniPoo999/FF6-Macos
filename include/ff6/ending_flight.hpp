#pragma once
#include "ff6/ending_characters.hpp"
#include "ff6/perspective.hpp"
#include "rom-materials/ending_flight_default_priorities.macros.inc"
#include "rom-materials/ending_flight_shared_fade_periods.macros.inc"
#include "rom-materials/ending_flight_planes.macros.inc"
#include "rom-materials/ending_flight_default_view.macros.inc"

namespace ff6 {
// The ten authored flight entries share one native camera, lettering and object
// pipeline. Selectors are resolved at the event boundary, not used as opcodes.
enum class EndingFlightScene { clouds, spiral, above_clouds, distant_airship,
                              boat, sea_airship, land_birds, landscape, big_airship, jet };
bool is_ending_flight_segment(std::uint8_t selector);
EndingFlightScene ending_flight_segment(std::uint8_t selector);
struct EndingCameraStep {
    int x = 0, y = 0, rotation = 0, slope = 0, depth = 0;
    std::uint8_t duration = 254;
};
std::vector<EndingCameraStep> decode_ending_camera(ByteView data);
struct EndingFlightPage {
    unsigned start = 0;
    std::vector<CutsceneSprite> lettering;
};
enum class EndingFlightObject { left_airship, shadow, right_airship, big_airship,
    tiny_airship, tiny_shadow, splash, propeller_left, propeller_right,
    boat_left, boat_right, flying_bird, bird0, bird1, bird2, bird3, bird4, bird5, count };
struct EndingFlightBird { unsigned x = 0, y = 0; };
struct EndingFlightAssets {
    EndingFlightScene kind = EndingFlightScene::clouds;
    AffineLayer terrain;
    std::optional<TileLayer> sky;
    std::array<CutscenePalette, 8> colors{}, object_colors{};
    std::array<std::int16_t, 256> sine{};
    std::vector<EndingCameraStep> camera;
    std::vector<EndingFlightPage> pages;
    std::array<EndingSpriteAnimation, static_cast<unsigned>(EndingFlightObject::count)> animations;
    std::array<EndingFlightBird, 6> birds{};
};
EndingFlightAssets load_ending_flight_assets(const ContentPack& pack, EndingFlightScene kind);
class EndingFlight {
  public:
    explicit EndingFlight(EndingFlightAssets assets);
    bool advance();
    bool complete() const;
    unsigned updates() const { return updates_; }
    RasterScene scene() const;
  private:
    struct Palette {
        CutscenePalette colors{}, target{};
        CutscenePaletteFade fade{FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0};
        void to(const CutscenePalette& next, std::uint8_t period);
    };
    enum class Motion { linear, bob, splash_bob, oscillate, flying };
    struct Object {
        EndingFlightObject animation;
        Motion motion = Motion::linear;
        CutsceneAnimationCursor cursor;
        std::uint16_t x = 0, y = 0;
        int vx = 0, vy = 0, base = 0;
        unsigned phase = 0, age = 0, delay = 0, life = 0, priority = FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_PRIORITIES_0;
        bool scroll = false;
    };
    EndingFlightAssets assets_;
    std::array<Palette, 8> background_, objects_;
    std::vector<Object> actors_;
    // The ROM initializes three coordinates and clears the final scroll field.
    PerspectiveView camera_{
        FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_VIEW_0 + 256 * FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_VIEW_1,
        FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_VIEW_2 + 256 * FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_VIEW_3,
        FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_VIEW_4 + 256 * FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_VIEW_5,
        0};
    std::uint16_t slope_ = 0, depth_ = FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_14;
    std::uint8_t rotation_ = 0;
    unsigned updates_ = 0, camera_frame_ = 0, camera_wait_ = 0;
    unsigned duration() const;
    void spawn(EndingFlightObject animation, int x, int y, Motion motion = Motion::linear,
               int vx = 0, int vy = 0, unsigned life = 0, unsigned priority = FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_PRIORITIES_0);
    void advance_camera();
    void advance_object(Object& object);
};
} // namespace ff6
