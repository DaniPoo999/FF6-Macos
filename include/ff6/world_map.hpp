#pragma once
#include "rom-materials/world_normal_walk_speed.macros.inc"
#include "rom-materials/world_onfoot_camera_scale.macros.inc"
#include "rom-materials/airship_liftoff_initial_motion.macros.inc"
#include "rom-materials/airship_liftoff_camera_words.macros.inc"
#include "rom-materials/airship_liftoff_camera_center.macros.inc"
#include "rom-materials/airship_smoke_initial.macros.inc"
#include "rom-materials/airship_crash_initial.macros.inc"
#include "rom-materials/falcon_initial_position.macros.inc"
#include "rom-materials/falcon_ship_frame.macros.inc"
#include "rom-materials/world_projection_center.macros.inc"
#include "rom-materials/world_vehicle_ground_origin.macros.inc"
#include "rom-materials/approach_skyline_scroll.macros.inc"
#include "rom-materials/approach_spotlight_angles.macros.inc"
#include "rom-materials/chocobo_idle_frame.macros.inc"
#include "rom-materials/world_judgment_forward_origin.macros.inc"
#include "rom-materials/world_judgment_forward_vertical_and_end.macros.inc"
#include "rom-materials/world_judgment_random_seed.macros.inc"
#include "rom-materials/world_explosion_initial_brightness.macros.inc"
#include "ff6/field_battle.hpp"
#include "ff6/field_entrances.hpp"
#include "ff6/input.hpp"

