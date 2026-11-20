#include "ff6/ending_flight.hpp"
#include "rom-materials/ending_flight_add_arithmetic.macros.inc"
#include "rom-materials/ending_credit_word_speed.macros.inc"
#include "rom-materials/ending_flight_cloud_scale_mask.macros.inc"
#include "rom-materials/ending_flight_sea_scroll_mask.macros.inc"
#include "rom-materials/ending_flight_land_scroll_mask.macros.inc"
#include "rom-materials/ending_flight_airship_scroll_mask.macros.inc"
#include "rom-materials/ending_flight_jet_tiles_mask.macros.inc"
#include "rom-materials/ending_flight_cloud_tiles_mask.macros.inc"
#include "rom-materials/ending_flight_actor_priorities.macros.inc"
#include "rom-materials/ending_flight_text_palette_banks.macros.inc"
#include "rom-materials/ending_flight_white_palette_bank.macros.inc"
#include "rom-materials/ending_flight_jet_palette_banks.macros.inc"
#include "rom-materials/ending_flight_white_fade_period.macros.inc"
#include "rom-materials/ending_flight_exit_fade_wait.macros.inc"
#include "rom-materials/ending_flight_splash_bound.macros.inc"
#include "rom-materials/ending_flight_splash_period_mask.macros.inc"
#include "rom-materials/ending_flight_sea_sky_origin.macros.inc"
#include "rom-materials/ending_flight_land_sky_origin.macros.inc"
#include "rom-materials/ending_flight_sky_period_mask.macros.inc"
#include "rom-materials/ending_flight_jet_period_mask.macros.inc"
#include "rom-materials/ending_flight_secondary_view.macros.inc"
#include "rom-materials/ending_flight_projection_origins.macros.inc"
#include "rom-materials/ending_flight_projection_boundary.macros.inc"
#include "rom-materials/ending_bird_deadlines.macros.inc"
#include "rom-materials/ending_flight_initial_objects.macros.inc"
#include "rom-materials/ending_flight_spawn_bytes.macros.inc"
#include "rom-materials/ending_flight_spawn_words.macros.inc"
#include "rom-materials/ending_flight_spawn_deadlines.macros.inc"
#include "rom-materials/ending_flight_animated_tiles.macros.inc"
#include "rom-materials/ending_flight_animation_cells.macros.inc"
#include "rom-materials/ending_flight_angles.macros.inc"
#include "rom-materials/ending_flight_above_view.macros.inc"
#include "rom-materials/ending_flight_jet_view.macros.inc"
#include "rom-materials/ending_credit_horizon.macros.inc"
#include <algorithm>
#include <cstdlib>
#include "rom-materials/ending_credit_word_duration.macros.inc"
#include "rom-materials/ending_credit_word_deadlines.macros.inc"
#include "rom-materials/ending_credit_word_offset.macros.inc"

