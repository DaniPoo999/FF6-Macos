#pragma once
#include "ff6/ending_flight.hpp"
#include "ff6/music_player.hpp"
#include "rom-materials/ending_airship_trail_layout.macros.inc"
#include "rom-materials/ending_final_scroll_origin.macros.inc"
#include "rom-materials/ending_final_star_initial.macros.inc"
#include "rom-materials/ending_airship_trail_phases.macros.inc"
#include "rom-materials/ending_airship_origin.macros.inc"
#include "rom-materials/ending_airship_velocity.macros.inc"
#include "rom-materials/ending_airship_frames.macros.inc"
#include "rom-materials/ending_airship_speed_curve.macros.inc"

namespace ff6 {
struct EndingAirshipAssets {
    AffineLayer terrain;
    std::array<std::vector<RasterSprite>, 108> images;
    std::array<std::uint8_t, 256> random{};
    std::array<std::uint8_t, FF6_ROM_VALUE_ENDING_AIRSHIP_TRAIL_PHASES_0> trail_frames{};
    std::uint8_t travel_x = 0, travel_y = 0;
};
EndingAirshipAssets load_ending_airship_assets(const ContentPack& pack);
// BF has its own Falcon motion; ordinary takeoff/departure has different curves.
class EndingAirship {
  public:
    explicit EndingAirship(EndingAirshipAssets assets, std::uint8_t random = 0);
    bool advance();
    bool complete() const { return finished_; }
    unsigned updates() const { return updates_; }
    RasterScene scene() const;
  private:
    struct Trail { unsigned phase = 0, period = 1, wait = 1, x = 0, y = 0; };
    EndingAirshipAssets assets_;
    // Two alternating update banks and one published bank; the scene
    // extent is private ROM data, not an authored count retained here.
    std::array<Trail, FF6_ROM_VALUE_ENDING_AIRSHIP_TRAIL_LAYOUT_0 * 2> trail_{};
    std::array<Trail, FF6_ROM_VALUE_ENDING_AIRSHIP_TRAIL_LAYOUT_0> published_trail_{};
    // EE/552F initializes this scene separately from ordinary vehicle travel.
    std::uint16_t x_ = FF6_ROM_VALUE_ENDING_AIRSHIP_ORIGIN_0, y_ = FF6_ROM_VALUE_ENDING_AIRSHIP_ORIGIN_1;
    int vx_ = FF6_ROM_VALUE_ENDING_AIRSHIP_VELOCITY_0, vy_ = FF6_ROM_VALUE_ENDING_AIRSHIP_VELOCITY_1;
    unsigned picture_x_ = FF6_ROM_VALUE_ENDING_AIRSHIP_ORIGIN_0 / 256,
             picture_y_ = FF6_ROM_VALUE_ENDING_AIRSHIP_ORIGIN_1 / 256,
             frame_ = FF6_ROM_VALUE_ENDING_AIRSHIP_FRAMES_0;
    unsigned camera_x_ = 0, camera_y_ = 0,
             speed_ = FF6_ROM_VALUE_ENDING_AIRSHIP_SPEED_CURVE_2 * 32;
    unsigned updates_ = 0, rotor_ = 0, rotor_wait_ = 1, first_ = 0, drift_ = 0, brightness_ = 0;
    std::uint8_t random_ = 0;
    bool finished_ = false;
};
struct TheEndAssets {
    std::vector<Tile> tiles;
    std::array<CutscenePalette, 8> palettes{};
    std::array<std::uint8_t, 256> random{};
};
TheEndAssets load_the_end_assets(const ContentPack& pack);
enum class TheEndPhase { first_cue, reveal, second_cue, lettering, song_end, silence, stars, hold };
// BB is terminal: it owns display permanently. Music sequence cues are semantic
// feedback from the native player, independent of audio-device buffering.
class TheEnd {
  public:
    explicit TheEnd(TheEndAssets assets, std::uint8_t random = 0);
    std::vector<MusicPlayerCommand> advance(std::optional<std::uint8_t> cue, bool song_finished);
    TheEndPhase phase() const { return phase_; }
    unsigned updates() const { return updates_; }
    RasterScene scene() const;
  private:
    struct Star { unsigned x = 0, y = 0, vx = 0, vy = 0, growth = 0, size = 0, acceleration = 0, tile = FF6_ROM_VALUE_ENDING_FINAL_STAR_INITIAL_10; bool valid = true; };
    TheEndAssets assets_;
    // Capacity of the sprite format; active traversals use the extracted extent.
    std::array<Star, 128> stars_{};
    std::uint8_t random_ = 0;
    TheEndPhase phase_ = TheEndPhase::first_cue;
    unsigned updates_ = 0, age_ = 0, scroll_ = FF6_ROM_VALUE_ENDING_FINAL_SCROLL_ORIGIN_0, text_fade_ = 0, star_fade_ = 0;
    void initialize(Star& star);
    void move(Star& star);
};
} // namespace ff6