namespace ff6 {
// Positions are world pixels (16 per terrain tile), with a toroidal 4096-pixel
// extent. Field positions remain owned by the retained field, not repurposed.
struct WorldChocobo {
    std::uint32_t heading = 0; // Degrees with eight fractional bits, modulo 360.
    std::int16_t turn = 0;
    std::uint16_t speed = 0;
    std::uint8_t fraction_x = 0, fraction_y = 0, phase = FF6_ROM_VALUE_CHOCOBO_IDLE_FRAME_0,
                 frame = FF6_ROM_VALUE_CHOCOBO_IDLE_FRAME_0, frame_wait = 0,
                 period = 0;
    std::optional<std::uint8_t> dismount;
    bool operator==(const WorldChocobo&) const = default;
};
// Authored vehicle travel retains momentum through waits and battles. These
// are camera/movement quantities, not a machine register image.
struct WorldVehiclePicture {
    std::uint16_t x = 0, y = 0;
    std::int16_t lean = 0;
    std::uint8_t ground_rows = FF6_ROM_VALUE_WORLD_VEHICLE_GROUND_ORIGIN_0, arrow_clock = 0;
    bool arrows = false, left = false;
    std::uint32_t sky_heading = 0;
    std::uint16_t altitude = 0;
    std::uint8_t sprite_frame = 0;
    std::uint32_t clock = 0;
    bool operator==(const WorldVehiclePicture&) const = default;
};
struct WorldVehicle {
    std::uint32_t heading = 0; // Degrees, eight fractional bits.
    std::uint16_t movement_heading = 0, speed = 0, altitude = 0;
    std::int16_t turn = 0, climb = 0;
    std::uint8_t fraction_x = 0, fraction_y = 0, controls = 0;
    std::uint8_t arrow_clock = 0, movement_flags = 0;
    std::int16_t camera_lean = 0;
    std::uint16_t camera_ground_rows = FF6_ROM_VALUE_WORLD_VEHICLE_GROUND_ORIGIN_0;
    std::uint8_t center_x = FF6_ROM_VALUE_WORLD_PROJECTION_CENTER_0, center_y = FF6_ROM_VALUE_WORLD_PROJECTION_CENTER_1;
    bool arrows = false, choose_direction = false, left = false;
    bool esper = false;
    std::uint8_t sprite_phase = 0, sprite_wait = 1, sprite_frame = 0;
    std::optional<WorldVehiclePicture> picture = std::nullopt;
    bool operator==(const WorldVehicle&) const = default;
};
// The deck retains heading/height, but not forward momentum (EE/6EAD, EE/004F).
struct WorldFlightResume {
    std::uint16_t heading = 0, altitude = 0;
    bool operator==(const WorldFlightResume&) const = default;
};
struct WorldAirshipTransition {
    enum class Phase { lift_ground, lift_camera, descend, land_camera, land_ground };
    Phase phase = Phase::lift_ground;
    std::uint16_t frame = 0, velocity = FF6_ROM_VALUE_AIRSHIP_LIFTOFF_INITIAL_MOTION_0, timer = 1,
                  period = FF6_ROM_VALUE_AIRSHIP_LIFTOFF_INITIAL_MOTION_1;
    std::int32_t sprite_y = FF6_ROM_VALUE_AIRSHIP_LIFTOFF_INITIAL_MOTION_2;
    std::uint8_t sprite_frame = 0, propeller = 0, brightness = 15;
    std::uint16_t numerator = FF6_ROM_VALUE_AIRSHIP_LIFTOFF_CAMERA_WORDS_0 | (FF6_ROM_VALUE_AIRSHIP_LIFTOFF_CAMERA_WORDS_1 << 8U),
                  denominator = FF6_ROM_VALUE_AIRSHIP_LIFTOFF_CAMERA_WORDS_2 | (FF6_ROM_VALUE_AIRSHIP_LIFTOFF_CAMERA_WORDS_3 << 8U),
                  end_scale = FF6_ROM_VALUE_AIRSHIP_LIFTOFF_CAMERA_WORDS_4 | (FF6_ROM_VALUE_AIRSHIP_LIFTOFF_CAMERA_WORDS_5 << 8U);
    std::uint16_t ground_rows = FF6_ROM_VALUE_AIRSHIP_LIFTOFF_CAMERA_WORDS_6 | (FF6_ROM_VALUE_AIRSHIP_LIFTOFF_CAMERA_WORDS_7 << 8U),
                  center_y = FF6_ROM_VALUE_AIRSHIP_LIFTOFF_CAMERA_CENTER_0;
    bool operator==(const WorldAirshipTransition&) const = default;
};
struct WorldState {
    std::uint8_t map = 0, facing = 2, remaining = 0, walk_phase = 0, walk_clock = 0,
                 poison_flash = 0;
    std::uint16_t x = 0, y = 0;
    std::int8_t dx = 0, dy = 0;
    FieldTilePosition encounter_origin;
    bool forest = false, minimap = true;
    std::uint32_t clock = 0;
    std::optional<WorldChocobo> chocobo;
    std::optional<WorldVehicle> vehicle;
    std::optional<WorldAirshipTransition> airship_transition;
    bool operator==(const WorldState&) const = default;
};
struct WorldTrigger {
    FieldTilePosition position;
    std::uint32_t event = 0;
};
using DoomGazeLocations = std::array<FieldTilePosition, 2>;
// Vehicle initialization advances the field RNG once, by three. Encounter
// cells are four tiles wide and are regenerated on each vehicle-world load.
DoomGazeLocations initialize_doom_gaze_locations(std::uint8_t& random_index,
    const std::array<std::uint8_t, 256>& random);
bool encounters_doom_gaze(const WorldState&, const DoomGazeLocations&, Buttons, bool defeated);
struct WorldAssets {
    Bytes terrain;
    std::array<std::array<std::uint8_t, 4>, 256> cells{};
    std::array<std::uint16_t, 256> properties{};
    std::vector<Tile> tiles, minimap, misc;
    std::array<Rgb555, 256> colors{};
    std::array<std::array<Rgb555, 16>, 8> object_colors{};
    Bytes frames;
    std::array<std::uint8_t, 16> airship_direction_frames{};
    std::array<std::uint8_t, 178> parked_airship_projection{};
    std::uint32_t airship_ground_event = 0;
    std::uint32_t airship_deck_event = 0, phoenix_cave_event = 0, kefka_tower_event = 0,
                  gogo_cave_event = 0, doom_gaze_event = 0;
    std::array<std::uint8_t, 4> airship_songs{};
    std::array<std::uint8_t, 224> airship_transition_steps{};
    std::vector<Tile> approach_tiles, airship_tiles;
    Bytes approach_cells;
    std::array<Rgb555, 16> approach_colors{};
    std::array<std::uint8_t, 16> walk_frames{};
    FieldSpriteAssets characters;
    FieldEntrances entrances;
    std::vector<WorldTrigger> triggers;
    FieldBattleData battles;
    std::array<std::uint8_t, 512> groups{};
    std::array<std::uint8_t, 128> rates{};
    std::array<std::uint8_t, 64> encounter_rules{};
    std::array<std::uint8_t, 4> songs{};
    std::array<std::uint8_t, 68> battle_zoom{};
    Bytes sine, castle_particles;
    std::vector<Tile> animation, chocobo_tiles, sky_tiles;
    Bytes sky_cells, dismount_frames;
    std::array<std::uint8_t, 4> chocobo_songs{};
    std::array<std::uint8_t, 2> trench_songs{};
    std::array<Rgb555, 16> esper_colors{};
    Bytes airship_smoke_parameters, crash_jitter;
    std::array<std::uint8_t, 43> falcon_splash_frames{};
    std::array<std::uint8_t, 4> pigeon_frames{};
    std::vector<Tile> falcon_tiles;
    std::array<Rgb555, 16> falcon_colors{};
};
struct WorldCastleAnimation {
    std::uint16_t frame = 0;
    bool emerging = false, finished = false;
};
// Vector's authored approach is a cinematic, not free vehicle control.
// Searchlight edges retain their own update phase; each picture visits one pair.
struct WorldApproach {
    std::uint16_t frame = 0;
    // The selected origin is a negative signed word with eight fractional bits.
    static_assert(FF6_ROM_VALUE_APPROACH_SKYLINE_SCROLL_0 >= 32768);
    std::int32_t skyline_scroll = int{FF6_ROM_VALUE_APPROACH_SKYLINE_SCROLL_0} - 65536;
    // Original little-endian words stay ROM-owned, including zero-valued angles.
    std::array<std::uint16_t, 4> angles{
        FF6_ROM_VALUE_APPROACH_SPOTLIGHT_ANGLES_0 | (FF6_ROM_VALUE_APPROACH_SPOTLIGHT_ANGLES_1 << 8U),
        FF6_ROM_VALUE_APPROACH_SPOTLIGHT_ANGLES_2 | (FF6_ROM_VALUE_APPROACH_SPOTLIGHT_ANGLES_3 << 8U),
        FF6_ROM_VALUE_APPROACH_SPOTLIGHT_ANGLES_4 | (FF6_ROM_VALUE_APPROACH_SPOTLIGHT_ANGLES_5 << 8U),
        FF6_ROM_VALUE_APPROACH_SPOTLIGHT_ANGLES_6 | (FF6_ROM_VALUE_APPROACH_SPOTLIGHT_ANGLES_7 << 8U)};
    std::array<std::uint8_t, 8> tops{};
    std::array<std::array<std::uint8_t, 4>, 224> windows{};
};
struct WorldSmokeParticle {
    std::uint8_t frame = 0, period = 0, wait = 0, y = 0;
    std::uint16_t x = 0;
    std::int16_t drift = 0;
};
struct WorldAirshipSmoke {
    std::uint16_t sample = 0, height = FF6_ROM_VALUE_AIRSHIP_SMOKE_INITIAL_0 * 256;
    std::uint8_t jitter_wait = FF6_ROM_VALUE_AIRSHIP_SMOKE_INITIAL_1,
                 rotor_wait = FF6_ROM_VALUE_AIRSHIP_SMOKE_INITIAL_2, rotor = 0, first = 0;
    std::int8_t jitter_x = 0, jitter_y = 0;
    std::array<WorldSmokeParticle, 5> particles{}, picture{};
};
struct WorldAirshipCrash {
    std::uint16_t frame = 0, scale = FF6_ROM_VALUE_AIRSHIP_CRASH_INITIAL_0,
                  brightness = FF6_ROM_VALUE_AIRSHIP_CRASH_INITIAL_1, target_brightness = FF6_ROM_VALUE_AIRSHIP_CRASH_INITIAL_2;
    std::uint16_t angle = 0;
    bool finished = false;
};
// EE/1186: a scene-owned two-circle flash, independent of vehicle input.
struct WorldRuinExplosion {
    unsigned frames = 0, radius = 0;
    std::uint8_t brightness = FF6_ROM_VALUE_WORLD_EXPLOSION_INITIAL_BRIGHTNESS_0, target = 15, palette_scale = 255;
    bool finished = false;
};
struct WorldFalconSplash {
    std::uint16_t x = 0;
    std::uint8_t y = 0, phase = 0, period = 0, wait = 0, frame = 0;
};
struct WorldFalconRise {
    std::int32_t x = FF6_ROM_VALUE_FALCON_INITIAL_POSITION_0;
    std::uint16_t y = FF6_ROM_VALUE_FALCON_INITIAL_POSITION_1, angle = 0, speed_x = 0, speed_y = 0;
    std::int16_t picture_x = FF6_ROM_VALUE_FALCON_INITIAL_POSITION_0 / 256,
                 picture_y = FF6_ROM_VALUE_FALCON_INITIAL_POSITION_1 / 256;
    std::uint8_t rotor = 0, rotor_wait = 1, ship_frame = FF6_ROM_VALUE_FALCON_SHIP_FRAME_0, wake_frame = 0;
    std::uint8_t first = 0, drift = 0, random = 0, brightness = 0;
    std::array<WorldFalconSplash, 10> splashes{};
    std::array<WorldFalconSplash, 5> picture{};
    bool departing = false, finished = false;
};
// Kefka's two Light of Judgment shots share the authored beam and emitter.
struct WorldJudgment {
    std::uint16_t frame = 0, start_x = FF6_ROM_VALUE_WORLD_JUDGMENT_FORWARD_ORIGIN_0,
                  start_y = (FF6_ROM_VALUE_WORLD_JUDGMENT_FORWARD_VERTICAL_AND_END_0 & 65535U),
                  end_x = (FF6_ROM_VALUE_WORLD_JUDGMENT_FORWARD_VERTICAL_AND_END_0 >> 16U);
    std::uint8_t brightness = 0, random = FF6_ROM_VALUE_WORLD_JUDGMENT_RANDOM_SEED_0, drift = 0, first = 0;
    Rgb555 light = 0;
    std::array<std::array<std::uint8_t, 2>, 224> edges{};
    std::array<WorldFalconSplash, 10> particles{};
    std::array<WorldFalconSplash, 5> picture{};
    bool reverse = false, finished = false;
};
bool advance_world_judgment(const WorldAssets&, WorldJudgment&); // One-shot sound cue.
void advance_falcon_rise(const WorldAssets& assets, WorldFalconRise& rise);
bool advance_world_ruin_explosion(WorldRuinExplosion& scene); // One-shot sound cue.
std::array<std::uint8_t, 256> world_explosion_widths(unsigned radius);
struct WorldPresentation {
    bool actor_visible = true;
    std::uint8_t target_brightness = 15;
    std::optional<WorldCastleAnimation> castle;
    bool ship = false;
    std::optional<FieldTilePosition> parked_airship = std::nullopt;
    std::optional<WorldApproach> approach = std::nullopt;
    std::optional<std::uint8_t> pose = std::nullopt;
    std::optional<WorldAirshipSmoke> smoke = std::nullopt;
    std::optional<WorldAirshipCrash> crash = std::nullopt;
    std::optional<WorldRuinExplosion> ruin = std::nullopt;
    std::optional<WorldJudgment> judgment = std::nullopt;
    std::optional<WorldFalconRise> falcon = std::nullopt;
    bool pigeon = false;
    bool falcon_graphics = false;
    std::uint8_t pigeon_phase = 0, pigeon_wait = 1;
};
struct WorldScriptState {
    std::uint32_t position = 0;
    std::uint16_t wait = 0;
    WorldPresentation presentation;
    std::uint16_t movement_visits = 0, walk_distance = 0;
    std::uint8_t walk_speed = FF6_ROM_VALUE_WORLD_NORMAL_WALK_SPEED_0, walk_fraction = 0;
    std::int8_t walk_x = 0, walk_y = 0;
    std::optional<std::pair<std::uint8_t, std::uint8_t>> battle = std::nullopt;
    std::optional<FieldTilePosition> parked_airship = std::nullopt;
};
struct WorldEventExit {
    FieldEntrance entrance;
    std::uint8_t flags = 0;
    std::uint32_t resume = 0;
    std::uint8_t vehicle = 0;
};
// Authored world commands are a data language, distinct from field events.
// Each call advances one logical picture and returns a map handoff, if any.
std::optional<WorldEventExit> advance_world_script(ByteView script, WorldScriptState& event,
                                                   WorldState& world);
std::optional<WorldEventExit> advance_world_script(ByteView script, WorldScriptState& event,
                                                   WorldState& world, FieldFlags& flags);
std::optional<WorldEventExit> resolve_world_event(ByteView script, std::uint32_t entry,
                                                  const FieldFlags& flags);
WorldAssets load_world_assets(const ContentPack& pack, std::uint8_t map, const FieldFlags& flags);
std::uint16_t world_tile_properties(const WorldAssets& assets, const WorldState& state, int dx = 0,
                                    int dy = 0);
void enter_world_chocobo(WorldState& world);
void advance_world_vehicle(const WorldAssets& assets, WorldState& world, Buttons buttons,
                           FieldFlags& flags, bool player_control = false);
void begin_airship_liftoff(WorldState& world);
void begin_airship_landing(WorldState& world);
// Returns true at the completed on-foot/flying handoff. Presentation and input
// remain blocked until the rotor/camera sequence finishes.
bool advance_airship_transition(const WorldAssets& assets, WorldState& world);
void advance_world_approach(const WorldAssets& assets, WorldApproach& approach, WorldState& world);
void compose_world_approach(const WorldAssets& assets, const WorldApproach& approach, RasterScene& scene);
void advance_airship_smoke(const WorldAssets& assets, WorldAirshipSmoke& smoke);
void advance_airship_crash(const WorldAssets& assets, WorldAirshipCrash& crash, WorldState& world);
// Retain the picture published before the next authored movement update.
void prepare_world_vehicle_picture(WorldState& world);
int world_vehicle_lean(std::int16_t turn, std::uint16_t speed);
int world_vehicle_camera_lean(const WorldVehicle& vehicle);
int world_vehicle_ground_rows(const WorldVehicle& vehicle);
// Returns true only when the dismount presentation has finished.
bool advance_world_chocobo(const WorldAssets& assets, WorldState& world, Buttons buttons);
// Returns true on a newly admitted step. Direction is sampled only at tile
// boundaries, in the original right/left/down/up priority; blocked steps face
// the obstruction without moving or advancing encounter RNG.
bool advance_world_walk(const WorldAssets& assets, WorldState& state, Buttons buttons);
std::optional<FieldBattleRequest>
select_world_encounter(const WorldAssets& assets, const WorldState& world,
                       FieldBattleRandom& random, std::uint16_t& counter,
                       std::uint8_t equipment_effects, bool continue_music,
                       const std::bitset<512>& encountered_formations = {});
RasterScene compose_world_scene(const WorldAssets& assets, const WorldState& state,
                                std::uint8_t graphics, std::uint8_t palette,
                                std::uint8_t brightness = 15, std::uint16_t camera_scale = FF6_ROM_VALUE_WORLD_ONFOOT_CAMERA_SCALE_0,
                                const WorldPresentation& presentation = {});
} // namespace ff6