namespace ff6 {
namespace {
using Kind = EndingFlightScene;
using ObjectId = EndingFlightObject;
// Original palette addresses identify banks; origin/stride are format metadata.
constexpr unsigned text_palette_first = (FF6_ROM_VALUE_ENDING_FLIGHT_TEXT_PALETTE_BANKS_0 - 0x3149U) / 32U;
constexpr unsigned text_palette_second = (FF6_ROM_VALUE_ENDING_FLIGHT_TEXT_PALETTE_BANKS_1 - 0x3149U) / 32U;
// Initialization, loading and both fade directions must select the same pair.
static_assert([] {
    constexpr std::array<unsigned, 10> banks{
#include "rom-materials/ending_flight_text_palette_banks.inc"
    };
    for (unsigned i = 2; i < banks.size(); ++i)
        if (banks[i] != banks[i % 2]) return false;
    return true;
}());
// Default scenery priority is shared by four source task creators.
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_PRIORITIES_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_PRIORITIES_1);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_PRIORITIES_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_PRIORITIES_2);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_PRIORITIES_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_DEFAULT_PRIORITIES_3);
// Coalesced text/jet palette periods must agree across all original callers.
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_9);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_10);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_11);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_12);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_1);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_2);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_3);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_4);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_5);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_6);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_7);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_8);
unsigned index(ObjectId id) { return static_cast<unsigned>(id); }
void require(bool value, const char* why) { if (!value) throw std::invalid_argument(why); }
std::int16_t coordinate(int value) {
    const unsigned bits = static_cast<unsigned>(value) & 8191U;
    return static_cast<std::int16_t>(bits < 4096 ? int(bits) : int(bits) - 8192);
}
std::int16_t signed_word(unsigned value) {
    const auto word = value & 65535U;
    return static_cast<std::int16_t>(word < 32768 ? int(word) : int(word) - 65536);
}
// Assemble signed coordinates from generated data bytes without packed reads.
constexpr int camera_coordinate(int low, int high) {
    return low + (high < 128 ? high : high - 256) * 256;
}
Rgb555 gray(unsigned value) { return static_cast<Rgb555>(value * 1057); }
// C3/DDD2 builds two equal sampling rows per depth. Division uses the unsigned
// magnitude before restoring the sine's sign, including the zero-depth case.
std::int16_t coefficient(int sine, unsigned divisor) {
    unsigned quotient = divisor ? unsigned(std::abs(sine)) / 2 / divisor : 65535;
    return signed_word(sine < 0 ? 65536U - quotient : quotient);
}
}
void EndingFlight::Palette::to(const CutscenePalette& next, std::uint8_t period) {
    target = next; fade = CutscenePaletteFade{period};
}
unsigned EndingFlight::duration() const {
    constexpr std::array<unsigned, 10> frames{
#include "rom-materials/ending_flight_durations.inc"
};
    return frames[static_cast<unsigned>(assets_.kind)];
}
bool EndingFlight::complete() const { return updates_ >= duration() + FF6_ROM_VALUE_ENDING_FLIGHT_EXIT_FADE_WAIT_0; }
EndingFlight::EndingFlight(EndingFlightAssets assets) : assets_(std::move(assets)) {
    const auto kind = assets_.kind;
    require(static_cast<unsigned>(kind) <= static_cast<unsigned>(Kind::jet), "Invalid flight scene");
    require(assets_.terrain.cells.size() == 128 * 128 && assets_.terrain.lines.size() == 224,
            "Flight terrain is incomplete");
    for (const auto& stream : assets_.animations) {
        for (const auto& frame : stream.frames)
            require(frame.resource < stream.groups.size(), "Invalid flight image identity");
        if (!stream.frames.empty()) require(stream.frames.back().duration >= 254, "Unterminated flight image stream");
    }
    for (unsigned p = 0; p < 8; ++p) {
        background_[p].colors = background_[p].target = assets_.colors[p];
        objects_[p].colors = objects_[p].target = p == text_palette_first || p == text_palette_second ? CutscenePalette{} : assets_.object_colors[p];
        background_[p].fade.finished = objects_[p].fade.finished = true;
    }
    if (!assets_.camera.empty()) camera_wait_ = assets_.camera[0].duration;
    switch (kind) {
    case Kind::clouds: slope_ = FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_0; depth_ = FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_1; break;
    case Kind::spiral:
        static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_2 == FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_3);
        slope_ = depth_ = FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_2; break;
    case Kind::above_clouds:
        slope_ = FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_4; depth_ = FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_5;
        camera_ = {
            camera_coordinate(FF6_ROM_VALUE_ENDING_FLIGHT_ABOVE_VIEW_0, FF6_ROM_VALUE_ENDING_FLIGHT_ABOVE_VIEW_1),
            camera_coordinate(FF6_ROM_VALUE_ENDING_FLIGHT_ABOVE_VIEW_2, FF6_ROM_VALUE_ENDING_FLIGHT_ABOVE_VIEW_3),
            camera_coordinate(FF6_ROM_VALUE_ENDING_FLIGHT_ABOVE_VIEW_4, FF6_ROM_VALUE_ENDING_FLIGHT_ABOVE_VIEW_5),
            camera_coordinate(FF6_ROM_VALUE_ENDING_FLIGHT_ABOVE_VIEW_6, FF6_ROM_VALUE_ENDING_FLIGHT_ABOVE_VIEW_7)};
        break;
    case Kind::boat: depth_ = FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_6; break;
    case Kind::sea_airship:
        slope_ = FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_7; depth_ = FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_8;
        rotation_ = FF6_ROM_VALUE_ENDING_FLIGHT_ANGLES_0; break;
    case Kind::land_birds:
        slope_ = FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_9; depth_ = FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_10;
        rotation_ = FF6_ROM_VALUE_ENDING_FLIGHT_ANGLES_1; break;
    case Kind::landscape: depth_ = FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_11; break;
    case Kind::big_airship:
        static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_12 == FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_14);
        slope_ = FF6_ROM_VALUE_ENDING_FLIGHT_PLANES_13; break;
    case Kind::jet:
        camera_ = {
            camera_coordinate(FF6_ROM_VALUE_ENDING_FLIGHT_JET_VIEW_0, FF6_ROM_VALUE_ENDING_FLIGHT_JET_VIEW_1),
            camera_coordinate(FF6_ROM_VALUE_ENDING_FLIGHT_JET_VIEW_2, FF6_ROM_VALUE_ENDING_FLIGHT_JET_VIEW_3),
            camera_coordinate(FF6_ROM_VALUE_ENDING_FLIGHT_JET_VIEW_4, FF6_ROM_VALUE_ENDING_FLIGHT_JET_VIEW_5),
            camera_coordinate(FF6_ROM_VALUE_ENDING_FLIGHT_JET_VIEW_6, FF6_ROM_VALUE_ENDING_FLIGHT_JET_VIEW_7)};
        break;
    default: break;
    }
    if (kind == Kind::boat || kind == Kind::sea_airship || kind == Kind::land_birds) {
        for (unsigned i = 0; i < 6; ++i) {
            spawn(static_cast<ObjectId>(index(ObjectId::bird0) + i), 0, int(assets_.birds[i].y), Motion::oscillate);
            actors_.back().base = int(assets_.birds[i].x) * 256;
            actors_.back().scroll = kind == Kind::boat;
            // C3/CB70's unusual initial-phase read is deterministic here: the
            // six priority-two birds occupy slots $20..$2A in all three callers.
            // Extract those six data values during the user's build. Playback
            // consumes phase angles without reading or executing original code.
            constexpr std::array<unsigned, 6> initial_phase{
#include "rom-materials/ending_bird_initial_phases.inc"
};
            actors_.back().phase = initial_phase[i];
        }
    }
    if (kind == Kind::sea_airship) {
        // The task's horizontal reset agrees with the scene initializer.
        static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_0 == FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_1);
        spawn(ObjectId::left_airship, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_0,
              2 * FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_2, Motion::splash_bob);
        actors_.back().base = FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_2 * 256;
        spawn(ObjectId::shadow, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_3, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_4, Motion::linear, 0, 0, 0, FF6_ROM_VALUE_ENDING_FLIGHT_ACTOR_PRIORITIES_2);
    }
    if (kind == Kind::land_birds) spawn(ObjectId::right_airship, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_5, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_6, Motion::bob);
    if (kind == Kind::big_airship) {
        spawn(ObjectId::propeller_left, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_7, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_8, Motion::bob);
        spawn(ObjectId::propeller_right, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_9, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_10, Motion::bob);
        spawn(ObjectId::propeller_left, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_11, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_12, Motion::bob);
        spawn(ObjectId::propeller_right, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_13, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_14, Motion::bob);
        spawn(ObjectId::big_airship, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_15, FF6_ROM_VALUE_ENDING_FLIGHT_INITIAL_OBJECTS_16, Motion::bob);
    }
}
void EndingFlight::spawn(ObjectId animation, int x, int y, Motion motion,
                          int vx, int vy, unsigned life, unsigned priority) {
    const auto& stream = assets_.animations[index(animation)];
    require(!stream.frames.empty(), "Flight actor has no animation");
    Object object{}; object.animation = animation; object.motion = motion;
    object.cursor = {0, stream.frames[0].duration};
    object.x = static_cast<std::uint16_t>(x * 256); object.y = static_cast<std::uint16_t>(y * 256);
    object.base = y * 256; object.vx = vx; object.vy = vy; object.life = life; object.priority = priority;
    actors_.push_back(object);
}
void EndingFlight::advance_camera() {
    if (!assets_.camera.empty()) {
        const auto& motion = assets_.camera[camera_frame_];
        camera_.center_x = coordinate(camera_.center_x + motion.x);
        camera_.center_y = coordinate(camera_.center_y + motion.y);
        camera_.scroll_x = coordinate(camera_.scroll_x + motion.x);
        camera_.scroll_y = coordinate(camera_.scroll_y + motion.y);
        rotation_ = static_cast<std::uint8_t>(int(rotation_) + motion.rotation);
        slope_ = static_cast<std::uint16_t>(int(slope_) + motion.slope);
        depth_ = static_cast<std::uint16_t>(int(depth_) + motion.depth);
        // Match the shared animation duration language: timed entries include
        // their zero-counter visit; FE holds and FF restarts the stream.
        if (camera_wait_ == 255) { camera_frame_ = 0; camera_wait_ = assets_.camera[0].duration; }
        else if (camera_wait_ < 254) {
            if (camera_wait_) --camera_wait_;
            else {
                require(++camera_frame_ < assets_.camera.size(), "Camera exceeded motion data");
                camera_wait_ = assets_.camera[camera_frame_].duration;
            }
        }
    }
    if (assets_.kind == Kind::above_clouds) {
        camera_.center_x = coordinate(camera_.center_x - 1);
        camera_.scroll_x = coordinate(camera_.scroll_x - 1);
    }
    if (assets_.kind == Kind::big_airship && updates_ % (FF6_ROM_VALUE_ENDING_FLIGHT_AIRSHIP_SCROLL_MASK_0 + 1) == 0) {
        camera_.center_y = coordinate(camera_.center_y - 1);
        camera_.scroll_y = coordinate(camera_.scroll_y - 1);
    }
    if (assets_.kind == Kind::jet && updates_ % (FF6_ROM_VALUE_ENDING_FLIGHT_JET_PERIOD_MASK_0 + 1) == 0) {
        camera_.center_x = coordinate(camera_.center_x + 1);
        camera_.center_y = coordinate(camera_.center_y - 1);
    }
}
void EndingFlight::advance_object(Object& object) {
    if (object.delay) { --object.delay; return; }
    if (object.life && object.age >= object.life) return;
    const int sine = assets_.sine[object.phase & 255U];
    switch (object.motion) {
    case Motion::bob: object.y = static_cast<std::uint16_t>(object.base + sine / 16); break;
    case Motion::splash_bob:
        // This owner calls the moving-animation helper after assigning the bob
        // center; SpeedY is added a second time. The other airship owner doesn't.
        object.y = static_cast<std::uint16_t>(2 * object.base + sine / 16); break;
    case Motion::oscillate:
        object.x = static_cast<std::uint16_t>(2 * object.base + assets_.sine[(object.phase + 64) & 255U] / 16);
        break;
    case Motion::flying: {
        constexpr std::array<unsigned, 7> ends{
#include "rom-materials/ending_bird_deadlines.inc"
};
        constexpr std::array<int, 7> x{
#include "rom-materials/ending_bird_velocity_x.inc"
};
        constexpr std::array<int, 7> y{
#include "rom-materials/ending_bird_velocity_y.inc"
};
        const auto found = std::upper_bound(ends.begin(), ends.end(), object.age);
        const auto part = std::min<std::size_t>(static_cast<std::size_t>(found - ends.begin()), 6);
        object.vx = x[part]; object.vy = y[part];
        if (object.age >= FF6_ROM_VALUE_ENDING_BIRD_DEADLINES_6 && object.x / 256 == 0) { object.life = object.age; return; }
        break;
    }
    default: break;
    }
    object.x = static_cast<std::uint16_t>(int(object.x) + object.vx);
    object.y = static_cast<std::uint16_t>(int(object.y) + object.vy);
    ++object.phase; ++object.age;
    advance_cutscene_animation(object.cursor, assets_.animations[index(object.animation)].frames);
}
bool EndingFlight::advance() {
    if (complete()) return false;
    const auto kind = assets_.kind;
    for (const auto& page : assets_.pages) {
        if (updates_ == page.start)
            for (unsigned p : {text_palette_first, text_palette_second}) objects_[p].to(assets_.object_colors[p], FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0);
        if (updates_ == page.start + FF6_ROM_VALUE_ENDING_CREDIT_WORD_DEADLINES_1)
            for (unsigned p : {text_palette_first, text_palette_second}) objects_[p].to({}, FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0);
    }
    if (kind == Kind::distant_airship && updates_ == FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_DEADLINES_0) {
        spawn(ObjectId::tiny_airship, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_0, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_1, Motion::linear,
              signed_word(FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_WORDS_0), signed_word(FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_WORDS_1),
              FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_WORDS_2, FF6_ROM_VALUE_ENDING_FLIGHT_ACTOR_PRIORITIES_0);
        spawn(ObjectId::tiny_shadow, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_2, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_3, Motion::linear,
              signed_word(FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_WORDS_0), signed_word(FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_WORDS_1),
              FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_WORDS_2, FF6_ROM_VALUE_ENDING_FLIGHT_ACTOR_PRIORITIES_1);
    }
    if (kind == Kind::distant_airship && updates_ == FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_DEADLINES_1) {
        CutscenePalette white; white.fill(0x7fff); white[0] = 0;
        background_[(FF6_ROM_VALUE_ENDING_FLIGHT_WHITE_PALETTE_BANK_0 - 0x3049U) / 32U].to(white, FF6_ROM_VALUE_ENDING_FLIGHT_WHITE_FADE_PERIOD_0);
    }
    if (kind == Kind::boat && updates_ == FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_DEADLINES_2) {
        spawn(ObjectId::boat_left, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_4, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_5, Motion::linear,
              signed_word(FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_WORDS_3), 0, 0, FF6_ROM_VALUE_ENDING_FLIGHT_ACTOR_PRIORITIES_2);
        spawn(ObjectId::boat_right, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_6, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_7, Motion::linear,
              signed_word(FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_WORDS_4), 0, 0, FF6_ROM_VALUE_ENDING_FLIGHT_ACTOR_PRIORITIES_3);
        actors_.back().delay = FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_WORDS_5;
    }
    if (kind == Kind::land_birds && updates_ == FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_DEADLINES_3)
        for (const auto [x, y] : {std::pair{FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_8, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_9},
                                {FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_10, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_11},
                                {FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_12, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_13},
                                {FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_14, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_15}})
            spawn(ObjectId::flying_bird, x, y, Motion::flying);
    if (kind == Kind::sea_airship && (updates_ + 1) % 256 < FF6_ROM_VALUE_ENDING_FLIGHT_SPLASH_BOUND_0 &&
        updates_ % (FF6_ROM_VALUE_ENDING_FLIGHT_SPLASH_PERIOD_MASK_0 + 1) == 0)
        spawn(ObjectId::splash, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_16, FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_17, Motion::linear,
              FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_WORDS_6, signed_word(FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_WORDS_7), FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_BYTES_18, FF6_ROM_VALUE_ENDING_FLIGHT_ACTOR_PRIORITIES_0);
    if (kind == Kind::jet && updates_ == FF6_ROM_VALUE_ENDING_FLIGHT_SPAWN_DEADLINES_4)
        for (unsigned p : {(FF6_ROM_VALUE_ENDING_FLIGHT_JET_PALETTE_BANKS_0 - 0x3049U) / 32U,
                           (FF6_ROM_VALUE_ENDING_FLIGHT_JET_PALETTE_BANKS_1 - 0x3049U) / 32U,
                           (FF6_ROM_VALUE_ENDING_FLIGHT_JET_PALETTE_BANKS_2 - 0x3049U) / 32U}) background_[p].to({}, FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0);
    advance_camera();
    for (auto& object : actors_) advance_object(object);
    std::erase_if(actors_, [](const auto& actor) { return actor.life && actor.age >= actor.life; });
    for (auto& palette : background_) palette.fade.advance(palette.colors, palette.target);
    for (auto& palette : objects_) palette.fade.advance(palette.colors, palette.target);
    ++updates_;
    return true;
}
RasterScene EndingFlight::scene() const {
    RasterScene out;
    const auto kind = assets_.kind;
    auto terrain = assets_.terrain;
    for (unsigned p = 0; p < 8; ++p)
        std::copy(background_[p].colors.begin(), background_[p].colors.end(), terrain.colors.begin() + p * 16);
    if (kind == Kind::above_clouds) terrain.cells[FF6_ROM_VALUE_ENDING_FLIGHT_ANIMATION_CELLS_0] = updates_ % (FF6_ROM_VALUE_ENDING_FLIGHT_CLOUD_TILES_MASK_0 + 1)
        ? FF6_ROM_VALUE_ENDING_FLIGHT_ANIMATED_TILES_0 : FF6_ROM_VALUE_ENDING_FLIGHT_ANIMATED_TILES_1;
    if (kind == Kind::jet) {
        terrain.cells[FF6_ROM_VALUE_ENDING_FLIGHT_ANIMATION_CELLS_1] = updates_ % (FF6_ROM_VALUE_ENDING_FLIGHT_JET_TILES_MASK_0 + 1)
            ? FF6_ROM_VALUE_ENDING_FLIGHT_ANIMATED_TILES_2 : FF6_ROM_VALUE_ENDING_FLIGHT_ANIMATED_TILES_3;
        terrain.cells[FF6_ROM_VALUE_ENDING_FLIGHT_ANIMATION_CELLS_2] = updates_ % (FF6_ROM_VALUE_ENDING_FLIGHT_JET_TILES_MASK_0 + 1)
            ? FF6_ROM_VALUE_ENDING_FLIGHT_ANIMATED_TILES_4 : FF6_ROM_VALUE_ENDING_FLIGHT_ANIMATED_TILES_5;
    }
    const int sine = assets_.sine[rotation_], cosine = assets_.sine[(rotation_ + 64U) & 255U];
    for (unsigned y = 0; y < 224; ++y) {
        const auto depth = static_cast<std::uint16_t>(unsigned{depth_} - unsigned{slope_} * (111 - y / 2));
        const auto a = coefficient(cosine, depth >> 8), b = coefficient(sine, depth >> 8);
        auto row = make_affine_perspective_line(camera_, a, b, signed_word(65536U - unsigned(std::uint16_t(b))), a,
                                                static_cast<std::uint16_t>(y + 1));
        // All five original display bands share this projection boundary.
        static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_PROJECTION_BOUNDARY_0 == FF6_ROM_VALUE_ENDING_FLIGHT_PROJECTION_BOUNDARY_1);
        static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_PROJECTION_BOUNDARY_0 == FF6_ROM_VALUE_ENDING_FLIGHT_PROJECTION_BOUNDARY_2);
        static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_PROJECTION_BOUNDARY_0 == FF6_ROM_VALUE_ENDING_FLIGHT_PROJECTION_BOUNDARY_3);
        static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_PROJECTION_BOUNDARY_0 == FF6_ROM_VALUE_ENDING_FLIGHT_PROJECTION_BOUNDARY_4);
        if (kind == Kind::above_clouds && y < FF6_ROM_VALUE_ENDING_FLIGHT_PROJECTION_BOUNDARY_0)
            row = make_perspective_line({
                camera_coordinate(FF6_ROM_VALUE_ENDING_FLIGHT_SECONDARY_VIEW_0, FF6_ROM_VALUE_ENDING_FLIGHT_SECONDARY_VIEW_1),
                camera_coordinate(FF6_ROM_VALUE_ENDING_FLIGHT_SECONDARY_VIEW_2, FF6_ROM_VALUE_ENDING_FLIGHT_SECONDARY_VIEW_3),
                camera_coordinate(FF6_ROM_VALUE_ENDING_FLIGHT_SECONDARY_VIEW_4, FF6_ROM_VALUE_ENDING_FLIGHT_SECONDARY_VIEW_5),
                camera_coordinate(FF6_ROM_VALUE_ENDING_FLIGHT_SECONDARY_VIEW_6, FF6_ROM_VALUE_ENDING_FLIGHT_SECONDARY_VIEW_7)}, static_cast<std::int16_t>(FF6_ROM_VALUE_ENDING_FLIGHT_PROJECTION_ORIGINS_0 + (updates_ + FF6_ROM_VALUE_ENDING_FLIGHT_CLOUD_SCALE_MASK_0) / (FF6_ROM_VALUE_ENDING_FLIGHT_CLOUD_SCALE_MASK_0 + 1)),
                                        static_cast<std::uint16_t>(y + 1));
        if (kind == Kind::jet)
            row = make_perspective_line(camera_, static_cast<std::int16_t>(FF6_ROM_VALUE_ENDING_FLIGHT_PROJECTION_ORIGINS_1 + updates_ * 2),
                                        static_cast<std::uint16_t>(y + 1));
        if (assets_.sky && y < FF6_ROM_VALUE_ENDING_CREDIT_HORIZON_0) row.enabled = false;
        terrain.lines[y] = row;
    }
    out.affine_layers.push_back(std::move(terrain));
    if (assets_.sky) {
        auto sky = *assets_.sky;
        for (unsigned p = 0; p < 8; ++p) sky.palettes[p] = background_[p].colors;
        // Both original sky owners choose the same vertical origin.
        static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SEA_SKY_ORIGIN_0 == FF6_ROM_VALUE_ENDING_FLIGHT_LAND_SKY_ORIGIN_0);
        for (auto& line : sky.lines) {
            line.offset_x = kind == Kind::sea_airship ? -int((updates_ + FF6_ROM_VALUE_ENDING_FLIGHT_SEA_SCROLL_MASK_0) / (FF6_ROM_VALUE_ENDING_FLIGHT_SEA_SCROLL_MASK_0 + 1)) :
                            kind == Kind::land_birds ? int((updates_ + FF6_ROM_VALUE_ENDING_FLIGHT_LAND_SCROLL_MASK_0) / (FF6_ROM_VALUE_ENDING_FLIGHT_LAND_SCROLL_MASK_0 + 1)) : 0;
            line.offset_y = kind == Kind::big_airship ? FF6_ROM_VALUE_ENDING_FLIGHT_PROJECTION_ORIGINS_2 + int((updates_ + FF6_ROM_VALUE_ENDING_FLIGHT_SKY_PERIOD_MASK_0) / (FF6_ROM_VALUE_ENDING_FLIGHT_SKY_PERIOD_MASK_0 + 1)) : FF6_ROM_VALUE_ENDING_FLIGHT_SEA_SKY_ORIGIN_0;
        }
        out.layers.push_back(std::move(sky));
        out.operand_fallback_rows.resize(224);
        constexpr std::array<unsigned, 15> counts{
#include "rom-materials/credits_gray_lengths.inc"
};
        constexpr std::array<unsigned, 15> values{
#include "rom-materials/credits_gray_shades.inc"
};
        unsigned row = 0;
        for (unsigned i = 0; i < counts.size(); ++i)
            for (unsigned n = 0; n < counts[i] && row < 224; ++n) out.operand_fallback_rows[row++] = gray(values[i]);
    }
    if (kind == Kind::above_clouds) {
        // Resolve the authored fixed-color bands into semantic row effects;
        // the bottom region resumes subtraction against the lettering plane.
        out.color_effect_rows.resize(224);
        constexpr std::array<unsigned, 27> counts{
#include "rom-materials/ending_fog_lengths.inc"
};
        constexpr std::array<unsigned, 27> values{
#include "rom-materials/ending_fog_colors.inc"
};
        unsigned row = 0;
        for (unsigned i = 0; i < 27; ++i)
            for (unsigned n = 0; n < counts[i]; ++n)
                out.color_effect_rows[row++] = RasterColorEffect{gray(values[i]),
                    static_cast<ColorBlend>(FF6_ROM_VALUE_ENDING_FLIGHT_ADD_ARITHMETIC_0), false};
    }
    out.objects.emplace(); out.objects->lines.resize(224); out.objects->operand = true;
    auto draw = [&](const std::vector<CutsceneSprite>& group, int x, int y) {
        for (const auto& piece : group) {
            auto image = piece.image;
            image.x = wrap8(static_cast<unsigned>(image.x + x));
            image.y = wrap8(static_cast<unsigned>(image.y + y)) - 1;
            if (image.y >= 224) image.y -= 256;
            image.colors = objects_[piece.palette].colors;
            out.objects->sprites.push_back(std::move(image));
        }
    };
    // Credit tasks run at priority zero, before every scenery actor. Background
    // depth hides their opaque plane, but their texels still supply color math.
    for (const auto& page : assets_.pages)
        if (updates_ > page.start && updates_ <= page.start + FF6_ROM_VALUE_ENDING_CREDIT_WORD_DURATION_0) {
            const unsigned age = updates_ - page.start;
            draw(page.lettering, 0, FF6_ROM_VALUE_ENDING_CREDIT_WORD_OFFSET_0 -
                 int(std::min(age, unsigned{FF6_ROM_VALUE_ENDING_CREDIT_WORD_DEADLINES_0}) /
                     (256U / (65536U - FF6_ROM_VALUE_ENDING_CREDIT_WORD_SPEED_0))));
        }
    for (unsigned priority : {1U, 2U, 3U})
        for (const auto& actor : actors_) {
            if (actor.delay || actor.priority != priority) continue;
            const auto& animation = assets_.animations[index(actor.animation)];
            draw(animation.groups.at(animation.frames.at(actor.cursor.frame).resource),
                 int(actor.x / 256) - (actor.scroll ? camera_.scroll_x : 0), actor.y / 256);
        }
    const unsigned intensity = complete() ? 0 : updates_ > duration() ? duration() + FF6_ROM_VALUE_ENDING_FLIGHT_EXIT_FADE_WAIT_0 - updates_ : std::min(15U, updates_);
    out.intensity.assign(224, static_cast<std::uint8_t>(intensity));
    return out;
}
} // namespace ff6
